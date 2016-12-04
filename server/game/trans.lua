local skynet = require "skynet"
local service = require "service"

--内部阻塞RPC
function callDB( ... )
	return skynet.call(service.db, "lua", ...)
end

function callCenter( ... )
	return skynet.call(service.center, "lua", ...)
end

function callScene(roleid, ... )
	if SERVICE_NAME == "agent" then
		skynet.call(service.scene, "lua", roleid, ...)
	else
		skynet.call(service.scene[roleid], "lua", ...)
	end
end

function callAgent(roleid, ... )
	assert(SERVICE_NAME ~= "agent")
	return skynet.call(service.agent[roleid], "lua", ...)
end

--内部非阻塞RPC
function sendAgent(roleid, ... )
	local addr = roleid and service.agent[roleid] or service.agent
	skynet.send(addr, "lua", ...)
end

function sendScene(roleid, ... )
	local addr = roleid and service.scene[roleid] or service.scene
	skynet.send(addr, "lua", ... )
end

function sendDB( ... )
	skynet.send(service.db, "lua", ... )
end

function sendCenter( ... )
	skynet.send(service.center, "lua", ... )
end

--错误抛出
function returnError(errno)
	error(errno)
end

--单机推送
function sendClient(t, data, roleid)
	assert(not roleid or (roleid and SERVICE_NAME ~= "agent"))
	local fd = roleid and service.client[roleid] or service.client
	service.push(fd, t, data)
end

--多发广播
function sendMutiClient(roleids, t, data)
	table.loop(roleids, function(roleid)
		service.push(service.client[roleid], t, data)
	end)
end

--全服广播 可过滤 dis
function sendAllClient(t, data, dis)
	assert(SERVICE_NAME == "center" or SERVICE_NAME == "scene")
	local unfd
	if dis and service.client[dis] then
		unfd = service.client[dis]
		service.client[dis] = nil
	end

	table.loop(service.client, function(fd)
		service.push(fd, t, data)
	end)

	if unfd then
		service.client[dis] = unfd
	end
end