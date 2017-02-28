local zproto = require "zproto"

local parser = {}

local function sender_rsp(tp, session)
    if session then
        return function(tbl)
            local data, sz = zproto.encode(tp.tag, tp.response, tbl, session)
            return zproto.pack(data, sz)
        end
    end
end

function parser.parse(data, size)
    local stream, len, tyname, session = zproto.decode(zproto.find("header").request, zproto.unpack(data, size))
    local tp = zproto.find(tyname)
    local tyname, args = zproto.decode(tp.request, stream, len)
    return tyname, args, sender_rsp(tp, session)
end

function parser.packmsg(tyname, tbl)
    local tp = zproto.find(tyname)
    local data, sz = zproto.encode(proto.tag, tp.request, tbl)
    return zproto.pack(data, sz)
end

return host