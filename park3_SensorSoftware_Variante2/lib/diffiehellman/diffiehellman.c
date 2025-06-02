#include "diffiehellman.h"
#include "mini-gmp.h"
#include "sha3.h"
#include <stdlib.h>
#include <string.h>

//generator and prime are taken fro IETF RFC3526
static const char *generator = "2";
static const char *prime     = "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF";
//Group 18 8192 bit for secure 256 bit key exchange
//static const char *prime     =   "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA18217C32905E462E36CE3BE39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF6955817183995497CEA956AE515D2261898FA051015728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6BF12FFA06D98A0864D87602733EC86A64521F2B18177B200CBBE117577A615D6C770988C0BAD946E208E24FA074E5AB3143DB5BFCE0FD108E4B82D120A92108011A723C12A787E6D788719A10BDBA5B2699C327186AF4E23C1A946834B6150BDA2583E9CA2AD44CE8DBBBC2DB04DE8EF92E8EFC141FBECAA6287C59474E6BC05D99B2964FA090C3A2233BA186515BE7ED1F612970CEE2D7AFB81BDD762170481CD0069127D5B05AA993B4EA988D8FDDC186FFB7DC90A6C08F4DF435C93402849236C3FAB4D27C7026C1D4DCB2602646DEC9751E763DBA37BDF8FF9406AD9E530EE5DB382F413001AEB06A53ED9027D831179727B0865A8918DA3EDBEBCF9B14ED44CE6CBACED4BB1BDB7F1447E6CC254B332051512BD7AF426FB8F401378CD2BF5983CA01C64B92ECF032EA15D1721D03F482D7CE6E74FEF6D55E702F46980C82B5A84031900B1C9E59E7C97FBEC7E8F323A97A7E36CC88BE0F1D45B7FF585AC54BD407B22B4154AACC8F6D7EBF48E1D814CC5ED20F8037E0A79715EEF29BE32806A1D58BB7C5DA76F550AA3D8A1FBFF0EB19CCB1A313D55CDA56C9EC2EF29632387FE8D76E3C0468043E8F663F4860EE12BF2D5B0B7474D6E694F91E6DBE115974A3926F12FEE5E438777CB6A932DF8CD8BEC4D073B931BA3BC832B68D9DD300741FA7BF8AFC47ED2576F6936BA424663AAB639C5AE4F5683423B4742BF1C978238F16CBE39D652DE3FDB8BEFC848AD922222E04A4037C0713EB57A81A23F0C73473FC646CEA306B4BCBC8862F8385DDFA9D4B7FA2C087E879683303ED5BDD3A062B3CF5B3A278A66D2A13F83F44F82DDF310EE074AB6A364597E899A0255DC164F31CC50846851DF9AB48195DED7EA1B1D510BD7EE74D73FAF36BC31ECFA268359046F4EB879F924009438B481C6CD7889A002ED5EE382BC9190DA6FC026E479558E4475677E9AA9E3050E2765694DFC81F56E880B96E7160C980DD98EDD3DFFFFFFFFFFFFFFFFF";
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
