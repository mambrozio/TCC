
local function f(x)
    -- (x+1)*(x-4)
    return x*x - 3*x - 4
end

local minx = nil
local miny = math.huge

for x = -10, 10, 1 do
    local y = f(x)
    if y < miny then
        minx = x
        miny = y
    end
end

return minx, miny
