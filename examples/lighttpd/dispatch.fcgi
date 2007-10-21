#!/usr/bin/env ruby
require 'rhp'
#require 'profile'

RHP::run {|config|
  
  config.load_paths << "#{config.root_path}/lib"
  
  config.frameworks << 'my_lib'
  
  config.log_level = :debug
  
}
