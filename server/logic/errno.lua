local errno = {}

function errmsg(n)
	return errno[n] or "unknown"
end

local function E(n, msg)
	assert(n <= 0 and errno[n] == nil, string.format("Had the same errno [%x] msg = %s", n, msg))
	errno[n] = msg
	return n
end

SUCCESS 		= E(0, "成功")
E_FORWARD		= E(-1, "重定向")
E_TIMEOUT		= E(-2, "请求超时")
E_SERIALIZE		= E(-3, "序列化错误")
E_ARGUMENT		= E(-4, "参数错误")
E_NO_IMPL		= E(-5, "协议未实现")
E_ILL_OPR		= E(-6, "非法操作")
E_DB			= E(-7, "数据库操作失败")
E_MSG_TOO_LONG	= E(-8, "消息包太长")
E_NO_PROTO		= E(-9, "不存在此协议")
E_NO_LOGIN		= E(-10, "未登录")
E_SRV_STOP		= E(-11, "功能异常")
E_BUSY			= E(-12, "服务忙")
E_NETWORK		= E(-14, "网络异常")
E_FUNC_CLOSE	= E(-15, "功能暂时关闭")
E_OFFLINE		= E(-16, "断线")