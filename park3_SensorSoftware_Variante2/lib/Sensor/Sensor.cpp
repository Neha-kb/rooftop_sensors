#include "Sensor.hpp"
#include <Arduino.h>
#include <Preferences.h>
#include <vector>

#include <iostream>
#include <sstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "Connect.hpp"

using namespace std;


extern "C"
{
  #include "message.h"
  #include "diffiehellman.h"
  #include <string.h>
  #include <time.h>
}

Sensor::Sensor(String sensorid, unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE], String sensortype, String sensordescription, String unit, float accuracy, float (*measure)(void)){

    //Setup Buffers
    values.CreateRingbuffer();
    Ringbuffer_Semaphor = xSemaphoreCreateMutex();    
    xSemaphoreGive( Ringbuffer_Semaphor );
    Preferences_Semaphor = xSemaphoreCreateMutex();    
    xSemaphoreGive( Preferences_Semaphor );
    this->sensorid = sensorid;
    this->sharedsecret = sharedsecret;
    this->sensortype = sensortype;
    this->sensordescription = sensordescription;
    this->unit = unit;
    this->accuracy = accuracy;
    this->measurement = measure;
}
Sensor::~Sensor(){}

void Sensor::measure(){
    float a = measurement();
    //Wait until Ringbuffer is free to use
    while (xSemaphoreTake( Ringbuffer_Semaphor, ( TickType_t ) 5 ) == pdFALSE){
    vTaskDelay(2);
    }

    values.AddValue(a);
    //Release Ringbuffer
    xSemaphoreGive( Ringbuffer_Semaphor );
}


void Sensor::createSensorvalue(time_t t){
    String key = "";
    key = "d" + sensortype;

    
    while (xSemaphoreTake(Preferences_Semaphor, ( TickType_t ) 5 ) == pdFALSE){vTaskDelay(2);}
    pref.begin(key.c_str(), false);

    //load values
    key = "n" + sensortype;
    unsigned char numvalues = pref.getUChar(key.c_str(), 0);
    floatbuffer.resize(numvalues);
    timebuffer.resize(numvalues);
    if(numvalues){
        key = "f" + sensortype;
        pref.getBytes(key.c_str(), &floatbuffer[0], numvalues * sizeof(float));
        key = "t" + sensortype;
        pref.getBytes(key.c_str(), &timebuffer[0], numvalues * sizeof(time_t));
    }

    //Wait until Ringbuffer is free to use
    while (xSemaphoreTake(Ringbuffer_Semaphor, ( TickType_t ) 5 ) == pdFALSE){vTaskDelay(2);}
    //Compute the mean Value of all new measurements
    float meanValue = values.GetMeanValue();        
    //Release Ringbuffer
    xSemaphoreGive( Ringbuffer_Semaphor );

    //save value at last position
    if(numvalues < MAXBUFFERELEMENTS){
        floatbuffer.push_back(meanValue);
        timebuffer.push_back(t);
    }
    //save all remaining values
    key = "n" + sensortype;
    pref.putUChar(key.c_str(), floatbuffer.size());
    key = "f" + sensortype;
    pref.putBytes(key.c_str(), &floatbuffer[0], floatbuffer.size() * sizeof(float));
    key = "t" + sensortype;
    pref.putBytes(key.c_str(), &timebuffer[0], timebuffer.size() * sizeof(time_t));
    
    pref.end();
    xSemaphoreGive( Preferences_Semaphor );
}

