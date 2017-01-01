require "./util/luaext"
local zp = require "./zproto_parser"

local protocol = zp.fparse{"../game-proto/test.proto"}
dump(protocol)
