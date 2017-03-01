local lpeg = require "lpeg"
local P = lpeg.P
local S = lpeg.S
local R = lpeg.R
local C = lpeg.C
local Ct = lpeg.Ct
local Cg = lpeg.Cg
local V = lpeg.V


local exception = lpeg.Cmt(lpeg.Carg(1), function (_, pos, state)
    error(string.format("Syntax error at protocol file:%s line:%d.", state.file or "", state.line))
    return pos
end)
local function count_lines(_, pos, state)
    if state.pos < pos then
        state.line = state.line + 1
        state.pos = pos
    end
    return pos
end

local eof = P(-1)
local alnum = R"09"
local blank0 = S" \t"^0
local blanks = S" \t"^1
local alpha = R"az" + R"AZ" + "_"
local word = alpha * (alpha + alnum)^0
local name = C(word)
local typename = C(word * ("." * word)^0)
local tag = P"0"^0 * R"19"^1 * R"09"^0 / tonumber
local protoid = R"09"^1 / tonumber
local lineend = lpeg.Cmt((P"\n" + "\r\n") * lpeg.Carg(1), count_lines)
local comment = "--" * (1 - lineend) ^0 * (lineend + eof)
local dummy = (S" \t" + lineend + comment)^0
local function blankpat(pat) return blank0 * pat * blank0 end
local newline = blankpat(lineend + comment)^1
local function custompat(pat) return dummy * "{" * ((newline * pat * newline) + dummy) * "}" end
local function namedpat(name, pat) return Ct(Cg(lpeg.Cc(name), "type") * pat) end
local function multipat(pat) return Ct(pat^0 * (newline * pat)^0) end

local protocol = P {
    "ALL",
    ALL = multipat(V"STRUCT" + V"PROTOCOL"),
    STRUCT = namedpat("struct", name * custompat(multipat(V"FIELD" + V"STRUCT"))),
    PROTOCOL = namedpat("protocol", protoid * blankpat"=" * blankpat(name) * V"PROTO"),
    PROTO = custompat(V"RESPONSE" + multipat(V"FIELD") * (newline * V"RESPONSE")^-1),
    RESPONSE = namedpat("response", P"#" * (custompat(multipat(V"FIELD")) + blankpat(typename))),
    FIELD = namedpat("field", typename * blanks * name * (C"[]" + C"{}")^-1 * blankpat"=" * tag),
}
local protofile = dummy * protocol * dummy * eof

local buildin = { number = -1, string = -2, bool = -3 }
local function checkname(str)
    assert(not buildin[str], "Buildin type: ".. str .." cann't be use for name.")
    return str
end
local function checktype(all, ptype, t)
    if buildin[t] then
       return t
    end
    local fullname = ptype .. "." .. t
    if all.struct[fullname] then
        return fullname
    end
    ptype = ptype:match "(.+)%..+$"
    if ptype then
        return checktype(all, ptype, t)
    elseif all.struct[t] then
        return t
    end
end
local convert = {}
function convert.field(all, repeats, p, typename)
    local f = { type = p[1], name = p[2], tag = p[3], key = 0}
    if p[4] then
        f.tag = p[4]
        if p[3] == "[]" then
            f.key = 1
        elseif p[3] == "{}" then
            f.key = 2
        end
    end
    f.name = checkname(f.name)
    assert(not repeats[f.tag], "Redefined tag in type:".. typename ..".".. f.name ..".")
    assert(not repeats[f.name], "Redefined name in type:".. typename ..".".. f.name ..".")
    repeats[f.tag] = true
    repeats[f.name] = true
    f.type = checktype(all, typename, f.type)
    assert(f.type, "Undefined type:"..p[1].." in field:".. typename ..".".. f.name..".")
    return f
end
function convert.struct(all, obj)
    local result = { name = checkname(obj[1]), field = {} }
    local tags = {}
    if obj[2] then
        for _, f in ipairs(obj[2]) do
            if f.type == "field" then
                local r = convert.field(all, tags, f, result.name)
                result.field[r.name] = r
            else
                assert(f.type == "struct")    -- nest type
                local nesttypename = result.name .. "." .. checkname(f[1])
                f[1] = nesttypename
                assert(all.struct[nesttypename] == nil, "Redefined type:" .. nesttypename..".")
                all.struct[nesttypename] = convert.struct(all, f)
            end
        end
    end

    return result
