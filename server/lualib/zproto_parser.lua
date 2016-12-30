local lpeg = require "lpeg"
local P = lpeg.P
local S = lpeg.S
local R = lpeg.R
local C = lpeg.C
local Ct = lpeg.Ct
local Cg = lpeg.Cg
local V = lpeg.V


local exception = lpeg.Cmt(lpeg.Carg(1), function (_, pos, state)
    error(string.format("syntax error at file %s:%d!", state.file or "", state.line))
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
local protoid = alnum^1 / tonumber
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
    RESPONSE = namedpat("response", P"response" * (custompat(multipat(V"FIELD")) + blankpat"=" * typename)),
    FIELD = namedpat("field", typename * (C"[]" + (P"[" * blankpat(name) * "]"))^-1 * blanks * name * blankpat"=" * tag),
}
local protofile = dummy * protocol * dummy * eof

local all = { struct = {} , protocol = {} }
local buildin = { number = -1, string = -2, bool = -3, response = "response"}
local function checkname(str)
    assert(not buildin[str], "Buildin type: ".. str .." cann't be use for name!")
    return str
end
local function checktype(ptype, t)
    if buildin[t] then
       assert(t ~= "response", "Buildin type: response cann't be use for typename!")
       return t
    end
    local fullname = ptype .. "." .. t
    if all.struct[fullname] then
        return fullname
    end
    ptype = ptype:match "(.+)%..+$"
    if ptype then
        return checktype(ptype, t)
    elseif all.struct[t] then
        return t
    end
end
local convert = {_ss = 0 }
function convert.field(repeats, p, typename)
    local f = { type = p[1], name = p[2], tag = p[3] }
    if p[4] then
        f.key = p[2]
        f.name = p[3]
        f.tag = p[4]
    end
    f.name = checkname(f.name)
    assert(not repeats[f.tag], "Redefined tag in ".. typename ..".".. f.name)
    assert(not repeats[f.name], "Redefined name in ".. typename ..".".. f.name)
    repeats[f.tag] = true
    repeats[f.name] = true
    f.type = checktype(typename, f.type)
    assert(f.type, "Undefined type["..p[1].."] in field[".. typename ..".".. f.name.."].")

    if f.key and f.key ~= "[]" then
        assert(not buildin[f.type] and all.struct[f.type] and all.struct[f.type][f.key], "Undefined map key: "..f.key.." in field: "..typename.."."..f.name)
    end

    return f
end
function convert.struct(obj)
    convert._ss = convert._ss + 1
    local result = { name = checkname(obj[1]), field = {}, sort = convert._ss }
    local tags = {}
    local names = {}

    if obj[2] then
        for _, f in ipairs(obj[2]) do
            if f.type == "field" then
                local r = convert.field(tags, f, result.name)
                result.field[r.name] = r
            else
                assert(f.type == "struct")    -- nest type
                local nesttypename = result.name .. "." .. checkname(f[1])
                f[1] = nesttypename
                assert(all.struct[nesttypename] == nil, "Redefined " .. nesttypename)
                all.struct[nesttypename] = convert.struct(f)
            end
        end
    end

    return result
end
function convert.protocol(obj)
    local result = {tag = obj[1], name = checkname(obj[2]), field = {}}
    local response = obj[4]
    local field = nil
    if obj[3] then
        if obj[3].type == "response" then
            response = obj[3]
        elseif obj[3].type == nil then
            field = obj[3]
        end
    end

    if field then
        local t = {}
        for _, p in ipairs(field) do
            local f = convert.field(all, t, p, result.name)
            result.field[f.name] = f
        end
    end
            
    if response then
        if "table" == type(response[1]) then
            result.response = result.name ..".response"
            local rsp = convert.struct(all,  {result.response, response[1]})
            all.struct[rsp.name] = rsp
        else
            result.response = response[1]
            assert(all.struct[result.response], "Undefined type "..result.response.." in protocol ".. result.name)
        end
    end
    return result
end
local function adjust(r)
    local t = {}
    for _, obj in ipairs(r) do
        local set = all[obj.type]
        local name = obj[1]
        if obj.type == "protocol" then
            local tag = obj[1]
            assert(t[tag] == nil , "Redefined protocol id " .. tag)
            t[tag] = true
            name = obj[2]
        end
        name = checkname(name)
        assert(t[name] == nil , "Redefined type" .. name)
        t[name] = true
        set[name] = convert[obj.type](obj)
    end
    return all
end


local function link()
    local taginc = 0
    local function gentypetag(t)
        if buildin[t] then
            return buildin[t]
        else
            assert(all.struct[t])
            if not all.struct[t].tag then
                taginc = taginc + 1
                all.struct[t].tag = taginc
            end
            return all.struct[t].tag
        end
    end
    local function linkfield(f)
        if f.key and f.key ~= "[]" then
            dump(f)
            dump(all.struct[f.type])
            f.key = all.struct[f.type].field[f.key].tag 
        end
        if not buildin[f.type] then
            local fs = {}
            for _,f in pairs(all.struct[f.type].field) do
                --dump(f)
                linkfield(f)
                fs[name] = f
                f.name = nil
            end
            all.struct[f.type].field = fs
        end
        f.type = gentypetag(f.type)
    end

    local result = { P = {}, T = {} }

    local tmp = {}
    for _,p in pairs(all.protocol) do
        table.insert(tmp, p)
    end
    table.sort(tmp, function (a, b) return a.tag < b.tag end)--sort 只支持数组排序
    for _,p in pairs(tmp) do
        local fs = {}
        for name, f in pairs(p.field) do
            linkfield(f)
            fs[name] = f
            f.name = nil
        end
        p.field = fs

        if p.response then
            assert(type(p.response) == "string")
            p.response = gentypetag(p.response)
        end

        result.P[tostring(p.tag)] = p
        p.tag = nil
    end

    local tmp = {}
    for _,t in pairs(all.struct) do
        table.insert(tmp, t)
    end

    table.sort(tmp, function (a, b) return a.sort < b.sort end)--sort 只支持数组排序
    for _,t in pairs(tmp) do
        t.sort = nil
            
        if t.tag then
            local fs = {}
            for fname, f in pairs(t.field) do
                linkfield(f)
                fs[name] = f
                f.name = nil
            end
            t.field = fs
--            dump(t)

            result.T[t.tag] = t
            t.tag = nil
        end
    end
    dump(result.T)
    all = nil
    return result
end

function parser(text, filename)
    local state = { file = filename, pos = 0, line = 1 }
    local r = lpeg.match(protofile + exception , text , 1, state )
    return link(adjust(r))
end


--[[
-- The protocol of zproto
--1.字段tag从1开始
--2.字段类型后边加[] 表示数组 而 加[name] 表示以name为key的map
--3.协议中不允许定义自定义类型，如果有 response 字段 表示返回包
--4.内建关键字为 number, string, bool, response 不允许作为 字段name
--5.自定义类型可以内嵌自定义类型
type{
    string name = 1
    field {
        string name     =1
        integer buildin = 2
        integer[] type  = 3
        integer tag     =4
        boolean integer = 5
    }
    field[name] fields  =2
}

0=protocol{
    string name         =   1--fdfsdf
    integer buildin     = 2--fdfsdf
    response {
      integer index     = 1
    }
}

1 = roup {
    response = type.field
}
]]