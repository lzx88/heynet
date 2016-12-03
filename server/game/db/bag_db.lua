local bag = {}

function bag.load(key, DB)
	local k = key("size")
	DB:set(k, 100)
	local size = DB:get(k)
	return {size = size}
end

return bag