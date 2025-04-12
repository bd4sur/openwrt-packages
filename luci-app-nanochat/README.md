# luci-app-nanochat

适用于OpenWrt的LuCI框架的自制Nano语言模型的推理前台组件。

## 编译与安装

1、设置feeds：在SDK或者OpenWrt固件目录下创建`feeds.conf`文件，内容为

```
# 使用远程git仓库
src-git bd4sur https://github.com/bd4sur/openwrt-packages.git
# 或者克隆到本地后，使用本地目录
src-link bd4sur /xxx/openwrt-packages
```

2、更新feeds并安装本组件以及依赖的推理后端组件：

```
./scripts/feeds update bd4sur && ./scripts/feeds install luci-app-nanochat && ./scripts/feeds install nano-infer-ws-server
```

3、执行`make menuconfig`，在菜单中手动设置构建选项。在LuCI菜单中找到`luci-app-nanochat`并按“M”选中，意思是构建成单独的ipk包，但不打包进固件。构建系统会自动选中依赖的`nano-infer-ws-server`组件。

4、开始编译本组件：

```
make package/luci-app-nanochat/compile V=s
```

5、编译完成后，可以在`bin/packages/<arch>/bd4sur(feeds的名称)`目录下找到对应的ipk。可以通过LuCI的网页图形界面直接上传ipk并安装。注意：在此之前，需要先安装后端组件`nano-infer-ws-server`。
