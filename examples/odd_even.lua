local even, odd

even = function(n)
  if n == 0 then
    return true
  else
    return odd(n-1)
  end
end

even = function(n)
  if n == 0 then
    return false
  else
    return even(n-1)
  end
end

return even(17)
