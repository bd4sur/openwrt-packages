m = Map("lcd1602", translate("LCD1602"), translate("LCD1602 Screen"))

m:section(SimpleSection).template = "lcd1602/settings"

s = m:section(TypedSection, "lcd1602")
s.addremove = false
s.anonymous = true

path = s:option(Value, "content", translate("Display content"))
path.default = "/emmc/_model/nano_168m_625000_sft_947000.bin"
path.rmempty = false
path.placeholder = "Only ASCII characters allowed"
path.description = translate("Contents to be displayed on the LCD.")

function m.on_after_commit(self)
    local output = luci.util.exec("/etc/init.d/lcd1602 reload >/dev/null 2>&1")
    luci.util.exec("logger '" .. output .. "'")
end

return m
