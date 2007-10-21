#!/usr/bin/env ruby
require 'eruby'
require 'fcgi'
require 'logger'
require 'benchmark'
require 'time'
require 'stringio'
require 'rhp.so'

=begin
Jobbigast:
 24.07     0.13      0.13       22     5.91    15.45  Hash#each
 22.22     0.25      0.12     1144     0.10     0.12  String#xml_safe
 12.96     0.32      0.07     3080     0.02     0.02  FCGI::Stream#write
  5.56     0.35      0.03       44     0.68     2.73  Logger#add
  3.70     0.37      0.02       44     0.45     0.68  MonitorMixin.mon_exit
  3.70     0.39      0.02       44     0.45     0.45  Logger::Formatter#format_datetime
  3.70     0.41      0.02     4576     0.00     0.00  String#gsub
  3.70     0.43      0.02       22     0.91     0.91  FCGI::Stream#print
  1.85     0.44      0.01       44     0.23     0.23  Array#each
=end

# Object additions
class Object
  def xml_safe; self.to_s.xml_safe; end
  def uri_safe; self.to_s.uri_safe; end
end

# String additions
class String
  def uri_safe
    self.gsub(/[^a-zA-Z0-9_\-.]/n){ sprintf("%%%02X", $&.unpack("C")[0]) }
  end
end

# Array additions
class Array
  def sum
    f = 0.0
    self.each {|v| f += v }
    f
  end
end

# Error additions
class Exception
  def info
    "#{self.class}: #{message}#$/#{backtrace.join($/)}"
  end
end

# Manages erb/rhp files/templates
module RHP
  # Singleton cache manager
  class FileManager
    class << self
      #@@compiler = ERuby::Compiler.new
      @@compiler = RHP::Compiler.new
      
      def run(filename, binding)
        file = open(filename)
        begin
          Kernel::eval(compile(file), binding, filename)
        ensure
          file.close
        end
      end
      
      def compile(file)
        @@compiler.compile_file(file)
      end
    end
  end
  

  # Raised when trying to send headers when output has starded
  class ResponseAlreadyStartedError < StandardError; end

  # Reponse object. A RHP file is processed within a Response binding.
  class Response
    attr_reader :request, :buffered, :out, :has_sent_headers
  	attr_accessor :headers
	
    def initialize(request)
      @headers = {}
      @request = request
      @has_sent_headers = false
      @buffered = false
      @out = request.out
    end
  
    # Start buffering output instead of sending it directly.
    # Decreases performance but may make things easier in some cases where 
    # for example headers need to be modified after a few prints.
    def buffer!
      if not @buffered
        if @has_sent_headers
          raise ResponseAlreadyStartedError, 'Headers has already been sent'
        end
        @out = StringIO.new
        @buffered = true
      end
    end
  
    def print(*o)
      if not @has_sent_headers and not @buffered
        self.send_headers!
      end
      @out.print o
    end
  
    def write(o)
      if not @has_sent_headers and not @buffered
        self.send_headers!
      end
      @out.write o
    end
  
    def puts(s)
      self.print s
    end
  
    def send_headers!
      if not @has_sent_headers and not @buffered
        _send_headers!
      end
    end
  
    def include(filename)
      RHP::FileManager.run(filename, binding)
    end
  
    def get_binding
      binding
    end
  
    def has_begun?
      @has_sent_headers
    end
  
    def response
      self
    end
  
    def send!
      if @buffered
        @headers['Content-Length'] = @out.size
        self._send_headers!
        @out.rewind
        @request.out.write @out.read
      elsif not @has_sent_headers
        self._send_headers!
      end
    end
  
    protected
  
    def _send_headers!
      @headers.each_pair {|name, value|
        if value.is_a? Array
          value.each {|v|
            @request.out.write "#{name}: #{v}\r\n"
          }
        else
          @request.out.write "#{name}: #{value}\r\n"
        end
      }
      @request.out.write "\r\n\r\n"
      @has_sent_headers = true
    end
  end

  # Handles http transactions
  class FCGIHandler
    def initialize(logger=nil)
      if logger
        $log = logger
      elsif $log == nil
        $log = Logger.new(STDERR)
        $log.level = Logger::DEBUG
      end
    end
  
    def run()
      start_time = Time.now
    
      # XXX debug/info stuff. Maybe remove in release...
      _level_info = $log.level <= Logger::INFO
      request_count = 0.0
      rs_time = start_time
      request_times = []
      request_times_size = 100
    
      # The accept loop
      FCGI.each {|request|
        # Keep some stats while in info/debug mode
        if _level_info
          $log.info { 'Accepted %s:%d %s' % [
                      request.env['REMOTE_ADDR'], 
                      request.env['REMOTE_PORT'], 
                      request.env['REQUEST_URI']]
                     }
          request_count += 1
          rs_time = Time.now
        end
      
        # Setup a response object
        response = Response.new request
      
        # Redirect stdout to fcgi output stream
        $stdout = request.out
      
        # Execute ERB
        filename = request.env['SCRIPT_FILENAME']
        begin
          RHP::FileManager.run(filename, response.get_binding)
          #code = @compiler.compile_file(file)
          #eval(code, response.get_binding, filename)
          response.send!
        rescue Exception
          self.on_error(response)
        end
      
        # Finish request
        request.finish
      
        # Some info/debug logging
        if _level_info
          rtime = Time.now.to_f-rs_time.to_f
          if request_times.length == request_times_size
            request_times.shift
          end
          request_times.push rtime
          rs = 1.0/rtime
          uptime = Time.now.to_f-start_time.to_f
          $log.info { 'perform: %.1f r/s, load: %.4f r, processed: %.0f r, uptime: %.1f s' % [
                      rs,
                      request_times.sum/uptime,
                      request_count,
                      uptime ]
                    }
        end
      }
    end
  
    # Handle internal error
    def on_error(response)
      if not response.has_begun?
        s = %q{<html>
      <head><title>500 - Internal Server Error</title></head>
      <body>
        <h1>500 - Internal Server Error</h1>
        <p>%s</p>
        <ol style="list-style-type:none;">} % $!.message.xml_safe
        $!.backtrace.each {|frame|
          s += "\n      <li><tt>" + frame.xml_safe + "</tt></li>"
        }
        s += "\n    </ol>\n</body>\n</html>\n"
        response.headers = {
          'Status' => 500,
          'Content-Length' => s.length
        }
        response.write s
      else
        response.write "\nERROR: #{$!.message.xml_safe} - %s\n" % $!.backtrace[0].xml_safe
      end
      $log.error { $!.info }
    end
  end
  
end # module RHP

if $0 == __FILE__
  begin
    RHP::FCGIHandler.new.run
  rescue Interrupt
  end
end
