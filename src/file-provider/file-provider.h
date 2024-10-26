#ifndef SEAFILE_CLIENT_FILE_PROVIDER_H_
#define SEAFILE_CLIENT_FILE_PROVIDER_H_

#include <QMap>
#include <QUrl>

struct Domain {
    QString identifier;
    bool userEnabled;
};

void fileProviderAskUserToEnable();

bool fileProviderListDomains(QMap<QString, Domain> *domains);

bool fileProviderAddDomain(const QString domain_id, const QString display_name, bool hidden = false);

bool fileProviderRemoveDomain(const QString domain_id, const QString display_name = "");

void fileProviderReenumerate(const QString domain_id, const QString display_name);

void fileProviderDisconnect (const QString domain_id, const QString display_name);

void fileProviderConnect (const QString domain_id, const QString display_name);

bool fileProviderGetUserVisibleURL(const QString domain_id, const QString display_name, QUrl *url);

#endif // SEAFILE_CLIENT_FILE_PROVIDER_H_
