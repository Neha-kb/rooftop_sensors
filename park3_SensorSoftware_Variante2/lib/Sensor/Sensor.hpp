#ifndef Sensor_hpp
#define Sensor_hpp

#include <Ringbuffer.h>
#include <string>
#include <vector>
#include <Preferences.h>
#include "Connect.hpp"
#include <chrono>
#include <Wire.h>


extern "C"
{
  #include "message.h"
  #include "diffiehellman.h"
}

using namespace std;

#define MAXBUFFERELEMENTS 25

class Sensor{

  public:
    Sensor(String sensorid, unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE], String sensortype, String sensordescription, String unit, float accuracy, float (*measure)(void));
    virtual ~Sensor();
    //void sendSensorValues0(std::vector<float> &floatbuffer, std::vector<time_t> &timebuffer, String SERVERIP, const int SERVERPORT);
    void setSharedSecret(unsigned char* sharedsecret);
    void measure();
    void createSensorvalue(time_t t);
    bool sendSensorvalue(bool wakeUp);
  //private:
    String sensorid, sensortype, sensordescription, unit;
    float (*measurement)(void);
    Preferences pref;
    unsigned char *sharedsecret;
    Ringbuffer values;    
    SemaphoreHandle_t Ringbuffer_Semaphor;
    SemaphoreHandle_t Preferences_Semaphor;
    std::vector<float> floatbuffer;
    std::vector<time_t> timebuffer;
    float accuracy;
  private:
    const uint maxqueue = 20; //max queue length to wait until first value is dropped -> has to be lower than MAXBUFFERELEMENTS
    
};

#endif