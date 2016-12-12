local skynet = require "skynet"
local service = require "service"
local client = require "client"
local log = require "log"

local auth = {}
local users = {}
local handler = {}

local SUCC = { ok = true }
local FAIL = { ok = false }

--用户唯一seesion 由用户注册后由 后台 产生的 至少跟用户名 和密码 对口的唯一 md5字符串 
--隐藏真实密码

--注册用户 { session, username, source } 用户名 密码 来源等 返回给后台session 和 基础信息
--可以由PHP后台直接访问数据库实现 
function handler:signup(rqt)
	log("signup userid = %s", rqt.userid)

	if users[rqt.userid] then
		return FAIL
	else
		users[rqt.userid] = true
		return SUCC
	end
end

function handler:create_role(rqt)--创建角色 { session, name,	sex} 用户是否存在 角色名是否重复 是否有名字屏蔽词 如果注册成功  可以直接走 登录流程
	log("signup userid = %s", rqt.userid)

	if users[rqt.userid] then 
		return FAIL
	else
		users[rqt.userid] = true
		return SUCC
	end
end

--[[
登录 { session, name } 
DB 查询 是否存在该用户 返回 在线状态
如果在线 就踢下线 
然后  在分配的agent上用随机数生成 登录令牌 和 角色id给前端
向agent 处理 login消息的时候 验证token 以防止 刷登录]] 
function handler:signin(rqt)
	log("signin userid = %s", rqt.userid)
	if users[rqt.userid] then
		self.userid = rqt.userid
		self.exit = true
		return SUCC
	else
		return FAIL
	end
end

function handler:ping()
	log("ping")
end

function auth.shakehand(fd)
	local c = client.dispatch({ fd = fd }, handler)
	return c.userid
end

local function init()
	client.proto()
	client.init(handler)
end

service.init {
	command = auth,
	info = users,
	init = init,
	require = {"db"}
}