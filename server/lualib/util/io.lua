local function assert_open( ... ) return assert(io.open( ... )) end
local function assert_popen( ... ) return assert(io.popen( ... )) end
local function assert_write(f, ... ) return assert(f:write( ... )) end
local function assert_seek(f, ... ) return assert(f:seek( ... )) end
local function pcall_input(... ) return pcall(io.input( ... )) end
local function pcall_lines(... ) return pcall(io.lines( ... )) end
local function pcall_flines(f, ... ) return pcall(f:lines( ... )) end

--遍历path下所有以ext后缀的文件
function io.loopfile(path, fun, ext)
    local f = assert_popen("ls " .. path .. "*" .. ext)
    local fname
    while fname = f:read("l") do
        fun(fname)
    end
    io.close(f)
end

function io.readfile(path)
    local content = nil
    local f = assert_open(path, "r")
    content = f:read("*a")
    io.close(f)
    return content
end

function io.writefile(path, ...)
    local f = assert_open(path, "a+b")
    assert_write(f, ...)
    io.close(f)
end

function io.pathinfo(path)
    local pos = string.len(path)
    local extpos = pos + 1
    while pos > 0 do
        local b = string.byte(path, pos)
        if b == 46 then -- 46 = char "."
            extpos = pos
        elseif b == 47 then -- 47 = char "/"
            break
        end
        pos = pos - 1
    end

    local ret = {}
    ret.dir = string.sub(path, 1, pos)
    ret.file = string.sub(path, pos + 1)
    extpos = extpos - pos
    ret.base = string.sub(ret.file, 1, extpos - 1)
    ret.ext = string.sub(ret.file, extpos)
    return ret
end

function io.filesize(path)
    local size
    local f = assert_open(path, "r")
    local curr = assert_seek(f)
    size = assert_seek(f, "end")
    assert_seek(f, "set", curr)
    return size
end