local N = 300000.0
local steps = 0

for init=1.0, N do
  local n = init
  while n ~= 1 do
    if n % 2 == 0 then
      n = n / 2
    else
      n = 3*n + 1
    end
    steps = steps + 1
  end
end

return steps
