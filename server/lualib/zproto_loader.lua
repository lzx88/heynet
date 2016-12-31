require "./util/luaext"
local zp = require "./zproto_parser"

local f = assert(io.open("../game-proto/test.proto"), "Can't open sproto file")
local text = f:read "*a"
f:close()

local P = zp.parse(text, "test.proto").P
dump(P)