local command = require "gm/gm_command"

local gm_ctrl = {}
function gm_ctrl.dispatch(cmd, args)
	log("gm command %s #args = %d", cmd, #args)
	local ec
	if cmd == "man" then
		return SUCCESS, command.man[args[1]]
	end
	local func = command[cmd]
	if func then
		local ok, err = xpcall(function()		
				ec = func(table.unpack(args))
			end,
			debug.traceback)
		if not ok then
			log(err)
			return E_SRV_STOP
		end
		if not ec then
			ec = SUCCESS
		else
			log(errmsg(ec))
		end
	else
		log("gm command[%s] no exist", cmd)
	end
    return ec
end

return gm_ctrl