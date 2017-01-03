local bag = {}

function bag.setSize(DB, roleid)
	local k = KEY("size")
	DB:set(k, 100)
	local size = DB:get(k)
	return {size = size}
end

return bag