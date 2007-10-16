#!/usr/bin/env ruby
require 'eruby'
require 'fcgi'
require 'cgi'
require 'logger'
require 'benchmark'

class String
  def xml_safe
    self.gsub(/&/, "&#38;").gsub(/\"/, "&#34;").gsub(/</, "&#60;").gsub(/>/, "&#62;")
  end
end

class ERProcess
  def initialize()
    $log = Logger.new(STDERR)
    $log.level = Logger::INFO
    @compiler = ERuby::Compiler.new
  end
  
  def run()
    _level_info = $log.level <= Logger::INFO
    start_time = Time.now
    request_count = 0.0
    rs_time = start_time
    
    FCGI.each {|request|
      if _level_info then
        rs_time = Time.now
        request_count += 1
        #$log.debug { 'Accepted %s:%d %s' % [
        #            request.env['REMOTE_ADDR'], 
        #            request.env['REMOTE_PORT'], 
        #            request.env['REQUEST_URI']]
        #           }
      end
      
      $stdout = request.out
      
      print "Content-Type: text/html\r\n\r\n" # XXX
      
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
  
      request.finish
      
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


ERProcess.new.run

