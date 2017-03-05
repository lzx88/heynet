local zproto = require "zproto"

local msg = {}

function msg.pack(tyname, tbl, session)
    local tp = zproto.find(tyname)
    if not tp.response then
        session = nil
    end
    return zproto.pack(zproto.encode(tp.request, tbl, tp.tag, session))
end

function msg.request(tyname, tbl)
    if not msg.session then
        msg.session = 0
        msg.reply = {}
    end
    msg.session = msg.session + 1
    msg.reply[msg.session] = tbl
    return msg.pack(tyname, tbl, msg.session)
end

local function response(tp, session)
    if session then
        return function(tbl, session)
            return zproto.pack(zproto.encode(tp.response, tbl, tp.tag, session))
        end 
    end
end

function msg.server(data, size)
    local header, stream, len = zproto.decode_header(zproto.unpack(data, size))
    local tp = zproto.find(header.tag)
    local args = zproto.decode(tp.request, stream, len, header.shift)
    return tp.name, args, header.session, response(tp, header.session) 
end

function msg.client(data, size)
    local header, stream, len = zproto.decode_header(zproto.unpack(data, size))
    local tp = zproto.find(header.tag)
    local args = zproto.decode(header.session and tp.response or tp.request, stream, len, header.shift)
    return tp.name, args, header.session
end

return msg
