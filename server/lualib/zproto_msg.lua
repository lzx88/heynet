local zproto = require "zproto"

local msg = {}

local function msg_rsp(tp, session)
    if session then
        return function sender(tbl)
            return zproto.pack(zproto.encode(tp.response, tbl, tp.tag, session))
        end
    end
end

function msg.parse(data, size)
    local header, stream, len = zproto.decode_header(zproto.unpack(data, size))
    local tp = zproto.find(header.tag)
    local args = zproto.decode(tp.request, stream, len, header.shift)
    return tp.name, args, msg_rsp(tp, header.session)
end

function msg.pack(tyname, tbl)
    local tp = zproto.find(tyname)
    return zproto.pack(zproto.encode(tp.request, tbl, tp.tag))
end

return host