local skynet = require "skynet"
local proxy = require "socket_proxy"
local msg = require "message"
local log = require "log"


local client = {}

function client.dispatch(ctx, consumer)
	local fd = ctx.fd
	local handler = consumer
	proxy.subscribe(fd)
	while true do
		if ctx.exit then
			return ctx
		end
		local data, sz = proxy.read(fd)
		local name, args, session, reply = msg.server(data, sz)
		local f = handler[name]
		if f then
			-- f may block , so fork and run
			skynet.fork(function()
				local ok, result = pcall(f, ctx, args)
				if ok then
					if reply then proxy.write(fd, reply(result)) end
				elseif type(result) == "number" then
					proxy.write(fd, msg.pack("error", result, session))
				else
					log("REQ%s raise error: %s", name, result)
				end
			end)
		else
			-- unsupported command, disconnected
			error ("Invalid command: " .. name)
		end
	end
end

function client.close(fd)
	proxy.close(fd)
end

function client.push(fd, data)
	proxy.write(fd, data)
end

return client
