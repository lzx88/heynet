local actor = require "actor"
local client = require "client"
local log = require "log"

local api = {}
local req = {}
local auth = {}

local function gettoken()
	return math.random()
end

--用户唯一seesion 由用户注册后由 后台 产生的 至少跟用户名 和密码 对口的唯一 md5字符串 
--隐藏真实密码

--注册用户 { session, username, source } 用户名 密码 来源等 返回给后台session 和 基础信息
--可以由PHP后台直接访问数据库实现
function req:signup(args)
	log("signup username = %s", args.username)
	local userid = actor.call("db", "user.signup", args)
	return { userid = userid, session = args.session }
end

function req:create_role(args)
--创建角色 { session, name, sex} 用户是否存在 角色名是否重复 是否有名字屏蔽词 如果注册成功  可以直接走 登录流程
	local roleid = actor.call("db", "user.create_role", args)
	return { roleid = roleid, session = args.session }
end

--[[
登录 { session, name } 
DB 查询 是否存在该用户 返回 在线状态
如果在线 就踢下线 
然后  在分配的agent上用随机数生成 登录令牌 和 角色id给前端
向agent 处理 login消息的时候 验证token 以防止 刷登录]] 
function req:signin(args)
	dump(args)
	log("signin rolename = %s", args.name)
	if self.session then
		error(E_ILL_OPR)
	end
	self.session = args.session
	return {token = 1}
	--[[local userid, loginrole = actor.call("db", "user.query", args.session)
	self.userid = userid
	self.loginrole = loginrole
	self.token = gettoken()

	self.exit = true
	return {token = self.token}]]
end

function api.shakehand(fd)
	log(fd)
	local ctx = client.dispatch({ fd = fd }, req)
	return ctx.userid, ctx.token, ctx.loginrole
end

local function init()
	
end

actor.run{
	command = api,
	init = init,
	info = auth,
}
