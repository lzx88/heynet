require "util/luaext"
local file = "../../protocol/base.proto"
local f = assert(io.open(file), "Can't open protocol file:".. file ..".")
local text = f:read "*a"
f:close()
parser = require("zproto_grammar").parser
local result = parser(text, file)
dump(result)

