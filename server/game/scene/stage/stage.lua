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

function stage.add_role(id, info)
	roles[id] = info
end

function stage.remove_role(id)
	roles[id] = nil

end

return stage