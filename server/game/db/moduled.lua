local require_db = {
	R = "role_db",
	B = "bag_db",
	U = "user_db",
	T = "team_db",
}

local keys = {}

function genRedisKey(mod, middle, suffix)
	assert(keys[mod], "Redis key".. mkey .." no exist!")
	middle = middle and ("." .. middle) or ""
	suffix = suffix and ("." .. suffix) or ""
	return keys[mod] .. middle .. suffix
end

local moduled = {}
function moduled.register(DB, CMD)
	local function loopcmd(files, func)
		table.loop(files, function(fname, mkey)
			local pos = string.match(fname, "()_db$")
			local mod = string.sub(fname, 1, pos and (pos - 1))
			assert(keys[mod] == nil, "Redis key".. mkey .." repeat!")
			keys[mod] = mkey
			local cmds = require(fname)
			table.loop(cmds, function(f, k)
				if type(f) == "function" then
					func(mod, k, f)
				end
			end)
		end)
	end

	loopcmd(require_db, function(mod, k, func)
		local cmd = mod ..".".. k
		assert(CMD[cmd] == nil, "DB RPC command ".. cmd .." repeat!")
		CMD[cmd] = function( ... )
			local function genkey(middle, suffix)
				return genRedisKey(mod, middle, suffix)
			end
			return func(DB, genkey, ... )
		end
	end)
	require_db = nil
end

function moduled.call()
	-- body
end

return moduled