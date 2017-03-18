local PATH,IP = ...

IP = IP or "127.0.0.1"

package.path = string.format("%s/client-simple/?.lua;%s/skynet/lualib/?.lua;%s/lualib/?.lua", PATH, PATH, PATH)
package.cpath = string.format("%s/skynet/luaclib/?.so;%s/client-simple/lsocket/?.so;%s/lualib-c/?.so", PATH, PATH, PATH)

local socket = require "simplesocket"
local message = require "simplemessage"

message.register(string.format("%s/protocol/*.proto", PATH))

message.peer(IP, 9001)
message.connect()

local event = {}

message.bind({}, event)

function event:__error(what, err, req, session)
	print("error", what, err)
end

function event:ping()
	print("ping")
end

function event:signin(req, resp)
	print("signin", req.userid, resp.ok)
	if resp.ok then
		message.request "ping"	-- should error before login
		message.request "login"
	else
		-- signin failed, signup
		message.request("signup", { userid = "alice" })
	end
end

function event:signup(req, resp)
	print("signup", resp.ok)
	if resp.ok then
		message.request("signin", { userid = req.userid })
	else
		error "Can't signup"
	end
end

function event:login(_, resp)
	print("login", resp.ok)
	if resp.ok then
		message.request "ping"
	else
		error "Can't login"
	end
end

function event:error(args)
	print("server raise error: [%d]%s", args.errno, errmsg[args.errno])
end

function event:push(args)
	print("server push", args.text)
end

message.request("signin", { userid = "alice", session = 1323 })

--while true do
--	message.update()
--end
