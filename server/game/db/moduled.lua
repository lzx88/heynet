local require_db_role = {
	role = "role_db",
	bag = "bag_db",
}

local require_db = {
	user = "user_db",
	team = "team_db",
}

local keys = {}

local function loopCommand(files, func)
	table.loop(files, function(fname, mkey)
		local pos = string.match(fname, "()_db$")
		local mod = string.sub(fname, 1, pos and (pos - 1))
		assert(keys[mod] == nil, "Redis key".. mkey .." repeat!")
		keys[mod] = mkey
		local cmds = require(fname)
		table.loop(cmds, function(f, k)
			if type(f) == "function" then
				func(mkey, mod ..".".. k, f)
			end
		end)
	end)
end

local moduled = {}
function moduled.register(DB, CMD)
	loopCommand(require_db_role, function(mkey, k, func)
		assert(CMD[k] == nil, "DB RPC command ".. k .." repeat!")
		CMD[k] = function(roleid, ... )
			local function fkey(suffix)
				return mkey .. "." ..roleid ..".".. suffix
			end
			return func(DB, fkey, ...)
		end
	end)
	require_db_role = nil
	loopCommand(require_db, function(mkey, k, func)
		assert(CMD[k] == nil, "DB RPC command ".. k .." repeat!")
		CMD[k] = function( ... )
			local function fkey(suffix)
				return  mkey .. ".".. suffix
			end
			return func(DB, fkey, ...)
		end
	end)
	require_db = nil
end

function moduled.genKey(mod, ...)
	assert(keys[mod])
	return genKey(keys[mod], ...)
end

return moduled