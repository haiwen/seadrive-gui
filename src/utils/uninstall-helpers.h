#ifndef UNINSTALL_TOOLS_H
#define UNINSTALL_TOOLS_H

#include <QString>


int get_seadrive_dir(QString *ret);
int get_seadrive_data_dir(const QString& seadrive_dir, QString *ret);
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
