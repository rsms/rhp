module RHP
  # Singleton
  class Processor
    class << self
      #@@compiler = ERuby::Compiler.new
      @@compiler = RHP::Compiler.new
      
      def run(filename, binding)
        file = open(filename)
        dir = File.dirname(filename)
        old_wd = Dir.getwd
        if dir.length
          Dir.chdir(dir)
        end
        begin
          Kernel::eval(compile(file), binding, filename)
        ensure
          file.close
          Dir.chdir(old_wd)
        end
      end
      
      def compile(file)
        @@compiler.compile_file(file)
      end
      
      def compiler
        @@compiler
      end
    end
  end
end # module RHP