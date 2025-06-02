#include "Connect.hpp"

#define NOLORA true

char useConn = 1;               //1:WiFi, 2:LoRa, 3:...


uint32_t waitmillis_rec = 100; // random amount of milliseconds to wait (between 1ms and 999ms)

const TickType_t xDelay_rec = pdMS_TO_TICKS(waitmillis_rec); // Get amount of FreeRtos ticks from milliseconds (time to tick conversion)


Manager* connManager;
LoRaSpace::LoRaManager LoRaM;

std::map<std::string, enum MESSAGE_MESSAGETYPE> clients;

void sendData(MESSAGE_Data* m){
    // if(connManager->getManagerType() == MANAGER_WIFI) {
    //     if(connManager->connectionStatus() < -82) {
    //         delete(connManager);
    //         connManager = new LoRaSpace::LoRaManager();
    //     }
    // }
    connManager->sendMessage(m);
}
enum MESSAGE_MESSAGETYPE receiveMessage(MESSAGE_Data* md, String sensorid){
    unsigned long waitstart = millis();
    while(millis() - waitstart <= timeout){
        //read in packet
        connManager->receiveMessage(md);

        if(md->length > 0){
            boolean package = true;     	                //messages are no longer delivered directly and must vbe filtered
            MESSAGE_Data* id = MESSAGE_getMessageID(md);
            if(id->length != sensorid.length()){
                package = false;
            }
            else {
                for(unsigned int i = 0; i < id->length; i++){
                    if(sensorid[i] != id->data[i]){
                        package = false;
                    }
                }
            }
            MESSAGE_deleteMessageData(id);
            if(package) break;
            md->length = 0;
        } 
        vTaskDelay(xDelay_rec);
    }
    return MESSAGE_getMessageType(md);
}
enum MESSAGE_MESSAGETYPE receiveMessageLookUp(MESSAGE_Data* md){            //for Gateway-Mode only. Waiting or filtering packages arent needed
    connManager->receiveMessage(md);    
    return MESSAGE_getMessageType(md);
}

