#import <MPMessagePack/MPXPCClient.h>

class HelperClient
{
public:
    HelperClient();
    void getVersion();
    bool installKext(bool *finished, bool *ok);

private:
    void ensureConnected();
    void connect();

    MPXPCClient *xpc_client_;
};
