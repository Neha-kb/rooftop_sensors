#include "MAcknowledged.hpp"
#include <cstring>
extern "C"
{
    #include "Utility/message.h"
}

MAcknowledged::MAcknowledged(std::string data) : Message(data)
{
	if(isValid()){
		struct MESSAGE_Data d;
		d.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(data.data()));
		d.length = static_cast<unsigned int>(data.length());
		struct MESSAGE_Acknowledged *ack = MESSAGE_createAcknowledged(&d);
		if(ack == nullptr)
		{
			throw -1;
		}
		sensorid = std::string(ack->sensorid);
		hash = std::string(ack->messagehash, MESSAGE_HASHSIZE);
		MESSAGE_deleteAcknowledged(ack);
	}
}
MAcknowledged::MAcknowledged(std::string sensorid, std::string hash, std::string secret) : sensorid(sensorid), hash(hash)
{
	this->secret = secret;
}
MAcknowledged::~MAcknowledged()
{

}
std::string MAcknowledged::toString(){
    std::string ret = "";
    ret += "Sensorid: "     + sensorid + "\n";
    ret += "Messagehash: "  + hash     ;
    return ret;
}
std::string MAcknowledged::toMessage()
{
	std::string ret;
	struct MESSAGE_Acknowledged ack;
    ack.sensorid = const_cast<char*>(sensorid.c_str());
    memcpy(ack.messagehash, hash.data(), MESSAGE_HASHSIZE);
    struct MESSAGE_Data s;
    s.data = reinterpret_cast<unsigned char*>(const_cast<char*>(secret.data()));
    s.length = static_cast<unsigned int>(secret.length());
	struct MESSAGE_Data *data = MESSAGE_makeMessageFromAcknowledged(&ack, &s);
	if(data != nullptr)
	{
		ret = std::string(reinterpret_cast<char*>(data->data), data->length);
		MESSAGE_deleteMessageData(data);
	}
	return ret;
}
std::string MAcknowledged::getSensorID()
{
    return sensorid;
}
std::string MAcknowledged::getHash()
{
	return hash;
}
