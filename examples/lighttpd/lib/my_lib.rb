class MyPerson
  attr_accessor :name
  
  class << self
    @@persistent_data = ''
    attr_accessor :persistent_data
  end
end
