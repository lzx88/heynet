local skynet = require "skynet"
local config = require "config"
local log = require "log"

skynet.start(function()
	if not skynet.getenv "daemon" then
		skynet.newservice "console" 
	end
	local debug_port = tonumber(skynet.getenv "debug_port")
	skynet.newservice("debug_console", debug_port)

	config.load(skynet.getenv("config_path") or "./config/*.cfg")

	local hub = skynet.uniqueservice "hub"	
	skynet.uniqueservice "db"
	skynet.uniqueservice "center"

	local protopath = skynet.getenv("proto_path") or "./protocol/*.proto"
	local gate_port = tonumber(skynet.getenv "gate_port")
	skynet.call(hub, "lua", "load_protocol", protopath)
	skynet.call(hub, "lua", "open", "0.0.0.0", gate_port)
	log"Server is already start finish!"
	skynet.exit()
end)