end
function convert.protocol(all, obj)
    local result = {tag = obj[1], name = checkname(obj[2]), response = obj[4]}
    local field = {}
    if obj[3] then
        if obj[3].type == "response" then
            result.response = obj[3]
        elseif obj[3].type == nil then
            field = obj[3]
        end
    end
    all.struct[result.name] = convert.struct(all, {result.name, field}) 
            
    if result.response then
        if "table" == type(result.response[1]) then
            local rsp = convert.struct(all, {result.name ..".response", result.response[1]})
            all.struct[rsp.name] = rsp
            result.response = rsp.name
        else
            result.response = result.response[1]
            if result.response then
                assert(all.struct[result.response], "Undefined type:"..result.response.." in protocol:".. result.name..".")
            end
        end
    end
    return result
end
local function adjust(r)
    local all = { struct = {} , protocol = {} }
    local t = {}
    for _, obj in ipairs(r) do
        local result = convert[obj.type](all, obj)
        assert(not t[result.name], "Redefined typename:".. result.name..".")
        t[result.name] = true
        all[obj.type][result.name] = result

        if obj.type == "protocol" then
            assert(not t[result.tag], "Redefined protocol no.".. result.tag.."!")
            t[result.tag] = true

            result.request = result.name
            result.name = nil
        end
    end
    return all
end

local function link(all, result, filename)
    filename = filename and filename .."." or ""
    local taginc = #result.T
    local function gentypetag(t, T)
        if buildin[t] then
            return buildin[t]
        else
            assert(all.struct[t])
            if not all.struct[t].tag then
                all.struct[t].tag = taginc
                T[taginc] = all.struct[t]
                taginc = taginc + 1
            end
            return all.struct[t].tag
        end
    end
    local function linkfield(f, T)
        if type(f.type) == "number" then
            return
        end
        if not buildin[f.type] then
            for _,f in pairs(all.struct[f.type].field) do
                linkfield(f, T)
            end
        end
        f.type = gentypetag(f.type, T)
    end

    local tmp = {}
    for _,p in pairs(all.protocol) do
        all.struct[p.request].name = filename .. p.request
        p.request = gentypetag(p.request, tmp)
        if p.response then
            assert(type(p.response) == "string")
            p.response = gentypetag(p.response, tmp)
        else
            p.response = -1
        end
        table.insert(result.P, p)
    end
    table.sort(result.P, function(a, b)
        return a.tag < b.tag
    end)
    local tt = -1
    for _,p in pairs(result.P) do
        assert(p.tag > tt, "Redefined protocol no.".. p.tag.."!")
        tt = p.tag
    end

    for _,t in pairs(tmp) do
        for _, f in pairs(t.field) do
            linkfield(f, result.T)
        end
        result.T[t.tag] = t
    end
    for _,t in pairs(result.T) do
        local fs = {}
        for _, f in pairs(t.field) do
            table.insert(fs, f)
        end
        table.sort(fs, function(a, b)
            return a.tag < b.tag
        end)
        t.field = fs
    end
    all = nil
    return result
end

--[[
The protocol of zproto
--1.字段tag从1开始，协议号 可以从0开始
--2.字段名加[] 表示数组 而 加{} 表示map
--3.协议中， 不允许内嵌自定义类型，如果有 #的字段则表示返回包
--4.内建关键字为 number, string, bool 不允许作为 字段name
--5.自定义类型可以内嵌自定义类型
--6.支持--单行注释
]]


local grammar = {}

function grammar.parse(text, filename, result)
    if not result then result = { T = {}, P = {} } end
    local state = { file = filename, pos = 0, line = 1 }
    local r = lpeg.match(protofile + exception, text, 1, state )
    local fnamepat = ((P(1) - (P"/" + "\\"))^0 * (P"/" + "\\")^1)^0 * C(word) * (P"." * (alpha + alnum)^0)^0
    return link(adjust(r), result, fnamepat:match(filename))
end

function grammar.fparse(paths)
    local result = { T = {}, P = {} }
    for _,path in pairs(paths) do
        local f = assert(io.open(path), "Can't open proto file:".. path ..".")
        local text = f:read "*a"
        f:close()
        grammar.parse(text, path, result)
    end
    return result
end

return grammar