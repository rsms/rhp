#!/usr/bin/env ruby
require 'rhp'
require 'benchmark'

n = 50000
puts "Each test compiles index.rhp #{n} times"
TEST_RHP_FILE = '../../examples/lighttpd/pulic/index.rhp'

Benchmark.bmbm() do |x|
  x.report("Reuse compiler") {
    compiler = RHP::Compiler.new
    n.times do
      f = File.open(TEST_RHP_FILE)
      compiler.compile_file(f)
      f.close
    end
  }
  x.report("Dispose compiler") {
    n.times do
      f = File.open(TEST_RHP_FILE)
      RHP::Compiler.new.compile_file(f)
      f.close
    end
  }
end
