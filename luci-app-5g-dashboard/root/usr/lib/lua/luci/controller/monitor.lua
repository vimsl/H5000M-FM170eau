module("luci.controller.monitor", package.seeall)

function index()
	entry({"admin", "monitor"}, firstchild(), _("5G Monitor"), 1).dependent = false
	entry({"admin", "monitor", "5g"}, template("5g_dashboard/index"), _("5G 仪表盘"), 10).leaf = false
	entry({"admin", "5g_dashboard"}, alias("admin", "monitor", "5g")).dependent = false
end