-- 
-- The reason for "luaL_checklong" does not guarantee that every test is correct
--

local sf = require "snowflake"
local socket = require("socket")

function sleep(n)
    socket.select(nil, nil, n)
 end

sf.init(0x0, 0x1, 1609459200000, 8, 8, 8, 10)
local function check (sf ,start)
    local a = 1000
    local i = 0
    local last_offset
    while( a>0 )
    do
        local offset = sf.getmillisecond() - start
        local id = sf.next_id()
        if last_offset ~= offset then
            i = 0
        end
        local result =  sf.bit_split(id)
        if ("1, "..offset..", " .. i ~= result) then
            print("result:" .. result)
            print("offset:" .. offset)
            print("\n1, "..offset..", " .. i)
        end
        sleep(0.02)
        last_offset = offset
        a = a-1
        i = i+1
    end
end
check(sf, 160945920000)
print("success\n")