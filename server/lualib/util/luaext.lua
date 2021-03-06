local function random()
    math.randomseed(os.time())
    return math.random
end
math.random = random()

function tonum(e, base)
    return tonumber(e, base) or 0
end

function toint(...)
    return math.tointeger(tonumber(...) or 0)
end

function is_true(v)
    return (v ~= nil and v ~= false)
end

function clone(obj)
    local lookup = {}
    local function _copy(obj)
        if type(obj) ~= "table" then
            return obj
        elseif lookup[obj] then
            return lookup[obj]
        end
        local tbl = {}
        lookup[obj] = tbl
        for k, v in pairs(obj) do
            tbl[_copy(k)] = _copy(v)
        end
        return setmetatable(tbl, getmetatable(obj))
    end
    return _copy(obj)
end

function copy(obj)
    if not obj then return obj end
	 local new = {}
	 for k, v in pairs(obj) do
		local t = type(v)
		if t == "table" then
			new[k] = copy(v)
		elseif t == "userdata" then
			new[k] = copy(v)
		else
			new[k] = v
		end
	 end
    return new
end

function class(name, super)
    local C = {}
	if super then
		setmetatable(C, {__index = super})
		C.super = super
	else
		C.ctor = function() end
	end
	C.__cname = name
	C.__ctype = 2 -- lua
	C.__index = C

	function C.new(...)
		local obj = setmetatable({}, C)
		obj.class = C
		obj:ctor(...)
		return obj
	end
    return C
end

--将变量转换成缩进格式字符串，若是表 会递归缩进
function val2str(arg1, arg2)
    local str = ""
    local tab = 1
    local function _2str(v, k, n)
        for i = 1, n do
            str = str .. "  "
        end
        k = k and ((type(k) == "number") and "["..k.."] = " or k .. " = ") or ""
        local t = type(v)
        if t == "table" then
            if not next(v) then
                str = str .. k .. (n == tab and "{}" or "{}\n")
                return
            end
            str = str .. k .. "{\n"
            n = n + 1
            local s = 0
            for k_,v_ in ipairs(v) do
                _2str(v_, k_, n)
                s = k_
            end
            for k_,v_ in pairs(v) do
                if type(k_) == "number" and (k_ > s or k_ < 1) then _2str(v_, k_, n) end
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
        elseif t == "nil" or "number" or "function" or "boolean" or "userdata" then
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

function dump(arg1, arg2)
    print(val2str(arg1, arg2))
end