local skynet = require "skynet"
local log = require "log"
require "errno"

local service = {}

function getResult(ok, e, ...)
	if ok then
		return e, ...
	end
	error(e)--错误抛出
end

local function setResult(ok, e, ...)
	if not ok and type(e) ~= "number" then
		log("Raise error = %s", e)
		return false, E_SRV_STOP
	end
	return ok, e, ...
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
				log("Unknown command : [%s]", cmd)
				skynet.response()(false)
			end
		end)
	end)
end

function service.call(addr, ...)
	return getResult(skynet.call(service[addr], "lua", ...))
end

function service.addTimer(interval, func, ...)
	local ok, e = pcall(func, ...)
	if not ok then
		log("Raise error:\n%s", e)
	end
	skynet.timeout(interval, function()
		service.addTimer(interval, func, ...)
	end)
end

return service