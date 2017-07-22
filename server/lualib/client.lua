local skynet = require "skynet"
local proxy = require "socket_proxy"
local msg = require "message"
local log = require "log"


local client = {}

function client.dispatch(ctx, consumer)
	local fd = ctx.fd
	local handler = consumer
	local ok = pcall(proxy.subscribe(fd))
	if not ok then
		error("Offline: "..fd)
	end
	while true do
		if ctx.exit then
			return ctx
		end
		local data, sz = proxy.read(fd)
		local name, args, reply = msg.server(data, sz)
		local f = handler[name]
		if f then
			-- f may block , so fork and run
			skynet.fork(function()
				local ok, result, arg3 = pcall(f, ctx, args)
				local t = type(result)
				if t == "number" then
					name = "error"
				elseif ok then
					if t == "nil" then
						result = {} 
					elseif t == "string" then
						name = result
						result = arg3 or {}
					elseif t ~= "table" then
						error ("Invalid response: " .. name)
					end
				else
					log("REQ.%s raise error: %s", name, result)
					reply = nil
				end
				if reply then
					proxy.write(fd, reply(name, result))
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
