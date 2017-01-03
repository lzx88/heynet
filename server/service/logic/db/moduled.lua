local require_db = {
	R = "role_db",
	B = "bag_db",
	U = "user_db",
	T = "team_db",
}

local mt = {}
local keys = {}

local moduled = {}
function moduled.register(DB, CMD)
	local function loopcmd(files, func)
		table.loop(files, function(fname, mainkey)
			local pos = string.match(fname, "()_db$")
			local mod = string.sub(fname, 1, pos and (pos - 1))
			assert(keys[mod] == nil, "Redis key".. mainkey .." repeat!")
			keys[mod] = mainkey
			local tbl = require(fname)
			table.loop(tbl, function(f, k)
				if type(f) == "function" then
					func(mod ..".".. k, f)
				end
			end)
		end)
	end

	loopcmd(require_db, function(cmd, func)
		assert(mt[cmd] == nil, "DB RPC command ".. cmd .." repeat!")
		mt[cmd] = function( ... )
			return func(DB, ...)
		end
	end)

	require_db = nil
	setmetatable(CMD, __index = mt)
end

--call partner
function moduled.call( cmd, ... )
	return mt[cmd](...)
end

--gen redis key
function genRedisKey( mod, middle, suffix )
	assert(keys[mod], "Redis key".. mod .." no exist!")
	return keys[mod] .. (middle and ("." .. middle) or "") .. (suffix and ("." .. suffix) or "")
end

return moduled