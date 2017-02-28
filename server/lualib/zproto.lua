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
        protocol.tag, protocol.name, protocol.request, protocol.response = core.load(typename)
        __cache[typename] = protocol
    end
    return protocol
end

function zproto.encode(typename, tbl)
    return core.encode(proto.tag, proto.request, tbl)
end

function zproto.decode(ty, data)
    return core.decode(ty, data)
end


return zproto