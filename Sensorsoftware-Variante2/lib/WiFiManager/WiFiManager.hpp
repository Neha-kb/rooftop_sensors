#ifndef WiFiManager_hpp
#define WiFiManager_hpp

#include "Manager.hpp"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <iostream>
//#include "message.h"

extern "C"
{
  #include "message.h"
}

class WiFiManager: public Manager{
public:
    WiFiManager(): Manager(){mt = MANAGER_WIFI;};
    ~WiFiManager();
    bool initConnection();
    void sendMessage(MESSAGE_Data* outgoing);
    void receiveMessage(MESSAGE_Data* incoming); 
    int  connectionStatus();
private:
    WiFiUDP udp;    
    int packetsize;
    char buffer[1501] = {0};

//network constants           PK Buero     //Dach  //PK Mini   // Dach Phillipp blau // TP-Link CPE210 (park3) // 
    const String SSID       = "IPV-SENSOR";//"ubnt";//"TP-LINK_AC42";   //"Sensornetzwerk"; // "PARK3_WSN"; //
    const String PASSWORD   = "07175608"; //"park2019";//"62393218";       //"12345678"; // "Park3_1234"; //
    const String SERVERIP   = "192.168.2.10";//"192.168.1.10";//"192.168.0.10";   //"192.168.0.101"; // "192.168.1.31"; //
    const int    SERVERPORT = 1234;
};



#endif