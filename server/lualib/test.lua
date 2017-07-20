require "util/luaext"
local file = "../../protocol/base.proto"
local f = assert(io.open(file), "Can't open protocol file:".. file ..".")
local text = f:read "*a"
f:close()
local result 
result = require("zproto_grammar")(text, file)
local file = "../../protocol/fdfd.proto"
local f = assert(io.open(file), "Can't open protocol file:".. file ..".")
local text = f:read "*a"
f:close()

result = require("zproto_grammar")(text, file)
dump(result)

