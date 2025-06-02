//Libraries for LoRa module
#include "LoRaManager.hpp"

//pins used by the LoRa module (Slave)
#define SCK 5     //SPI Clk-Pin
#define MOSI 27   //Master Out Slave In
#define MISO 19   //Master In Slave Out
#define SS 18     //Slave Select ()
#define RST 14    //Resetpin
#define DIO0 26   //DIO1 33, DIO2 32

//LoRa definitions          *Gateway and Endnode need the same configuration*
#define TxPower 14          //Power output in dBm. 14 Corresponds to max. 25mW EIRP for EU, under consideration of Antenna max. gain 2.25dB
#define BANDWIDTH 250E3     //For highest Datarate, Bandwidth should be as high as possible
#define syncWord 0x62       //between 0x00 and 0xFF, 0x34 default (for GFSK: default 0xC194C1)
#define PreAmble 10         //Length of Preamble, 8 default, SF7: longer Preamble is better
#define CRdn 5              // 4/x FEC denominator x, 5 to 8; if many packages
#define CRC true            //CRC for the package
#define IMPLICITMODE false  //Sending package with known parameters to save traffic for packageheader "SF6" needs this option on, Implementation for sending is missing
#define USEISR true        //Use an ISR to receive LoRa Packages
                            //invert IQ for sending as receiver
#define USECHANNELS 1       
#define USESF8 false        //Use SF8, amount of channels doubles. Datarate halfs!

#define TIMEOUT 5000

namespace LoRaSpace{
  namespace{                              //anonymous namespace to make receiveISR private without beeing static
    std::vector<MESSAGE_Data> incoming;
    MESSAGE_Data* dummy;
    int channel;
    char spreadingFactor;

    void onReceive(int packetSize) {
      unsigned char message[256] = {0};
      for(int i = 0; LoRa.available(); i++) {
        message[i] = (char)LoRa.read();
      }
      MESSAGE_Data md;
      md.data = message;
      md.length = packetSize;
      incoming.push_back(md);
    }

    struct Channel{                   //currently there is no more variation between channels than Frequency (and SF), Bandwidth and even SyncWord could be changed per Channel!
      long frequency;
    } channelsEU[] = {
      (long) 868.15E6,
      (long) 868.45E6,
      (long) 869.85E6,
      (long) 865.15E6,
      (long) 865.45E6,
      (long) 865.75E6,
      (long) 866.05E6,
      (long) 866.35E6,
      (long) 866.65E6,
      (long) 866.95E6,
      (long) 867.25E6,
      (long) 867.55E6,
      (long) 867.85E6
      };
  }
  LoRaManager::LoRaManager():Manager(){
    mt = MANAGER_LORA;
    dummy = (struct MESSAGE_Data*) malloc(sizeof(sizeof(struct MESSAGE_Data)));
    dummy->length = 0;
  }
  LoRaManager::~LoRaManager(){
    while(!incoming.empty()){
      //free(incoming.begin()->data);
      incoming.erase(incoming.begin());
    }
    LoRa.onReceive(NULL);
    LoRa.end();
    free(dummy);
  }
  
  bool LoRaManager::promoteToGateway(){
    bool ret = false;
    switch(LoRaState){
      case 1:
          break;
      case 3:
          sendGuard(dummy);
          if(sendTime > 0){
            LoRaState = 2; 
            Serial.print("I´m now listening on ");
            Serial.println(channelsEU[channel].frequency);
            LoRa.disableInvertIQ();
            LoRa.receive();                         //LoRa Module receiving mode
            ret = true;
          }
          break;
      case 2:
          if(sendTime < -1000){
            LoRaState = 3;
            Serial.println("Not enough Send Time for Gateway Mode.");
            LoRa.idle();
          }
          else{
            ret = true;
          }
          break;
      case 0:
        if(initConnection()){
          LoRaState = 2; 
          Serial.print("I´m now listening on ");
          Serial.println(channelsEU[channel].frequency);
          LoRa.disableInvertIQ();
          LoRa.receive();                         //LoRa Module receiving mode
          ret = true;
          break;
        }
    }
    return ret;
  }

  void LoRaManager::sendMessage(MESSAGE_Data* outgoing){
    if(LoRaState == 1) {
      LoRa.idle();
      LoRa.disableInvertIQ();
    }
    else {
      LoRa.idle();
      LoRa.enableInvertIQ();
    }
    if(outgoing->length <= 256) {
      LoRa.beginPacket();
      LoRa.write(outgoing->data, outgoing->length);
      LoRa.endPacket();

      sendGuard(outgoing);                    //calculate and inform about available sending Time
    }
    else{
      Serial.println("LoRa cant handle such big packages. Sending denied.");
    }
    if(LoRaState == 1) {
      LoRa.enableInvertIQ();
      LoRa.idle();
    }
    else {
      LoRa.disableInvertIQ();
      LoRa.receive();
    }
  }
  
  
  // void LoRaManager::receiveMessage(MESSAGE_Data* msg){
  //   if(LoRaState == 1) LoRa.receive();                           //End Node is in Idle Mode and needs to start listening
  //   msg->length = 0;
  //   long started = millis();
  //   if(LoRaState == 2) started -= TIMEOUT;       //GatewayMode calls this function whenever possible, waiting isnt needed, explanation follows
  //   do{
  //     if(!incoming.empty()){
  //       msg->data = (unsigned char*) realloc(msg->data, incoming.begin()->length);
  //       memcpy(msg->data, incoming.begin()->data, incoming.begin()->length);
  //       msg->length = incoming.begin()->length;
  //       incoming.erase(incoming.begin());
  //       break;
  //     }
  //     vTaskDelay(200);
  //   }while(millis()-started < TIMEOUT);         //substitude a:started b:(time since started) t:TIMEOUT (a+b)-(a-t)<t == b+t<t, so gateway runs loop once
    
