local function f(x)
    if x >= 0 then
        return x*x + 3*x
    else
        return math.sin(x)
    end
end

local sum = 0
for i=1.0, 1000.0, 0.5 do
    sum = sum + f(i)
end

return sum
