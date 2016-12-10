local user = {}

function user.register(DB, KEY, username, session, password, source)
	local data = {
		id = 1,
		name = "heynet",
		level = 5,
	}

	return data
end

function user.create_role(DB, KEY)
	local data = {
		id = 1,
		name = "heynet",
		level = 5,
	}

	return data
end

function user.query(DB, KEY)
	local data = {
		id = 1,
		name = "heynet",
		level = 5,
	}

	return data
end

return user