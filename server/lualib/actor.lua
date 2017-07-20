local skynet = require "skynet"
local wrap = require "func_wrapper"
local log = require "log"

local actor = { partner = {} }
local partner = actor.partner


local rpcHandle = wrap.rpcHandle
local rpcResult = wrap.rpcResult
local callTimer = wrap.callTimer

local function hook(tbl)
	if tbl then
		for _, name in pairs(tbl) do
			if type(name) ~= "table" then
				partner[name] = skynet.uniqueservice(name)
			else
				for _, nm in pairs(name) do
					partner[nm] = {}
				end
			end
		end
	end
	assert(not partner[SERVICE_NAME], "Can’t redirect self!")
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
		hook(require("trans").tbl[SERVICE_NAME])
		if options.init then
			options.init()
		end
		skynet.dispatch("lua", function (_,_,cmd,...)
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
	if type(partner[name]) == "table" then
		partner[name][key] = addr
	else
		partner[name] = addr
	end
	assert(not partner[SERVICE_NAME], "Can’t redirect self!")
end

function actor.call(addr, ...)
	return rpcResult(skynet.call(partner[addr], "lua", ...))
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