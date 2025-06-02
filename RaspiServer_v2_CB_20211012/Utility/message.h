#ifndef message_h
#define message_h

#include <stdbool.h>
#include <time.h>

#define MESSAGE_HASHSIZE 16
#define MESSAGE_MACSIZE  12

enum MESSAGE_MESSAGETYPE
{
    MESSAGE_INVALID,
    MESSAGE_SENSORVALUE,
    MESSAGE_ACKNOWLEDGED,
    MESSAGE_TIMEINFO,
    MESSAGE_KEYEXCHANGE
};

//combine binary data with its length
struct MESSAGE_Data
{
    unsigned char *data;
    unsigned int  length;
};

//sensorvalue
struct MESSAGE_Sensorvalue
{
    char  *sensorid;
    char  *sensortype;
    char  *sensordescription;
    time_t time;
    char  *unit;
    float value;
    float accuracy;
};

//acknowledged
struct MESSAGE_Acknowledged
{
    char *sensorid;
    char messagehash[MESSAGE_HASHSIZE];
};

//timeinfo
struct MESSAGE_Timeinfo
{
    char      *sensorid;
    time_t    time;
};

//keyexchange
struct MESSAGE_Keyexchange
{
    char                *sensorid;
    struct MESSAGE_Data key;
    int                 step;
};

enum MESSAGE_MESSAGETYPE     MESSAGE_getMessageType(const struct MESSAGE_Data *data);
void                         MESSAGE_deleteMessageData(struct MESSAGE_Data *data);
bool                         MESSAGE_calculateMessageHash(const struct MESSAGE_Data *data, unsigned char hash[MESSAGE_HASHSIZE]);
bool                         MESSAGE_isValid(const struct MESSAGE_Data *data, const struct MESSAGE_Data *secret);
struct MESSAGE_Data*         MESSAGE_getMessageID(const struct MESSAGE_Data *data);

//functions for message "sensorvalue"
struct MESSAGE_Sensorvalue*  MESSAGE_createSensorValue(const struct MESSAGE_Data *data, const struct MESSAGE_Data *secret);
void                         MESSAGE_deleteSensorValue(struct MESSAGE_Sensorvalue *val);
struct MESSAGE_Data*         MESSAGE_makeMessageFromSensorValue(const struct MESSAGE_Sensorvalue *val, const struct MESSAGE_Data *secret);

//functions for message "acknowledged"
struct MESSAGE_Acknowledged* MESSAGE_createAcknowledged(const struct MESSAGE_Data *data);
void                         MESSAGE_insertExpectedHash(struct MESSAGE_Data *data, unsigned char hash[MESSAGE_HASHSIZE]);
void                         MESSAGE_deleteAcknowledged(struct MESSAGE_Acknowledged *val);
struct MESSAGE_Data*         MESSAGE_makeMessageFromAcknowledged(const struct MESSAGE_Acknowledged *val, const struct MESSAGE_Data *secret);

//functions for message "timeinfo"
struct MESSAGE_Timeinfo*     MESSAGE_createTimeInfo(const struct MESSAGE_Data *data);
void                         MESSAGE_deleteTimeInfo(struct MESSAGE_Timeinfo *val);
struct MESSAGE_Data*         MESSAGE_makeMessageFromTimeInfo(const struct MESSAGE_Timeinfo *val, const struct MESSAGE_Data *secret);

//functions for message "keyexchange"
struct MESSAGE_Keyexchange*  MESSAGE_createKeyExchange(const struct MESSAGE_Data *data);
void                         MESSAGE_deleteKeyExchange(struct MESSAGE_Keyexchange *val);
struct MESSAGE_Data*         MESSAGE_makeMessageFromKeyExchange(const struct MESSAGE_Keyexchange *val, const struct MESSAGE_Data *secret);

#endif
