module("luci.controller.lcd1602", package.seeall)

function index()
    if not nixio.fs.access("/etc/config/lcd1602") then
        return
    end
    entry({ "admin", "services", "lcd1602" }, firstchild(), _("LCD1602 Screen"), 80).dependent = false
    entry({ "admin", "services", "lcd1602", "settings" }, cbi("lcd1602/settings"), _("LCD1602 Screen Settings"), 1)
    entry({ "admin", "services", "lcd1602", "action_status" }, call("action_status"))
    entry({"admin", "services", "lcd1602", "action_stop"}, call("action_stop"), nil)
    entry({"admin", "services", "lcd1602", "action_restart"}, call("action_restart"), nil)
end

function action_status()
    local e = {}
    e.running = luci.sys.call("pgrep /usr/bin/lcd1602-daemon >/dev/null") == 0
    luci.http.prepare_content("application/json")
    luci.http.write_json(e)
end

function action_stop()
    local result = os.execute("/etc/init.d/lcd1602 stop")
    luci.http.prepare_content("application/json")
    luci.http.write_json({ success = result == 0 })
end

function action_restart()
    local result = os.execute("/etc/init.d/lcd1602 restart")
    luci.http.prepare_content("application/json")
    luci.http.write_json({ success = result == 0 })
end
