#include "message.h"
#include "sha3.h"
#include "aes.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static bool calculateMAC(const struct MESSAGE_Data *data, const struct MESSAGE_Data *secret, unsigned char mac[MESSAGE_MACSIZE]){
    unsigned char *buffer = malloc(secret->length + data->length);
    if(buffer == NULL)
    {
        return false;
    }
    memcpy(buffer, secret->data, secret->length);
    memcpy(buffer + secret->length, data->data, data->length);
    FIPS202_SHAKE128(buffer, secret->length + data->length, mac, MESSAGE_MACSIZE);
    free(buffer);
    return true;
}
enum MESSAGE_MESSAGETYPE MESSAGE_getMessageType(const struct MESSAGE_Data *data){
    enum MESSAGE_MESSAGETYPE mt = MESSAGE_INVALID;
    if(data->length > MESSAGE_MACSIZE){
    	char lsensorid = data->data[1];
        switch(data->data[0]){
            case MESSAGE_SENSORVALUE:
                if(data->length >= MESSAGE_MACSIZE + 8 + lsensorid){
                    mt = MESSAGE_SENSORVALUE;
                }break;
            case MESSAGE_ACKNOWLEDGED:
                if(data->length >= MESSAGE_MACSIZE + 2 + lsensorid){
                    mt = MESSAGE_ACKNOWLEDGED;
                }break;
            case MESSAGE_TIMEINFO:
                if(data->length >= MESSAGE_MACSIZE + 3 + lsensorid){
                    mt = MESSAGE_TIMEINFO;
                }break;
            case MESSAGE_KEYEXCHANGE:
                if(data->length >= MESSAGE_MACSIZE + 3 + 1 + lsensorid){
                    mt = MESSAGE_KEYEXCHANGE;
                }break;
        }
    }
    return mt;
}
void MESSAGE_deleteMessageData(struct MESSAGE_Data *data){
    free(data->data);
    free(data);
}
bool MESSAGE_calculateMessageHash(const struct MESSAGE_Data *data, unsigned char hash[MESSAGE_HASHSIZE]){
    //all data except the mac at the end is being hashed
    if(data->length <= MESSAGE_MACSIZE)
    {
        return false;
    }
    FIPS202_SHAKE128(data->data, data->length - MESSAGE_MACSIZE, hash, MESSAGE_HASHSIZE);
    return true;
}
bool MESSAGE_isValid(const struct MESSAGE_Data *data, const struct MESSAGE_Data *secret){
    if(data->length <= MESSAGE_MACSIZE)
    {
        return false;
    }
    struct MESSAGE_Data d;
    d.data = data->data;
    d.length = data->length - MESSAGE_MACSIZE;
    unsigned char calmac[MESSAGE_MACSIZE];
    bool noerror = calculateMAC(&d, secret, calmac);
    return noerror && memcmp(data->data + (data->length - MESSAGE_MACSIZE), calmac, MESSAGE_MACSIZE) == 0;
}
struct MESSAGE_Data* MESSAGE_getMessageID(const struct MESSAGE_Data *data){
    struct MESSAGE_Data* ret = malloc(sizeof(struct MESSAGE_Data));
    int offset = 0;
    switch(MESSAGE_getMessageType(data)){
        case MESSAGE_SENSORVALUE:
        case MESSAGE_ACKNOWLEDGED:
            offset = 2;
            break;
        case MESSAGE_TIMEINFO:
        case MESSAGE_KEYEXCHANGE:
            offset = 3;
            break;
        case MESSAGE_INVALID:
            break;
    }
    ret->data = calloc(data->data[1] + 1, sizeof(char));
    if(ret->data != NULL) memcpy(ret->data, data->data + offset, data->data[1]);
    ret->length = data->data[1];
    return ret;
}

//functions for message "sensorvalue"
struct MESSAGE_Sensorvalue* MESSAGE_createSensorValue(const struct MESSAGE_Data *data, const struct MESSAGE_Data *secret){
    int encryptionLength = data->length - MESSAGE_MACSIZE - 2 - data->data[1];
    encryptionLength -= encryptionLength%16;
    uint8_t iv[16];
    memcpy(iv, data->data + 2 + data->data[1], 16);

