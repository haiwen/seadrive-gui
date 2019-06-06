#ifndef SEADRIVE_QLGEN_PROTO_H
#define SEADRIVE_QLGEN_PROTO_H

const char *kSFQLGenMachPort = "com.seafile.seadrive.qlgen.machport";

#define ZERO_INIT(TYPE, VAR) \
    TYPE VAR;                \
    bzero(&VAR, sizeof(TYPE));

// TODO: For now we use fixed size body size for code simplicity. In
// the end we should use a variable-length array (VLA) to reduce the
// cost.
#define FIXED_BODY_SIZE 5120
typedef struct {
    mach_msg_header_t header;
    char body[FIXED_BODY_SIZE];
} SFQLGenRequest;

typedef struct {
    mach_msg_header_t header;
    char body[FIXED_BODY_SIZE];
} SFQLGenReply;

#endif // SEADRIVE_QLGEN_PROTO_H
