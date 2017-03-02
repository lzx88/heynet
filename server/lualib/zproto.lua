local core = require "zproto.core"

local zproto = {}

function zproto.load(path)
    local files = {}
    io.loopfile(path, function(p)
        table.insert(files, p)
    end)
    local TP = require("zproto_grammar").fparse(files)
    local zp = core.save(TP)
    assert(zp)
end

local __cache = setmetatable( {} , { __mode = "v"})

function zproto.find(typename)
    local protocol = __cache[typename]
    if not protocol then
        protocol = {}
        protocol.tag, protocol.request, protocol.response = core.load(typename)
        __cache[typename] = protocol
    end
    return protocol
end

zproto.pack = core.pack
zproto.unpack = core.unpack
zproto.decode = core.decode
zproto.encode = core.encode


return zproto