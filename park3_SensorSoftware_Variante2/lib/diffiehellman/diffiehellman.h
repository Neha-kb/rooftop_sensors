#ifndef diffiehellman_h
#define diffiehellman_h

#include <stdbool.h>

#define DIFFIEHELLMAN_SHAREDSECRETSIZE 16

struct DIFFIEHELLMAN_Data
{
    unsigned char *data;
    unsigned int  length;
};

struct DIFFIEHELLMAN_Data* DIFFIEHELLMAN_generatePublicKey(struct DIFFIEHELLMAN_Data *secretprivatekey);
bool                       DIFFIEHELLMAN_generateSharedSecret(struct DIFFIEHELLMAN_Data *oppositepublickey, struct DIFFIEHELLMAN_Data *secretprivatekey, unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE]);
void                       DIFFIEHELLMAN_deleteKeyData(struct DIFFIEHELLMAN_Data *data);

#endif
