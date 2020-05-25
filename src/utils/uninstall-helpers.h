#ifndef UNINSTALL_TOOLS_H
#define UNINSTALL_TOOLS_H

#include <QString>


int delete_dir_recursively(const QString& path_in);

/**
 * stop running seaDrive-gui by rpc
 */
void do_stop_app();

/**
 * Remove ccnet and seafile-data
 */
void do_remove_user_data();

#endif
