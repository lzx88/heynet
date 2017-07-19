local skynet = require "skynet"

local log = {}

function log.format(fmt, ...)
	skynet.error(string.format("@%s "..fmt, SERVICE_NAME, ...))
end

function log.__call(self, ...)
	if select("#", ...) == 1 then
		skynet.error(tostring((...)))
	else
		self.format(...)
	end
end

return setmetatable(log, log)

