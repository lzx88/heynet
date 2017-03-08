local skynet = require "skynet"
local service = require "service"

--block RPC
function callDB(...)
	assert(SERVICE_NAME ~= "db")
	return rpcResult(skynet.call(service.db, "lua", ...))
end

function callCenter(...)
	assert(SERVICE_NAME ~= "center")
	return rpcResult(skynet.call(service.center, "lua", ...))
end

function callAgent(roleid, ...)
	assert(SERVICE_NAME ~= "agent")
	if not service.agent[roleid] then
		return callCenter("callAgent", roleid, ...)
	end
	return rpcResult(skynet.call(service.agent[roleid], "lua", ...))
end

function callScene(roleid, ...)
	if SERVICE_NAME == "agent" then
		return rpcResult(skynet.call(service.scene, "lua", roleid, ...))
	end
	assert(SERVICE_NAME ~= "scene")
	return rpcResult(skynet.call(service.scene[roleid], "lua", ...))
end

--no block RPC
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

function sendDB( ... )
	assert(SERVICE_NAME ~= "db")
	skynet.send(service.db, "lua", ... )
end

function sendCenter( ... )
	assert(SERVICE_NAME ~= "center")
	skynet.send(service.center, "lua", ... )
end

--push
function sendClient(roleid, t, data)
	if SERVICE_NAME == "agent" then
		service.push(service.client, roleid, t)
	end
	assert(service.client[roleid])
	service.push(service.client[roleid], t, data)
end

function sendClients(roleids, t, data)
	assert(SERVICE_NAME == "center" or SERVICE_NAME == "scene")
	local args = {t = t, tbl = data}
	table.loop(roleids, function(roleid)
		service.pushdata(service.client[roleid], t, args)
	end)
end

function sendAllClient(t, data, dis)
	assert(SERVICE_NAME == "center")
	local unfd
	if dis and service.client[dis] then
		unfd = service.client[dis]
		service.client[dis] = nil
	end
	local args = {t = t, tbl = data}
	table.loop(service.client, function(fd)
		service.pushdata(fd, args)
	end)

	if unfd then
		service.client[dis] = unfd
	end
end
