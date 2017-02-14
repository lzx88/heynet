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
        __cache = setmetatable( {} , { __mode = "v"}),
    }
    return setmetatable(self, zproto)
end

function zproto.load()
    local self = zproto.new()
    local tbl = table.pack(core.load())
    local i = 1
    while i < tbl.n do
        local name =  tbl[i]
        self.__cache[name] = { tag = tbl[i + 1], request = tbl[i + 2], response = tbl[i + 3] }
        i = i + 4
    end
end

function zproto:find(name)
    return self.__cache[name]
end

function zproto:encode(typename, tbl)
    local proto = self:find(typename)
    return core.encode(proto.tag, proto.request, tbl)
end

function zproto:decode(tag, data)
    return core.decode(tag, data)
end


return zproto