local skynet = require "skynet"
local game = require "game"
local redis = require "redis"
local moduled = require "moduled"

local conf
local DB

local CMD = {}
local function init()
	conf = {
		host = skynet.getenv "redis_host",
		port = skynet.getenv "redis_port",
		db = skynet.getenv "redis_db",
        auth,
    }
    DB = redis.connect(conf)

    moduled.dispatch(DB, CMD)
end

game.start{
	command = CMD,
	init = init,
}