local skynet = require "skynet"
local modulea = require "modulea"
local log = require "log"
local this = {}
local role = {}

local function onlogin()
	local data = callDB("role.load", this.id)
	this = data
	if nil == this.mapid then
		this.mapid = 30
	end
	role.id = this.id
end

local function onlogout()
	
end

local function social_info()
	return {
		id = this.id,
		name = this.name,
		mapid = this.mapid
	}
end

local function enity_info()
	return {
		id = this.id,
		name = this.name,
		mapid = this.mapid
	}
end

function role.login( root )
	this.id = root.userid
	onlogin()
	root["role"] = this
	modulea.loop(function(name, impl)
		if impl.onlgoin then
			impl.onlgoin(this.id, root)--登录接口只做一些基础的数据加载，不能推送消息
		end
	end)
	local scene = callCenter("login", social_info(), skynet.self(), root.fd)
	subScene(scene, this.id)
	callScene("login", enity_info(), root.fd)
	subClient(root.fd)
end

function role.login_finish()
	log("[%s] %s login succ", this.id, this.name)
	sendClient("push", { text = "welcome" })
	return { ok = true }
end

function role.logout()
	callCenter("logout", this.id)
	callScene("logout", this.id)
	subScene(nil, this.id)
	modulea.anti(function(m)
		if m.onlogout then
			m.onlogout()
		end
	end)
	onlogout()
	subClient(nil)
end

function role.init( client, agent )
	 modulea.splitter( client, agent )
end

return role