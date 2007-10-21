#!/usr/bin/env ruby
require 'rhp_c.so'
require 'rhp/core_ext'
require 'rhp/initializer'
require 'rhp/processor'
require 'rhp/response'
require 'rhp/connector'
require 'rhp/connector/fcgi'


module RHP
  
  def self.run(configuration = Configuration.new)
    yield configuration if block_given?
    initializer = Initializer.new(configuration)
    initializer.process
    @@configuration = configuration
    $log = RHP_DEFAULT_LOGGER
    $log.debug { 'Setting up connector' }
    Connector::FCGI.new.run
  rescue Interrupt
  end
  
  def self.configuration
    @@configuration
  end
  
end # module RHP


RHP.run if $0 == __FILE__
