local skynet = require "skynet"
local config = require "config"
local protocol = require "zproto"

skynet.start(function()
	if not skynet.getenv "daemon" then
		skynet.newservice "console" 
	end
	local debug_port = tonumber(skynet.getenv "debug_port")
	skynet.newservice("debug_console", debug_port)

	config.load(skynet.getenv("config_path") or "./config/*.cfg")
	protocol.load(skynet.getenv("proto_path") or "./protocol/*.proto")
	
	local hub = skynet.uniqueservice "hub"
	skynet.uniqueservice "db"
	skynet.uniqueservice "center"
	local gate_port = tonumber(skynet.getenv "gate_port")
	skynet.call(hub, "lua", "open", "0.0.0.0", gate_port)
	
	skynet.error "Server is already start finish!\n"
	skynet.exit()
end)