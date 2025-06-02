#include "MessageFactory.hpp"
#include "Type/MSensorValue.hpp"
#include "Type/MAcknowledged.hpp"
#include "Type/MTimeInfo.hpp"
#include "Type/MKeyExchange.hpp"
extern "C"
{
    #include "Utility/message.h"
}
#include <iostream>

MessageFactory::MessageFactory()
{
    
}
MessageFactory::~MessageFactory()
{
    
}
std::unique_ptr<Message> MessageFactory::createMesssage(std::string data)
{
	std::unique_ptr<Message> message;
    struct MESSAGE_Data d;
    d.data = reinterpret_cast<unsigned char*>(const_cast<char*>(data.data()));
    d.length = (unsigned int)data.length();
	enum MESSAGE_MESSAGETYPE type = MESSAGE_getMessageType(&d);
	if(type == MESSAGE_SENSORVALUE)
	{
		try
		{
			message = std::unique_ptr<Message>(new MSensorValue(data));
		}
		catch (...){}
	}
	else if(type == MESSAGE_ACKNOWLEDGED)
	{
		try
		{
			message = std::unique_ptr<Message>(new MAcknowledged(data));
		}
		catch (...){}
	}
	else if(type == MESSAGE_TIMEINFO)
	{
		try
		{
			message = std::unique_ptr<Message>(new MTimeInfo(data));
		}
		catch (...){}
	}
	else if(type == MESSAGE_KEYEXCHANGE)
	{
		try
		{
			message = std::unique_ptr<Message>(new MKeyExchange(data));
		}
		catch (...){}
	}
    else
    {
        std::cout << "fehlerhafte Nachricht in -messageFectory-" << std::endl;
        std::cout.flush();
    }
	return message;
}
