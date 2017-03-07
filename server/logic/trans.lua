local skynet = require "skynet"
local service = require "service"

--内部阻塞RPC
function callDB(cmd, ...)
	assert(SERVICE_NAME ~= "db")
	return getResult(cmd, skynet.call(service.db, "lua", cmd, ...))
end

function callCenter(cmd, ...)
	assert(SERVICE_NAME ~= "center")
	return getResult(cmd, skynet.call(service.center, "lua", cmd, ...))
end

function callAgent(roleid, cmd, ...)
	assert(SERVICE_NAME ~= "agent")
	if not service.agent[roleid] then
		return callCenter("callAgent", roleid, cmd, ...)
	end
	return getResult(cmd, skynet.call(service.agent[roleid], "lua", cmd, ...))
end

function callScene(roleid, cmd, ...)
	if SERVICE_NAME == "agent" then
		return getResult(cmd, skynet.call(service.scene, "lua", roleid, cmd, ...))
	end
	assert(SERVICE_NAME ~= "scene")
	return getResult(cmd, skynet.call(service.scene[roleid], "lua", cmd, ...))
end

--内部非阻塞RPC
function sendScene(roleid, ...)
	if SERVICE_NAME == "agent" then
		return skynet.send(service.scene, "lua", roleid, ...)
	end
	assert(SERVICE_NAME ~= "scene")
	return skynet.send(service.scene[roleid], "lua", ...)
end

function sendAgent(roleid, ...)
	assert(SERVICE_NAME ~= "agent")
	skynet.send(service.agent[roleid], "lua", ...)
end

function sendDB(...)
	assert(SERVICE_NAME ~= "db")
	skynet.send(service.db, "lua", ...)
end

function sendCenter(...)
	assert(SERVICE_NAME ~= "center")
	skynet.send(service.center, "lua", ...)
end

--单机推送
function sendMyClient(t, data)
	assert(SERVICE_NAME == "agent")
end

function sendClient(roleid, t, data)
	if SERVICE_NAME == "agent" then
		service.push(service.client, roleid, t)
	end
	assert(service.client[roleid])
	service.push(service.client[roleid], t, data)
end

--多发广播
function sendMutiClient(roleids, t, data)
	assert(SERVICE_NAME == "center" or SERVICE_NAME == "scene")
	table.loop(roleids, function(roleid)
		service.push(service.client[roleid], t, data)
	end)
end

--全服广播 可过滤 dis
function sendAllClient(t, data, dis)
	assert(SERVICE_NAME == "center")
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
