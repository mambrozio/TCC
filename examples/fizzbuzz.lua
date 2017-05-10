-- This is some code with two hot branches in the if
-- LuaJIT has to patch together 2 traces to get it to work

local function fizzbuzz(N)
    local r = 0

    for i=1,N do
        if i % 3 == 0 then
            r = r + 30
        else
            r = r + 10
        end
    end

    return r
end

return fizzbuzz(1000)
