local core = require "zproto.core"
local parser = require "zproto_parser"

local zproto = { cache = {} }

function zproto.register(path)
    local tmp = {}
    io.loopfile(path, function(p)
        table.insert(tmp, p)
    end)
    local pt = parser.fparse(tmp)
    assert(pt)
    local zp = core.save(pt)
    assert(zp)
end

function zproto.load()
    zproto.cache = core.load()
end

function zproto.find(name)
    return zproto.cache[name]
end

return zproto