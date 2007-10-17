#!/usr/bin/env ruby
require 'eruby'
require 'fcgi'
require 'logger'
require 'benchmark'

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

# Reponse object
class Response
  def initialize(request)
    @headers = []
    @out = request.out
  end  
  attr_reader :headers
	attr_writer :headers
  attr_reader :out
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
      $stdout = response.out
      
      # Send headers
      # XXX fix this
      response.out.print response.headers.join("\r\n"), "\r\n\r\n"
      
      # Execute ERB
      filename = request.env['SCRIPT_FILENAME']
      file = open(filename)
      begin
        code = @compiler.compile_file(file)
        eval(code, nil, filename)
      rescue
        self.on_error(request)
      ensure
        file.close
      end
      
      # Finish request
      request.finish
      
      # Some info/debug logging
      if _level_info then
        rs = Time.now.to_f-rs_time.to_f
        $log.info { 'perform: %.1f r/s, load: %.1f r/s, processed: %.0f r, uptime: %.1f s' % [
                    1.0/rs,
                    request_count/(Time.now.to_f-start_time.to_f),
                    request_count,
                    Time.now.to_f-start_time.to_f ]
                  }
      end
    }
  end
  
  # Handle internal error
  def on_error(request)
    print %q{<html>
  <head><title>500 - Internal Server Error</title></head>
  <body>
    <h1>500 - Internal Server Error</h1>
    <p>%s</p>
    <ol style="list-style-type:none;">} % $!.message.xml_safe
    $!.backtrace.each {|frame|
      print "\n      <li><tt>", frame.xml_safe, "</tt></li>"
    }
    print "\n    </ol>\n</body>\n</html>\n"
    $log.error { "%s. %s" % [$!.message, $!.backtrace.inspect ]}
  end
end


FCGIHandler.new.run

