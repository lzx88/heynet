local skynet = require "skynet"
local proxy = require "socket_proxy"
local msg = require "message"
local log = require "log"


local client = {}

local handler

function client.dispatch(ctx)
	local fd = ctx.fd
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
					log("@NetMsg Raise error: %s", result)
				end
			end)
		else
			-- unsupported command, disconnected
			error ("Invalid command: " .. name)
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

function client.pushdata(fd, args)
	if not args.data then
		args.data = msg.pack(args.t, args.tbl)
	end
	proxy.write(fd, args.data)
end

function client.init(cmds)
	handler = cmds
end

return client
