m = Map("nanochat", translate("NanoChat"), translate("Chat with Nano LM"))

m:section(SimpleSection).template = "nanochat/settings"

s = m:section(TypedSection, "nanochat")
s.addremove = false
s.anonymous = true

path = s:option(Value, "path", translate("Model path"))
path.default = "/emmc/_model/nano_168m_625000_sft_947000.bin"
path.rmempty = false
path.placeholder = translate("Input model file path")
path.description = translate("Path to model file (.bin)")

max_len = s:option(Value, "max_len", translate("Max sequence length"))
max_len.default = "512"
max_len.rmempty = false
max_len.placeholder = translate("Max sequence length")
max_len.description = translate("The maximum length of the generated text and the prompt.")

port = s:option(Value, "port", translate("Server port"))
port.default = "8080"
port.rmempty = false
port.placeholder = translate("default: 8080")
port.description = translate("The port which model service is listening.")

function m.on_after_commit(self)
    local output = luci.util.exec("/etc/init.d/nanochat reload >/dev/null 2>&1")
    luci.util.exec("logger '" .. output .. "'")
end

return m
