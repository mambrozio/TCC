
local function f(x)
    -- (x+1)*(x-4)
    return x*x - 3*x - 4
end

local minx = nil
local miny = math.huge

for x = -10.0, 10.0, 0.025 do
    local y = f(x)
    if y < miny then
        minx = x
        miny = y
    end
end

print(minx, miny)
