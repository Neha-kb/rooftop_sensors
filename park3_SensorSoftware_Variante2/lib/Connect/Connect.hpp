#ifndef Connect_hpp
#define Connect_hpp

#include "WiFiManager.hpp"
#include "LoRaManager.hpp"
#include <map>
#include <chrono>


extern "C"
{
  #include "message.h"
  #include "diffiehellman.h"
}

const unsigned long timeout = 6000; // muss 6000ms sein wegen key exchange message
const unsigned int maxtries = 1;              //LoRa does not like sending things twice
const unsigned int privatekeysize = 128;
const String psk("abc");


//send/receive on primary carrier (WiFi LoRa BLE)

enum MESSAGE_MESSAGETYPE receiveMessage(MESSAGE_Data* md, String sensorid);
enum MESSAGE_MESSAGETYPE receiveMessageLookUp(MESSAGE_Data* md);
void sendData(MESSAGE_Data* m);

void initConnection();
//Gateway Mode for other LoRa Sensors
void gatewayIdle();

bool authenticateSensor(String sensorid, unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE], time_t *time);
bool sendSensorValue(String sensorid, unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE], String sensortyp, String sensordescription, String unit, float accuracy, float floatbuffer, time_t timebuffer, bool fullData = false);
bool getServerTime(String sensorid, unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE], time_t *time);

#endif