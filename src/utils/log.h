#ifndef SEAFILE_CLIENT_UTILS_LOG_H
#define SEAFILE_CLIENT_UTILS_LOG_H

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

G_BEGIN_DECLS

int applet_log_init (const char *seadrive_dir);

void applet_log_rotate (const char *seadrive_dir);

G_END_DECLS

#endif // SEAFILE_CLIENT_UTILS_LOG_H
