local skynet = require "skynet"
local core = require "zproto.core"

local zproto = {}

function zproto.load(path)
    local result
    io.loopfile(path, function(file)
        local f = assert(io.open(file), "Can't open protocol file:".. file ..".")
        local text = f:read "*a"
        f:close()
        result = require("zproto_grammar").parse(text, file, result)
        skynet.error("Load protocol", file)
    end)
    core.save(core.create(result))
end

local __cache = setmetatable( {} , { __mode = "v"})

function zproto.find(key)
    local protocol = __cache[key]
    if not protocol then
        protocol = {}
        protocol.tag, protocol.name, protocol.request, protocol.response = core.load(key)
        __cache[protocol.tag] = protocol
        __cache[protocol.name] = protocol
    end
    return protocol
end

zproto.pack = core.pack
zproto.unpack = core.unpack
zproto.decode = core.decode
zproto.encode = core.encode
zproto.decode_header = core.decode_header

return zproto

