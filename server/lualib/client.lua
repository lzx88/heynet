local skynet = require "skynet"
local proxy = require "socket_proxy"
local sprotoloader = require "sprotoloader"
local log = require "log"

local client = {}

local host
local send
local handler 

function client.dispatch(c)
	local fd = c.fd
	proxy.subscribe(fd)
	local ERROR = {}
	while true do
		local msg, sz = proxy.read(fd)
		local type, name, args, response = host:dispatch(msg, sz)
		assert(type == "REQUEST")
		if c.exit then
			return c
		end
		local f = handler[name]
		if f then
			-- f may block , so fork and run
			skynet.fork(function()
				local ok, result = pcall(f, c, args)
				if ok then
					proxy.write(fd, response(result))
				else
					log("raise error = %s", result)
					proxy.write(fd, send("error", result))
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

function client.push(fd, t, data)
	proxy.write(fd, send(t, data))
end

function client.proto()
	local slot = skynet.getenv "sproto_slot_c2s"
	host = sprotoloader.load(slot):host "package"
	local slot = skynet.getenv "sproto_slot_c2s"
	send = host:attach(sprotoloader.load(slot))
end

function client.init(cmds)
	handler = cmds
end


return client