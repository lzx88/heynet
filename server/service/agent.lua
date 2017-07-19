local skynet = require "skynet"
local actor = require "actor"
local client = require "client"
local log = require "log"
local role = require "role"

local api = {}
local req = {}
local agent = {}

function req:ping()
	assert(self.login)
	log "ping"
end

function req:login()
	assert(not self.login)
	if agent.fd then
		log("login fail %s fd=%d", agent.userid, self.fd)
		return { ok = false }
	end
	agent.fd = self.fd
	role.online(agent)
	self.login = true --必须有机制确保登录未完成 客户端不请求
	
	actor.addTimer(100, role.timeout)
	return {ok = true}
end

local function new_user(fd)
	local ok, err = pcall(client.dispatch , { fd = fd }, req)
	log("fd=%d is gone. error = %s", fd, err)
	client.close(fd)
	if agent.fd == fd then
		agent.fd = nil
		skynet.sleep(1000)	-- exit after 10s
		if agent.fd == nil then
			-- double check
			if not agent.exit then
				agent.exit = true	-- mark exit
				role.offline()
				actor.call("manager", "exit", agent.userid)
				agent = {}
				--skynet.exit()
			end
		end
	end
end

function api.assign(fd, userid, token, loginrole)
	if agent.exit then
		return false
	end
	if agent.userid == nil then
		agent.userid = userid
	end
	assert(agent.userid == userid)
	skynet.fork(new_user, fd)
	return true
end

function api.transpond(msg)
	client.push(agent.fd, msg)
end

local function init()
	role.init(api, req)
	actor.sendClient = api.transpond
end

actor.run{
	command = api,
	info = agent,
	init = init,
}