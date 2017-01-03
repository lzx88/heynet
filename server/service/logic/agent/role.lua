local skynet = require "skynet"
local modulea = require "modulea"
local log = require "log"
local this = {}
local role = {}

local function login()
	local data = callDB("role.load", this.id)
	this = data
	if nil == this.mapid then
		this.mapid = 30
	end
	role.id = this.id
end

local function login_finish()
	log("[%s] %s login succ", this.id, this.name)
	sendClient("push", { text = "welcome" })
end

local function logout()
	
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

function role.online( root )
	this.id = root.userid
	login()
	root["role"] = this
	modulea.loop(function(name, impl)
		if impl.login then
			impl.login(root)
		end
	end)
	local scene = callCenter("login", social_info(), skynet.self(), root.fd)
	subScene(scene, this.id)
	callScene("login", enity_info(), root.fd)
	subClient(root.fd)
	login_finish()
end

function role.offline()
	callCenter("logout", this.id)
	callScene("logout", this.id)
	subScene(nil, this.id)
	modulea.anti(function(m)
		if m.logout then
			m.logout()
		end
	end)
	logout()
	subClient(nil)
end

function role.init( client, agent )
	 modulea.splitter( client, agent )
end

return role