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
		local name, args, reply = msg.server(data, sz)
		local f = handler[name]
		if f then
			-- f may block , so fork and run
			skynet.fork(function()
				local ok, result, pname = pcall(f, ctx, args)
				local t = type(result)
				if ok then
					if t == "number" then
						name = "errno"
					elseif t == "nil" then
						result = SUCCESS
					elseif t == "table" then
						if pname then name = pname end
					else
						log ("Invalid response: " .. name)
						name = "errno"
						result = E_SERIALIZE
					end
				else
					if t ~= "number" then
						log("[REQ]%s raise error: %s", name, result)
						result = E_SRV_STOP
					end
					name = "errno"
				end
				if reply then
					proxy.write(fd, reply(name, result))
				end
			end)
		else
			-- unsupported command, disconnected
			error("Invalid command: " .. name)
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
