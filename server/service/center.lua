local actor = require "actor"
local log = require "log"
local modulec = require "modulec"
local scene_mgr = require "scene_mgr"

local center = {
	scenes = {},
	roles = {},
}

local function onlogin()
end

local function onlogout()
	-- body
end

local api = {}
function api.login(role, agent)
	log("Role[%d] %s login", role.id, role.name)
	actor.hook("agent", agent, role.id)
	center.roles[role.id] = role
	onlogin()
	modulec.loop(function(mgr)
		if mgr.onlogin then
			mgr.onlogin()
		end
	end)
	return scene_mgr.get_scene_addr(role.mapid)
end

function api.logout(roleid)
	modulec.loop(function(mgr)
		if mgr.onlogout then
			mgr.onlogout()
		end
	end)
	onlogout()
	actor.hook("agent", nil, role.id)
	center.roles[roleid] = nil
end

local function init()
	scene_mgr.init()
end

actor.start{
	command = api,
	info = center,
	init = init,
}