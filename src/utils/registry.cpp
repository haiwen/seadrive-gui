#include <windows.h>
#include <shlwapi.h>
#include <vector>

#include "utils/stl.h"
#include "utils/utils.h"


#include "registry.h"

namespace {

#if defined(_MSC_VER)
const int kMAX_VALUE_NAME = 16383;
const int kMAX_KEY_LENGTH = 255;
const wchar_t kHKCU_NAMESPACE_PATH[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace";
#endif

LONG openKey(HKEY root, const QString& path, HKEY *p_key, REGSAM samDesired = KEY_ALL_ACCESS)
{
    LONG result;
    result = RegOpenKeyExW(root,
                           path.toStdWString().c_str(),
                           0L,
                           samDesired,
                           p_key);

    return result;
}

QString softwareSeafile()
{
    return QString("SOFTWARE\\%1").arg(getBrand());
}

} // namespace

RegElement::RegElement(const HKEY& root, const QString& path,
                       const QString& name, const QString& value, bool expand)
    : root_(root),
      path_(path),
      name_(name),
      string_value_(value),
      dword_value_(0),
      type_(expand ? REG_EXPAND_SZ : REG_SZ)
{
}

RegElement::RegElement(const HKEY& root, const QString& path,
                       const QString& name, DWORD value)
    : root_(root),
      path_(path),
      name_(name),
      string_value_(""),
      dword_value_(value),
      type_(REG_DWORD)
{
}

int RegElement::openParentKey(HKEY *pKey)
{
    DWORD disp;
    HRESULT result;

    result = RegCreateKeyExW (root_,
                              path_.toStdWString().c_str(),
                              0, NULL,
                              REG_OPTION_NON_VOLATILE,
                              KEY_WRITE | KEY_WOW64_64KEY,
                              NULL,
                              pKey,
                              &disp);

    if (result != ERROR_SUCCESS) {
        return -1;
    }

    return 0;
}

int RegElement::add()
{
    HKEY parent_key;
    DWORD value_len;
    LONG result;

    if (openParentKey(&parent_key) < 0) {
        return -1;
    }

    if (type_ == REG_SZ || type_ == REG_EXPAND_SZ) {
        // See http://msdn.microsoft.com/en-us/library/windows/desktop/ms724923(v=vs.85).aspx
        value_len = sizeof(wchar_t) * (string_value_.toStdWString().length() + 1);
        result = RegSetValueExW (parent_key,
                                 name_.toStdWString().c_str(),
                                 0, REG_SZ,
                                 (const BYTE *)(string_value_.toStdWString().c_str()),
                                 value_len);
    } else {
        value_len = sizeof(dword_value_);
        result = RegSetValueExW (parent_key,
                                 name_.toStdWString().c_str(),
                                 0, REG_DWORD,
                                 (const BYTE *)&dword_value_,
                                 value_len);
    }

    if (result != ERROR_SUCCESS) {
        return -1;
    }

    return 0;
}

int RegElement::removeRegKey(HKEY root, const QString& path, const QString& subkey)
{
    HKEY hKey;
    LONG result = RegOpenKeyExW(root,
                                path.toStdWString().c_str(),
                                0L,
                                KEY_ALL_ACCESS,
                                &hKey);

    if (result != ERROR_SUCCESS) {
        return -1;
    }

    result = SHDeleteKeyW(hKey, subkey.toStdWString().c_str());
    if (result != ERROR_SUCCESS) {
        return -1;
    }

    return 0;
}

#if defined(_MSC_VER)
bool RegElement::isSeadriveRegister(const QString &seadrive_key)
{

    HKEY hKey;
    DWORD type = REG_SZ;

    wchar_t seadrive_value_w[kMAX_VALUE_NAME];
    DWORD value_size = sizeof(seadrive_value_w);

    wchar_t seadrive_key_w[kMAX_KEY_LENGTH];

    swprintf(seadrive_key_w, kMAX_KEY_LENGTH, L"%s\\%s",
             kHKCU_NAMESPACE_PATH,
             seadrive_key.toStdWString().c_str());

    LONG result = openKey(HKEY_CURRENT_USER,
                          QString::fromWCharArray(seadrive_key_w),
                          &hKey,
                          KEY_READ);

    if (result != ERROR_SUCCESS) {
        qWarning("failed to open registry item %s, errorcode is %d",
                  QString::fromWCharArray(seadrive_key_w).toUtf8().data(),
                  GetLastError());
        return false;
    }


    result = RegQueryValueExW(hKey,
                             0,
                             0,
                             &type,
                             (LPBYTE)&seadrive_value_w,
                             &value_size );

    if (result != ERROR_SUCCESS) {
        qWarning("failed to read registry item %s, errorcode is %d",
                  seadrive_key.toUtf8().data(),
                  GetLastError());
        return false;
    }

    QString reg_value = QString::fromWCharArray(seadrive_value_w);

    if (reg_value.contains("seadrive")) {
        return true;
    }
    return false;
}

QStringList RegElement::collectRegisterKeys(HKEY root, const QString& path)
{
    HKEY parent_key;
    QStringList subkey_list;

    LONG result = openKey(root,
                          path,
                          &parent_key,
                          KEY_READ);

    if (result != ERROR_SUCCESS) {
        qWarning("open registry item:  %s, errorcode is: %d",
                  path.toUtf8().data(),
                  GetLastError());
        return subkey_list;
    }

    DWORD subkeys_count = 0;
    DWORD subkeys_max_len = 0;

    // Query registry subkey numbers.
    result = RegQueryInfoKeyW(parent_key,
                              NULL,
                              NULL,
                              NULL,
                              &subkeys_count,
                              &subkeys_max_len,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              NULL);

    if (result != ERROR_SUCCESS) {
        qWarning("failed to query registry info");
        return subkey_list;
    }

    DWORD subkey_len = 1024;
    wchar_t subkey_name[1024] = L"";
    for (DWORD subkey_index = 0; subkey_index < subkeys_count; ++subkey_index)
    {
        RegEnumKeyExW(parent_key,
                      subkey_index,
                      subkey_name,
                      &subkey_len,
                      NULL,
                      NULL,
                      NULL,
                      NULL);

        QString key_name = QString::fromStdWString(subkey_name);

        if(!isSeadriveRegister(key_name)) {
            memset(subkey_name, 0, sizeof(subkey_name));
            subkey_len = 1024;
            continue;
        }
        subkey_list.push_back(key_name);
        memset(subkey_name, 0, sizeof(subkey_name));
        subkey_len = 1024;
    }

    RegCloseKey(parent_key);

    return subkey_list;
}

void RegElement::removeIconRegItem()
{
    int result = 0;
    QStringList subkey_list = collectRegisterKeys(HKEY_CURRENT_USER,
                                                  QString::fromWCharArray(kHKCU_NAMESPACE_PATH));

    if (subkey_list.size() == 0) {
        return ;
    }

    Q_FOREACH(QString key, subkey_list) {
        result = removeRegKey(HKEY_CURRENT_USER,
                              QString::fromWCharArray(kHKCU_NAMESPACE_PATH),
                              key);

        if (result != 0) {
            qWarning("failed to remove the key: %s", key.toUtf8().data());
        }
    }
    return;
}
#endif

bool RegElement::exists()
{
    HKEY parent_key;
    LONG result = openKey(root_, path_, &parent_key, KEY_READ);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    result = RegQueryValueExW (parent_key,
                               name_.toStdWString().c_str(),
                               NULL,             /* reserved */
                               NULL,             /* output type */
                               NULL,             /* output data */
                               NULL);            /* output length */

    RegCloseKey(parent_key);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    return true;
}

void RegElement::read()
{
    string_value_.clear();
    dword_value_ = 0;
    HKEY parent_key;
    LONG result = openKey(root_, path_, &parent_key, KEY_READ);
    if (result != ERROR_SUCCESS) {
        return;
    }
    const std::wstring std_name = name_.toStdWString();

    DWORD len;
    // get value size
    result = RegQueryValueExW (parent_key,
                               std_name.c_str(),
                               NULL,             /* reserved */
                               &type_,           /* output type */
                               NULL,             /* output data */
                               &len);            /* output length */
    if (result != ERROR_SUCCESS) {
        RegCloseKey(parent_key);
        return;
    }
    // get value
#ifndef UTILS_CXX11_MODE
    std::vector<wchar_t> buf;
#else
    utils::WBufferArray buf;
#endif
    buf.resize(len);
    result = RegQueryValueExW (parent_key,
                               std_name.c_str(),
                               NULL,             /* reserved */
                               &type_,           /* output type */
                               (LPBYTE)&buf[0],  /* output data */
                               &len);            /* output length */
    buf.resize(len);
    if (result != ERROR_SUCCESS) {
        RegCloseKey(parent_key);
        return;
    }

    switch (type_) {
        case REG_EXPAND_SZ:
        case REG_SZ:
            {
                // expand environment strings
                wchar_t expanded_buf[MAX_PATH];
                DWORD size = ExpandEnvironmentStringsW(&buf[0], expanded_buf, MAX_PATH);
                if (size == 0 || size > MAX_PATH)
                    string_value_ = QString::fromWCharArray(&buf[0], buf.size());
                else
                    string_value_ = QString::fromWCharArray(expanded_buf, size);
            }
            break;
        case REG_NONE:
        case REG_BINARY:
            string_value_ = QString::fromWCharArray(&buf[0], buf.size() / 2);
            break;
        case REG_DWORD_BIG_ENDIAN:
        case REG_DWORD:
            if (buf.size() != sizeof(int))
                return;
            memcpy((char*)&dword_value_, buf.data(), sizeof(int));
            break;
        case REG_QWORD: {
            if (buf.size() != sizeof(int))
                return;
            qint64 value;
            memcpy((char*)&value, buf.data(), sizeof(int));
            dword_value_ = (int)value;
            break;
        }
        case REG_MULTI_SZ:
        default:
          break;
    }

    RegCloseKey(parent_key);

    // workaround with a bug
    string_value_ = QString::fromUtf8(string_value_.toUtf8());

    return;
}

void RegElement::remove()
{
    HKEY parent_key;
    LONG result = openKey(root_, path_, &parent_key, KEY_ALL_ACCESS);
    if (result != ERROR_SUCCESS) {
        return;
    }
    result = RegDeleteValueW (parent_key, name_.toStdWString().c_str());
    RegCloseKey(parent_key);
}

QVariant RegElement::value() const
{
    if (type_ == REG_SZ || type_ == REG_EXPAND_SZ
        || type_ == REG_NONE || type_ == REG_BINARY) {
        return string_value_;
    } else {
        return int(dword_value_);
    }
}

int RegElement::getIntValue(HKEY root,
                            const QString& path,
                            const QString& name,
                            bool *exists,
                            int default_val)
{
    RegElement reg(root, path, name, "");
    if (!reg.exists()) {
        if (exists) {
            *exists = false;
        }
        return default_val;
    }
    if (exists) {
        *exists = true;
    }
    reg.read();

    if (!reg.stringValue().isEmpty())
        return reg.stringValue().toInt();

    return reg.dwordValue();
}

int RegElement::getPreconfigureIntValue(const QString& name)
{
    bool exists;
    int ret = getIntValue(
        HKEY_CURRENT_USER, softwareSeafile(), name, &exists);
    if (exists) {
        return ret;
    }

    return RegElement::getIntValue(
        HKEY_LOCAL_MACHINE, softwareSeafile(), name);
}

QString RegElement::getStringValue(HKEY root,
                                   const QString& path,
                                   const QString& name,
                                   bool *exists,
                                   QString default_val)
{
    RegElement reg(root, path, name, "");
    if (!reg.exists()) {
        if (exists) {
            *exists = false;
        }
        return default_val;
    }
    if (exists) {
        *exists = true;
    }
    reg.read();
    return reg.stringValue();
}

QString RegElement::getPreconfigureStringValue(const QString& name)
{
    bool exists;
    QString ret = getStringValue(
        HKEY_CURRENT_USER, softwareSeafile(), name, &exists);
    if (exists) {
        return ret;
    }

    return RegElement::getStringValue(
        HKEY_LOCAL_MACHINE, softwareSeafile(), name);
}

QVariant RegElement::getPreconfigureValue(const QString& name)
{
    QVariant v = getValue(HKEY_CURRENT_USER, softwareSeafile(), name);
    return v.isNull() ? getValue(HKEY_LOCAL_MACHINE, softwareSeafile(), name) : v;
}

QVariant RegElement::getValue(HKEY root,
                              const QString& path,
                              const QString& name)
{
    RegElement reg(root, path, name, "");
    if (!reg.exists()) {
        return QVariant();
    }
    reg.read();

    return reg.value();
}

void RegElement::installCustomUrlHandler()
{
#if defined(Q_OS_WIN32)
    QList<RegElement> list;
    HKEY root = HKEY_CURRENT_USER;

    QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    QString cmd = QString("\"%1\" -f ").arg(exe) + " \"%1\"";

    QString classes_seafile = "Software\\Classes\\seafile";

    list.append(RegElement(root, classes_seafile,
                           "", "URL:seafile Protocol"));

    list.append(RegElement(root, classes_seafile,
                           "URL Protocol", ""));

    list.append(RegElement(root, classes_seafile + "\\shell",
                           "", ""));

    list.append(RegElement(root, classes_seafile + "\\shell\\open",
                           "", ""));

    list.append(RegElement(root, classes_seafile + "\\shell\\open\\command",
                           "", cmd));
    for (int i = 0; i < list.size(); i++) {
        RegElement& reg = list[i];
        reg.add();
    }
#endif
}