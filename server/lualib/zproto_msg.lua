local zproto = require "zproto"

local msg = {}

local function msg_rsp(tp, session)
    if session then
        return function sender(tbl)
            return zproto.encode(tp.response, tbl, tp.tag, session)
        end
    end
end

function msg.parse(data, size)
    local stream, len, tyname, session = zproto.decode(zproto.find("header").request, zproto.unpack(data, size))
    local tp = zproto.find(tyname)
    local tyname, args = zproto.decode(tp.request, stream, len)
    return tyname, args, msg_rsp(tp, session)
end

function msg.pack(tyname, tbl)
    local tp = zproto.find(tyname)
    local data, sz = zproto.encode(tp.tag, tp.request, tbl)
    return zproto.pack(data, sz)
end

return host