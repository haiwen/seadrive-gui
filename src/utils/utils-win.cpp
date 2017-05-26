#include <windows.h>

#include <QSet>

#include "utils/utils-win.h"


namespace utils {
namespace win {

namespace {
OSVERSIONINFOEX osver; // static variable, all zero
bool osver_failure = false;

// From http://stackoverflow.com/a/36909293/1467959 and http://yamatyuu.net/computer/program/vc2013/rtlgetversion/index.html
typedef void(WINAPI *RtlGetVersion_FUNC)(OSVERSIONINFOEXW *);
BOOL CustomGetVersion(OSVERSIONINFOEX *os)
{
    HMODULE hMod;
    RtlGetVersion_FUNC func;
#ifdef UNICODE
    OSVERSIONINFOEXW *osw = os;
#else
    OSVERSIONINFOEXW o;
    OSVERSIONINFOEXW *osw = &o;
#endif

    hMod = LoadLibrary(TEXT("ntdll.dll"));
    if (hMod) {
        func = (RtlGetVersion_FUNC)GetProcAddress(hMod, "RtlGetVersion");
        if (func == 0) {
            FreeLibrary(hMod);
            return FALSE;
        }
        ZeroMemory(osw, sizeof(*osw));
        osw->dwOSVersionInfoSize = sizeof(*osw);
        func(osw);
#ifndef UNICODE
        os->dwBuildNumber = osw->dwBuildNumber;
        os->dwMajorVersion = osw->dwMajorVersion;
        os->dwMinorVersion = osw->dwMinorVersion;
        os->dwPlatformId = osw->dwPlatformId;
        os->dwOSVersionInfoSize = sizeof(*os);
        DWORD sz = sizeof(os->szCSDVersion);
        WCHAR *src = osw->szCSDVersion;
        unsigned char *dtc = (unsigned char *)os->szCSDVersion;
        while (*src)
            *dtc++ = (unsigned char)*src++;
        *dtc = '\0';
#endif

    } else
        return FALSE;
    FreeLibrary(hMod);
    return TRUE;
}


inline bool isInitializedSystemVersion() { return osver.dwOSVersionInfoSize != 0; }
inline void initializeSystemVersion() {
    if (isInitializedSystemVersion()) {
        return;
    }
    osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    // according to the document,
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms724451%28v=vs.85%29.aspx
    // this API will be unavailable once windows 10 is out
    if (!CustomGetVersion(&osver)) {
        qWarning("failed to get OS vesion.");
        osver_failure = true;
    }
}

inline bool _isAtLeastSystemVersion(unsigned major, unsigned minor, unsigned patch)
{
    initializeSystemVersion();
    if (osver_failure) {
        return false;
    }
#define OSVER_TO_NUM(major, minor, patch) ((major << 20) + (minor << 10) + (patch))
#define OSVER_SYS(ver) OSVER_TO_NUM(ver.dwMajorVersion, ver.dwMinorVersion, ver.wServicePackMajor)
    if (OSVER_SYS(osver) < OSVER_TO_NUM(major, minor, patch)) {
        return false;
    }
#undef OSVER_SYS
#undef OSVER_TO_NUM
    return true;
}

// compile statically
template<unsigned major, unsigned minor, unsigned patch>
inline bool isAtLeastSystemVersion()
{
    return _isAtLeastSystemVersion(major, minor, patch);
}
} // anonymous namesapce

void getSystemVersion(unsigned *major, unsigned *minor, unsigned *patch)
{
    initializeSystemVersion();
    // default to XP
    if (osver_failure) {
        *major = 5;
        *minor = 1;
        *patch = 0;
    }
    *major = osver.dwMajorVersion;
    *minor = osver.dwMinorVersion;
    *patch = osver.wServicePackMajor;
}

bool isAtLeastSystemVersion(unsigned major, unsigned minor, unsigned patch)
{
    return _isAtLeastSystemVersion(major, minor, patch);
}

bool isWindowsVistaOrHigher()
{
    return isAtLeastSystemVersion<6, 0, 0>();
}

bool isWindows7OrHigher()
{
    return isAtLeastSystemVersion<6, 1, 0>();
}

bool isWindows8OrHigher()
{
    return isAtLeastSystemVersion<6, 2, 0>();
}

bool isWindows8Point1OrHigher()
{
    return isAtLeastSystemVersion<6, 3, 0>();
}

bool isWindows10OrHigher()
{
    return isAtLeastSystemVersion<10, 0, 0>();
}

QSet<QString> getUsedLetters()
{
    wchar_t drives[1024];
    wchar_t *p;
    QSet<QString> used;

    GetLogicalDriveStringsW (sizeof(drives), drives);
    for (p = drives; *p != L'\0'; p += wcslen(p) + 1) {
        QString d = QString::fromWCharArray(p);
        while (d.endsWith("\\") or d.endsWith(":")) {
            d.truncate(d.length() - 1);
        }
        used.insert(d);
    }

    return used;
}

bool diskLetterAvailable(const QString& disk_letter)
{
    return !getUsedLetters().contains(QString(disk_letter.at(0)));
}

QStringList getAvailableDiskLetters()
{
    QStringList letters;
    QSet<QString> used = getUsedLetters();

    // All possible disk letters with the most common ones A,B,C removed.
    QString all_letters = "DEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i = 0; i < all_letters.length(); i++) {
        QString letter(all_letters.at(i));
        if (!used.contains(letter)) {
            letters << letter;
        }
    }
    return letters;

}

std::string getLocalPipeName(const char *pipe_name)
{
    DWORD buf_char_count = 32767;
    char user_name_buf[buf_char_count];

    if (GetUserName(user_name_buf, &buf_char_count) == 0) {
        qWarning ("Failed to get user name, GLE=%lu\n",
                  GetLastError());
        return pipe_name;
    }
    else {
        std::string ret(pipe_name);
        ret += user_name_buf;
        return ret;
    }
}

} // namespace win
} // namespace utils
