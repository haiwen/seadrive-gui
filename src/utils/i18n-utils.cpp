#include <QString>
#include <QObject>

#include "utils.h"
#include "i18n-utils.h"

namespace i18n {

QString getDiskLetterDialogTitle()
{
    return QObject::tr("Choose the disk letter for %1").arg(getBrand());
}

}
