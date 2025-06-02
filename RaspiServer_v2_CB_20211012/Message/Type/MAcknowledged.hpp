#ifndef MESSAGE_TYPE_MACKNOWLEDGED_HPP_
#define MESSAGE_TYPE_MACKNOWLEDGED_HPP_

#include "Message.hpp"

class MAcknowledged : public Message
{
public:
	MAcknowledged(std::string data);
    MAcknowledged(std::string sensorid, std::string hash, std::string secret);
	virtual ~MAcknowledged();
	std::string toString();
	std::string toMessage();
    std::string getSensorID();
	std::string getHash();

protected:
	std::string sensorid, hash;
};

#endif
