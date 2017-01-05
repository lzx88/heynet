local core = require "zproto.core"
local parser = require "zproto_parser"

local zproto = {}

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

function zproto.new()
    local self = {
        __cache = setmetatable( {} , { __mode = "kv"}),
    }
    return setmetatable(self, zproto)
end

function zproto.load()
    local self = zproto.new()
    local tbl = table.pack(core.load())
    local i = 1
    repeat
        local proto = { tag = tbl[i], request = tbl[i + 2], response = tbl[i + 3] }
        local name =  tbl[i + 1]
        self.__cache[name] = proto
        i = i + 4
    until i >= tbl.n
end

function zproto:find(name)
    return self.__cache[name]
end

return zproto