#include "WiFiManager.hpp"


void WiFiManager::sendMessage(MESSAGE_Data* msg){
    bool udpbegintag = udp.beginPacket(SERVERIP.c_str(), SERVERPORT);    //
    udp.write(msg->data, msg->length);
    bool udpendtag = udp.endPacket();
    // Serial.println(udpbegintag);
    // Serial.println(udpendtag);
    //Serial.println("UDP Packet ended!");
}
void WiFiManager::receiveMessage(MESSAGE_Data* incoming){
    incoming->length = 0;
    packetsize = udp.parsePacket();
    if(packetsize > 0){
        udp.read(buffer, 1500);
        incoming->data = (unsigned char*) realloc(incoming->data, packetsize);
        memcpy(incoming->data, buffer, packetsize);
        incoming->length = packetsize;
    } 
}

int  WiFiManager::connectionStatus(){
    return WiFi.RSSI();
}
WiFiManager::~WiFiManager(){
    
}
bool WiFiManager::initConnection(){
    bool ret = false;
    //start wifi
    Serial.printf("Connecting to %s ", SSID.c_str());
    WiFi.begin(SSID.c_str(), PASSWORD.c_str());
    bool success = (WiFi.status() != WL_CONNECTED);
    for(int i = 0; (success && i<40); i++){
        vTaskDelay(500);
        Serial.print(".");
        if(i==39){
            Serial.print(" WiFi couldnt be");
        }
        success = (WiFi.status() != WL_CONNECTED);
    }
    Serial.println(" connected");
    if(!success){
        //start udp server
        udp.begin(2345);
        Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), 2345);
        ret = true;
    }
    //WiFi.disconnect();
    //WiFi.mode(WIFI_OFF);
    return ret;
}