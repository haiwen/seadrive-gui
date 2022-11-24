#ifndef SEAFILE_CLIENT_FILE_PROVIDER_H_
#define SEAFILE_CLIENT_FILE_PROVIDER_H_

#include <QMap>

struct Domain {
    QString identifier;
    bool userEnabled;
};

void fileProviderAskUserToEnable();

bool fileProviderListDomains(QMap<QString, Domain> *domains);

bool fileProviderAddDomain(const QString domain_id, const QString display_name, bool hidden = false);

bool fileProviderRemoveDomain(const QString domain_id, const QString display_name = "");

#endif // SEAFILE_CLIENT_FILE_PROVIDER_H_
