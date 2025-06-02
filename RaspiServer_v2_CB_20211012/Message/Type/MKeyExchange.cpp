#include "MKeyExchange.hpp"
extern "C"
{
	#include "Utility/message.h"
}

MKeyExchange::MKeyExchange(std::string data) : Message(data)
{
	if(isValid()){
		struct MESSAGE_Data d;
		d.data = reinterpret_cast<unsigned char*>(const_cast<char*>(data.data()));
		d.length = static_cast<unsigned int>(data.length());
		struct MESSAGE_Keyexchange *kex = MESSAGE_createKeyExchange(&d);
		if(kex == nullptr)
		{
			throw -1;
		}
		sensorid = std::string(kex->sensorid);
		key = std::string(reinterpret_cast<char*>(kex->key.data), kex->key.length);
		step = kex->step;
		MESSAGE_deleteKeyExchange(kex);
	}
}

MKeyExchange::MKeyExchange(std::string sensorid, std::string key, std::string secret, int step) : sensorid(sensorid), key(key), step(step)
{
    this->secret = secret;
}
MKeyExchange::~MKeyExchange()
{

}
std::string MKeyExchange::toString(){
    std::string ret = "";
    ret += "Sensorid: "     + sensorid  + "\n";
    ret += "Key: "          + key       + "\t";
    ret += "Step: "         + step      ;
    return ret;
}
std::string MKeyExchange::toMessage()
{
	std::string ret;
	struct MESSAGE_Keyexchange kex;
	kex.sensorid   = const_cast<char*>(sensorid.c_str());
    kex.key.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(key.data()));
	kex.key.length = static_cast<unsigned int>(key.length());
	kex.step 	   = step;
    struct MESSAGE_Data s;
    s.data = reinterpret_cast<unsigned char*>(const_cast<char*>(secret.data()));
    s.length = static_cast<unsigned int>(secret.length());
	struct MESSAGE_Data *data = MESSAGE_makeMessageFromKeyExchange(&kex, &s);
	if(data != nullptr)
	{
		ret = std::string(reinterpret_cast<char*>(data->data), data->length);
		MESSAGE_deleteMessageData(data);
	}
	return ret;
}
std::string MKeyExchange::getSensorID()
{
    return sensorid;
}
std::string MKeyExchange::getKey()
{
	return key;
}
int MKeyExchange::getStep()
{
	return step;
}

