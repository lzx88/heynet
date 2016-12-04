local skynet = require "skynet"
local service = require "service"
local game = require "game"
local client = require "client"
local log = require "log"
local role = require "role"

local CMD = {}
local data = {}
local handler = {}

function handler:ping()
	assert(self.login)
	log "ping"
end

function handler:login()
	assert(not self.login)
	if data.fd then
		log("login fail %s fd=%d", data.userid, self.fd)
		return { ok = false }
	end
	data.fd = self.fd
	role.login(data)
	self.login = true --必须有机制确保登录未完成 客户端不请求
	return role.login_finish()
end

local function new_user(fd)
	local ok, error = pcall(client.dispatch , { fd = fd })
	log("fd=%d is gone. error = %s", fd, error)
	client.close(fd)
	if data.fd == fd then
		data.fd = nil
		skynet.sleep(1000)	-- exit after 10s
		if data.fd == nil then
			-- double check
			if not data.exit then
				skynet.call(service.manager, "lua", "exit", data.userid)
				data = {}
				data.exit = true	-- mark exit

				role.logout()
				--skynet.exit()
			end
		end
	end
end

function CMD.assign(fd, userid)
	if data.exit then
		return false
	end
	if data.userid == nil then
		data.userid = userid
	end
	assert(data.userid == userid)
	skynet.fork(new_user, fd)
	return true
end

local function init()
	role.init(handler, CMD)
	client.init(handler)
end

game.start {
	command = CMD,
	require = {"manager"},
	info = data,
	init = init,
}