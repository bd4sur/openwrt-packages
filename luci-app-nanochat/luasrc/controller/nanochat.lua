module("luci.controller.nanochat", package.seeall)

function index()
    if not nixio.fs.access("/etc/config/nanochat") then
        return
    end
    entry({ "admin", "services", "nanochat" }, firstchild(), _("Chat with Nano LM"), 80).dependent = false
    entry({ "admin", "services", "nanochat", "chat" }, template("nanochat/chatview"), _("Chat with Nano LM"), 1)
    entry({ "admin", "services", "nanochat", "settings" }, cbi("nanochat/settings"), _("Chat Setting"), 2)
    entry({ "admin", "services", "nanochat", "action_status" }, call("action_status"))
    entry({"admin", "services", "nanochat", "action_stop"}, call("action_stop"), nil)
    entry({"admin", "services", "nanochat", "action_restart"}, call("action_restart"), nil)
end

function action_status()
    local e = {}
    e.running = luci.sys.call("pgrep /usr/bin/nano-infer-ws-server >/dev/null") == 0
    luci.http.prepare_content("application/json")
    luci.http.write_json(e)
end

function action_stop()
    local result = os.execute("/etc/init.d/nanochat stop")
    luci.http.prepare_content("application/json")
    luci.http.write_json({ success = result == 0 })
end

function action_restart()
    local result = os.execute("/etc/init.d/nanochat restart")
    luci.http.prepare_content("application/json")
    luci.http.write_json({ success = result == 0 })
end
