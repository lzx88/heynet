require "util/luaext"

local G = require("zproto_grammar")
P = G.parser()

local file = "../../protocol/base.proto"
local f = assert(io.open(file), "Can't open protocol file:".. file ..".")
local text = f:read "*a"
f:close()
P(text, file)

local file = "../../protocol/fdfd.proto"
local f = assert(io.open(file), "Can't open protocol file:".. file ..".")
local text = f:read "*a"
f:close()
P(text, file)
R=G.result()

dump(R)

