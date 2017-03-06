local kHashName = genRedisKey("role", "name")
local kID = genRedisKey("role", "_id")

local role = {}

function role.load(DB)
	local data = {
		id = 1,
		name = "heynet",
		level = 5,
	}

	return data
end

function role.isexists(DB, name)
	if 1 == DB:hexists(kHashName, name) then
		error(E_ROLE_REPEAT)
	end
end

function role.create(DB, name, sex, userid)
	local roleid = DB:incr(kID)
	local hkey = genRedisKey("role", roleid)
	if 1 == DB:exists(hkey) then
		error(E_ROLE_REPEAT)
	end

	DB:hmset(hkey,
		"roleid", roleid,
		"name", name,
		"sex", sex,
		"userid", userid)
	
	return roleid
end

return role