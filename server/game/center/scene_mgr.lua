local skynet = require "skynet"
local log = require "log"

local pool = {}

local copys = {}
local citys = {}

local function new_copy()
	local copy = table.remove(pool)
	if not copy then
		copy = skynet.newservice "scene"
	end
	return copy
end

local function free_copy(copy)
	table.insert(pool, copy)
end

local scene_mgr = {}

local function new_city()
	local config = {}
	for i = 1, 2 do
		local city = skynet.newservice "scene"
		citys[i] = city
		skynet.call(city, "lua", "load", i)
	end
end

function scene_mgr.init()
	new_city()
end

function scene_mgr.get_scene_addr(mapid)
	--if city[mapid] then
		return city[1]
end

return scene_mgr