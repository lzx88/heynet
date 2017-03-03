local skynet = require "skynet"
local proxy = require "socket_proxy"
local msg = require "zproto_msg"
local log = require "log"


local client = {}

local handler

function client.dispatch(c)
	local fd = c.fd
	proxy.subscribe(fd)
	while true do
		if c.exit then
			return c
		end
		local data, sz = proxy.read(fd)
		local name, args, reply = msg.request(data, sz, "RQT")
		local f = handler[name]
		if f then
			-- f may block , so fork and run
			skynet.fork(function()
				local ok, result = pcall(f, c, args)
				if ok then
					proxy.write(fd, reply(result))
				elseif type(result) == "number" then
					proxy.write(fd, msg.pack("error", result))
				else
					log("Raise error = %s", result)
				end
			end)
		else
			-- unsupported command, disconnected
			error ("Invalid command " .. name)
		end
	end
end

function client.subscribe(fd)
	proxy.subscribe(fd)
end

function client.close(fd)
	proxy.close(fd)
end

function client.push(fd, t, tbl)
	proxy.write(fd, msg.pack(t, tbl))
end

function client.init(cmds)
	handler = cmds
end

return client