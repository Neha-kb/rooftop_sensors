#ifndef LoRa_hpp
#define LoRa_hpp

#include <SPI.h>
#include <LoRa.h>
#include "Manager.hpp"
//#include "message.h"
extern "C"
{
  #include "message.h"
}

namespace LoRaSpace{

  class LoRaManager : public Manager{
    public:
      LoRaManager();
      ~LoRaManager();
      bool initConnection();
      bool promoteToGateway();
      void sendMessage(MESSAGE_Data* outgoing);
      void receiveMessage(MESSAGE_Data* incoming); 
      int  connectionStatus() { return 0; };
    private:
      char LoRaState = 0;             //0: Uninitialized; 1:initialized; 2:Gateway-Mode; 3:Waiting(GatewayMode)
      void sendGuard(MESSAGE_Data* outgoing);
      double sendTime = 0;
      long lastMillis = 0;
  };

}

#endif