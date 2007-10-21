require 'time'

module RHP
  module Connector
    class Base
      
      def initialize
        @log = RHP_DEFAULT_LOGGER
      end
      
      def before_runloop
        @start_time = Time.now
        # XXX debug/info stuff. Maybe remove in release...
        @_level_info = @log.level <= Logger::INFO
        @rs_time = @start_time
        @request_count = 0
        @request_times = []
        @request_times_size = 100
      end
      
      def after_runloop
      end
      
      def handle_request(request)
        # Keep some stats while in info/debug mode
        if @_level_info
          @log.info { 'Accepted %s:%d %s' % [
                      request.env['REMOTE_ADDR'], 
                      request.env['REMOTE_PORT'], 
                      request.env['REQUEST_URI']]
                     }
          @request_count += 1
          @rs_time = Time.now
        end
    
        # Setup a response object
        response = Response.new request
    
        # Redirect stdout to fcgi output stream
        #$stdout = request.out
    
        # Execute ERB
        filename = request.env['SCRIPT_FILENAME']
        begin
          Processor.run(filename, response.get_binding)
          #code = @compiler.compile_file(file)
          #eval(code, response.get_binding, filename)
          response.send!
        rescue Exception
          self.on_error(response)
        end
    
        # Finish request
        request.finish
    
        # Some info/debug logging
        if @_level_info
          rtime = Time.now.to_f - @rs_time.to_f
          if @request_times.length == @request_times_size
            @request_times.shift
          end
          @request_times.push rtime
          rs = 1.0/rtime
          uptime = Time.now.to_f - @start_time.to_f
          @log.info { 'perform: %.1f r/s, load: %.4f r, processed: %d r, uptime: %.1f s' % [
                      rs,
                      @request_times.sum/uptime,
                      @request_count,
                      uptime ]
                    }
        end
      end
      
      def run
        self.before_runloop
        begin
          raise NotImplementedError, 'Base connector can not serve any requests'
          #self.handle_request(request)
        ensure
          self.after_runloop
        end
      end
  
      # Handle internal error
      def on_error(response)
        @log.error { $!.info }
        if not response.has_begun?
          @log.debug { 'Replacing response with error message' }
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
          if response.buffered
            response.clear_buffer!
          end
          response.headers = {
            'Status' => 500,
            'Content-Length' => s.length
          }
          response.write s
          if response.buffered
            response.send!
          end
        else
          @log.debug { 'Appending error message to response' }
          response.write "\nERROR: #{$!.message.xml_safe} - %s\n" % $!.backtrace[0].xml_safe
        end
      end
    end

  end # module Connector
end # module RHP
