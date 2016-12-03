local skynet = require "skynet"
local share = require "sharedata"

local function check(fname, data)
	assert(fname ~= "load")
	if data.check then
		data.check()
	end
end

local function share_config(path)
	local fname = io.getfilename(path)
	local content = io.readfile(path)
	local data = load(content)()
	check(fname, data)
	share.new(fname, data)
	skynet.error("Load config", path)
end

local config = {}
function config.load(path)
	io.loopfile(path, share_config)
end

local mt = function(t, k)
	return share.query(k)
end
return setmetatable(config, {__index = mt})
