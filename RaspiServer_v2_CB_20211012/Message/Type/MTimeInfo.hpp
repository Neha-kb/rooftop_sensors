#ifndef MESSAGE_TYPE_MTIMEINFO_HPP_
#define MESSAGE_TYPE_MTIMEINFO_HPP_

#include "Message.hpp"
#include <chrono>

class MTimeInfo : public Message
{
public:
	MTimeInfo(std::string data);
    MTimeInfo(std::string sensorid, time_t time, std::string secret);
	virtual ~MTimeInfo();
	std::string toString();
	std::string toMessage();
    std::string getSensorID();
	time_t getTime();

protected:
    std::string sensorid;
	time_t tp;
};

#endif
