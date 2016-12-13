local moduled = require "moduled"
local user = {}

function user.signup(DB, KEY, args)
	local ku = KEY("hUsername")
	if 0 == DB:hexists(ku, args.username) then
		errReturn(E_USER_REPEAT)
	end
	local ks = KEY("hSession")
	if 0 == DB:hexists(ks, args.session) then
		errReturn(E_USER_REPEAT)
	end

	moduled.call("role.isexists", args.name)

	local kid = KEY("_id")
	local userid = DB:incr(kid)
	local k = KEY(userid)
	if 0 == DB:exists(k) then
		errReturn(E_USER_REPEAT)
	end

	local roleid = moduled.call("role.create", args.name, userid)

	DB:hmset(k, "userid", userid,
				"bonline", false,
				"userid", userid,
				"username", args.username,
				"session", args.session,
				"password", args.password,
				"source", args.source)

	DB:hset(ku, args.username, userid)
	DB:hset(ks, args.session, userid)
	return userid
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