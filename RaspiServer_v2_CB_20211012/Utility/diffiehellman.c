#include "diffiehellman.h"
#include "mini-gmp.h"
#include "sha3.h"
#include <stdlib.h>
#include <string.h>

//generator and prime are taken fro IETF RFC3526
static const char *generator = "2";
static const char *prime     = "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF";

static unsigned int convertHexToNumber(char hex)
{
    unsigned int n = 0;
    if(hex == '1')
    {
        n = 1;
    }
    else if(hex == '2')
    {
        n = 2;
    }
    else if(hex == '3')
    {
        n = 3;
    }
    else if(hex == '4')
    {
        n = 4;
    }
    else if(hex == '5')
    {
        n = 5;
    }
    else if(hex == '6')
    {
        n = 6;
    }
    else if(hex == '7')
    {
        n = 7;
    }
    else if(hex == '8')
    {
        n = 8;
    }
    else if(hex == '9')
    {
        n = 9;
    }
    else if(hex == 'A' || hex == 'a')
    {
        n = 10;
    }
    else if(hex == 'B' || hex == 'b')
    {
        n = 11;
    }
    else if(hex == 'C' || hex == 'c')
    {
        n = 12;
    }
    else if(hex == 'D' || hex == 'd')
    {
        n = 13;
    }
    else if(hex == 'E' || hex == 'e')
    {
        n = 14;
    }
    else if(hex == 'F' || hex == 'f')
    {
        n = 15;
    }
    return n;
}
static struct DIFFIEHELLMAN_Data* convertHexToData(char *hexstring)
{
    if(strlen(hexstring) == 0)
    {
        return NULL;
    }
    struct DIFFIEHELLMAN_Data *data = malloc(sizeof(struct DIFFIEHELLMAN_Data));
    if(data == NULL)
    {
        return NULL;
    }
    data->data = malloc(strlen(hexstring) / 2 + strlen(hexstring) % 2);
    if(data->data == NULL)
    {
        free(data);
        return NULL;
    }
    data->length = (unsigned int)(strlen(hexstring) / 2 + strlen(hexstring) % 2);
    
    //convert hexstring to binary
    if(strlen(hexstring) % 2 != 0)
    {
        data->data[0] = convertHexToNumber(hexstring[0]);
        for(unsigned int i=1; i<data->length; i++)
        {
            data->data[i] = convertHexToNumber(hexstring[2*i-1]) * 16 + convertHexToNumber(hexstring[2*i]);
        }
    }
    else
    {
        for(unsigned int i=0; i<data->length; i++)
        {
            data->data[i] = convertHexToNumber(hexstring[2*i]) * 16 + convertHexToNumber(hexstring[2*i+1]);
        }
    }
    return data;
}
static char* convertDataToHex(struct DIFFIEHELLMAN_Data *data)
{
    char *hex = calloc(data->length * 2 + 1, sizeof(char));
    if(hex == NULL)
    {
        return NULL;
    }
    for(unsigned int i=0; i<data->length; i++)
    {
        unsigned int s1 = data->data[i] / 16;
        unsigned int s2 = data->data[i] % 16;
        hex[i*2]        = (s1<10)? s1+48 : s1+55;
        hex[i*2+1]      = (s2<10)? s2+48 : s2+55;
    }
    return hex;
}
struct DIFFIEHELLMAN_Data* DIFFIEHELLMAN_generatePublicKey(struct DIFFIEHELLMAN_Data *secretprivatekey)
{
    //convert binary data to hex
    char *hexnumber = convertDataToHex(secretprivatekey);
    if(hexnumber == NULL)
    {
        return NULL;
    }
    
    //calculate public key
    mpz_t a, g, p, res;
    mpz_init_set_str(a, hexnumber, 16);
    mpz_init_set_str(g, generator, 16);
    mpz_init_set_str(p, prime,     16);
    mpz_init(res);
    mpz_powm(res, g, a, p);
    char *ret = mpz_get_str(NULL, 16, res);
    mpz_clear(a);
    mpz_clear(g);
    mpz_clear(p);
    mpz_clear(res);
    free(hexnumber);
    if(ret == NULL)
    {
        return NULL;
    }
    struct DIFFIEHELLMAN_Data *data = convertHexToData(ret);
    free(ret);
    return data;
}
bool DIFFIEHELLMAN_generateSharedSecret(struct DIFFIEHELLMAN_Data *oppositepublickey, struct DIFFIEHELLMAN_Data *secretprivatekey, unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE])
{
    //convert binary data to hex
    char *hexopposite = convertDataToHex(oppositepublickey);
    char *hexprivate  = convertDataToHex(secretprivatekey);
    if(hexopposite == NULL || hexprivate == NULL)
    {
        free(hexopposite);
        free(hexprivate);
        return false;
    }
    mpz_t opk, a, p, res;
    mpz_init_set_str(opk, hexopposite, 16);
    mpz_init_set_str(a,   hexprivate,  16);
    mpz_init_set_str(p,   prime,       16);
    mpz_init(res);
    mpz_powm(res, opk, a, p);
    char *ret = mpz_get_str(NULL, 16, res);
    mpz_clear(opk);
    mpz_clear(a);
    mpz_clear(p);
    mpz_clear(res);
    free(hexopposite);
    free(hexprivate);
    if(ret == NULL)
    {
        return false;
    }
    FIPS202_SHAKE128((const unsigned char*)ret, (unsigned int)strlen(ret), sharedsecret, DIFFIEHELLMAN_SHAREDSECRETSIZE);
    free(ret);
    return true;
}
void DIFFIEHELLMAN_deleteKeyData(struct DIFFIEHELLMAN_Data *data)
{
    free(data->data);
    free(data);
}
