module("luci.controller.5g_dashboard", package.seeall)

function index()
    entry({"admin", "5g_dashboard"}, template("5g_dashboard/index"), _("5G 监控看板"), 2).dependent = false
    entry({"admin", "5g_dashboard", "overview"}, template("5g_dashboard/index"), nil)
end
