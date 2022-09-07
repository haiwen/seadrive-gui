#ifndef SEAFILE_CLIENT_FINDER_SYNC_LISTENER_H_
#define SEAFILE_CLIENT_FINDER_SYNC_LISTENER_H_

/// \brief start the listener
///
void finderSyncListenerStart();

/// \brief stop the listener
///
void finderSyncListenerStop();

void fileProviderAddDomain(const char *domain_id, const char *display_name, bool hidden);

void fileProviderRemoveDomain(const char *domain_id);

#endif // SEAFILE_CLIENT_FINDER_SYNC_LISTENER_H_
