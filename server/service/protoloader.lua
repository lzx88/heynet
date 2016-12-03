local skynet = require "skynet"
local sprotoloader = require "sprotoloader"
local service = require "service"
local log = require "log"

local loader = {}

function loader.load(path)
	local slot = skynet.getenv "sproto_slot_c2s"
	local name = path .. "sproto.c2s"
	print(name)
	sprotoloader.register(name, slot)
	log("Load proto [%s] in slot %d", name, slot)
	local name = path .. "sproto.s2c"
	local slot = skynet.getenv "sproto_slot_s2c"
	print(name)
	sprotoloader.register(name, slot)
	log("Load proto [%s] in slot %d", name, slot)

	--skynet.exit() --todo 未知原因 不能直接退出
end

service.init {
	command = loader,
}