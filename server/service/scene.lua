local actor = require "actor"
local log = require "log"

local scene

local CMD = {}

local function iscopy(id)
	return id >= 10000
end

local function iscity(id)
	return id < 10000
end

function api.load(id)
	if iscopy(id) then
		scene = require "stage/copy"
	elseif iscity(id) then
		scene = require "stage/city"
	end
	scene.load(id)
	log("load api %s", id)
end

function api.login(role)
	log("Role[%d] %s login", role.id, role.name)
	scene.enter(role)
end

function api.logout(roleid)
	scene.exit(roleid)
	log("[%d]%s exit api %s", role.id, "玩家x", "xxxx")
end

actor.run{
	command = api,
	info = scene,
}