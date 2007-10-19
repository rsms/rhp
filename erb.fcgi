#!/usr/bin/env ruby
require 'eruby'
require 'fcgi'
require 'logger'
require 'benchmark'
require 'time'

require 'stringio'

# Object additions
class Object
  def xml_safe; self.to_s.xml_safe; end
  def uri_safe; self.to_s.uri_safe; end
end

# String additions
class String
  def xml_safe
    self.gsub(/&/, "&#38;").gsub(/\"/, "&#34;").gsub(/</, "&#60;").gsub(/>/, "&#62;")
  end
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

class ResponseAlreadyStartedError < StandardError; end

# Reponse object
class Response
  attr_reader :request, :buffered, :out, :has_sent_headers
	attr_accessor :headers, :status
	
  def initialize(request)
    @headers = {}
    @request = request
    @has_sent_headers = false
    @buffered = false
    @out = request.out
  end
  
  def buffer!
    if not @buffered then
      if @has_sent_headers then
        raise ResponseAlreadyStartedError, 'Headers has already been sent'
      end
      @out = StringIO.new
      @buffered = true
    end
  end
  
  def print(*o)
    if not @has_sent_headers and not @buffered then
      self.send_headers!
    end
    @out.print o
  end
  
  def write(o)
    if not @has_sent_headers and not @buffered then
      self.send_headers!
    end
    @out.write o
  end
  
  def puts(s)
    self.print s
  end
  
  def send_headers!
    #@request.out.write "Status: #{@status}\r\n"
    @headers.each_pair {|name, value|
      if value.is_a? Array then
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
    if @buffered then
      @headers['Content-Length'] = @out.size
      self.send_headers!
      @out.rewind
      @request.out.write @out.read
    elsif not @has_sent_headers then
      self.send_headers!
    end
  end
end

# Handles http transactions
class FCGIHandler
  def initialize()
    $log = Logger.new(STDERR)
    $log.level = Logger::INFO
    @compiler = ERuby::Compiler.new
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
      if _level_info then
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
      file = open(filename)
      begin
        code = @compiler.compile_file(file)
        eval(code, response.get_binding, filename)
        response.send!
      rescue Exception
        self.on_error(response)
      ensure
        file.close
      end
      
      # Finish request
      request.finish
      
      # Some info/debug logging
      if _level_info then
        rtime = Time.now.to_f-rs_time.to_f
        if request_times.length == request_times_size then
          request_times.shift
        end
        request_times.push rtime
        rs = 1.0/rtime
        uptime = Time.now.to_f-start_time.to_f
        $log.info { 'perform: %.1f r/s, load: %.4f r/s, processed: %.0f r, uptime: %.1f s' % [
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
    if not response.has_begun? then
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

begin
  FCGIHandler.new.run
rescue Interrupt
end

