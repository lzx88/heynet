local skynet = require "skynet"
local actor = require "actor"
local msg = require "message"

local agents = acotr.partner.agent
local msg_pack = msg.pack

function sendClient(t, data, id)
	local p = msg_pack(t, data)
	if actor.sendClient then
		actor.sendClient(p)
	else
		acotr.send(agents[id], "transpond", p)
	end
end

function sendClients(ids, t, data)
	local p = msg_pack(t, data)
	table.loop(ids, function(id)
		acotr.send(agents[id], "transpond", p)
	end)
end

function sendAllClient(t, data, dis)
	local p = msg_pack(t, data)
	table.loop(agents, function(addr, id)
		if id ~= dis then
			acotr.send(addr, "transpond", p)
		end
	end)
end

local transfer = {}

--各个服务的中转关系
transfer.tbl = {
	auth = {"manager", "db"},
	agent = {"manager", "db", {"scene"}, "center"},
	center = {"db", {"scene", "agent"}},
	scene = {"db", {"agent"}},
	db = {},
	hub = {"auth", "manager"},
}

return transfer