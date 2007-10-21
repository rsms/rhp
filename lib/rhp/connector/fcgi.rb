require 'fcgi'

module RHP
  module Connector
    class FCGI < Base
      def run
        self.before_runloop
        begin
          ::FCGI.each {|request|
            self.handle_request(request)
          }
        ensure
          self.after_runloop
        end
      end
    end # class FCGI
  end # module Connector
end # module RHP
