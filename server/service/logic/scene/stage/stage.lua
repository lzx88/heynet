local map = require "stage/map"

--[[AOI算法:
	用九宫格订阅方式
	另一种做法是 稀疏矩阵即十字链表 其三元组链表的效率
	]]

local npcs = {}
local roles = {}
local monsters = {}
local mantle = {}

local stage = {}

function stage.load(id)
	
end

function stage.enter(role)
	roles[role.id] = role
end

function stage.exit(roleid)
	roles[roleid] = nil

end

return stage