require "./util/luaext"
require "./zproto_parser"


local f = assert(io.open("../game-proto/test.proto"), "Can't open sproto file")
local text = f:read "*a"
f:close()

parser(text, "test.proto")