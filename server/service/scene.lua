local actor = require "actor"
local log = require "log"

local stage
local scene = {}
local api = {}

local function iscopy(id)
	return id >= 10000
end

local function iscity(id)
	return id < 10000
end

function api.load(id)
	if iscopy(id) then
		stage = require "stage/copy"
	elseif iscity(id) then
		stage = require "stage/city"
	end
	stage.load(id)
	log("load api %s", id)
end

function api.login(role)
	log("Role[%d] %s login", role.id, role.name)
	stage.enter(role)
end

function api.logout(roleid)
	stage.exit(roleid)
	log("[%d]%s exit api %s", role.id, "玩家x", "xxxx")
end

actor.run{
	command = api,
	info = scene,
}