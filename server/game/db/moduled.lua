local require_db_role = {
	R = "role_db",
	RB = "bag_db",
}

local require_db = {
	T = "team_db",
}

local RoleKey = {}
local RoleFunc = {}

table.loop(require_db_role, function(path, key)
	local mod = string.match(path, "[%a%d]+")
	RoleFunc[mod] = require(path)
	RoleKey[mod] = key
end)
require_db_role = nil

local Func = {}
local __genkey

function genDBkey(mkey, suffix, roleid)
	return mkey .. roleid ..".".. suffix
end

local DB_

table.loop(require_db, function(path, key)
	local mod = string.match(path, "[%a%d]+")
	local tbl = require(path)
	table.loop(tbl, function( func, k )
		local cmd = mod ..".".. k
		Func[cmd] = function ( ... )
			__genkey = function(suffix)
				return genDBkey(key, suffix)
			end
			return func(DB_, ... )
		end
	end)
end)
require_db = nil

local RDB
local __genRkey

local moduled = {}
function moduled.dispathR(roleid, cmd, ...)
	local mod, func = string.match(cmd, "([%a%d]+)%.([_%a%d]+)")
	assert(RoleFunc[mod] and RoleFunc[mod][func], "DB Logic no impl: [role]" .. cmd)
	assert(RoleKey[mod])
	__genRkey = function(suffix)
		return genDBkey(RoleKey[mod], suffix, roleid)
	end
	return RoleFunc[mod][func](RDB, ...)
end

function moduled.init(cmd, DB)
	local mt = function(t, k)
		return function (_, key, ...)
			return DB[k](_, __genRkey(key), ...)
		end
	end
	RDB = setmetatable({}, {__index = mt})
	local mt = function(t, k)
		return function (_, key, ...)
			return DB[k](_, __genkey(key), ...)
		end
	end
	DB_ = setmetatable({}, {__index = mt})
	setmetatable(cmd, {__index = Func})
end

return moduled