local skynet = require "skynet"
local service = require "service"

require "trans"

local game = {}
local client

function subAgent(addr, roleid)
	assert(SERVICE_NAME ~= "agent")
	assert(service.agent[roleid] ~= addr)
	service.agent[roleid] = addr
end

function subScene(addr, roleid)
	assert(SERVICE_NAME ~= "scene")
	if SERVICE_NAME == "agent" then
		service.scene = addr
	else
		service.scene[roleid] = addr
	end
end

function subClient(addr, roleid)
	if SERVICE_NAME == "agent" then
		service.client = addr
	else
		service.client[roleid] = addr
	end
end

--各个服务的中转关系
local transfer = {
	agent = {"db", "client", "scene", "center"},
	center = {"db", "client", "scene", "agent"},
	scene = {"db", "client"},--场景服务不主动请求 代理服
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