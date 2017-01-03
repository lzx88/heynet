require "./util/luaext"
local zp = require "./zproto_parser"

local protocol = zp.fparse{"../.proto/test.proto"}
dump(protocol)
