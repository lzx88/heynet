local skynet = require "skynet"
local service = require "service"
local log = require "log"

local onlines = {}
local pool = {}
local function new_agent()
	local agent = table.remove(pool)
	if not agent then
		agent = skynet.newservice "agent"
	end
	return agent
end

local function free_agent(agent)
	table.insert(pool, agent)
end

local manager = {}
function manager.assign(fd, userid)
	local agent
	repeat
		agent = onlines[userid]
		if not agent then
			agent = new_agent()
			if not onlines[userid] then
				-- double check
				onlines[userid] = agent
			else
				free_agent(agent)
				agent = onlines[userid]
			end
		end
	until skynet.call(agent, "lua", "assign", fd, userid)
	log("Assign %d to %s [%s]", fd, userid, agent)
end

function manager.exit(userid)
	local agent = onlines[userid]
	assert(agent)
	onlines[userid] = nil
end

function manager.create(n)
	n = n or 5
	for i = 1, n do
		local agent = skynet.newservice "agent"
		free_agent(agent)
	end
end

service.init {
	command = manager,
	data = onlines,
}