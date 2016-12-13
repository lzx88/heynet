local require_db = {
	R = "role_db",
	B = "bag_db",
	U = "user_db",
	T = "team_db",
}

local mt = {}

local moduled = {}
function moduled.register(DB, CMD)
	local keytbl = {}
	local function loopcmd(files, func)
		table.loop(files, function(fname, mainkey)
			local pos = string.match(fname, "()_db$")
			local mod = string.sub(fname, 1, pos and (pos - 1))
			assert(keytbl[mod] == nil, "Redis key".. mainkey .." repeat!")
			keytbl[mod] = mainkey
			local tbl = require(fname)
			table.loop(tbl, function(f, k)
				if type(f) == "function" then
					local cmd = mod ..".".. k
					func(mainkey, cmd, f)
				end
			end)
		end)
	end

	loopcmd(require_db, function(mainkey, cmd, func)
		assert(mt[cmd] == nil, "DB RPC command ".. cmd .." repeat!")
		mt[cmd] = function( ... )
			local function gen_redis_key(middle, suffix)
				return mainkey .. (middle and ("." .. middle) or "") .. (suffix and ("." .. suffix) or "")
			end
			return func(DB, gen_redis_key, ... )
		end
	end)

	require_db = nil
	setmetatable(CMD, __index = mt)
end

function moduled.call( cmd, ... )
	return getResult(pcall( mt[cmd], ... ))
end

return moduled