bool getServerTime(String sensorid, unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE], time_t *time){
    bool hastime = false;
    unsigned int tries = 0;

    //create timemessage
    struct MESSAGE_Timeinfo tif = {};
    tif.sensorid = const_cast<char*>(sensorid.c_str());
    tif.time = *time;
    struct MESSAGE_Data sharedsecretdata;
    sharedsecretdata.data = sharedsecret;
    sharedsecretdata.length = DIFFIEHELLMAN_SHAREDSECRETSIZE;
    MESSAGE_Data *mtif = MESSAGE_makeMessageFromTimeInfo(&tif, &sharedsecretdata);

    MESSAGE_Data incoming;
    incoming.data = (unsigned char*) malloc(2);
    while(!hastime && (tries < maxtries)){
        tries++;

        sendData(mtif);
        
        if(MESSAGE_TIMEINFO == receiveMessage(&incoming, sensorid)){
            if(MESSAGE_isValid(&incoming, &sharedsecretdata)){
                struct MESSAGE_Timeinfo *servertimeinfo = MESSAGE_createTimeInfo(&incoming);
                *time = servertimeinfo->time;
                MESSAGE_deleteTimeInfo(servertimeinfo);
                hastime = true;
            }
        }
    }    
    
    //clean up
    MESSAGE_deleteMessageData(mtif);
    free(incoming.data);

    return hastime;
}
bool authenticateSensor(String sensorid, unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE], time_t *time){
    //generate private key
    unsigned char privatekey[privatekeysize];
    for(int i=0; i<privatekeysize; i++){
        privatekey[i] = random(0, 255);
    }
    DIFFIEHELLMAN_Data privatedata;
    privatedata.data = privatekey;
    privatedata.length = privatekeysize;

    //calculate own public key
    DIFFIEHELLMAN_Data *publickey = DIFFIEHELLMAN_generatePublicKey(&privatedata);

    //create key exchange message
    struct MESSAGE_Keyexchange kex;
    kex.sensorid   = (char*)sensorid.c_str();
    kex.key.data   = publickey->data;
    kex.key.length = publickey->length;
    kex.step       = 1;

    struct MESSAGE_Data pskdata;
    pskdata.data = (unsigned char*) psk.c_str();
    pskdata.length = psk.length();
    MESSAGE_Data *mkex = MESSAGE_makeMessageFromKeyExchange(&kex, &pskdata);

    //try to exchange and verify keys
    bool secretisvalid = false;
    unsigned int authtries = 3;
    bool hassharedsecret = false;
    MESSAGE_Data incoming;
    incoming.data = (unsigned char*) malloc(2);
    while(!hassharedsecret && authtries) {
        --authtries;

        sendData(mkex);
        
        //wait for the right response message
        if(MESSAGE_KEYEXCHANGE == receiveMessage(&incoming, sensorid)){
            if(MESSAGE_isValid(&incoming, &pskdata)){   
                Serial.println("KEX Message is valid");         
                struct MESSAGE_Keyexchange *kexa = MESSAGE_createKeyExchange(&incoming);
                DIFFIEHELLMAN_Data oppositekey;
                oppositekey.data = kexa->key.data;
                oppositekey.length = kexa->key.length;
                DIFFIEHELLMAN_generateSharedSecret(&oppositekey, &privatedata, sharedsecret);
                MESSAGE_deleteKeyExchange(kexa);
                hassharedsecret = true;
            }
        }
    }
    
    //clean up
    DIFFIEHELLMAN_deleteKeyData(publickey);
    MESSAGE_deleteMessageData(mkex);
    free(incoming.data);

    //Test key with a time request
    if(hassharedsecret){
        secretisvalid = getServerTime(sensorid, sharedsecret, time);
        if(secretisvalid) Serial.println("Secret is valid");
    }

    return secretisvalid;
}
bool sendSensorValue(String sensorid, unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE], String sensortyp, String sensordescription, String unit, float accuracy, float value, time_t time, bool fullData){
    //sendtries: how many acktimeout does this function take (maximum)?
    int sendtries = 3;//floatbuffer.size();
    if(connManager->getManagerType() == MANAGER_LORA) sendtries = 1;
    boolean acked = false;

    //calculate timestamp
    //struct tm timestamp = *gmtime(&timebuffer[0]);
    /*char timestring[sizeof(time_t)+1] = {0};
    memcpy(timestring, &time, sizeof(time_t));*/
    //convert timestamp to String
    //char timestring[100] = {};
    //strftime(timestring, 100, "%Y-%m-%dT%H:%M:%SZ", &timestamp);

    //assemble message
    struct MESSAGE_Sensorvalue senval = {0};
    senval.sensorid = (char*)sensorid.c_str();              //needed to detect Password
    senval.sensortype = (char*)sensortyp.c_str();          //needed to differ many Sensors in one Device
    senval.time = time;                                             //timestamp &
    senval.value = value;                                           //value
    if((connManager->getManagerType() == MANAGER_WIFI) || fullData){
        senval.sensordescription = (char*)sensordescription.c_str();
        senval.unit = (char*)unit.c_str();
        senval.accuracy = accuracy;
    }
    else{
        senval.sensordescription = (char*)"";
        senval.unit = (char*)"";
        senval.accuracy = -666;                 //Codeword for message.c lib; float would normally converted to string via scanf
    }
    
    // Serial.print(senval.sensorid);
    // Serial.print("\t");
    // Serial.print(senval.sensortype);
    // Serial.print("\t");
    // Serial.print(senval.sensordescription);
    // Serial.print("\t");
    // Serial.print(senval.unit);
    // Serial.print("\t");
    // Serial.print(senval.value);
    // Serial.print("\t");
    // Serial.print(senval.time);
    // Serial.println("\t");

    //create the message
    MESSAGE_Data sharedsecretdata;
    sharedsecretdata.data = sharedsecret;
    sharedsecretdata.length = DIFFIEHELLMAN_SHAREDSECRETSIZE;
    MESSAGE_Data *msenval = MESSAGE_makeMessageFromSensorValue(&senval, &sharedsecretdata);

    //calculate the message hash
    unsigned char hash[MESSAGE_HASHSIZE];
    MESSAGE_calculateMessageHash(msenval, hash);
    
    MESSAGE_Data incoming;
    incoming.data = (unsigned char*) malloc(2);
    while(!acked && sendtries){
        sendtries--;

        //send the message
        sendData(msenval);

        
        //wait for the right response message
        if(MESSAGE_ACKNOWLEDGED == receiveMessage(&incoming, sensorid)){
            MESSAGE_insertExpectedHash(&incoming, hash);
            if(MESSAGE_isValid(&incoming, &sharedsecretdata)){
                struct MESSAGE_Acknowledged *ack = MESSAGE_createAcknowledged(&incoming);
                if(0 == memcmp(ack->messagehash, hash, MESSAGE_HASHSIZE)){
                    acked = true;
                }
                MESSAGE_deleteAcknowledged(ack);  
            }      
        }
    }
    
    //clean up
    MESSAGE_deleteMessageData(msenval);
    free(incoming.data);
    
    return acked;
}

