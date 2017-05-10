#!/usr/bin/lua
-- See copyright notice at end of file

--
-- usage: lua ./lua-minilua FILE [NITER]
-- 
-- The file can be either Lua source or bytecode
--

-- for luajit:
if not table.pack then
    table.pack = function(...)
        local t = {...}
        t.n = select("#", ...)
        return t
    end
end

local filename  = arg[1]
local niter_str = arg[2]

if not filename then
    local msg = string.format("usage: %s FILE [N]\n", arg[0])
    io.stderr:write(msg)
    os.exit(1)
end

local niter
if niter_str then
    niter = tonumber(niter_str)
    if not niter then
        local msg = string.format("%s is not a number\n", niter_str)
        io.stderr:write(msg)
        os.exit(1)
    end
else
    niter = 1
end


local f = loadfile(filename)
if not f then
    io.stderr:write("error loading file")
    os.exit(1)
end


local res
for i=1,niter do
    res = table.pack(f())
end
for i=1, res.n do
    print(res[i])
end

--******************************************************************************
-- Copyright (C) 2017 Hugo Musso Gualandi
--
-- Permission is hereby granted, free of charge, to any person obtaining
-- a copy of this software and associated documentation files (the
-- "Software"), to deal in the Software without restriction, including
-- without limitation the rights to use, copy, modify, merge, publish,
-- distribute, sublicense, and/or sell copies of the Software, and to
-- permit persons to whom the Software is furnished to do so, subject to
-- the following conditions:
--
-- The above copyright notice and this permission notice shall be
-- included in all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
-- EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
-- MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
-- IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
-- CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
-- TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
-- SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
--*****************************************************************************
