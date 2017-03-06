local bag = {}

function bag.set_size(DB, roleid)
	local k = genRedisKey("bag", roleid, "size")
	DB:set(k, 100)
	local size = DB:get(k)
	return {size = size}
end

return bag