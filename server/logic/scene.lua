local skynet = require "skynet"
local client = require "client"
local log = require "log"

local stage

local CMD = {}

local function iscopy(id)
	return id >= 10000
end

local function iscity(id)
	return id < 10000
end

function CMD.load(id)
	if iscopy(id) then
		stage = require "stage/copy"
	elseif iscity(id) then
		stage = require "stage/city"
	end
	stage.load(id)
	log("load CMD %s", id)
end

function CMD.login(role, fd)
	log("Role[%d] %s login", role.id, role.name)
	stage.enter(role)
	subClient(fd, role.id)
end

function CMD.logout(roleid)
	stage.exit(roleid)
	subClient(nil, roleid)
	log("[%d]%s exit CMD %s", role.id, "玩家x", "xxxx")
end

require("launcher").start {
	command = CMD,
	info = {stage = stage},
}