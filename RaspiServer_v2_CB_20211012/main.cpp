#include "SensorConnection/UDPSensorConnection.hpp"
#include "Message/Authenticator.hpp"
#include "Message/Type/MKeyExchange.hpp"
#include "Message/Type/MTimeInfo.hpp"
#include "Message/MessageFactory.hpp"
#include "Message/MessageHandler.hpp"
extern "C"
{
#include "Utility/message.h"
#include "Utility/diffiehellman.h"
}
#include <iostream>
#include <random>
#include <vector>
#include <string>
#include <cstdlib>
#include <memory>
#include <ctime>
#include <cstdlib>

int main(int argc, const char * argv[])
{

     //test for Authenticator, simulate "numsensors" sensors
    /*unsigned int numsensors = 100;
     std::string psk = "abc";

     //generate sensorids and privatekeys
     std::vector<std::string> sensorids, privatekeys, sharedsecrets;
     for(int i=0; i<numsensors; i++)
     {
         std::string id, key;

         //id
         id = std::string("T-") + std::to_string(i);

         //key
         //random values for key
         std::knuth_b generator(std::chrono::high_resolution_clock::now().time_since_epoch().count());
         std::uniform_int_distribution<unsigned char> distribution(0, 255);
         for(unsigned int i=0; i<256; i++)
         {
             unsigned char c = distribution(generator);
             key += std::string((char*)&c, 1);
         }

         //append to vectors
         sensorids.push_back(id);
         privatekeys.push_back(key);
     }

     //exchange public keys and calculate shared secrets
     for(unsigned int i=0; i<numsensors; i++)
     {
         struct DIFFIEHELLMAN_Data privatekey;
         privatekey.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(privatekeys[i].data()));
         privatekey.length = static_cast<unsigned int>(privatekeys[i].length());

         //calculate public key
         std::string publickey;
         struct DIFFIEHELLMAN_Data *ourpub = DIFFIEHELLMAN_generatePublicKey(&privatekey);
         if(ourpub != nullptr)
         {
             publickey = std::string(reinterpret_cast<char*>(ourpub->data), ourpub->length);
             DIFFIEHELLMAN_deleteKeyData(ourpub);
         }
         else
         {
             std::cout << "Fehler bei diffiehellman public key!";
             return 1;
         }

         //send public key to Server and retrieve answer
         struct MESSAGE_Keyexchange kex;
         kex.sensorid   = const_cast<char*>(sensorids[i].c_str());
         kex.key.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(publickey.data()));
         kex.key.length = static_cast<unsigned int>(publickey.length());
         kex.step       = 1;
         struct MESSAGE_Data secret;
         secret.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(psk.c_str()));
         secret.length = static_cast<unsigned int>(psk.length());

         struct MESSAGE_Data *mkex = MESSAGE_makeMessageFromKeyExchange(&kex, &secret);
         MKeyExchange message(std::string(reinterpret_cast<char*>(mkex->data), mkex->length));
         MESSAGE_deleteMessageData(mkex);
         if(!Authenticator::getInstance().isValid(message))
         {
             std::cout << "Fehler bei Gueltigkeit des Standardkeys!";
             return 1;
         }
         std::unique_ptr<MKeyExchange> retmes = Authenticator::getInstance().processMessageKeyExchange(message);
         if(retmes->getStep() != 2 || retmes->getSensorID() != std::string("Server"))
         {
             std::cout << "Fehler bei step oder id!";
             return 1;
         }

         //calculate shared secret
         std::string messagekey = retmes->getKey();
         unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE];
         struct DIFFIEHELLMAN_Data oppositepublickey;
         oppositepublickey.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(messagekey.data()));
         oppositepublickey.length = static_cast<unsigned int>(messagekey.length());
         if(false == DIFFIEHELLMAN_generateSharedSecret(&oppositepublickey, &privatekey, sharedsecret))
         {
             std::cout << "Fehler bei shared secret!";
             return 1;
         }
         sharedsecrets.push_back(std::string(reinterpret_cast<char*>(sharedsecret), DIFFIEHELLMAN_SHAREDSECRETSIZE));
    }

    //check if messages could be validated with the generated shared secrets
    for(int i=0; i<numsensors; i++)
    {
        struct MESSAGE_Timeinfo ti;
        ti.sensorid = const_cast<char*>(sensorids[i].c_str());
        ti.time.tm_sec = 1;
        ti.time.tm_min = 2;
        ti.time.tm_hour = 3;
        ti.time.tm_mday = 20;
        ti.time.tm_mon = 5;
        ti.time.tm_year = 200;
        ti.time.tm_isdst = -1;

        struct MESSAGE_Data sharedsecretdata;
        sharedsecretdata.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(sharedsecrets[i].data()));
        sharedsecretdata.length = static_cast<unsigned int>(sharedsecrets[i].length());

        struct MESSAGE_Data *mti = MESSAGE_makeMessageFromTimeInfo(&ti, &sharedsecretdata);
        MTimeInfo testmessage(std::string(reinterpret_cast<char*>(mti->data), mti->length));
        MESSAGE_deleteMessageData(mti);

        if(!Authenticator::getInstance().isValid(testmessage))
        {
            std::cout << "Fehler, nicht valid " << i;
            return 1;
        }
    }
    std::cout << std::endl << "Ales funktioniert und stimmt!";*/

    /*
     //-----------simulate one sensor---------------------------

     //get unique secret key from server first
     std::string sensorid("T-5-7");
     std::string psk("abc");
     std::string sharedkey;
     std::string privatekey("467385a48273a436725364987152345264736253647584736254985735245364758463524354657682");
     std::string publickey;

     char *pubkey = DIFFIEHELLMAN_generatePublicKey(privatekey.c_str());
     if(pubkey != nullptr)
     {
     publickey = std::string(pubkey);
     }
     free(pubkey);
     struct MESSAGE_Keyexchange kex;
     kex.sensorid = const_cast<char*>(sensorid.c_str());
     kex.key      = const_cast<char*>(publickey.c_str());
     kex.step     = 1;
     char *mkex = MESSAGE_makeJSONFromKeyExchange(&kex, psk.c_str());

     //send message keyexchange and retreive answer
     std::string mkexa = MessageHandler::processMessage(MessageFactory::createMesssage(mkex))[0].get()->toJSONString();
     free(mkex);
     struct MESSAGE_Keyexchange *kexa = MESSAGE_createKeyExchange(mkexa.c_str());

     //calculate shared key
     char *shared = DIFFIEHELLMAN_generateSharedSecret(kexa->key, privatekey.c_str());
     if(shared != nullptr)
     {
     sharedkey = std::string(shared);
     }
     free(shared);
     MESSAGE_deleteKeyExchange(kexa);

     //- - - - - loop - - - - - -
     //get the time
     struct MESSAGE_Timeinfo tif = {0};
     tif.sensorid = const_cast<char*>(sensorid.c_str());
     char *mtif = MESSAGE_makeJSONFromTimeInfo(&tif, sharedkey.c_str());

     //send message timeinfo and retreive answer
     std::string answer = MessageHandler::processMessage(MessageFactory::createMesssage(mtif))[0].get()->toJSONString();
     free(mtif);
     struct MESSAGE_Timeinfo *tifa = MESSAGE_createTimeInfo(answer.c_str());

     //ToDO: extract timestamp
     std::string time("2017-01-01T10:30:00Z");
     MESSAGE_deleteTimeInfo(tifa);

     //send message until answer*/


    UDPSensorConnection UDP(1234);
    std::cin.get();
    std::cout << "Hello, World!\n";
    return 0;
}

