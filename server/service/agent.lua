local skynet = require "skynet"
local actor = require "actor"
local client = require "client"
local log = require "log"
local role = require "role"

local api = {}
local rqt = {}
local ctx = {}

function rqt:ping()
	assert(self.login)
	log "ping"
end

function rqt:login()
	assert(not self.login)
	if ctx.fd then
		log("login fail %s fd=%d", ctx.userid, self.fd)
		return { ok = false }
	end
	ctx.fd = self.fd
	role.online(ctx)
	self.login = true --必须有机制确保登录未完成 客户端不请求
	
	actor.addTimer(100, role.timeout)
	return {ok = true}
end

local function new_user(fd)
	local ok, err = pcall(client.dispatch , { fd = fd }, rqt)
	log("fd=%d is gone. error = %s", fd, err)
	client.close(fd)
	if ctx.fd == fd then
		ctx.fd = nil
		skynet.sleep(1000)	-- exit after 10s
		if ctx.fd == nil then
			-- double check
			if not ctx.exit then
				ctx.exit = true	-- mark exit
				role.offline()
				actor.call("manager", "exit", ctx.userid)
				ctx = {}
				--skynet.exit()
			end
		end
	end
end

function api.assign(fd, userid, token, loginrole)
	if ctx.exit then
		return false
	end
	if ctx.userid == nil then
		ctx.userid = userid
	end
	assert(ctx.userid == userid)
	skynet.fork(new_user, fd)
	return true
end

function api.transpond(msg)
	client.push(ctx.fd, msg)
end

local function init()
	role.init(rqt, api)
	actor.sendClient = api.transpond
end

actor.run{
	command = api,
	info = ctx,
	init = init,
}