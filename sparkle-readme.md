## Setup WinSparkle environment

[WinSparkle](https://github.com/vslavik/winsparkle) 是 Mac 上的 Sparkle 框架在 windows 上的实现，用于软件自动更新.

* 下载 winsparkle 发布包  https://github.com/vslavik/winsparkle/releases/download/v0.5.3/WinSparkle-0.5.3.zip, 并解压
* 把 include 下的文件拷贝到 /usr/local/lib
* 把 Release/winsparkle.dll 拷贝到 /mingw32/bin
* 把 winsparkle.lib 拷贝到 seadrive-gui 目录下

在编译时需要加上 `BUILD_SPARKLE_SUPPORT` flag:
```sh
cmake -DBUILD_SPARKLE_SUPPORT=ON .
```


Appcast instructions:

Q. appcast是什么？
A. appcast -- a RSS 2.0 feed with some extensions.

Q. appcast在WinSparkle中的作用是什么？
A. WinSparkle reads information about available versions from an appcast.

Q. appcast怎么写，哪些字段需要注意？
A. 有一个示例文件appcast.example.xml可以参考。
   * sparkle:releaseNotesLink 中应给出版本发布说明页面的链接。
   * enclosure url 中应给出新版本的下载链接。
   * Note that, the RSS Best Practice Profile recommends setting
     length to 0 in case a publisher can't determine the enclosure's size.
    
Q. 准备好appcast后如何发布？
A. Appcasts are accessed over HTTPS or HTTP, so it's enough to
   upload its XML file to your web server.

Q. 发布新的版本时，需要更新appcast中的哪些字段？
A. * sparkle:releaseNotesLink 字段所指发布说明页面的发布说明内容；
   * pubDate 字段；
   * enclosure 中新版本的下载地址 url 字段；
   * enclosure 中新版本的版本号 sparkle:version 字段；
   * 另外 title, description 字段可根据需要更新；

Q. 本地开发时如何搭建一个简单的 server 来让 seadrive-gui 去获取 appcast.xml ?
A. 1. 准备好用于本次发布的新版安装包，以及相关的发布说明页面。
   2. 准备好用于本次发布的 appcast.xml 文件。 
   3. 在本地搭建一个简单的 web server.
   4. 将步骤1, 2中准备好的文件上传到步骤3中搭建的 web server 中。
   5. 在程序中调用 win_sparkle_set_appcast_url(const char *url) 设置访问 appcast.xml 的地址，
      其中参数 url 是步骤4中上传的 appcast.xml 的访问地址。
      根据需要调用相应的 WinSparkle api 设置相关参数，可参考 winsparkle.h 文件。
   6. 调用 win_sparkle_check_update_with_ui() 手动访问 appcast.xml 并下载跟新，
      或者调用 win_sparkle_init() 在设定的更新间隔后自动更新。