bool Sensor::sendSensorvalue(bool wakeUp){
    bool ret = false;
    String key = "";
    key = "d" + sensortype;

    
    while (xSemaphoreTake(Preferences_Semaphor, ( TickType_t ) 5 ) == pdFALSE){vTaskDelay(2);}
    pref.begin(key.c_str(), false);

    //load values
    key = "n" + sensortype;
    unsigned char numvalues = pref.getUChar(key.c_str(), 0);

    //Serial.println(numvalues);

    floatbuffer.resize(numvalues);
    timebuffer.resize(numvalues);
    if(numvalues){
        key = "f" + sensortype;
        pref.getBytes(key.c_str(), &floatbuffer[0], numvalues * sizeof(float));
        key = "t" + sensortype;
        pref.getBytes(key.c_str(), &timebuffer[0], numvalues * sizeof(time_t));

        

        if(floatbuffer.size() <= maxqueue){
            
            //try to send values
            if(sendSensorValue(sensorid, sharedsecret, sensortype, sensordescription, unit, accuracy, *floatbuffer.begin(), *timebuffer.begin(), wakeUp)){
                ret = true;                 //tell MainTask about succesfull delivery
                floatbuffer.erase(floatbuffer.begin());
                timebuffer.erase(timebuffer.begin());
                if(!floatbuffer.empty()){                   //try to slowly deplete Buffer by sending 2 packages per Interval, if needed
                    if(sendSensorValue(sensorid, sharedsecret, sensortype, sensordescription, unit, accuracy, *floatbuffer.begin(), *timebuffer.begin())){
                        floatbuffer.erase(floatbuffer.begin());
                        timebuffer.erase(timebuffer.begin());
                    }
                }
            }
        }
        else{
            // Drop first value in queue
            floatbuffer.erase(floatbuffer.begin());
            timebuffer.erase(timebuffer.begin());

            // Send next value in queue
            sendSensorValue(sensorid, sharedsecret, sensortype, sensordescription, unit, accuracy, *floatbuffer.begin(), *timebuffer.begin());
            floatbuffer.erase(floatbuffer.begin());
            timebuffer.erase(timebuffer.begin());
        }
    }
    else{
        ret = true;
    }
    
    
    //save all remaining values
    key = "n" + sensortype;
    pref.putUChar(key.c_str(), floatbuffer.size());
    if(!floatbuffer.empty()){
        key = "f" + sensortype;
        pref.putBytes(key.c_str(), &floatbuffer[0], floatbuffer.size() * sizeof(float));
        key = "t" + sensortype;
        pref.putBytes(key.c_str(), &timebuffer[0], timebuffer.size() * sizeof(time_t));
    }
    Serial.print("Number of Values in queue: "); Serial.print(floatbuffer.size()); Serial.print(", for Sensor: "); Serial.println(sensortype);
    
    Serial.println(floatbuffer.size());

    pref.end();
    xSemaphoreGive( Preferences_Semaphor );
    return ret;
}

void Sensor::setSharedSecret(unsigned char* sharedsecret){this->sharedsecret = sharedsecret;}
/*
void Sensor::sendSensorValues0( std::vector<float> &floatbuffer, std::vector<time_t> &timebuffer, String SERVERIP, const int SERVERPORT)
{   
    WiFiUDP Udp;
    //sendtries: how many acktimeout does this function take (maximum)?
    int sendtries = 3;//floatbuffer.size();
    int acktimeout  = 1000;

    while(!floatbuffer.empty() && sendtries > 0)
    {
        sendtries--;

        //calculate timestamp
        struct tm timestamp = *gmtime(&timebuffer[0]);

        //convert timestamp to String
        char timestring[40] = {};
        strftime(timestring, 40, "%Y-%m-%dT%H:%M:%SZ", &timestamp);

        //assemble message
        struct MESSAGE_Sensorvalue senval = {0};
        senval.sensorid = (char*)sensorid.c_str();
        senval.sensortype = (char*)sensortype.c_str(); //"ambient_temperature";
        senval.sensordescription = (char*)sensordescription.c_str();//"LUFFT_WS510";
        senval.time = timestring;
        senval.unit = (char*)unit.c_str();//"[°C]";
        senval.value = floatbuffer[0];
        senval.accuracy = accuracy;



        //create the message
        MESSAGE_Data sharedsecretdata;
        sharedsecretdata.data = sharedsecret;
        sharedsecretdata.length = DIFFIEHELLMAN_SHAREDSECRETSIZE;
        MESSAGE_Data *msenval = MESSAGE_makeMessageFromSensorValue(&senval, &sharedsecretdata);



        //calculate the message hash
        unsigned char hash[MESSAGE_HASHSIZE];
        MESSAGE_calculateMessageHash(msenval, hash);


        //send the message
        Udp.beginPacket(SERVERIP.c_str(), SERVERPORT);
        Udp.write(msenval->data, msenval->length);
        Udp.endPacket();


        //clean up
        MESSAGE_deleteMessageData(msenval);


        //wait for the right response message
        bool hasresponse = false;
        unsigned long waitstart = millis();
        while(!hasresponse && millis() - waitstart <= acktimeout)
        {
            int packetsize = Udp.parsePacket();
            if(packetsize > 0)
            {
                char buffer[1501] = {};
                Udp.read(buffer, 1500);
                struct MESSAGE_Data bufferdata;
                bufferdata.data = (unsigned char*)buffer;
                bufferdata.length = packetsize;
                if(MESSAGE_getMessageType(&bufferdata) == MESSAGE_ACKNOWLEDGED && MESSAGE_isValid(&bufferdata, &sharedsecretdata))
                {
                    struct MESSAGE_Acknowledged *ack = MESSAGE_createAcknowledged(&bufferdata);
                    if(0 == memcmp(ack->messagehash, hash, MESSAGE_HASHSIZE))
                    {
                        hasresponse = true;
                        floatbuffer.erase(floatbuffer.begin());
                        timebuffer.erase(timebuffer.begin());
                    }
                    MESSAGE_deleteAcknowledged(ack);
                }
            }
        }
    }
}*/