local skynet = require "skynet"
local socket = require "socket"
local proxy = require "socket_proxy"
local log = require "log"
local actor = require "actor"

local api = {}
local hub = { socket = {} }

local function auth_socket(fd)
	return actor.call("auth", "shakehand", fd)
end

local function assign_agent(fd, userid, token, loginrole)
	return actor.call("manager", "assign", fd, userid, token, loginrole)
end

function new_socket(fd, addr)
	hub.socket[fd] = "[AUTH]"
	proxy.subscribe(fd)
	local ok, userid, token, loginrole = pcall(auth_socket, fd)
	if ok then
		hub.socket[fd] = userid
		if assign_agent(fd, userid, token, loginrole) then
			return	-- succ
		else
			log("Assign failed %s to %s", addr, userid)
		end
	else
		log("Auth faild %s", addr)
	end
	proxy.close(fd)
	hub.socket[fd] = nil
end

function api.open(ip, port)
	local n = skynet.getenv("agent_pool_init") or 5
	actor.call("manager", "create_agent_pool", n)

	log("Listen %s:%d", ip, port)
	assert(hub.fd == nil, "Already open")
	hub.fd = socket.listen(ip, port)
	hub.ip = ip
	hub.port = port
	socket.start(hub.fd, new_socket)
end

function api.close()
	assert(hub.fd)
	log("Close %s:%d", hub.ip, hub.port)
	socket.close(hub.fd)
	hub.ip = nil
	hub.port = nil
end

actor.run{
	command = api,
	info = hub,
}
