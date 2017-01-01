require "./util/luaext"
local zp = require "./zproto_parser"

local protocol = zp.fparse{"../game-proto/test.proto", "../game-proto/test1.proto"}
dump(protocol.P)
