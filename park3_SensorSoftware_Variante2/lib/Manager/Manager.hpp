#ifndef MANAGER_hpp
#define MANAGER_hpp

//#include "message.h"
extern "C"
{
  #include "message.h"
}

enum MANAGER_TYPE{
    MANAGER_INVALID,
    MANAGER_WIFI,
    MANAGER_LORA
};

class Manager{
public:
    Manager(){mt = MANAGER_INVALID;};
    virtual ~Manager(){};
    virtual bool initConnection() = 0;
    virtual void sendMessage(MESSAGE_Data* outgoing) = 0;
    virtual void receiveMessage(MESSAGE_Data* incoming) = 0; 
    virtual int  connectionStatus() = 0;
    MANAGER_TYPE getManagerType(){return mt;};
protected:
    MANAGER_TYPE mt;
};

#endif