local N = 10001
local res = 0
local i = 1
while i <= N do
    local j = 1
    while j <= N do
        res = res + 1
        j = j + 1
    end
    i = i + 1
end
return res
