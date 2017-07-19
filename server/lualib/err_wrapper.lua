local skynet = require "skynet"
require "errno"

local wrap = {}
local isdebug = skynet.getenv "debug"
if isdebug == "" then
	wrap.rpcHandle = function(cmd, ...)
					local function result(ok, e, ...)
						if ok then
							return true, e, ...
						elseif type(e) ~= "number" then
							log("@RPC[%s] raise error: %s ", cmd, e)
							e = E_SRV_STOP
						end
						return false, e
					end
					return result(pcall(...))
				end
	wrap.rpcResult = function(ok, e, ...)
					if ok then
						return e, ...
					end
					error(e)
				end
	wrap.callTimer = function( ... )
					local ok, e = pcall(...)
					if not ok then
						log("@Timer raise error: %s", e)
					end
				end
else
	wrap.rpcHandle = function (cmd, f, ...) return f(...) end
	wrap.rpcResult = function(...) return ... end
	wrap.callTimer = function(f, args) return f(args) end
end

return wrap