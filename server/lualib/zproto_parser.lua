function val2str(arg1, arg2)
    local str = ""
    local tab = 1
    local function _2str(v, k, n)
        for i = 1, n do
            str = str .. "  "
        end
        k = k and k .. ' = ' or ""
        local t = type(v)
        if t == "table" then
            str = str .. k .. "{\n"
            n = n + 1
            local s = 0
            for k_,v_ in ipairs(v) do
                _2str(v_, k_, n)
                s = k_
            end
            for k_,v_ in pairs(v) do
                if type(k_) == "number" and k_ > s then _2str(v_, k_, n) end
            end
            for k_,v_ in pairs(v) do
                if type(k_) ~= "number" then _2str(v_, k_, n) end
            end
            n = n - 1
            for i = 1, n do
                str = str .. "  "
            end
            str = str .. (n == tab and "}" or "}\n")
        elseif t == "string" then
            str = str .. k .. "\"" .. v .. "\"\n"
        elseif t == "number" then
            str = str .. k .. v .. "\n"
        elseif t == "function" or t == "boolean" then
            str = str .. k .. tostring(v) .. "\n"
        else
            assert(false)
        end
    end
    
    local val = arg2 and arg2 or arg1
    local name = arg2 and arg1 or nil
    _2str(val, name, tab)
    return str
end
function dump(arg1, arg2) print(val2str(arg1, arg2)) end

local lpeg = require "lpeg"
local P = lpeg.P
local S = lpeg.S
local R = lpeg.R
local C = lpeg.C
local Ct = lpeg.Ct
local Cg = lpeg.Cg
local Cc = lpeg.Cc
local V = lpeg.V

local function count_lines(_, pos, parser_state)
    if parser_state.pos < pos then
        parser_state.line = parser_state.line + 1
        parser_state.pos = pos
    end
    return pos
end

local exception = lpeg.Cmt(lpeg.Carg(1), function (_, pos, parser_state)
    error(string.format("syntax error at file %s:%d", parser_state.file or "", parser_state.line))
    return pos
end)

local eof = P(-1)
local alnum = R"09"
local blank0 = S" \t" ^ 0
local blanks = S" \t" ^ 1
local alpha = R"az" + R"AZ" + "_"
local word = alpha * (alpha + alnum) ^ 0
local name = C(word)
local typename = C(word * ("." * word) ^ 0)
local tag = P"0"^0 * R"19"^1 * R"09"^0 / tonumber

local lineend = lpeg.Cmt((P"\n" + "\r\n") * lpeg.Carg(1) ,count_lines)
local comment = "--" * (1 - lineend) ^0 * (lineend + eof)
local dummy = (S" \t" + lineend + comment)^0
local newline = (blank0 * (lineend + comment) * blank0)^1


local function namedpat(name, pat)
    return Ct(Cg(Cc(name), "type") * pat)
end

local function blankpat(pat)
    return blank0 * pat * blank0
end

local function custompat(pat)
    return dummy * "{" * ((newline * pat * newline) + dummy) * "}"
end

local function multipat(pat)
    return Ct(pat^0 * (newline * pat)^0)
end

local protocol = P {
    "ALL",
    ALL = multipat(V"STRUCT" + V"PROTOCOL"),
    STRUCT = namedpat("struct", name * custompat(multipat(V"FIELD" + V"STRUCT"))),
    PROTOCOL = namedpat("protocol", tag * blankpat"=" * blankpat(name) * V"PROTO"),
    PROTO = custompat(V"RESPONSE" + multipat(V"FIELD") * (newline * V"RESPONSE")^-1),
    RESPONSE = namedpat("response", P"response" * (custompat(multipat(V"FIELD")) + blankpat"=" * typename)),
    FIELD = namedpat("field", typename * (C"[]" + (P"[" * blankpat(name) * "]"))^-1 * blanks * name * blankpat"=" * tag),
}

local protofile = dummy * protocol * dummy * eof

-- The protocol of zproto
--[[
1.字段tag从1开始
2.字段类型后边加[] 表示数组 而 加[name] 表示以name为key的map
3.协议中不允许定义自定义类型，如果有esponse 字段 表示返回包 
type{
    string name = 1
    field {
        string name =1
        integer buildin = 2
        integer[] type = 3
        integer tag =4
        boolean integer = 5
    }
    field[name] fields =2
}

0=protocol{
    string name = 1--fdfsdf
    integer buildin  = 2--fdfsdf
    response {
      integer index = 1
    }
}

1 = roup {
    response = type.field
}
]]

local convert = {}

function convert.protocol(all, obj)
    local result = { tag = obj[1] }
    for _, p in ipairs(obj[3]) do
        assert(result[p[1]] == nil)
        local typename = p[2]
        if type(typename) == "table" then
            local struct = typename
            typename = obj[1] .. "." .. p[1]
            all.type[typename] = convert.type(all, { typename, struct })
        end
        result[p[1]] = typename
    end
    return result
end

function convert.struct(all, obj)
    local result = {}
    local typename = obj[1]
    local tags = {}
    local names = {}
    for _, f in ipairs(obj[2]) do
        if f.type == "field" then
            local name = f[1]
            if names[name] then
                error(string.format("redefine %s in type %s", name, typename))
            end
            names[name] = true
            local tag = f[2]
            if tags[tag] then
                error(string.format("redefine tag %d in type %s", tag, typename))
            end
            tags[tag] = true
            local field = { name = name, tag = tag }
            table.insert(result, field)
            local fieldtype = f[3]
            if fieldtype == "*" then
                field.array = true
                fieldtype = f[4]
            end
            local mainkey = f[5]
            if mainkey then
                assert(field.array)
                field.key = mainkey
            end
            field.typename = fieldtype
        else
            assert(f.type == "type")    -- nest type
            local nesttypename = typename .. "." .. f[1]
            f[1] = nesttypename
            assert(all.type[nesttypename] == nil, "redefined " .. nesttypename)
            all.type[nesttypename] = convert.type(all, f)
        end
    end
    table.sort(result, function(a,b) return a.tag < b.tag end)
    return result
end

local function adjust(r)
    local result = { struct = {} , protocol = {} }
    local t = {}
    for _, obj in ipairs(r) do
        local set = result[obj.type]
        local name = obj[1]
        if obj.type == "protocol" then
            name = obj[2]
        end  
        assert(t[name] == nil , "redefined " .. name)
        t[name] = 0
        set[name] = convert[obj.type](result, obj)
    end

    return result
end

local function parser(text, filename)
    local state = { file = filename, pos = 0, line = 1 }
    local r = lpeg.match(protofile + exception , text , 1, state )
    return adjust(r)
end

local f = assert(io.open("test.proto"), "Can't open sproto file")
local text = f:read "*a"
f:close()

parser(text, "test.proto")