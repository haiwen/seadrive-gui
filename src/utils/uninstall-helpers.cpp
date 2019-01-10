#include "uninstall-helpers.h"
#include "rpc/rpc-server.h"


void do_stop_app()
{
    SeaDriveRpcServer::Client *client = SeaDriveRpcServer::getClient();
    if (!client->connect()) {
        printf ("failed to connect to applet rpc server\n");
        return;
    }
    if (client->sendExitCommand()) {
        printf ("exit command: success\n");
    } else {
        printf ("exit command: failed\n");
    }
}