    struct AES_ctx ctx;
	AES_init_ctx_iv(&ctx, secret->data, iv);
	AES_CBC_decrypt_buffer(&ctx, (uint8_t*)(data->data + 2 + data->data[1] + 16), encryptionLength-16);
    memcpy(iv, data->data + data->length - MESSAGE_MACSIZE - 16, 16);
    AES_ctx_set_iv(&ctx, iv);
	AES_CBC_decrypt_buffer(&ctx, (uint8_t*)(data->data + 2 + data->data[1]), 16);


    int offset = 1;
    //length fields
    unsigned char lsensorid          = data->data[offset++];    offset += lsensorid;
    unsigned char lsensortype        = data->data[offset++];
    unsigned char lsensordescription = data->data[offset++];
    unsigned char ltime              = data->data[offset++];
    unsigned char lunit              = data->data[offset++];
    unsigned char lvalue             = data->data[offset++];
    unsigned char laccuracy          = data->data[offset++];
    

    //allocate memory
    struct MESSAGE_Sensorvalue *v = malloc(sizeof(struct MESSAGE_Sensorvalue));
    if(v == NULL)
    {
        return NULL;
    }
    
    v->sensorid          = calloc(lsensorid          + 1, sizeof(char));
    v->sensortype        = calloc(lsensortype        + 1, sizeof(char));
    v->sensordescription = calloc(lsensordescription + 1, sizeof(char));
    //v->time              = calloc(ltime              + 1, sizeof(char));
    v->unit              = calloc(lunit              + 1, sizeof(char));
    if(v->sensorid == NULL || v->sensortype == NULL || v->sensordescription == NULL /*|| v->time == NULL*/ || v->unit == NULL)
    {
        free(v->sensorid);
        free(v->sensortype);
        free(v->sensordescription);
        //free(v->time);
        free(v->unit);
        free(v);
        return NULL;
    }
    
    //fill memory with data
    offset = 2;
    char value   [256] = {0};
    char accuracy[256] = {0};
    memcpy(v->sensorid,          data->data + offset, lsensorid);           offset += lsensorid + 6;
    memcpy(v->sensortype,        data->data + offset, lsensortype);         offset += lsensortype;
    memcpy(v->sensordescription, data->data + offset, lsensordescription);  offset += lsensordescription;
    memcpy(&(v->time),           data->data + offset, ltime);               offset += ltime;
    memcpy(v->unit,              data->data + offset, lunit);               offset += lunit;
    memcpy(value,                data->data + offset, lvalue);              offset += lvalue;           sscanf(value,    "%f", &v->value);
    if(laccuracy > 0) {
        memcpy(accuracy,             data->data + offset, laccuracy);       offset += laccuracy;        sscanf(accuracy, "%f", &v->accuracy);
    }
    else{
        v->accuracy = -666;
    }
    return v;
}
void MESSAGE_deleteSensorValue(struct MESSAGE_Sensorvalue *val){
    free(val->sensorid);
    free(val->sensortype);
    free(val->sensordescription);
    //free(val->time);
    free(val->unit);
    free(val);
}
struct MESSAGE_Data* MESSAGE_makeMessageFromSensorValue(const struct MESSAGE_Sensorvalue *val, const struct MESSAGE_Data *secret){
    //length fields for strings
    unsigned char lsensorid          = strlen(val->sensorid);
    unsigned char lsensortype        = strlen(val->sensortype);
    unsigned char lsensordescription = strlen(val->sensordescription);
    unsigned char ltime              = sizeof(time_t);
    unsigned char lunit              = strlen(val->unit);
    
    //value and accuracy, snprintf only writes n-1 bytes
    char value[256];
    unsigned char lvalue = snprintf(value, 256, "%f", val->value);
    char accuracy[256];
    unsigned char laccuracy;
    if(val->accuracy == -666){
        laccuracy = 0;
        accuracy[0] = '\0';           //set the terminating c String charakter
        accuracy[1] = '\0';
    }
    else{
        laccuracy = snprintf(accuracy, 256, "%f", val->accuracy);
    }
    
