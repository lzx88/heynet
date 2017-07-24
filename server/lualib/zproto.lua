local core = require "zproto.core"

local zproto = {}

local function loopfile(path, fun, ext)
    ext = ext and ("*" .. ext) or ""
    local f = assert(io.popen("ls " .. path .. ext))
    local fname = f:read("l")
    while fname do
        fun(string.match(fname, "%g+"))
        fname = f:read("l")
    end
    io.close(f)
end

function zproto.load(path)    
    local grammar = require("zproto_grammar")
    local parse = grammar.parser()
    loopfile(path, function(file)
        local f = assert(io.open(file), "Can't open protocol file:".. file ..".")
        local text = f:read "*a"
        f:close()
        parse(text, file)
    end)
    local result = grammar.result()
    zproto._cobj = core.create(result)
    core.save(zproto._cobj)
end

local __cache = setmetatable( {} , { __mode = "v"})

function zproto.find(key)
    local protocol = __cache[key]
    if not protocol then
        protocol = {}
        protocol.tag, protocol.name, protocol.request, protocol.response = core.load(key)
        __cache[protocol.tag] = protocol
        __cache[protocol.name] = protocol
    end
    return protocol
end

zproto.pack = core.pack
zproto.unpack = core.unpack
zproto.decode = core.decode
zproto.encode = core.encode
zproto.decode_header = core.decode_header

local mt = {}
function mt:__gc()
    if self._cobj then
        core.delete(self._cobj)
    end
end

return setmetatable(zproto, mt)