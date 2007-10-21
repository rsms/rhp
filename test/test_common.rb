#!/usr/bin/env ruby
require 'rhp'
require 'test/unit'

class RHPTest < Test::Unit::TestCase
  # def setup
  # end

  # def teardown
  # end

  def test_xml_safe
    assert('<>"&'.xml_safe == '&#62;&#60;&#34;&#38;')
  end
  
  def test_uri_safe
    assert('/f?s='.uri_safe == '%2Ff%3Fs%3D')
  end
end

if $0 == __FILE__
  require 'test/unit/ui/console/testrunner'
  Test::Unit::UI::Console::TestRunner.run(RHPTest)
end