void gatewayIdle(){
    if(NOLORA) return;
    if(connManager->getManagerType() != MANAGER_WIFI) return;    //Only in WiFi-Mode beeing a Gateway makes Sense           
    if(!LoRaM.promoteToGateway()) return;   
    MESSAGE_Data incoming;
    std::string sensorid = "";
    incoming.data = (unsigned char*) malloc(2);
    LoRaM.receiveMessage(&incoming);
    if(incoming.length){
        MESSAGE_MESSAGETYPE mt;
        switch(MESSAGE_getMessageType(&incoming)){
            case MESSAGE_KEYEXCHANGE: mt = MESSAGE_KEYEXCHANGE; break;
            case MESSAGE_TIMEINFO:    mt = MESSAGE_TIMEINFO; break;
            case MESSAGE_SENSORVALUE: mt = MESSAGE_ACKNOWLEDGED; break;
            default: Serial.println("Unexpected Message Type occured for LoRa-package. Returning."); return;            
        }
        MESSAGE_Data *ID = MESSAGE_getMessageID(&incoming);
        for(unsigned int i = 0; i < ID->length; i++) sensorid += ID->data[i];
        MESSAGE_deleteMessageData(ID);
        sendData(&incoming);
        clients[sensorid] = mt;
        Serial.println("Got an message from other sensor.");
        free(incoming.data);
    }
    incoming.data = (unsigned char*) malloc(2);
    if(MESSAGE_INVALID != receiveMessageLookUp(&incoming)){
        Serial.println("Received a message in LookUp");
        MESSAGE_Data *ID = MESSAGE_getMessageID(&incoming);
        sensorid = "";
        for(unsigned int i = 0; i < ID->length; i++) sensorid += ID->data[i];
        MESSAGE_deleteMessageData(ID);
        if(MESSAGE_getMessageType(&incoming) == clients[sensorid]){
            LoRaM.sendMessage(&incoming);
            clients[sensorid] = MESSAGE_INVALID;
            Serial.println("Message goes out on LoRa");
        }
        free(incoming.data);
    }
}

void initConnection(){
    if(connManager != NULL){
        delete(connManager);
    }
    switch(useConn){
        case 1:
            connManager = new WiFiManager();
            if(connManager->initConnection()){            
                break;
            }
            delete(connManager);
            //no break; Code running past this point is wanted behaviour
        case 2:
            if(!NOLORA) {
                connManager = new LoRaSpace::LoRaManager();     //Namespace is crucial, dont try to delete it
                if(connManager->initConnection()){
                    break;
                }
                delete(connManager);
            }
    }

}