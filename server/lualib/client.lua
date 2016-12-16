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
	while true do
		if c.exit then
			return c
		end
		local msg, sz = proxy.read(fd)
		local type, name, args, response = host:dispatch(msg, sz)
		assert(type == "REQUEST")
		local f = handler[name]
		if f then
			-- f may block , so fork and run
			skynet.fork(function()
				local ok, result = pcall(f, c, args)
				if ok then
					proxy.write(fd, response(result))
				elseif type(result) == "number" then
					proxy.write(fd, send("error", result))
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

function client.push(fd, t, data)
	proxy.write(fd, send(t, data))
end

function client.proto()
	local slot = skynet.getenv "sproto_slot_c2s"
	host = sprotoloader.load(slot):host "package"
	local slot = skynet.getenv "sproto_slot_s2c"
	send = host:attach(sprotoloader.load(slot))
	log("Load sproto")
end

function client.init(cmds)
	handler = cmds
end

return client