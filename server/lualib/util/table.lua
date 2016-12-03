function table.loop(t, func_v_k)
    for k,v in pairs(t) do
        func_v_k(v, k)
    end
end

function table.count(t)
    local n = 0
    table.loop(t, function() n = n + 1 end)
    return n
end

function table.empty(t)
    return _G.next(t) == nil
end

--从字符串创建表
function table.from_string(str)
    assert(load(str)())
end

function table.keys(t)
    local keys = {}
    table.loop(t, function(_, k)
        table.insert(keys, k)
    end)
    return keys
end

function table.values(t)
    local values = {}
    table.loop(t, function(v)
        table.insert(values, v)
    end)
    return values
end

-- k v翻转
function table.roll(t)
    local rolls = {}
    table.loop(t, function(v, k)
        --assert(type(v) ~= "table" and type(v) ~= "function")
        rolls[v] = k
    end)
    return rolls
end

function table.map(t, func_v_k)
    table.loop(t, function(v, k) t[k] = func_v_k(v, k) end)
end

--过滤不满足条件f的key
function table.filter(t, func_v_k)
    table.loop(t, function(v, k)
        if not func_v_k(v, k) then t[k] = nil end
    end)
end

function table.rename_key(t, old, new)
    assert(t[old], string.format("key of %s dose not exist!", old))
    t[new] = t[old]
    t[old] = nil
end

-- --二分查找
-- function table.bsearch(elements, x, field, low, high)
--     local meta = getmetatable(elements)
--     low = low or 1
--     high = high or (meta and meta.__len(elements) or #elements)
--     if low > high then
--         return -1
--     end

--     local mid = math.ceil((low + high) / 2)
--     local element = elements[mid]
--     local value = field and element[field] or element

--     if x == value then
--         while mid > 1 do
--             local prev = elements[mid - 1]
--             value = field and prev[field] or prev
--             if x ~= value then
--                 break
--             end
--             mid = mid - 1
--             element = prev
--         end
--         return mid
--     end

--     if x < value then
--         return table.bsearch(elements, x, field, low, mid - 1)
--     end

--     if x > value then
--         return table.bsearch(elements, x, field, mid + 1, high)
--     end
-- end