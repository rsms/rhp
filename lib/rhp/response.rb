require 'stringio'

module RHP
  # Raised when trying to send headers when output has starded
  class ResponseAlreadyStartedError < StandardError; end
  
  # Raised when include has been called to deep
  class RecursionLimitError < StandardError; end
  
  # Reponse object. A RHP file is processed within a Response binding.
  class Response
    attr_reader :request, :buffered, :out, :has_sent_headers
  	attr_accessor :headers, :include_depth_limit
	
    def initialize(request)
      @headers = {}
      @request = request
      @has_sent_headers = false
      @buffered = false
      @out = request.out
      $stdout = @out
      @include_depth = 0
      @include_depth_limit = 25
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
        $stdout = @out
        @buffered = true
      end
    end
    
    def clear_buffer!
      if @buffered
        @out = StringIO.new
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
      @include_depth += 1
      if @include_depth == @include_depth_limit
        raise RecursionLimitError, 'Too deep include'
      end
      Processor.run(filename, binding)
      @include_depth -= 1
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
end # module RHP