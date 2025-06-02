#include <Database/DBConnection.hpp>
#include "MessageHandler.hpp"
#include "Authenticator.hpp"
#include "Type/MSensorValue.hpp"
#include "Type/MTimeInfo.hpp"
#include "Type/MAcknowledged.hpp"
#include "Type/MKeyExchange.hpp"
#include <typeinfo>
#include <string>
#include <map>
#include <chrono>
#include <iostream>
extern "C"
{
	#include "Utility/message.h"
}

#define Zeit std::chrono::system_clock
#define GHOSTINTERVAL 10


struct Sensor{
	time_t lastSync;			//holds the last received time from sensorvalue
	time_t serverTime;			//servertime at last receive
	char tries;
	time_t tried;
	time_t serverTimeByTimeInfo;
};

std::map<std::string, Sensor> sensors;
//Declaration cant be inside switch
MSensorValue* msv;
MAcknowledged* mack;
MTimeInfo* mtf;
MKeyExchange* mkex;
struct Sensor* sensor;

MessageHandler::MessageHandler()
{

}
MessageHandler::~MessageHandler()
{

}
std::vector<std::unique_ptr<Message>> MessageHandler::processMessage(std::unique_ptr<Message> message)
{
    std::vector<std::unique_ptr<Message>> vec;
    
    //is message authenticated?
	if(message->isValid())	{
		std::cout << "Nachricht ist authentifiziert" << std::endl;

	    std::cout << "Empfangene Nachricht: " << std::endl;
	    std::cout << message->toString() << std::endl;
	    std::cout.flush();

	    //determine type of message
	    //Message &type = *message.get();
	    switch(message->getMType()){
	    	case MESSAGE_KEYEXCHANGE:
				vec.push_back(Authenticator::getInstance().processMessageKeyExchange(dynamic_cast<MKeyExchange&>(*message.get())));
	    		break;
	    	case MESSAGE_TIMEINFO:
	    		sensor = &sensors[message->getSensorID()];
	    		//if(sensor->serverTimeByTimeInfo <= Zeit::to_time_t(Zeit::now()) - GHOSTINTERVAL){
	    			//sensor->serverTimeByTimeInfo = Zeit::to_time_t(Zeit::now());
					//answer with servertime
				    //MTimeInfo mtf = dynamic_cast<MTimeInfo&>(*message.get());
				    vec.push_back(std::unique_ptr<Message>(new MTimeInfo(message->getSensorID(), Zeit::to_time_t(Zeit::now()), message->getSecret())));
	    		//}
			    break;
	    	case MESSAGE_SENSORVALUE:
	    		msv = dynamic_cast<MSensorValue*>(message.get());
	    		sensor = &sensors[(message->getSensorID()+msv->getSensorType())];
				if(sensor->lastSync < msv->getTime()){	//case: new Sensorvalue arrives
					sensor->lastSync = msv->getTime();
					sensor->serverTime = Zeit::to_time_t(Zeit::now());		//Update cached Servertime to filter ghosts
					sensor->tries = 0;										//A new Value arrived at Server, Sensorbuffer should be cleared; stop sending Acks for older ones
					DBConnection::getInstance().WriteDB(*msv);
					vec.push_back(std::unique_ptr<Message>(new MAcknowledged(message->getSensorID(), message->calculateHash(), message->getSecret())));
				}
				else if(sensor->lastSync == msv->getTime()){	//case: Ack from last Sensorvalue failed
					if(sensor->serverTime <= Zeit::to_time_t(Zeit::now()) - GHOSTINTERVAL){	//its not a "ghost" from an other Gateway
						sensor->serverTime = Zeit::to_time_t(Zeit::now());
						vec.push_back(std::unique_ptr<Message>(new MAcknowledged(message->getSensorID(), message->calculateHash(), message->getSecret())));
					}
				}
				else if(sensor->tried > msv->getTime()){		//case: Sensor tries to send wrongly stamped Messages after power lost with connection lost etc.
					if(sensor->serverTime <= Zeit::to_time_t(Zeit::now()) - GHOSTINTERVAL){	//its not a "ghost" from an other Gateway
						++(sensor->tries);
						sensor->serverTime = Zeit::to_time_t(Zeit::now());
						if(sensor->tries > 5){					//accept the wrong message(s) to deplete Sensor bufer
							sensor->serverTime = Zeit::to_time_t(Zeit::now());
							vec.push_back(std::unique_ptr<Message>(new MAcknowledged(message->getSensorID(), message->calculateHash(), message->getSecret()))); //Give the sensor Acks for depletion
						}
					}
				}
				else{
					//something went wrong, server tries to catch up
				}
			    break;
	    	case MESSAGE_ACKNOWLEDGED:
	    		std::cout << "Why sends someone an authorized Ack to the Server?" << std::endl;
	    		break;
	    }

	    /*

	    if(typeid(type) == typeid(MKeyExchange))
	    {
	        vec.push_back(Authenticator::getInstance().processMessageKeyExchange(dynamic_cast<MKeyExchange&>(*message.get())));
	    }
	    else if(typeid(type) == typeid(MTimeInfo))
	    {
	        //answer with servertime
	        MTimeInfo mtf = dynamic_cast<MTimeInfo&>(*message.get());
	        vec.push_back(std::unique_ptr<Message>(new MTimeInfo(mtf.getSensorID(), std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), Authenticator::getInstance().getSecretForID(mtf.getSensorID()))));
	    }
	    else if(typeid(type) == typeid(MSensorValue))
	    {
	    	//std::cout << dynamic_cast<MSensorValue&>(*message.get()).getSensorID() << std::endl;
	    	//std::cout << dynamic_cast<MSensorValue&>(*message.get()).getSensorType() << std::endl;
	    	//std::cout << dynamic_cast<MSensorValue&>(*message.get()).getTime() << std::endl;
	    	//std::cout.flush();

	    	DBConnection dbCon;
	    	dbCon.WriteDB(dynamic_cast<MSensorValue&>(*message.get()));
	        MSensorValue msv = dynamic_cast<MSensorValue&>(*message.get());

	    	//new DBConnection(dynamic_cast<MSensorValue&>(*message.get()));

	        vec.push_back(std::unique_ptr<Message>(new MAcknowledged(msv.getSensorID(), message->calculateHash(), Authenticator::getInstance().getSecretForID(msv.getSensorID()))));
	    }
	    else if(typeid(type) == typeid(MAcknowledged))
	    {
	        //should never be received by the server
	    }
	/*
	    //set secret of all messages
	    for(int i=0; i<vec.size(); i++)
	    {
	    	Authenticator::getInstance().setSecretOfMessage(*vec[i].get());
	    }*/
	}
	else
	{
		std::cout << "Nachricht ist !nicht! authentifiziert" << std::endl;
	}


    return vec;
}
