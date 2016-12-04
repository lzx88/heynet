local skynet = require "skynet"
local game = require "game"
local log = require "log"
local modulec = require "modulec"
local scene_mgr = require "scene_mgr"

local data = {
	scenes = {},
	roles = {},
}

local function onlogin()
end

local function onlogout()
	-- body
end

local CMD = {}
function CMD.login(role, agent, fd)
	log("Role[%d] %s login", role.id, role.name)
	subAgent(agent, role.id)
	subClient(fd, role.id)
	data.roles[role.id] = role
	onlogin()
	modulec.loop(function(mgr)
		if mgr.onlogin then
			mgr.onlogin()
		end
	end)
	return scene_mgr.get_scene_addr(role.mapid)
end

function CMD.logout(roleid)
	modulec.loop(function(mgr)
		if mgr.onlogout then
			mgr.onlogout()
		end
	end)
	onlogout()
	subAgent(nil, roleid)
	subClient(nil, roleid)
	data.roles[roleid] = nil
end

local function init()
	scene_mgr.init()
end

game.start{
	command = CMD,
	info = data,
	init = init,
}