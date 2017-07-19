local modulea = {}
--模块加载是有序的
local require_impl = {
	"bag/bag_impl",
}

local require_ctrl = {
	"bag/bag_ctrl",
}

local mlist = {}
local midxs = {}
local hlist = {}

local function register(paths, func)
	for i = 1, #paths do
		local p = paths[i]
		local tbl = require(p)
		local folder, file = string.match(p, "([_%a%d]+)[/%.]([_%a%d]+)")
		func(folder, tbl)
	end
end

register(require_impl, function(name, impl)
	assert(not mlist[name], "Impl module " .. name .. "repeat")
	mlist[name] = impl
	table.insert(midxs, {name, impl})
end)
require_impl = nil

register(require_ctrl, function(name, ctrl)
	table.loop(ctrl, function(func, cmd)
		if type(func) == "function" then
			assert(not hlist[cmd], "Ctrl proto cmd ".. name ..".".. cmd .. " repeat")
			hlist[cmd] = func
		end
	end)
end)
require_ctrl = nil

function modulea.loop( func_mod )
	for i = 1, #midxs do
	 	func_mod(midxs[i][1], midxs[i][2])
	end
end

function modulea.anti( func_mod )
	for i = #midxs, 1, -1 do
	 	func_mod(midxs[i][1], midxs[i][2])
	end
end

function modulea.splitter(api, req)
	table.loop(hlist, function( func, cmd )
		assert(not req[cmd], "Ctrl proto:".. cmd .. " req aready exist")
		hlist[cmd] = function (_, ... )
			func( ... )
		end
	end)
	setmetatable(req, {__index = hlist})
	hlist = nil

	local mt = {}
	table.loop(mlist, function( impl, name )
		assert(not api[name], "Impl module:".. name .. " api aready exist")
		table.loop(impl, function( func, k )
			mt[name..'.'..k] = func
		end)
	end)
	setmetatable(api, {__index = mt})
	mlist = nil
end

return modulea