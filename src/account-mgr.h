#ifndef _SEAF_ACCOUNT_MGR_H
#define _SEAF_ACCOUNT_MGR_H

#include <vector>

#include <QObject>
#include <QHash>
#include <QMutex>

#include "account.h"

struct sqlite3;
struct sqlite3_stmt;
class ApiError;
class SeafileRpcClient;


#if defined(_MSC_VER)
class SyncRootInfo {
public:

    SyncRootInfo(QString url, QString username, QString sync_root_path) {
        url_ = url;
        username_ = username;
        sync_root_path_ = sync_root_path;
    }

    QString getUrl() { return url_; }
    QString getUserName() { return username_; }
    QString syncRootName() { return sync_root_path_; }

private:
    QString url_;
    QString username_;
    QString sync_root_path_;

};
#endif

class AccountManager : public QObject {
    Q_OBJECT

public:
    AccountManager();
    ~AccountManager();

    int start();

#if defined(_MSC_VER)
    void loadSyncRootInfo();
#endif
    // Load the accounts from local db when client starts.
    const std::vector<Account>& loadAccounts();

    /**
     * Account operations
     */

    // Save the account state after being logged in.
    void enableAccount(const Account& account);
    // Save the account state after being logged out.
    void disableAccount(const Account& account);

    // Remove the account. Used when user removes an account from the
    // account menu.
    int removeAccount(const Account& account);

    // Use all valid accounts, or re-login if no valid account.
    void validateAndUseAccounts();

    // Update AccountInfo (e.g. nick name, quota etc.) for the given
    // account.
    void updateAccountInfo(const Account& account, const AccountInfo& info);

    /**
     * Accessors
     */
    const std::vector<Account>& accounts() const;
    const QVector<Account> activeAccounts() const;
# if defined(_MSC_VER)
    const std::vector<SyncRootInfo>& getSyncRootInfos() const;
#endif
    const Account currentAccount() const;
    bool hasAccount() const;
    bool accountExists(const QUrl& url, const QString& username) const;

    Account getAccountByHostAndUsername(const QString& host,
                                        const QString& username) const;

    Account getAccountBySignature(const QString& account_sig) const;

    void clearAccountToken(const Account& account,
                           bool force_relogin=false);
#if defined(_MSC_VER)
    const QString getSyncRootName() { return sync_root_name_; }
#endif

public slots:
    void reloginAccount(const Account &account);

signals:
    /**
     * Account added/removed/switched.
     */
    void accountsChanged();
    void accountRequireRelogin(const Account& account);

    void requireAddAccount();
    void accountInfoUpdated(const Account& account);

private slots:
    void slotUpdateAccountInfoSucess(const AccountInfo& info);
    void slotUpdateAccountInfoFailed();
    void serverInfoSuccess(const Account &account, const ServerInfo &info);
    void serverInfoFailed(const ApiError&);

private:
    Q_DISABLE_COPY(AccountManager)

    void fetchAccountInfoFromServer(const Account& account);
    void updateAccountServerInfo(const Account& account);
#if defined(_MSC_VER)
    static bool loadSyncRootInfoCB(struct sqlite3_stmt *stmt, void *data);
    const QString getOldSyncRootDir(const Account& account);
    const QString genSyncRootName(const Account& account);
#endif
    static bool loadAccountsCB(struct sqlite3_stmt *stmt, void *data);
    static bool loadServerInfoCB(struct sqlite3_stmt *stmt, void *data);

    void updateAccountLastVisited(const Account& account);
#if defined(_MSC_VER)
    void updateSyncRootInfo(SyncRootInfo& sync_root_info);
#endif

    struct sqlite3 *db;
    std::vector<Account> accounts_;

#if defined(_MSC_VER)
    // Store All sync root information
    std::vector<SyncRootInfo> sync_root_infos_;

    QString sync_root_name_;
#endif
};

#endif  // _SEAF_ACCOUNT_MGR_H