  //   if(LoRaState == 1) LoRa.idle();
  // }
  void LoRaManager::receiveMessage(MESSAGE_Data* msg){
    /*****************************************************
    Old Function wasnt able to receive packages without extreme corruption.
    In an ISR however, everything worked as intended.
    After some retesting and Libraryupdates, both ways are working.
    If there comes any trouble again, receiving can be switched.
    *****************************************************/
    if(LoRaState == 1) LoRa.receive();
    msg->length = 0;
    long started = millis();
    if(LoRaState == 2) started -= TIMEOUT;       //GatewayMode calls this function whenever possible, waiting isnt needed, explanation follows
    do{
      if(!USEISR){
        msg->length = LoRa.parsePacket();  
        if(msg->length){
          char buffer[256];
          int incomingLength = 0;
          while(LoRa.available()){
            ++incomingLength;
            buffer[incomingLength] = (char) LoRa.read();
          }
          msg->data = (unsigned char*) realloc(msg->data, msg->length);
          memcpy(msg->data, buffer, msg->length);
          break;
        } 
      }
      else{
        if(!incoming.empty()){
          msg->data = (unsigned char*) realloc(msg->data, incoming.begin()->length);
          memcpy(msg->data, incoming.begin()->data, incoming.begin()->length);
          msg->length = incoming.begin()->length;
          incoming.erase(incoming.begin());
          break;
        }
      }
      vTaskDelay(200);
    }while(millis()-started < TIMEOUT);

    if(LoRaState == 1) LoRa.idle();
  }

  void LoRaManager::sendGuard(MESSAGE_Data* outgoing){
    double ts =  (pow(2,spreadingFactor)/BANDWIDTH) * 1000;        //duration of one symbol
    double sPre = PreAmble + 4.25;             //number of Symbols for Preamble
    double sPay = (8+CRdn+(8*outgoing->length-4*spreadingFactor+28+16*CRC-20*IMPLICITMODE)/(4*spreadingFactor)*CRdn); //numbor of Symbols for Header, Payload etc.
    double f = (millis() - lastMillis);
    lastMillis = millis();
    sendTime = sendTime + f*0.01 - ts*(sPre + sPay);
    
    if(sendTime < 0) {
      Serial.print("Send Time left: ");Serial.print(sendTime);
      switch(LoRaState){
        case 1:
          Serial.println("ms.\tConsider increasing updateinterval, if this message keeps appearing. Sensor will not stop working!");  
          break;
        case 2:
          Serial.println("ms.\tMany Acks are sent from this Gateway, it may stops working. Hopefully there is a Backup-Gateway.");
          break;
        case 3:
          Serial.println("ms.\tGatewaymode is turned off, waiting for new Send Time.");
          break;
      }
    }
  }

  bool LoRaManager::initConnection(){  
    bool ret = false;
    if(USECHANNELS < sizeof(channelsEU)/sizeof(struct Channel)){
      channel = random(0, USECHANNELS - 1);
    }
    else{
      channel = random(0, sizeof(channelsEU)/sizeof(struct Channel) - 1);
    }
    
    spreadingFactor = 7;
    if(USESF8) {
      spreadingFactor += (channel % 2);   //Use SF7 first on an Frequency, in case of uneven Usechannels
      channel = (int) (channel / 2);      //channel should refer to the Frequency used, by adding an other SF, needed Frequencies for USECHANNELS halfs
    }

    //SPI LoRa pins
    SPI.begin(SCK, MISO, MOSI, SS);    
    //setup LoRa transceiver module
    LoRa.setPins(SS, RST, DIO0);    //Must be called before LoRa.begin();
    LoRa.end();
    vTaskDelay(100);
    if (LoRa.begin(channelsEU[channel].frequency)) {
      Serial.println("Status: LoRa - OK");
      LoRaState = 1;
      LoRa.setTxPower(TxPower);
      LoRa.setSpreadingFactor(spreadingFactor);
      LoRa.setSignalBandwidth(BANDWIDTH);
      LoRa.setSyncWord(syncWord);
      LoRa.setPreambleLength(PreAmble);
      LoRa.setCodingRate4(CRdn);
      if(CRC) LoRa.enableCrc();
      else LoRa.disableCrc();

      LoRa.idle();
      LoRa.enableInvertIQ();
      if(USEISR) LoRa.onReceive(onReceive);     //set ISR for incoming transmission
      ret = true;
    }
    else{
      Serial.println("Status: LoRa - Failed");
      LoRaState = 0;
    }
    return ret;
  }

}