#ifndef SEAFILE_CLIENT_FILE_PROVIDER_H_
#define SEAFILE_CLIENT_FILE_PROVIDER_H_

void fileProviderAddDomain(const char *domain_id, const char *display_name, bool hidden);

void fileProviderRemoveDomain(const char *domain_id);

#endif // SEAFILE_CLIENT_FILE_PROVIDER_H_
