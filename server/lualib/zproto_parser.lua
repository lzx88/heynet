local lpeg = require "lpeg"
local P = lpeg.P
local S = lpeg.S
local R = lpeg.R
local C = lpeg.C
local Ct = lpeg.Ct
local Cg = lpeg.Cg
local V = lpeg.V

local exception = lpeg.Cmt(lpeg.Carg(1), function (_, pos, state)
    error(string.format("syntax error at file %s:%d.", state.file or "", state.line))
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
local newline = (blank0 * (lineend + comment) * blank0)^1

local function blankpat(pat)
    return blank0 * pat * blank0
end
local function custompat(pat)
    return dummy * "{" * ((newline * pat * newline) + dummy) * "}"
end
local function namedpat(name, pat)
    return Ct(Cg(lpeg.Cc(name), "type") * pat)
end
local function multipat(pat)
    return Ct(pat^0 * (newline * pat)^0)
end

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

local buildin = { number = 1, string = 2, bool = 3, response = 4}

local function getname(str)
    assert(not buildin[str], "Buildin type ".. str .." cann't be use for name.")
    return str
end

local function checktype(types, ptype, t)
    if buildin[t] then
       assert(t ~= "response", "Buildin type response cann't be use for name.")
       return t
    end
    local fullname = ptype .. "." .. t
    if types[fullname] then
        return fullname
    else
        ptype = ptype:match "(.+)%..+$"
        if ptype then
            return checktype(types, ptype, t)
        elseif types[t] then
            return t
        end
    end
end

local convert = {}

function convert.field(t, p, typename)
    local f = { type = p[1], name = getname(p[2]), tag = p[3] }
    if p[4] then
        f.tag = p[4]
        f.name = getname(p[3])
        if p[2] == "[]" then
            f.array = true
        else 
            f.key = p[2]
        end
    end
    assert(not t[f.tag], "Redefined tag in ".. typename ..".".. f.name)
    assert(not t[f.name], "Redefined name in ".. typename ..".".. f.name)
    t[f.tag] = true
    t[f.name] = true
    
    return f
end

function convert.protocol(all, obj)
    local result = {tag = obj[1]}
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
            if not result.field then
                result.field = {}
            end
            local f = convert.field(t, p, obj[2])
            result.field[f.name] = f
        end
    end
            
    if response then
        if "table" == type(response[1]) then 
            local t = {}
            for _, p in ipairs(response[1]) do
                if not result.response then
                    result.response = {}
                end
                local f = convert.field(t, p, obj[2] ..".response")
                result.response[f.name] = f
            end
        else
            result.response = response[1]
        end
    end
    return result
end

function convert.struct(all, obj)
    local result = {}
    local typename = getname(obj[1])
    local tags = {}
    local names = {}
    for _, f in ipairs(obj[2]) do
        if f.type == "field" then
            if not result.field then
                result.field = {}
            end
            local r = convert.field(tags, f, typename)
            result.field[r.name] = r
        else
            assert(f.type == "struct")    -- nest type
            local nesttypename = typename .. "." .. f[1]
            f[1] = nesttypename
            assert(all.struct[nesttypename] == nil, "Redefined " .. nesttypename)
            all.struct[nesttypename] = convert.struct(all, f)
        end
    end
    return result
end

local function link(r)
    local typetag = 1
    local function gentypetag(t)
        if not buildin[t] then
            local stru = r.struct[t]
            assert(stru)
            if not stru.tag then
                stru.tag = typetag
                typetag = typetag + 1
            end
            t = stru.tag
        end
        return t
    end
    local function linktype(f)
        if f.key then
            assert(r.struct[f.type].field[f.key], "Field ".. f.key .. " is not in Type ".. f.type)
            f.key = r.struct[f.type].field[f.key].tag 
        end
        f.type = gentypetag(f.type)
    end
    local result = {type = {}, proto = {}}
    table.sort(r.protocol, function(a,b) return a.tag < b.tag end)
    for name, p in pairs(r.protocol) do
        result.proto[tostring(p.tag)] = p
        p.name = name
        p.tag = nil

        if p.field then
            for name, f in pairs(p.field) do
                linktype(f)
            end
        end

        if type(p.response) == "table" then
            for name, f in pairs(p.response) do
                linktype(f)
            end
        elseif type(p.response) == "string" then
            p.response = gentypetag(p.response)
        end
    end
    for name, t in pairs(r.struct) do
        if t.tag then
            result.type[t.tag] = t
            t.tag = nil
            t.name = name
            if t.field then
                for fname, f in pairs(t.field) do
                    linktype(f)
                end
            end
        end
    end
    return result
end

local function check(r)
    for name, t in pairs(r.struct) do
        if t.field then
            for _, f in pairs(t.field) do
                local fullname = checktype(r.struct, name, f.type)
                if fullname == nil then
                    error(string.format("Undefined type %s in type %s", f.type, name))
                end
                f.type = fullname
            end
        end
    end

    for name, v in pairs(r.protocol) do
        for tag, f in pairs(v) do
            if type(tag) == "number" then
                checktype(r.struct, "", f.type)
            elseif tag == "response" then
                if type(f) == "string" and not r.struct[f] then
                    error(string.format("Undefined response type %s in protocol %s", f, name))
                end
                if type(f) == "table" then
                    for tag_, f_ in pairs(f) do
                        checktype(r.struct, "", f_.type)
                    end
                end
            end
        end
    end
    return r
end

local function adjust(r)
    local result = { struct = {} , protocol = {} }
    local t = {}
    for _, obj in ipairs(r) do
        local set = result[obj.type]
        local name = obj[1]
        if obj.type == "protocol" then
            local tag = obj[1]
            assert(t[tag] == nil , "Redefined protocol id " .. tag)
            t[tag] = true
            name = obj[2]
        end
        name = getname(name)
        assert(t[name] == nil , "Redefined type" .. name)
        t[name] = true
        set[name] = convert[obj.type](result, obj)
    end

    return result
end

function parser(text, filename)
    local state = { file = filename, pos = 0, line = 1 }
    local r = lpeg.match(protofile + exception , text , 1, state )
    return link(check(adjust(r)))
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