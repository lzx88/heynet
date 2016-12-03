local skynet = require "skynet"
local client = require "client"
local game = require "game"
local log = require "log"

local stage

local scene = {}

local function iscopy(id)
	return id >= 10000
end

local function iscity(id)
	return id < 10000
end

function scene.load(id)
	if iscopy(id) then
		stage = require "stage/copy"
	elseif iscity(id) then
		stage = require "stage/city"
	end
	stage.load(id)
	log("load scene %s", id)
end

function scene.enter(roleid, info, agent, fd)
	stage.add_role(roleid, info)

	subService("agent", agent, roleid)
	subService("client", fd, roleid)
end

function scene.exit(roleid)
	stage.remove_role(rpleid)

	subService("agent", nil, roleid)
	subService("client", nil, roleid)
	log("[%d]%s exit scene %s", role.id, "玩家x", "xxxx")
end

game.start {
	command = scene,
	info = {stage = stage},
}