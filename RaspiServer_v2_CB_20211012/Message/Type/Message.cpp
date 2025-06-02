#include "Message.hpp"
#include "../Authenticator.hpp"
extern "C"
{
	#include "Utility/message.h"
}

Message::Message()
{

}
Message::Message(std::string data)
{
    rawmessage = data;
    struct MESSAGE_Data d;
    d.data = reinterpret_cast<unsigned char*>(const_cast<char*>(data.data()));
    d.length = (unsigned int)data.length();
    MESSAGE_Data* id = MESSAGE_getMessageID(&d);
    for(unsigned int i = 0; i < id->length; i++) sensorid += id->data[i];
    MESSAGE_deleteMessageData(id);
    mType = MESSAGE_getMessageType(&d);
    if(mType==MESSAGE_KEYEXCHANGE) secret = Authenticator::getInstance().getPSK();
    else secret = Authenticator::getInstance().getSecretForID(sensorid);
}
Message::~Message()
{

}
void Message::setSecret(std::string secret)
{
	this->secret = secret;
}
std::string Message::calculateHash()
{
	std::string hash;
    unsigned char data[MESSAGE_HASHSIZE];
    struct MESSAGE_Data h;
    h.data = reinterpret_cast<unsigned char*>(const_cast<char*>(rawmessage.data()));
    h.length = (unsigned int)rawmessage.length();
	if(MESSAGE_calculateMessageHash(&h, data))
	{
		hash = std::string(reinterpret_cast<char*>(data), MESSAGE_HASHSIZE);
	}
	return hash;
}
bool Message::isValid()
{
    struct MESSAGE_Data d;
    struct MESSAGE_Data s;
    d.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(rawmessage.data()));
    s.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(secret.data()));
    d.length = (unsigned int)rawmessage.length();
    s.length = (unsigned int)secret.length();
	return MESSAGE_isValid(&d, &s);
}

std::string Message::getSensorID(){
	return sensorid;
}

char Message::getMType(){
	return mType;
}

std::string Message::getSecret(){
	return secret;
}
