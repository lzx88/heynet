local function assert_open( ... ) return assert(io.open( ... )) end
local function assert_popen( ... ) return assert(io.popen( ... )) end
local function assert_write(f, ... ) return assert(f:write( ... )) end
local function assert_seek(f, ... ) return assert(f:seek( ... )) end
local function pcall_input(... ) return pcall(io.input( ... )) end
local function pcall_lines(... ) return pcall(io.lines( ... )) end
local function pcall_flines(f, ... ) return pcall(f:lines( ... )) end

--遍历path下所有以ext后缀的文件
function io.loopfile(path, fun, ext)
    ext = ext and ("*" .. ext) or ""
    local f = assert_popen("ls " .. path .. ext)
    local fname = f:read("l")
    while fname do
        fun(string.match(fname, "%g+"))
        fname = f:read("l")
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

function io.getfilename(path)
    return string.match(path, "([^/^%.]+)%.(%w+)$")
end

function io.filesize(path)
    local size
    local f = assert_open(path, "r")
    local curr = assert_seek(f)
    size = assert_seek(f, "end")
    assert_seek(f, "set", curr)
    return size
end