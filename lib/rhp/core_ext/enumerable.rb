module Enumerable
  # Calculates a sum from the elements. Examples:
  #
  #  payments.sum { |p| p.price * p.tax_rate }
  #  payments.sum(&:price)
  #
  # This is instead of payments.inject { |sum, p| sum + p.price }
  #
  # Also calculates sums without the use of a block:
  #   [5, 15, 10].sum # => 30
  #
  # The default identity (sum of an empty list) is zero. 
  # However, you can override this default:
  #
  # [].sum(Payment.new(0)) { |i| i.amount } # => Payment.new(0)
  #
  def sum(identity = 0, &block)
    return identity unless size > 0
    if block_given?
      map(&block).sum
    else
      inject { |sum, element| sum + element }
    end
  end

  # Convert an enumerable to a hash. Examples:
  # 
  #   people.index_by(&:login)
  #     => { "nextangle" => <Person ...>, "chade-" => <Person ...>, ...}
  #   people.index_by { |person| "#{person.first_name} #{person.last_name}" }
  #     => { "Chade- Fowlersburg-e" => <Person ...>, "David Heinemeier Hansson" => <Person ...>, ...}
  # 
  def index_by
    inject({}) do |accum, elem|
      accum[yield(elem)] = elem
      accum
    end
  end
  
end
