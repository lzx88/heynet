local modulec = {}
local require_mgr = {
	"social/social_mgr",
	"team/team_mgr",
	"mail/mail_mgr",
}
local mlist = {}

local function register(paths, func)
	for i = 1, #paths do
		local p = paths[i]
		local tbl = require(p)
		local folder, file = string.match(p, "([_%a%d]+)[/%.]([_%a%d]+)")
		func(folder, tbl)
	end
end

register(require_mgr, function(name, mgr)
	assert(not mlist[name], "Manage " .. name .. "repeat")
	mlist[name] = mgr
end)
require_mgr = nil

function modulec.loop(func_mgr)
	table.loop(mlist, func_mgr)
end

return modulec