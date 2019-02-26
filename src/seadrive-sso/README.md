# seadrive windows client Kerberos sso

## 原理

涉及的实体:

- seadrive 客户端
- seahub (云盘服务) / apache

免登录的原理和浏览器端一样， Windows 的 WinInet http client library 在发送请求时可以自动处理 NTLM/Kerberos 认证。云盘服务布署在 apache 后面，apache 的 kerberos 模块认证完成之后将用户信息传递给云盘。这样不需要用户在客户端输入用户名密码就能自动登录。

## 参考资料

- 发送请求依次需要调用: [InternetOpen](https://msdn.microsoft.com/en-us/library/windows/desktop/aa385096(v=vs.85).aspx) [InternetConnect](https://msdn.microsoft.com/en-us/library/windows/desktop/aa384363(v=vs.85).aspx) [HttpOpenRequest](https://msdn.microsoft.com/en-us/library/windows/desktop/aa384233(v=vs.85).aspx)
- 读取响应头: [HttpQueryInfo](https://msdn.microsoft.com/en-us/library/windows/desktop/aa384238(v=vs.85).aspx)
- 读取响应内容: [InternetQueryDataAvailable](https://msdn.microsoft.com/en-us/library/windows/desktop/aa385100(v=vs.85).aspx)
- 获取 Cookie: [InternetGetCookieEx](https://msdn.microsoft.com/en-us/library/windows/desktop/aa384714(v=vs.85).aspx)

## 其他

IE8 以上版本有两种模式： 普通模式和安全模式，两种模式各自有独立的 cookie. 一般程序调用 WinInet 库所使用的是普通模式下的 cookie.
