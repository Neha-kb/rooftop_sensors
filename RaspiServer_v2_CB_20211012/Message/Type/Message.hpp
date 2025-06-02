#ifndef Message_hpp
#define Message_hpp

#include <string>

class Message
{
public:
	Message();
    Message(std::string data);
    virtual ~Message();
    void setSecret(std::string secret);
    std::string calculateHash();
    bool isValid();
    virtual std::string toString()    = 0;
    virtual std::string toMessage()   = 0;
    std::string getSensorID();
    std::string getSecret();
    char getMType();
private:
    //std::string sensorid;
protected:
    std::string rawmessage, secret, sensorid;
    char mType;
};

#endif
