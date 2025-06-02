#ifndef MESSAGE_TYPE_MKEYEXCHANGE_HPP_
#define MESSAGE_TYPE_MKEYEXCHANGE_HPP_

#include "Message.hpp"

class MKeyExchange : public Message
{
public:
	MKeyExchange(std::string data);
	MKeyExchange(std::string sensorid, std::string key, std::string secret, int step);
	virtual ~MKeyExchange();
	std::string toString();
	std::string toMessage();
	std::string getSensorID();
	std::string getKey();
	int getStep();

protected:
	std::string sensorid, key;
	int step;
};

#endif
