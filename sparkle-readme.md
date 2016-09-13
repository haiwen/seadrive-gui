## Setup WinSparkle environment

[WinSparkle](https://github.com/vslavik/winsparkle) 是 Mac 上的 Sparkle 框架在 windows 上的实现，用于软件自动更新.

* 下载 winsparkle 发布包  https://github.com/vslavik/winsparkle/releases/download/v0.5.2/WinSparkle-0.5.2.zip, 并解压
* 把 include 下的文件拷贝到 /usr/local/lib
* 把 Release/winsparkle.dll 拷贝到 /mingw32/bin
* 把 winsparkle.lib 拷贝到 seadrive-gui 目录下

在编译时需要加上 `BUILD_SPARKLE_SUPPORT` flag:
```sh
cmake -DBUILD_SPARKLE_SUPPORT=ON .
```
