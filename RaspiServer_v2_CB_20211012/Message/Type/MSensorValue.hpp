#ifndef MSensorValue_hpp
#define MSensorValue_hpp

#include "Message.hpp"
#include <string>
#include <chrono>

class MSensorValue : public Message
{
public:
    MSensorValue(std::string data);
    virtual ~MSensorValue();
    std::string toString();
	std::string toMessage();
    std::string getSensorID();
    std::string getSensorType();
    std::string getSensorDescription();
    std::string getUnit();
    time_t getTime();
    float getValue();
    float getAccuracy();

protected:
    std::string sensorid, sensortype, sensordescription, unit;
    time_t time;
    float value, accuracy;
};

#endif
