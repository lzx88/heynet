local moduled = require "moduled"
local user = {}

local kHashUsername = genRedisKey("user", "username")
local kHashSession = genRedisKey("user", "session")
local kID = genRedisKey("user", "_id")

function user.signup(DB, args)
	if 1 == DB:hexists(kHashUsername, args.username) then
		error(E_USER_REPEAT)
	end
	if 1 == DB:hexists(kHashSession, args.session) then
		error(E_USER_REPEAT)
	end

	moduled.call("role.isexists", args.name)

	local userid = DB:incr(kID)
	local hkey = genRedisKey("user", userid)
	if 1 == DB:exists(hkey) then
		error(E_USER_REPEAT)
	end

	local roleid = moduled.call("role.create", args.name, args.sex, userid)

	DB:hmset(hkey,
		"userid", userid,
		"loginrole", nil,
		"userid", userid,
		"username", args.username,
		"session", args.session,
		"password", args.password,
		"source", args.source)

	DB:hset(kHashUsername, args.username, userid)
	DB:hset(kHashSession, args.session, userid)
	return userid, roleid
end

function user.create_role(DB, args)
	local userid = DB:hget(kHashSession, args.session)
	if not userid then
		error(E_USER_NO_EXIST)
	end
	local hkey = genRedisKey("user", userid)
	if 0 == DB:exists(hkey) then
		error(E_USER_NO_EXIST)
	end

	return moduled.call("role.create", args.name, args.sex, userid)
end

function user.query(DB, session)
	local userid = DB:hget(kHashSession, session)
dump(kHashSession)
dump(session)
dump(userid)
	if not userid then
		error(E_USER_NO_EXIST)
	end
	local hkey = genRedisKey("user", userid)
	local loginrole = DB:hget(hkey, "loginrole")
	return userid, loginrole
end

return user
