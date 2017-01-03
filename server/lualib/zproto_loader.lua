local core = require "zproto.core"
local parser = require "zproto_parser"

local loader = {}

function loader.load(path)
	local tmp = {}
	io.loopfile(path, function(p)
		table.insert(tmp, p)
	end)
	local proto = parser.fparse(tmp)
	assert(proto)
	local zp = core.create(proto)
end