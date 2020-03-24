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


class AccountManager : public QObject {
    Q_OBJECT

public:
    AccountManager();
    ~AccountManager();

    int start();

    // Load the accounts from local db when client starts.
    const std::vector<Account>& loadAccounts();

    /**
     * Account operations
     */

    // Use the given account. This account would also be persisted to
    // the accounts db.
    void setCurrentAccount(const Account& account);

    // Remove the account. Used when user removes an account from the
    // account menu.
    int removeAccount(const Account& account);

    // Use the account if it's valid, otherwise require a re-login.
    void validateAndUseAccount(const Account& account);

    // Called when API returns 401 and we need to re-login current
    // account.
    void invalidateCurrentLogin();

    // Update AccountInfo (e.g. nick name, quota etc.) for the given
    // account.
    void updateAccountInfo(const Account& account, const AccountInfo& info);

    // Trigger server info refresh for all accounts when client
    // starts.
    void updateServerInfoForAllAccounts();

    /**
     * Accessors
     */
    const std::vector<Account>& accounts() const;
    const Account currentAccount() const;
    bool hasAccount() const;
    bool accountExists(const QUrl& url, const QString& username) const;

    Account getAccountByHostAndUsername(const QString& host,
                                        const QString& username) const;

    Account getAccountBySignature(const QString& account_sig) const;

    void clearAccountToken(const Account& account,
                           bool force_relogin=false);

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
    void serverInfoSuccess(const Account &account, const ServerInfo &info);
    void serverInfoFailed(const ApiError&);

    void onAccountsChanged();

private:
    Q_DISABLE_COPY(AccountManager)

    void updateAccountServerInfo(const Account& account);
    static bool loadAccountsCB(struct sqlite3_stmt *stmt, void *data);
    static bool loadServerInfoCB(struct sqlite3_stmt *stmt, void *data);

    void updateAccountLastVisited(const Account& account);

    QHash<QString, Account> accounts_cache_;

    struct sqlite3 *db;
    std::vector<Account> accounts_;

    QMutex accounts_mutex_;
    QMutex accounts_cache_mutex_;
};

#endif  // _SEAF_ACCOUNT_MGR_H
