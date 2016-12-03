local modulea = require "modulea"
local log = require "log"

local data = {}
local role = {}

local function onlogin()
	local info = callDB("role", "load", data.id)
	
end

local function onlogout()
	
end

local function social_info()
	return 
end

local function enity_info()
	-- body
end

function role.login(adata)
	data.id = adata.userid
	local db_info = onlogin()
	modulea.loop(function(name, impl)
		if impl.onlgoin then
			impl.onlgoin(db_info[name])--登录接口只做一些基础的数据加载，不能推送消息
		end
	end)
	local scene = callCenter("login", data.id, social_info(), skynet.self(), adata.fd)
	subService("scene", data.id, scene)
	callScene(data.id, "enter", data.id, enity_info(), skynet.self(), adata.fd)
	subService("client", adata.fd)
end

function role.login_finish()
	log("[%s] %s login succ", data.id, data.name)
	sendClient("push", { text = "welcome" })
	return { ok = true }
end

function role.logout()
	callCenter("logout", data.id)
	callScene(data.id, "exit", data.id)
	subService("scene", data.id)
	modulea.anti(function(m)
		if m.onlogout then
			m.onlogout()
		end
	end)
	onlogout()
	subService("client")
end

function role.init( client, agent )
	 modulea.splitter( client, agent )
end

return role