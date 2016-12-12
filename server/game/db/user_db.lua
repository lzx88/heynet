local user = {}

function user.signup(DB, KEY, username, session, password, source)
	local k = KEY(username)
	if DB:exists(k) then
		returnError(E_USER_REPEAT)
	end
	DB:hset(k, "username", username)
	DB:hset(k, "session", session)
	DB:hset(k, "password", password)
	DB:hset(k, "source", source)
	
	return username
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