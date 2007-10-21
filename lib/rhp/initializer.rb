require 'logger'

RHP_ROOT = '.' unless defined?(::RHP_ROOT)

module RHP
  
  class Initializer
    # The Configuration instance used by this Initializer instance.
    attr_reader :configuration
    
    # Runs the initializer
    def self.run(configuration = Configuration.new)
      yield configuration if block_given?
      initializer = new configuration
      initializer.process
      initializer
    end
    
    # Create a new Initializer instance that references the given Configuration
    # instance.
    def initialize(configuration)
      @configuration = configuration
    end
    
    def process
      #check_ruby_version
      set_load_path
      require_frameworks
      initialize_encoding
      initialize_logger
    end
    
    def set_load_path
      load_paths = configuration.load_paths
      load_paths.reverse_each { |dir| $LOAD_PATH.unshift(dir) if File.directory?(dir) }
      $LOAD_PATH.uniq!
    end
    
    # Requires all frameworks specified by the Configuration#frameworks list.
    def require_frameworks
      configuration.frameworks.each { |framework| require(framework.to_s) }
    end
    
    # This initialzation sets $KCODE to 'u' to enable the multibyte safe operations.
    # Plugin authors supporting other encodings should override this behaviour and
    # set the relevant +default_charset+ on ActionController::Base
    def initialize_encoding
      $KCODE='u'
    end
    
    # If Configuration#logger is not +nil+, this does nothing. Otherwise,
    # a new logger instance is created at Configuration#log_path, with a default 
    # log level of Configuration#log_level.
    #
    # If the log could not be created, the log will be set to output to
    # +STDERR+, with a log level of +WARN+.
    #
    # Sets RHP_DEFAULT_LOGGER to the created Logger.
    def initialize_logger
      unless logger = configuration.logger
        config_log_level = Logger.const_get(configuration.log_level.to_s.upcase)
        begin
          logger = Logger.new(configuration.log_path)
          logger.level = config_log_level
        rescue StandardError
          logger = Logger.new(STDERR)
          logger.level = config_log_level
          level_msg = 'The output has been directed to STDERR'
          if logger.level > Logger::WARN
            logger.level = Logger::WARN
            level_msg = 'The log level has been rised to WARN and the output directed to STDERR'
          end
          logger.warn(
            "Unable to access log file. Please ensure that #{configuration.log_path} " +
            "exists and is writable. #{level_msg} until the problem is fixed."
          )
        end
      end
      Object.const_set "RHP_DEFAULT_LOGGER", logger
    end
  end
  
  # The Configuration class holds all the parameters for the Initializer and
  # ships with defaults that suites most Rails applications. But it's possible
  # to overwrite everything. Usually, you'll create an Configuration file
  # implicitly through the block running on the Initializer, but it's also
  # possible to create the Configuration instance in advance and pass it in
  # like this:
  #
  #   config = Rails::Configuration.new
  #   Rails::Initializer.run(:process, config)
  class Configuration
    # The application's base directory.
    attr_reader :root_path
    
    # An array of additional paths to prepend to the load path. By default,
    # all +app+, +lib+, +vendor+ and mock paths are included in this list.
    attr_accessor :load_paths

    # The list of framework components that should be loaded.
    # For example 'my_lib' or 'active_record'. Empty by default.
    attr_accessor :frameworks
    
    # The log level to use for the default Rails logger. In production mode,
    # this defaults to <tt>:info</tt>. In development mode, it defaults to
    # <tt>:debug</tt>.
    attr_accessor :log_level
    
    # The path to the log file to use. Defaults to log/application.log
    # May also be a reference to a File, for example STDERR
    attr_accessor :log_path
    
    # The specific logger to use. By default, a logger will be created and
    # initialized using #log_path and #log_level, but a programmer may
    # specifically set the logger to use via this accessor and it will be
    # used directly.
    attr_accessor :logger
    
    # Create a new Configuration instance, initialized with the default
    # values.
    def initialize
      set_root_path!
      self.load_paths = default_load_paths
      self.frameworks = default_frameworks
      self.log_path   = default_log_path
      self.log_level  = default_log_level
    end
    
    # Set the root_path to RHP_ROOT and canonicalize it.
    def set_root_path!
      raise 'RHP_ROOT is not set' unless defined?(::RHP_ROOT)
      raise 'RHP_ROOT is not a directory' unless File.directory?(::RHP_ROOT)
      
      @root_path =
        # Pathname is incompatible with Windows, but Windows doesn't have
        # real symlinks so File.expand_path is safe.
        if RUBY_PLATFORM =~ /(:?mswin|mingw)/
          File.expand_path(::RHP_ROOT)
        # Otherwise use Pathname#realpath which respects symlinks.
        else
          require 'pathname'
          Pathname.new(::RHP_ROOT).realpath.to_s
        end
      
      Object.const_set(:RELATIVE_RHP_ROOT, ::RHP_ROOT.dup) unless defined?(::RELATIVE_RHP_ROOT)
      ::RHP_ROOT.replace @root_path
    end
    
    private
      
      def default_load_paths
        lib_path = "#{root_path}/lib"
        paths = []
        if File.directory?(lib_path)
          paths.push(lib_path)
        end
        paths
      end
      
      def default_frameworks
        []
      end

      def default_log_path
        File.join(root_path, 'log', "rhp.log")
      end

      def default_log_level
        :info
      end
  end
  
end
