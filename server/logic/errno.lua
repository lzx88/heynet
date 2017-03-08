local errno = {}

function errmsg(n)
	return errno[n] or "unknown"
end

local function E(n, msg)
	assert(errno[n] == nil, string.format("Had the same errno [%x] msg = %s", n, msg))
	errno[n] = msg
	return n
end

--系统错误码
SUCCESS			= E(0, "成功")
E_FORWARD		= E(1, "重定向")
E_TIMEOUT		= E(2, "请求超时")
E_SERIALIZE		= E(3, "序列化错误")
E_ARGUMENT		= E(4, "参数错误")
E_NO_IMPL		= E(5, "协议未实现")
E_ILL_OPR		= E(6, "非法操作")
E_DB			= E(7, "数据库操作失败")
E_MSG_TOO_LONG	= E(8, "消息包太长")
E_NO_PROTO		= E(9, "不存在此协议")
E_NO_LOGIN		= E(10, "未登录")
E_SRV_STOP		= E(11, "功能异常")
E_BUSY			= E(12, "服务忙")
E_NETWORK		= E(14, "网络异常")
E_FUNC_CLOSE	= E(15, "功能暂时关闭")

--角色错误
E_USER_REPEAT	= E(100, "用户已存在")
E_USER_NO_EXIST	= E(101, "用户不存在")
E_ROLE_REPEAT	= E(102, "角色已存在")
E_ROLE_NO_EXIST	= E(103, "角色不存在")
E_LEVEL_LIMIT	= E(104, "等级限制")
E_LOGINED		= E(105, "已登录")

--
E_NO_ENOUGH		= E(200, "等级限制")

