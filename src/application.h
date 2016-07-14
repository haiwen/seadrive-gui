/**
 * Show thr main window when the dock icon is clicked
 */
#include <QApplication>

class Application : public QApplication {
    Q_OBJECT

public:

    Application (int& argc, char **argv);
    bool event(QEvent * e);
    virtual ~Application() {};
};
