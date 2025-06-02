#include "MTimeInfo.hpp"
#include <iostream>
extern "C"
{
	#include "Utility/message.h"
}
#include <ctime>

MTimeInfo::MTimeInfo(std::string data) : Message(data)
{
	if(isValid()){
		struct MESSAGE_Data d;
		d.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(data.data()));
		d.length = static_cast<unsigned int>(data.length());
		struct MESSAGE_Timeinfo *tif = MESSAGE_createTimeInfo(&d);
		if(tif == nullptr)
		{
			throw -1;
		}
		sensorid = std::string(tif->sensorid);
		//secret = Authenticator::getInstance().getSecretForID(sensorid);
		tp = tif->time;
		MESSAGE_deleteTimeInfo(tif);
	}
}
MTimeInfo::MTimeInfo(std::string sensorid, time_t time, std::string secret) : sensorid(sensorid), tp(time)
{
	this->secret = secret;
}
MTimeInfo::~MTimeInfo()
{

}
std::string MTimeInfo::toString(){
    std::string ret = "";
    ret += "Sensorid: "     + sensorid + "\n";
    ret += "Zeitpunkt: ";
    ret += asctime(localtime(&tp));
    ret += "\n";
    return ret;
}
std::string MTimeInfo::toMessage()
{
	std::string ret;
	struct MESSAGE_Timeinfo tif;
    tif.sensorid = const_cast<char*>(sensorid.c_str());
	tif.time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct MESSAGE_Data s;
    s.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(secret.data()));
    s.length = static_cast<unsigned int>(secret.length());
	struct MESSAGE_Data *data = MESSAGE_makeMessageFromTimeInfo(&tif, &s);
	if(data != nullptr)
	{
		ret = std::string(reinterpret_cast<char*>(data->data), data->length);
		MESSAGE_deleteMessageData(data);
	}
	return ret;
}
std::string MTimeInfo::getSensorID()
{
    return sensorid;
}
time_t MTimeInfo::getTime()
{
	return tp;
}
