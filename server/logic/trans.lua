local skynet = require "skynet"
local service = require "service"

--内部阻塞RPC
function callDB( ... )
	assert(SERVICE_NAME ~= "db")
	return getResult(skynet.call(service.db, "lua", ...))
end

function callCenter( ... )
	assert(SERVICE_NAME ~= "center")
	return getResult(skynet.call(service.center, "lua", ...))
end

function callScene(roleid, ...)
	assert(SERVICE_NAME ~= "scene" and SERVICE_NAME ~= "agent")
	return getResult(skynet.call(service.scene[roleid], "lua", ...))
end

function callAgent(roleid, ...)
	assert(SERVICE_NAME ~= "agent")
	if not service.agent[roleid] then
		return callCenter("callAgent", roleid, ...)
	end
	return getResult(skynet.call(service.agent[roleid], "lua", ...))
end

function callMyScene( ... )
	assert(SERVICE_NAME == "agent")
	return getResult(skynet.call(service.scene, "lua", ...))
end


--内部非阻塞RPC
function sendMyScene( ... )
	assert(SERVICE_NAME == "agent")
	skynet.send(service.scene, "lua", ... )
end

function sendScene(roleid, ...)
	assert(SERVICE_NAME ~= "scene" and SERVICE_NAME ~= "agent")
	skynet.send(service.scene[roleid], "lua", ... )
end

function sendAgent(roleid, ... )
	assert(SERVICE_NAME ~= "agent")
	skynet.send(service.agent[roleid], "lua", ...)
end

function sendDB( ... )
	assert(SERVICE_NAME ~= "db")
	skynet.send(service.db, "lua", ... )
end

function sendCenter( ... )
	assert(SERVICE_NAME ~= "center")
	skynet.send(service.center, "lua", ... )
end


--单机推送
function sendMyClient(t, data)
	assert(SERVICE_NAME == "agent")
	service.push(service.client, t, data)
end

function sendClient(roleid, t, data)
	assert(SERVICE_NAME ~= "agent")
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