    //crate string
    struct MESSAGE_Data *data = malloc(sizeof(struct MESSAGE_Data));
    if(data == NULL)
    {
        return NULL;
    }

    uint8_t iv[16];
    for(int i=0; i<16; i++){
        iv[i] = (char) rand();
    }  

    int encryptionLength = lsensortype + lsensordescription + ltime + lunit + lvalue + laccuracy + 6;
    int offsetIV = encryptionLength%16;
    /*if(encryptionLength%16 != 0)*/ encryptionLength += 16 - encryptionLength%16;
    data->data = malloc(MESSAGE_MACSIZE + 2 + lsensorid + encryptionLength + offsetIV);

    if(data->data == NULL)
    {
        free(data);
        return NULL;
    }
    
	memset(data->data, 0, MESSAGE_MACSIZE + 2 + lsensorid + encryptionLength);

    
    int offset = 0;
    data->data[offset++] = MESSAGE_SENSORVALUE;
    data->data[offset++] = lsensorid;
    memcpy(data->data + offset, val->sensorid, lsensorid); offset += lsensorid;    
    data->data[offset++] = lsensortype;
    data->data[offset++] = lsensordescription;
    data->data[offset++] = ltime;
    data->data[offset++] = lunit;
    data->data[offset++] = lvalue;
    data->data[offset++] = laccuracy;
    memcpy(data->data + offset, val->sensortype, lsensortype);              offset += lsensortype;
    memcpy(data->data + offset, val->sensordescription, lsensordescription);offset += lsensordescription;
    memcpy(data->data + offset, &(val->time), ltime);                       offset += ltime;
    memcpy(data->data + offset, val->unit, lunit);                          offset += lunit;
    memcpy(data->data + offset, value, lvalue);                             offset += lvalue;
    memcpy(data->data + offset, accuracy, laccuracy);                       offset += laccuracy;
    memcpy(data->data + offset, iv, 16);                                    offset += 16;

    struct AES_ctx ctx;
	AES_init_ctx_iv(&ctx, secret->data, iv);
	AES_CBC_encrypt_buffer(&ctx, (uint8_t*)(data->data + 2 + lsensorid), encryptionLength);
	
    //offset = 2 + lsensorid + encryptionLength;
    //calculate mac
    data->length = offset;
    unsigned char mac[MESSAGE_MACSIZE];
    if(false == calculateMAC(data, secret, mac))
    {
        free(data->data);
        free(data);
        return NULL;
    }
    memcpy(data->data + offset, mac, MESSAGE_MACSIZE);                      offset += MESSAGE_MACSIZE;
    data->length = offset;
    return data;
}

//functions for message "acknowledged"
struct MESSAGE_Acknowledged* MESSAGE_createAcknowledged(const struct MESSAGE_Data *data){
    //length field
    unsigned char lsensorid          = data->data[1];
    
    //allocate memory
    struct MESSAGE_Acknowledged *v = malloc(sizeof(struct MESSAGE_Acknowledged));
    if(v == NULL)
    {
        return NULL;
    }
    v->sensorid = calloc(lsensorid + 1, sizeof(char));
    if(v->sensorid == NULL)
    {
        free(v->sensorid);
        free(v);
        return NULL;
    }
    
    //fill memory with data
    memcpy(v->sensorid,    data->data + 2, lsensorid);
    memcpy(v->messagehash, data->data + 2 + lsensorid, MESSAGE_HASHSIZE);
    return v;
}
void MESSAGE_insertExpectedHash(struct MESSAGE_Data *data, unsigned char hash[MESSAGE_HASHSIZE]){
    unsigned char lsensorid = data->data[1];
    unsigned char mac[MESSAGE_MACSIZE];
    data->data = realloc(data->data, data->length + MESSAGE_HASHSIZE);
    memcpy(mac, data->data + 2 + lsensorid, MESSAGE_MACSIZE);
    memcpy(data->data + 2 + lsensorid, hash, MESSAGE_HASHSIZE);
    memcpy(data->data + 2 + lsensorid + MESSAGE_HASHSIZE, mac, MESSAGE_MACSIZE);
    data->length = MESSAGE_HASHSIZE + MESSAGE_MACSIZE + 2 + lsensorid;
}
void MESSAGE_deleteAcknowledged(struct MESSAGE_Acknowledged *val){
    free(val->sensorid);
    free(val);
}
struct MESSAGE_Data* MESSAGE_makeMessageFromAcknowledged(const struct MESSAGE_Acknowledged *val, const struct MESSAGE_Data *secret){
    //length fields for strings
    unsigned char lsensorid          = strlen(val->sensorid);
    
