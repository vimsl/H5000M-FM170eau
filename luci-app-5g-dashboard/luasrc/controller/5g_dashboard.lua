module("luci.controller.5g_dashboard", package.seeall)

local jsonc = require("luci.jsonc")
local sys = require("luci.sys")
local util = require("luci.util")

local AIQOS_SCENARIOS = {
    auto = true,
    urban = true,
    rural = true,
    wilderness = true,
    high_speed_rail = true,
    dark_cci = true,
    dark_throttle = true
}

local AIQOS_ANTENNA_MODES = {
    auto = true,
    omni = true,
    directional = true
}

function index()
    -- Hidden alias: old /admin/quickstart URL must not 404
    entry({"admin", "quickstart"}, alias("admin", "5g_dashboard"), nil, 1).dependent = false
    entry({"admin", "5g_dashboard"}, template("5g_dashboard/index"), _("5G Dashboard"), 2).dependent = false
    entry({"admin", "5g_dashboard", "overview"}, template("5g_dashboard/index"), nil)
    entry({"admin", "5g_dashboard", "aiqos_status"}, call("action_aiqos_status"), nil).leaf = true
    entry({"admin", "5g_dashboard", "aiqos_scenario"}, call("action_aiqos_scenario"), nil).leaf = true
    entry({"admin", "5g_dashboard", "aiqos_antenna"}, call("action_aiqos_antenna"), nil).leaf = true
    entry({"admin", "5g_dashboard", "aiqos_antenna_mode"}, call("action_aiqos_antenna_mode"), nil).leaf = true
    entry({"admin", "5g_dashboard", "aiqos_spatial"}, call("action_aiqos_spatial"), nil).leaf = true
end

function action_aiqos_status()
    luci.http.prepare_content("application/json")

    local status_raw = sys.exec("ubus call aiqos get_status 2>/dev/null")
    local scenario_raw = sys.exec("ubus call aiqos get_scenario 2>/dev/null")

    local status = jsonc.parse(status_raw or "") or {}
    local scenario = jsonc.parse(scenario_raw or "") or {}
    status.current = scenario.current or status.scenario or "--"
    status.locked = (scenario.locked == true)

    luci.http.write_json(status)
end

function action_aiqos_scenario()
    luci.http.prepare_content("application/json")

    local scenario = luci.http.formvalue("scenario") or ""
    if not AIQOS_SCENARIOS[scenario] then
        luci.http.write_json({
            success = false,
            message = "invalid scenario"
        })
        return
    end

    local payload = util.shellquote('{"scenario":"' .. scenario .. '"}')
    local ret = sys.call("ubus call aiqos set_scenario " .. payload .. " 2>/dev/null")
    luci.http.write_json({
        success = (ret == 0),
        message = ret == 0 and "ok" or "set scenario failed"
    })
end

function action_aiqos_antenna()
    luci.http.prepare_content("application/json")

    local mode = sys.exec("uci -q get aiqos.global.antenna_mode 2>/dev/null")
    mode = (mode or ""):gsub("%s+", "")
    if mode == "" then mode = "auto" end
    luci.http.write_json({ mode = mode })
end

function action_aiqos_antenna_mode()
    luci.http.prepare_content("application/json")

    local mode = luci.http.formvalue("mode") or ""
    if not AIQOS_ANTENNA_MODES[mode] then
        luci.http.write_json({ success = false, message = "invalid antenna mode" })
        return
    end

    local ret = sys.call("uci set aiqos.global.antenna_mode='" .. mode .. "' && uci commit aiqos")
    sys.exec("echo '" .. mode .. "' > /tmp/aiqos_antenna_mode 2>/dev/null")
    luci.http.write_json({ success = (ret == 0), mode = mode, message = ret == 0 and "ok" or "set antenna mode failed" })
end

function action_aiqos_spatial()
    luci.http.prepare_content("application/json")

    local raw = sys.exec("ubus call aiqos get_spatial_info 2>/dev/null")
    local obj = jsonc.parse(raw or "") or {}
    luci.http.write_json(obj)
end