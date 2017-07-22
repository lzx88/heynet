local zproto = require "zproto"

local msg = {session = 0, reply = {} }

function msg.pack(tyname, tbl, session)
    local tp = zproto.find(tyname)
    return zproto.pack(zproto.encode(tp.request, tbl, tp.tag, tp.response and session)), tp.response ~= nil
end

function msg.request(tyname, tbl)
    msg.session = msg.session + 1
    local pkg, brsp = msg.pack(tyname, tbl, msg.session)
    if brsp then msg.reply[msg.session] = {proto = tyname, args = tbl} end
    return pkg
end

local function response(session)
    if session and tp.response then
        local s = session
        return function(name, tbl)
            return msg.pack(name, tbl, s)
        end
    end
end

function msg.server(data, size)
    local header, stream, len = zproto.decode_header(zproto.unpack(data, size))
	local tp = zproto.find(header.tag)
    local args = zproto.decode(tp.request, stream, len, header.shift)
    return tp.name, args, response(tp, header.session)
end

function msg.client(data, size)
    local header, stream, len = zproto.decode_header(zproto.unpack(data, size))
    local tp = zproto.find(header.tag)
    local p
    local req
    if header.session and msg.reply[header.session] then
        p = tp.response
        req = msg.reply[header.session].args
    else
        p = tp.request
    end
    local rsp = zproto.decode(p, stream, len, header.shift)
    return tp.name, rsp, req
end

return msg
