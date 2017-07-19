local skynet = require "skynet"
local wrap = require "err_wrapper"
local trans = require "trans"

local rpcHandle = wrap.rpcHandle
local rpcResult = wrap.rpcResult
local callTimer = wrap.callTimer

local actor = { partner = {} }

local function hook(tbl)
	if tbl then
		for _, name in pairs(tbl) do
			if type(name) ~= "table" then
				actor.partner[name] = skynet.uniqueservice(name)
			else
				for _, nm in pairs(name) do
					actor.partner[nm] = {}
				end)
			end
		end
	end
end

function actor.run(options)
	local api = options.command
	if options.info then
		skynet.info_func(function()
			return options.info
		end)
	end
	skynet.start(function()
		hook(options.require)
		hook(trans.tbl[SERVICE_NAME])
		if options.init then
			options.init()
		end
		skynet.dispatch("lua", function (_,_, cmd, ...)
			local f = api[cmd]
			if f then
				skynet.ret(skynet.pack(rpcHandle(cmd, f, ...)))
			else
				log("Unknown command: [%s]", cmd)
				skynet.response()(false)
			end
		end)
	end)
end

function actor.hook(name, addr, key)
	if type(actor.partner[name]) == "table" then
		actor.partner[name][key] = addr
	else
		actor.partner[name] = addr
	end
	assert(not actor.partner[SERVICE_NAME], "Can’t redirect self!")
end

function actor.call(addr, ...)
	return rpcResult(skynet.call(actor[addr], "lua", ...))
end

function actor.addTimer(ms, func, args)
	skynet.timeout(ms, function()
		callTimer(func, args)
		actor.addTimer(ms, func, args)
	end)
end

function actor.delayCall(ms, func, args)
	skynet.timeout(ms, function()
		callTimer(func, args)
	end)
end

return actor