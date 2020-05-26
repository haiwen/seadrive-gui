#ifndef SEADRIVE_CLIENT_CRASH_HANDLER
#define SEADRIVE_CLIENT_CRASH_HANDLER
#ifdef SEADRIVE_CLIENT_HAS_CRASH_REPORTER
#include <QString>

namespace Breakpad {

class CrashHandlerPrivate;
class CrashHandler
{
public:
    static CrashHandler* instance();
    void Init(const QString& reportPath);
private:
    Q_DISABLE_COPY(CrashHandler)

    CrashHandler();
    ~CrashHandler();
    CrashHandlerPrivate* d;
};
}
#endif // SEADRIVE_CLIENT_HAS_CRASH_REPORTER
#endif // SEADRIVE_CLIENT_CRASH_HANDLER

