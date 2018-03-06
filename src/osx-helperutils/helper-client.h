#import <MPMessagePack/MPXPCClient.h>

class HelperClient
{
public:
    HelperClient();
    void connect();
    void getVersion();

private:
    MPXPCClient *xpc_client_;
};
