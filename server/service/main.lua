local skynet = require "skynet"
local config = require "config"

skynet.start(function()
	if not skynet.getenv "daemon" then
		skynet.newservice "console" 
	end
	local debug_port = tonumber(skynet.getenv "debug_port")
	skynet.newservice("debug_console", debug_port)

	local config_path = skynet.getenv "config_path"
	config.load(config_path)
	local loader = skynet.uniqueservice "protoloader"	
	local proto_path = 	skynet.getenv "proto_path"
	skynet.call(loader, "lua", "load", proto_path)

	local hub = skynet.uniqueservice "hub"
	skynet.uniqueservice "db"
	skynet.uniqueservice "center"	
	local gate_port = tonumber(skynet.getenv "gate_port")
	skynet.call(hub, "lua", "open", "0.0.0.0", gate_port)

	skynet.error "Server is already start finish !"
	skynet.exit()
end)