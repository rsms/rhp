#!/usr/bin/env ruby
require 'rhp'

if '<>"&'.xml_safe == '&#62;&#60;&#34;&#38;' then
  puts 'TEST OK'
else
  puts 'TEST FAILED!'
  exit 1
end


compiler = RHP::Compiler.new
f = File.open('../index.rhp')
puts compiler.compile_file(f)
f.close

exit 0
puts 'Benchmarking String.xml_safe'
require 'benchmark'
class String
  def xml_safe2
    self.gsub(/&/, "&#38;").gsub(/\"/, "&#34;").gsub(/</, "&#60;").gsub(/>/, "&#62;")
  end
end
n = 50000
s = %q{<table class="header-table">
<tr class="top-aligned-row">
<td><strong>Module</strong></td>
<td class="class-name-in-header">Benchmark</td></tr>
<tr class="top-aligned-row">
<td><strong>In:</strong></td>
<td><a href="../files/benchmark_rb.html">benchmark.rb</a>}
Benchmark.bmbm(12) do |x|
  x.report("Ruby version:") { n.times do; s.xml_safe2; end }
  x.report("C version:") { n.times do; s.xml_safe; end }
end
