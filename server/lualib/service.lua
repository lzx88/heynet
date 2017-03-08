local skynet = require "skynet"
local log = require "log"
require "errno"

local rpcHandle = function(cmd, ...)
	local function result(ok, e, ...)
		if ok then
			return true, e, ...
		elseif type(e) ~= "number" then
			log("@RPC "..cmd.." raise error: "..e)
			e = E_SRV_STOP
		end
		return false, e
	end
	return result(pcall(...))
end

function rpcResult(ok, e, ...)
	if ok then
		return e, ...
	end
	error(e)
end

local callTimer = function( ... )
	local ok, e = pcall(...)
	if not ok then
		log("@Timer raise error: %s", e)
	end
end

local isdebug = skynet.getenv "debug"
if isdebug == "true" then
	rpcHandle = nil
	rpcHandle = function (cmd, f, ...) return f(...) end
	rpcResult = nil
	rpcResult = function(...) return ... end
	callTimer = nil
	callTimer = function(f, args) f(args) end
end

local service = {}

function service.init(mod)
	local funcs = mod.command
	if mod.info then
		skynet.info_func(function()
			return mod.info
		end)
	end

	skynet.start(function()
		if mod.require then
			local r = mod.require
			for _, name in ipairs(r) do
				service[name] = skynet.uniqueservice(name)
			end
		end
		if mod.init then
			mod.init()
		end
		skynet.dispatch("lua", function (_,_, cmd, ...)
			local f = funcs[cmd]
			if f then
				skynet.ret(skynet.pack(rpcHandle(cmd, f, ...)))
			else
				log("Unknown command: [%s]", cmd)
				skynet.response()(false)
			end
		end)
	end)
end

function service.call(addr, ...)
	return rpcResult(skynet.call(service[addr], "lua", ...))
end

function service.addTimer(interval, func, args)
	callTimer(func, args)
	skynet.timeout(interval, function()
		service.addTimer(interval, func, args)
	end)
end

return service
