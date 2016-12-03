local skynet = require "skynet"
local service = require "service"
local log = require "log"
confing = require "config"
require "errno"
require "trans"

local game = {}
local client

function subService(name, addr, roleid)
	assert(name == "agent" or name == "scene" or name == "client")
	if SERVICE_NAME == "agent" then
		service[name] = addr
	else
		assert(not service[name][roleid] and addr)
		assert(not addr and service[name][roleid])
		service[name][roleid] = addr
	end
	if name == "client" and addr then
		client.subscribe(addr)
	end
end

--各个服务的中转关系
local transfer = {
	agent = {"db", "client", "scene", "center"},
	center = {"db", "client", "scene", "agent"},
	scene = {"db", "client"},
	db = {},
}

function game.start(args)
	local init = args.init
	args.init = function ()
		local t = transfer[SERVICE_NAME]
		for _, name in pairs(t) do
			if name == "client" then
				if not client then
					client = require "client"
					client.proto()
				end
				service.push = client.push
				if SERVICE_NAME ~= "agent" then
					service.client = {}
				--else
				--	service.client = fd
				end
			elseif name == "agent" or name == "scene" then
				service[name] = {}
			else
				service[name] = skynet.uniqueservice(name)
			end
		end
		if init then
			init()
		end
		assert(not service[SERVICE_NAME], "Can’t redirect self!")
	end
	service.init(args)
end

return game