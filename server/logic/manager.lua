local skynet = require "skynet"
local service = require "service"
local log = require "log"

local onlines = {}

local pool = {agents = {}}
function pool.provide()
	local agent = table.remove(pool.agents)
	if not agent then
		agent = skynet.newservice "agent"
	end
	return agent
end
function pool.recycle(agent)
	table.insert(pool.agents, agent)
end

local manager = {}
function manager.assign(fd, userid, token, loginrole)
	local agent
	repeat
		agent = onlines[userid]
		if not agent then
			agent = pool.provide()
			if not onlines[userid] then
				-- double check
				onlines[userid] = agent
			else
				recycle(agent)
				agent = onlines[userid]
			end
		end
	until skynet.call(agent, "lua", "assign", fd, userid, token, loginrole)
	log("Assign %d to %s [%s]", fd, userid, agent)
end

function manager.exit(userid)
	local agent = onlines[userid]
	assert(agent)
	onlines[userid] = nil
	recycle(agent)
end

local function init(n)
	local n = skynet.getenv "agent_pool_init"
	for i = 1, n do
		dump(i)
		local agent = skynet.newservice "agent"
		dump(agent)
		pool.recycle(agent)
	end
end

service.init {
	init = init,
	command = manager,
	data = onlines,
}