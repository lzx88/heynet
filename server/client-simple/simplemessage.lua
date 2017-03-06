local socket = require "simplesocket"
require"util/luaext"
local msg = require "message"

local message = {}
local var = {
	object = {},
}

function message.register(path)
	require("zproto").load(path)
end

function message.peer(addr, port)
	var.addr = addr
	var.port = port
end

function message.connect()
	socket.connect(var.addr, var.port)
	socket.isconnect()
end

function message.bind(obj, handler)
	var.object[obj] = handler
end

function message.request(name, args)
	local data, size = msg.request(name, args)
	socket.write(msg.request(name, args))
end

--[[function message.update(ti)
	local stream = socket.read(ti)
	if not stream then
		return false
	end
	local t, session_id, resp, err = (msg)
	if t == "REQUEST" then
		for obj, handler in pairs(var.object) do
			local f = handler[session_id]	-- session_id is request type
			if f then
				local ok, err_msg = pcall(f, obj, resp)	-- resp is content of push
				if not ok then
					print(string.format("push %s for [%s] error : %s", session_id, tostring(obj), err_msg))
				end
			end
		end
	else
		local session = var.session[session_id]
		var.session[session_id] = nil

		for obj, handler in pairs(var.object) do
			if err then
				local f = handler.__error
				if f then
					local ok, err_msg = pcall(f, obj, session.name, err, session.req, session_id)
					if not ok then
						print(string.format("session %s[%d] error(%s) for [%s] error : %s", session.name, session_id, err, tostring(obj), err_msg))
					end
				end
			else
				local f = handler[session.name]
				if f then
					local ok, err_msg = pcall(f, obj, session.req, resp, session_id)
					if not ok then
						print(string.format("session %s[%d] for [%s] error : %s", session.name, session_id, tostring(obj), err_msg))
					end
				end
			end
		end
	end

	return true
end]]

return message
