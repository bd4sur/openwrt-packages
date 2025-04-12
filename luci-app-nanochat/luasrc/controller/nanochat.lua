module("luci.controller.nanochat", package.seeall)

function index()
    page = node("admin", "services", "nanochat")
    page.target = template("nanochat/chatview")
    page.title = _("Chat with NanoLM")
    page.order = 1
    page.acl_depends = { "luci-app-nanochat" }

    page = entry({"admin", "services", "nanochat", "action_stop"}, call("action_stop"), nil)
    page.acl_depends = { "luci-app-nanochat" }
    page.leaf = true

    page = entry({"admin", "services", "nanochat", "action_restart"}, call("action_restart"), nil)
    page.acl_depends = { "luci-app-nanochat" }
    page.leaf = true
end

function action_stop()
    local result = os.execute("/etc/init.d/nano-infer-ws-server stop")
    luci.http.prepare_content("application/json")
    luci.http.write_json({ success = result == 0 })
end

function action_restart()
    local result = os.execute("/etc/init.d/nano-infer-ws-server restart")
    luci.http.prepare_content("application/json")
    luci.http.write_json({ success = result == 0 })
end
