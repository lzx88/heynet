local skynet = require "skynet"
local share = require "sharedata"

local config = {}

local config_path = skynet.getenv "config_path"
local function add(fname)
	local name = string.match(fname, "([_%a%d]+).")
	config[name] = require(config_path .. fname)
end
io.loopfile(config_path, add, ".cfg")
share.new("config", config)
config = share.query("config")
return config