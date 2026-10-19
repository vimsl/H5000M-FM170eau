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

-- BUGFIX 2026-08-28: CSRF 缓解。状态修改 action(改场景/天线模式)必须同源请求。
-- 允许无 Referer(隐私模式/curl 本机)与同源 Referer；拒绝跨站 Referer(恶意网页 <img>/fetch 触发)。
local function check_same_origin()
    local ref = luci.http.getenv("HTTP_REFERER") or ""
    if ref == "" then return true end
    local host = luci.http.getenv("HTTP_HOST") or ""
    if host == "" then return false end
    local scheme, rest = ref:match("^(https?://)(.*)$")
    if not scheme then return false end
    local ref_host = rest:match("^([^/]+)")
    if not ref_host then return false end
    ref_host = ref_host:gsub(":%d+$", "")
    host = host:gsub(":%d+$", "")
    return ref_host == host
end

-- 状态修改 action 入口统一校验; 拒绝时直接 403 并返回 true
local function guard_state_change()
    if not check_same_origin() then
        luci.http.status(403)
        luci.http.prepare_content("application/json")
        luci.http.write_json({ success = false, message = "forbidden" })
        return true
    end
    return false
end

function index()
    -- Hidden alias: old /admin/quickstart URL must not 404
    entry({"admin", "quickstart"}, alias("admin", "5g_dashboard"), nil, 1).dependent = false
    entry({"admin", "5g_dashboard"}, template("5g_dashboard/index"), _("5G Dashboard"), 2).dependent = false
    entry({"admin", "5g_dashboard", "overview"}, template("5g_dashboard/index"), nil)
    entry({"admin", "5g_dashboard", "aiqos_status"}, call("action_aiqos_status"), nil).leaf = true
    entry({"admin", "5g_dashboard", "aiqos_all"}, call("action_aiqos_all"), nil).leaf = true
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

-- BUGFIX 2026-08-28: 聚合接口,一次返回 status + antenna + spatial,
-- 前端 3 个轮询请求合并为 1 个,后端负载降 3 倍
function action_aiqos_all()
    luci.http.prepare_content("application/json")

    local out = {}

    local status_raw = sys.exec("ubus call aiqos get_status 2>/dev/null")
    local scenario_raw = sys.exec("ubus call aiqos get_scenario 2>/dev/null")
    local status = jsonc.parse(status_raw or "") or {}
    local scenario = jsonc.parse(scenario_raw or "") or {}
    status.current = scenario.current or status.scenario or "--"
    status.locked = (scenario.locked == true)
    out.status = status

    -- antenna mode (兼容 @main[0] 与 legacy global)
    local mode = sys.exec("uci -q get aiqos.@main[0].antenna_mode 2>/dev/null")
    if mode == nil or mode == "" then
        mode = sys.exec("uci -q get aiqos.global.antenna_mode 2>/dev/null") or ""
    end
    mode = (mode or ""):gsub("%s+", "")
    if mode == "" then mode = "auto" end
    local forced = sys.exec("uci -q get aiqos.@main[0].antenna_forced 2>/dev/null")
    if forced == nil or forced == "" then
        forced = sys.exec("uci -q get aiqos.global.antenna_forced 2>/dev/null") or ""
    end
    forced = (forced or ""):gsub("%s+", "") == "1"
    out.antenna = { mode = mode, forced = forced }

    local raw = sys.exec("ubus call aiqos get_spatial_info 2>/dev/null")
    out.spatial = jsonc.parse(raw or "") or {}

    luci.http.write_json(out)
end

function action_aiqos_scenario()
    luci.http.prepare_content("application/json")

    if guard_state_change() then return end

    local scenario = luci.http.formvalue("scenario") or ""
    if not AIQOS_SCENARIOS[scenario] then
        luci.http.write_json({
            success = false,
            message = "invalid scenario"
        })
        return
    end

    -- BUGFIX 2026-08-28: 设备 UCI 配置为匿名 section @main[0],原代码用 aiqos.global 会导致写入失败(ret=1),
    -- dashboard 显示 'set scenario failed' / 'set antenna mode failed'。改用 @main[0] 并兼容两种键名。
    local ret = sys.call("uci set aiqos.@main[0].scenario='" .. scenario .. "' && uci commit aiqos && (killall -USR1 aiqosd 2>/dev/null || true)")
    -- 清理遗留的 legacy global section(避免两份配置同时存在造成混乱)
    sys.call("(uci -q delete aiqos.global.scenario 2>/dev/null; uci -q commit aiqos) || true")
    luci.http.write_json({
        success = (ret == 0),
        current = scenario,
        message = ret == 0 and "ok" or "set scenario failed"
    })
end

function action_aiqos_antenna()
    luci.http.prepare_content("application/json")

    -- BUGFIX 2026-08-28: 兼容 @main[0] 和 legacy global 两种键名
    local mode = sys.exec("uci -q get aiqos.@main[0].antenna_mode 2>/dev/null")
    if mode == nil or mode == "" then
        mode = sys.exec("uci -q get aiqos.global.antenna_mode 2>/dev/null") or ""
    end
    mode = (mode or ""):gsub("%s+", "")
    if mode == "" then mode = "auto" end
    local forced = sys.exec("uci -q get aiqos.@main[0].antenna_forced 2>/dev/null")
    if forced == nil or forced == "" then
        forced = sys.exec("uci -q get aiqos.global.antenna_forced 2>/dev/null") or ""
    end
    forced = (forced or ""):gsub("%s+", "") == "1"
    luci.http.write_json({ mode = mode, forced = forced })
end

function action_aiqos_antenna_mode()
    luci.http.prepare_content("application/json")

    if guard_state_change() then return end

    local mode = luci.http.formvalue("mode") or ""
    if not AIQOS_ANTENNA_MODES[mode] then
        luci.http.write_json({ success = false, message = "invalid antenna mode" })
        return
    end

    local force = luci.http.formvalue("force") == "1"
    -- v63.5.1 (H5 治理): 写入统一走「单一写者」helper /usr/libexec/aiqos-set-antenna。
    --   原先此处自行 uci set(+forced)+commit + 清理 legacy + 写 2 个 /tmp 文件，
    --   而 rpcd/aiqos 的 set_antenna_mode 各写一份（且不写 forced、不做 legacy 清理）
    --   → 同一操作走 LuCI 与走 ubus 结果不一致 = 双份漂移。现两处入口共用同一实现。
    --   mode 已由上方 AIQOS_ANTENNA_MODES 白名单校验（helper 内亦二次校验）。
    local ret = sys.call("/usr/libexec/aiqos-set-antenna '" .. mode .. "' " .. (force and "1" or "0"))
    luci.http.write_json({ success = (ret == 0), mode = mode, forced = force, message = ret == 0 and "ok" or "set antenna mode failed" })
end

function action_aiqos_spatial()
    luci.http.prepare_content("application/json")

    local raw = sys.exec("ubus call aiqos get_spatial_info 2>/dev/null")
    local obj = jsonc.parse(raw or "") or {}
    luci.http.write_json(obj)
end