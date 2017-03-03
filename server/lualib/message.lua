local zproto = require "zproto"

local msg = { session = 0 }

local function msg_rsp(tp, session)
    if session then
        return function sender(tbl)
            return zproto.pack(zproto.encode(tp.response, tbl, tp.tag, session))
        end 
    end
end

function msg.request(tyname, tbl)
    msg.session = msg.session + 1
    return msg.pack(tyname, tbl, msg.session)
end

function msg.pack(tyname, tbl, session)
    local tp = zproto.find(tyname)
    if not tp.response then
        session = nil
    end
    return zproto.pack(zproto.encode(tp.request, tbl, tp.tag, session))
end

function msg.server(data, size)
    local header, stream, len = zproto.decode_header(zproto.unpack(data, size))
    local tp = zproto.find(header.tag)
    local args = zproto.decode(tp.request, stream, len, header.shift)
    return tp.name, args, msg_rsp(tp, header.session)
end

function msg.client(data, size)
    local header, stream, len = zproto.decode_header(zproto.unpack(data, size))
    local tp = zproto.find(header.tag)
    local args = zproto.decode(tp.response, stream, len, header.shift)
    return tp.name, args, header.session
end

return msg