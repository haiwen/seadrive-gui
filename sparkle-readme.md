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
* WinSparkle reads information about available versions from
an appcast -- a RSS 2.0 feed with some extensions.
* WinSparkle uses Appcasts Feeds to get information about
available updates.
* Appcasts are accessed over HTTPS or HTTP, so it's enough to
upload its XML file to your web server.

The following shows a sample appcast.xml:
--------------- appcast.xml begin ---------------
<?xml version="1.0" encoding="utf-8"?>
<rss version="2.0" xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
<channel>
    <title>Seadrive updates</title>
    <description>Appcast for Test Seadrive updates.</description>
    <language>en</language>
    <item>
      <title>Version 0.4.1</title>
      <sparkle:releaseNotesLink>
          http://localhost/SeaDriveClientChangeLog.html
      </sparkle:releaseNotesLink>
      <pubDate>Tue, 16 Sep 2016 18:11:12 +0200</pubDate>
      <enclosure url="http://localhost/qt.exe"
                 sparkle:version="0.4.1"
                 length="0"
                 type="application/octet-stream"/>
    </item>
  </channel>
</rss>
--------------- appcast.xml end ---------------
* Note that, the RSS Best Practice Profile recommends setting
length to 0 in case a publisher can't determine the enclosure's
size.
* 发布新的版本时，需要更新的包括：
title 字段；
sparkle:releaseNotesLink 字段所指的更新说明页面；
pubDate 字段；
enclosure 中新版本下载地址 url 字段；
enclosure 中新版本号 sparkle:version 字段；



Native C/C++ applications
Once you hace a working appcast feed, follow the steps below
to intergrate WinSparkle in your app.

1. Add Winsparkle library
* 把 include 下的文件拷贝到 /usr/local/lib
Include its header;
---- code begin ----
#include <winsparkle.h>
---- code end ----
* 把 winsparkle.lib 拷贝到 seadrive-gui 目录下
Create import library (WinSparkle.lib) for the WinSparkle.dll
and add it to linker's libraries list.
* 把 Release/winsparkle.dll 拷贝到 /mingw32/bin

2. Set up application metadata
WinSparkle uses application metadata for information about
app's current version, name etc. In particular, following
fields are of interest and should be set to correct values:
* ProductName
* ProductVersion
* CompanyName
You can provide all this information via explicit API calls.
WIN_SPARKLE_API void __cdecl win_sparkle_set_app_details(const wchar_t *company_name,
                                                         const wchar_t *app_name,
                                                         const wchar_t *app_version);

3. Initialize WinSparkle
Your app must call win_sparkle_init() somewhere to initialize
WinSparkle and perform the check. Before calling win_sparkle_init(),
you must set the appcast URL with win_sparkle_set_appcast_url().
Finally, you should shut WinSparkle down cleanly with win_sparkle_cleanup()
when the app exits.

Manually using WinSparkle
/**
    Checks if an update is available, showing progress UI to the user.

    Normally, WinSparkle checks for updates on startup and only shows its UI
    when it finds an update. If the application disables this behavior, it
    can hook this function to "Check for updates..." menu item.

    When called, background thread is started to check for updates. A small
    window is shown to let the user know the progress. If no update is found,
    the user is told so. If there is an update, the usual "update available"
    window is shown.

    This function returns immediately.

    @note Because this function is intended for manual, user-initiated checks
          for updates, it ignores "Skip this version" even if the user checked
          it previously.

    @see win_sparkle_check_update_without_ui()
 */
WIN_SPARKLE_API void __cdecl win_sparkle_check_update_with_ui();
