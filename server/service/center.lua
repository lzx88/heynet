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

local center = {}
function center.login(roleid, info, agent, fd)
	game.subAgent(roleid, agent)
	game.subClient(roleid, fd)
	roles[roleid] = info
	onlogin()
	modulec.loop(function(mgr)
		if mgr.onlogin then
			mgr.onlogin()
		end
	end)
	return scene_mgr.get_scene_addr(info.mapid)
end

function center.logout(roleid)
	game.subscribe("agent", roleid)
	game.subscribe_client(roleid)

	modulec.loop(function(mgr)
		if mgr.onlogout then
			mgr.onlogout()
		end
	end)
	onlogout()
	roles[roleid] = nil
end

local function init()
	scene_mgr.init()
end

game.start{
	command = center,
	info = data,
	init = init,
}