local skynet = require "skynet"
local log = require "log"
require "errno"

local service = {}

function getResult(cmd, ok, e, ...)
	if ok then
		return e, ...
	elseif e == E_SRV_STOP then
		log("@RPC call "..cmd.." fail.")
	end
	error(e)
end

local function setResult(ok, e, ...)
	if ok then
		return ok, e, ...
	elseif type(e) ~= "number" then
		log("@RPC Raise error: "..e)
		e = E_SRV_STOP
	end
	return false, e
end

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
				skynet.ret(skynet.pack(setResult(pcall(f, ...))))
			else
				log("Unknown command: [%s]", cmd)
				skynet.response()(false)
			end
		end)
	end)
end

function service.call(addr, cmd, ...)
	return getResult(cmd, skynet.call(service[addr], "lua", cmd, ...))
end

function service.addTimer(interval, func, args)
	local ok, e = pcall(func, args)
	if not ok then
		log("@Timer raise error: %s", e)
	end
	skynet.timeout(interval, function()
		service.addTimer(interval, func, args)
	end)
end

return service