    //crate string
    struct MESSAGE_Data *data = malloc(sizeof(struct MESSAGE_Data));
    if(data == NULL)
    {
        return NULL;
    }
    data->data = malloc(MESSAGE_HASHSIZE + MESSAGE_MACSIZE + 2 + lsensorid);
    if(data->data == NULL)
    {
        free(data);
        return NULL;
    }
    data->data[0] = MESSAGE_ACKNOWLEDGED;
    data->data[1] = lsensorid;
    memcpy(data->data + 2, val->sensorid, lsensorid);
    memcpy(data->data + 2 + lsensorid, val->messagehash, MESSAGE_HASHSIZE);
    
    //calculate mac
    data->length = 2 + lsensorid + MESSAGE_HASHSIZE;
    unsigned char mac[MESSAGE_MACSIZE];
    if(false == calculateMAC(data, secret, mac))
    {
        free(data->data);
        free(data);
        return NULL;
    }
    //hash does not need to be sent in order to work properly, MAC does the Trick
    memcpy(data->data + 2 + lsensorid, mac, MESSAGE_MACSIZE);
    data->length = MESSAGE_MACSIZE + 2 + lsensorid;
    //memcpy(data->data + 2 + lsensorid + MESSAGE_HASHSIZE, mac, MESSAGE_MACSIZE);
    //data->length = MESSAGE_HASHSIZE + MESSAGE_MACSIZE + 2 + lsensorid;
    return data;
}

//functions for message "timeinfo"
struct MESSAGE_Timeinfo* MESSAGE_createTimeInfo(const struct MESSAGE_Data *data)
{
    //check if length and data types are valid
    if(MESSAGE_TIMEINFO != MESSAGE_getMessageType(data))
    {
        return NULL;
    }
    
    //length fields
    unsigned char lsensorid = data->data[1];
    unsigned char ltime     = data->data[2];
    
    //allocate memory
	struct MESSAGE_Timeinfo *v = malloc(sizeof(struct MESSAGE_Timeinfo));
	if(v == NULL){
		return NULL;
	}
	v->sensorid = calloc(lsensorid + 1, sizeof(char));
	if(v->sensorid == NULL){
		free(v->sensorid);
		free(v);
		return NULL;
	}

    //fill memory with data
    memcpy(v->sensorid, data->data + 3, lsensorid);
    memcpy(&(v->time), data->data + 3 + lsensorid, ltime);
    /*
    //extract time
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    char time [256] = {0};
    memcpy(time, data->data + 3 + lsensorid, ltime);
    if(sscanf(time, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
    {
        return NULL;
    }
    
    //allocate memory
    struct MESSAGE_Timeinfo *v = malloc(sizeof(struct MESSAGE_Timeinfo));
    if(v == NULL)
    {
        return NULL;
    }
    v->sensorid = calloc(lsensorid + 1, sizeof(char));
    if(v->sensorid == NULL)
    {
        free(v->sensorid);
        free(v);
        return NULL;
    }
    
    //fill memory with data
    memcpy(v->sensorid, data->data + 3, lsensorid);
    v->time.tm_sec = second;
    v->time.tm_min = minute;
    v->time.tm_hour = hour;
    v->time.tm_mday = day;
    v->time.tm_mon = month - 1;
    v->time.tm_year = year - 1900;
    v->time.tm_isdst = -1;*/
    return v;
}
void MESSAGE_deleteTimeInfo(struct MESSAGE_Timeinfo *val)
{
    free(val->sensorid);
    free(val);
}
struct MESSAGE_Data* MESSAGE_makeMessageFromTimeInfo(const struct MESSAGE_Timeinfo *val, const struct MESSAGE_Data *secret)
{
    //length fields for strings
    unsigned char lsensorid = strlen(val->sensorid);
    unsigned char ltime = sizeof(time_t);
    /*char time[255];
    unsigned char ltime = strftime(time, 255, "%Y-%m-%dT%H:%M:%SZ", &val->time);*/
    
