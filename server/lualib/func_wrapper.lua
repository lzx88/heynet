local skynet = require "skynet"
local log = require "log"
require "errno"

local wrap = {}
local isdebug = skynet.getenv "debug"
if isdebug == "true" then
	wrap.rpcHandle = function (cmd, f, ...) return f(...) end
	wrap.rpcResult = function(...) return ... end
	wrap.callTimer = function(f, args) return f(args) end
else
	wrap.rpcHandle = function(cmd, ...)
					local function result(ok, e, ...)
						if ok then
							return true, e, ...
						elseif type(e) ~= "number" then
							log("API.%s  raise error: %s", cmd, e)
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
						log("TIMER raise error: %s", e)
					end
				end
end

return wrap