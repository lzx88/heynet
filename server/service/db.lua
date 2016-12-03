local skynet = require "skynet"
local game = require "game"
local redis = require "redis"
local log = require "log"
local moduled = require "moduled"

local conf
local DB

local db = {}

function db.role(roleid, cmd, ...)
	return moduled.dispatchR(roleid, cmd, ...)
end

local function init()
	conf = {
		host = skynet.getenv "redis_host",
		port = skynet.getenv "redis_port",
		db = skynet.getenv "redis_db",
        auth,
    }
    DB = redis.connect(conf)

    moduled.init(db, DB)
end

game.start{
	command = db,
	init = init,
}