    //create string
    struct MESSAGE_Data *data = malloc(sizeof(struct MESSAGE_Data));
    if(data == NULL)
    {
        return NULL;
    }
    data->data = malloc(MESSAGE_MACSIZE + 3 + lsensorid + ltime);
    if(data->data == NULL)
    {
        free(data);
        return NULL;
    }
    data->data[0] = MESSAGE_TIMEINFO;
    data->data[1] = lsensorid;
    data->data[2] = ltime;
    memcpy(data->data + 3, val->sensorid, lsensorid);
    memcpy(data->data + 3 + lsensorid, &(val->time), ltime);
    
    //calculate mac
    data->length = 3 + lsensorid + ltime;
    unsigned char mac[MESSAGE_MACSIZE];
    if(false == calculateMAC(data, secret, mac))
    {
        free(data->data);
        free(data);
        return NULL;
    }
    memcpy(data->data + 3 + lsensorid + ltime, mac, MESSAGE_MACSIZE);
    data->length = MESSAGE_MACSIZE + 3 + lsensorid + ltime;
    return data;
}

//functions for message "keyexchange"
struct MESSAGE_Keyexchange* MESSAGE_createKeyExchange(const struct MESSAGE_Data *data)
{
    //check if length and data types are valid
    if(MESSAGE_KEYEXCHANGE != MESSAGE_getMessageType(data))
    {
        return NULL;
    }
    
    //length fields
    unsigned char lsensorid = data->data[1];
    unsigned char lkey      = data->data[2];
    
    //allocate memory
    struct MESSAGE_Keyexchange *v = malloc(sizeof(struct MESSAGE_Keyexchange));
    if(v == NULL)
    {
        return NULL;
    }
    v->sensorid = calloc(lsensorid + 1, sizeof(char));
    v->key.data = malloc(lkey);
    if(v->sensorid == NULL || (v->key.data == NULL && lkey != 0))
    {
        free(v->sensorid);
        free(v->key.data);
        free(v);
        return NULL;
    }
    
    //fill memory with data
    memcpy(v->sensorid, data->data + 3, lsensorid);
    memcpy(v->key.data, data->data + 3 + lsensorid, lkey);
    v->key.length = lkey;
    v->step = data->data[3 + lsensorid + lkey];
    return v;
}
void MESSAGE_deleteKeyExchange(struct MESSAGE_Keyexchange *val)
{
    free(val->sensorid);
    free(val->key.data);
    free(val);
}
struct MESSAGE_Data* MESSAGE_makeMessageFromKeyExchange(const struct MESSAGE_Keyexchange *val, const struct MESSAGE_Data *secret)
{
    //length fields for strings
    unsigned char lsensorid  = strlen(val->sensorid);
    unsigned char lkeylength = val->key.length;
    
    //crate string
    struct MESSAGE_Data *data = malloc(sizeof(struct MESSAGE_Data));
    if(data == NULL)
    {
        return NULL;
    }
    data->data = malloc(MESSAGE_MACSIZE + 3 + 1 + lsensorid + lkeylength);
    if(data->data == NULL)
    {
        free(data);
        return NULL;
    }
    data->data[0] = MESSAGE_KEYEXCHANGE;
    data->data[1] = lsensorid;
    data->data[2] = lkeylength;
    memcpy(data->data + 3, val->sensorid, lsensorid);
    memcpy(data->data + 3 + lsensorid, val->key.data, lkeylength);
    data->data[3 + lsensorid + lkeylength] = val->step;
    
    //calculate mac
    data->length = 3 + 1 + lsensorid + lkeylength;
    unsigned char mac[MESSAGE_MACSIZE];
    if(false == calculateMAC(data, secret, mac))
    {
        free(data->data);
        free(data);
        return NULL;
    }
    memcpy(data->data + 3 + 1 + lsensorid + lkeylength, mac, MESSAGE_MACSIZE);
    data->length = MESSAGE_MACSIZE + 3 + 1 + lsensorid + lkeylength;
    return data;
}
