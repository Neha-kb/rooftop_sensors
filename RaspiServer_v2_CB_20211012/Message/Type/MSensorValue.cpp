#include "MSensorValue.hpp"
#include <iostream>
#include <string>
#include <ios>
extern "C"
{
    #include "Utility/message.h"
    #include <stdlib.h>
}

MSensorValue::MSensorValue(std::string data) : Message(data){
	if(isValid()){
		struct MESSAGE_Data d;
		d.data = reinterpret_cast<unsigned char*>(const_cast<char*>(data.data()));
		d.length = (unsigned int)data.length();
		/*sensorid = "";
		for(int i = 0; i < d.data[1]; i++){
			sensorid += d.data[i+2];
		}
		secret = Authenticator::getInstance().getSecretForID(sensorid);*/
		MESSAGE_Data s;
		s.data = reinterpret_cast<unsigned char*>(const_cast<char*>(this->secret.data()));
		s.length = (unsigned int)(this->secret.length());
		struct MESSAGE_Sensorvalue *val = MESSAGE_createSensorValue(&d, &s);
		if(val == NULL)
		{
			throw -1;
		}
		sensorid = val->sensorid;
		sensortype = val->sensortype;
		sensordescription = val->sensordescription;
		unit = val->unit;
		time = val->time;
		value = val->value;
		accuracy = val->accuracy;
		MESSAGE_deleteSensorValue(val);
	}
}
MSensorValue::~MSensorValue()
{

}
std::string MSensorValue::toString(){
    std::string ret = "";
    ret += "Sensorid: "         + sensorid          + "\n";

    ret += "Sensortyp: "        + sensortype        + "\n";

    ret += "Sensorbeschreibung: "+ sensordescription+ "\n";

    ret += "Messwert: ";
    ret +=  std::to_string(value);
    ret += " " + unit+ "\n";

    ret += "Messzeitpunkt: ";
    ret += asctime(localtime(&time));
    //ret += "\n";		Already in asctime

    ret += "Genauigkeit: ";
    ret += std::to_string(accuracy);
    ret += "\n";
    return ret;
}
std::string MSensorValue::toMessage()
{
	std::string ret;
	struct MESSAGE_Sensorvalue senval;
	senval.sensorid 		 = const_cast<char*>(sensorid.c_str());
	senval.sensortype 		 = const_cast<char*>(sensortype.c_str());
	senval.sensordescription = const_cast<char*>(sensordescription.c_str());
	senval.time 			 = time;
	senval.unit 			 = const_cast<char*>(unit.c_str());
	senval.value 			 = value;
	senval.accuracy 		 = accuracy;
    struct MESSAGE_Data s;
    s.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(secret.data()));
    s.length = static_cast<unsigned int>(secret.length());
	struct MESSAGE_Data *data = MESSAGE_makeMessageFromSensorValue(&senval, &s);
	if(data != nullptr)
	{
		ret = std::string(reinterpret_cast<char*>(data->data), data->length);
		MESSAGE_deleteMessageData(data);
	}
	return ret;
}
std::string MSensorValue::getSensorID()
{
	return sensorid;
}
std::string MSensorValue::getSensorType()
{
	return sensortype;
}
std::string MSensorValue::getSensorDescription()
{
	return sensordescription;
}
std::string MSensorValue::getUnit()
{
	return unit;
}
time_t MSensorValue::getTime()
{
	return time;
}
float MSensorValue::getValue()
{
	return value;
}
float MSensorValue::getAccuracy()
{
	return accuracy;
}
