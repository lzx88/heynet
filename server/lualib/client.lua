local skynet = require "skynet"
local proxy = require "socket_proxy"
local protocol = require "zproto_parser"
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
		local msg, sz = proxy.read(fd)
		local name, args, response = protocol.parse(msg, sz)
		local f = handler[name]
		if f then
			-- f may block , so fork and run
			skynet.fork(function()
				local ok, result = pcall(f, c, args)
				if ok then
					proxy.write(fd, response(result))
				elseif type(result) == "number" then
					proxy.write(fd, protocol.packmsg("error", result))
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
	proxy.write(fd, protocol.packmsg(t, tbl))
end

function client.init(cmds)
	handler = cmds
end

return client