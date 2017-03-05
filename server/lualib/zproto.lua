local core = require "zproto.core"

local zproto = {}

function zproto.load(path)
    local files = {}
    io.loopfile(path, function(p)
        table.insert(files, p)
    end)
    local TP = require("zproto_grammar").fparse(files)
    dump(TP)
	local zp = core.save(TP)
    dump(zp)
    assert(zp)
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
