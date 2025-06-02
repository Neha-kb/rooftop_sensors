#include "Authenticator.hpp"
#include "Type/MKeyExchange.hpp"
extern "C"
{
    #include "Utility/message.h"
    #include "Utility/diffiehellman.h"
}
#include <iostream>
#include <typeinfo>
#include <chrono>
#include <random>
#include <cstring>

const std::string Authenticator::psk             = "abc";
const std::string Authenticator::secrettable    = "secretdb.db";
const unsigned int Authenticator::privatekeysize = 256;

Authenticator& Authenticator::getInstance()
{
    static Authenticator A;
    return A;
}
Authenticator::Authenticator()
{
    //open database
    if(SQLITE_OK != sqlite3_open(Authenticator::secrettable.c_str(), &secretdb))
    {
        std::cout << "Fehler in SQLITE-Open im Konstruktor";
        throw -1;
    }
    
    //create the table if not available
    sqlite3_stmt *stmt;
    if(SQLITE_OK != sqlite3_prepare_v2(secretdb, "CREATE TABLE IF NOT EXISTS 'Secrettable' (Sensorid TEXT PRIMARY KEY NOT NULL, Secret BLOB NOT NULL);", -1, &stmt, NULL))
    {
        std::cout << "Fehler in SQLITE-Prepare im Konstruktor";
        throw -1;
    }
    if(SQLITE_DONE != sqlite3_step(stmt))
    {
        std::cout << "Fehler in SQLITE-Step im Konstruktor";
        throw -1;
    }
    if(SQLITE_OK != sqlite3_finalize(stmt))
    {
        std::cout << "Fehler in SQLITE-Finalize im Konstruktor";
        throw -1;
    }

}
Authenticator::~Authenticator()
{
    sqlite3_close(secretdb);
}
std::unique_ptr<MKeyExchange> Authenticator::processMessageKeyExchange(MKeyExchange &message)
{
    std::lock_guard<std::mutex> lock(secretdblock);
    
    //step == 1: sensor sends his Diffie-Hellmann Public Key to us
    //step == 2: we send our Diffie-Hellmann Public Key to the sensor
    std::unique_ptr<MKeyExchange> retmessage;
    if(message.getStep() == 1)
    {
        std::string messagesensorid = message.getSensorID();
        std::string messagekey      = message.getKey();
        unsigned char privatekey[Authenticator::privatekeysize];
        unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE];
        struct DIFFIEHELLMAN_Data privatedata;
        privatedata.data   = privatekey;
        privatedata.length = Authenticator::privatekeysize;
        struct DIFFIEHELLMAN_Data oppositedata;
        oppositedata.data   = reinterpret_cast<unsigned char*>(const_cast<char*>(messagekey.data()));
        oppositedata.length = static_cast<unsigned int>(messagekey.length());
        std::string publickey;
        
        //generate private key (just random data)
        std::knuth_b generator(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<unsigned char> distribution(0, 255);
        for(unsigned int i=0; i<Authenticator::privatekeysize; i++)
        {
            privatekey[i] = distribution(generator);
        }
        
        //calculate public key
        struct DIFFIEHELLMAN_Data *publicdata = DIFFIEHELLMAN_generatePublicKey(&privatedata);
        if(publicdata == nullptr)
        {
            std::cout << "fehlerhafter public key in authenticator" << std::endl;
            std::cout.flush();
            return retmessage;
        }
        publickey = std::string(reinterpret_cast<char*>(publicdata->data), publicdata->length);
        DIFFIEHELLMAN_deleteKeyData(publicdata);
        
        //calculate shared secret
        if(false == DIFFIEHELLMAN_generateSharedSecret(&oppositedata, &privatedata, sharedsecret))
        {
            std::cout << "fehlerhafter shared secret in authenticator" << std::endl;
            std::cout.flush();
            return retmessage;
        }
        
        std::cout << "sharedsecret in authenticator for: " + message.getSensorID() << std::endl;
        for(unsigned int i=0; i<DIFFIEHELLMAN_SHAREDSECRETSIZE; i++)
        {
            std::cout << std::hex << (unsigned int)sharedsecret[i];
        }
        std::cout << std::endl << std::endl;
        std::cout.flush();
        
        //store shared secret in the database
        std::string query = std::string("INSERT OR REPLACE INTO Secrettable (Sensorid, Secret) VALUES (?, ?);");
        sqlite3_stmt *stmt;
        if(SQLITE_OK != sqlite3_prepare_v2(secretdb, query.c_str(), -1, &stmt, NULL))
        {
            std::cout << "Fehler in SQLITE-Prepare in -processMessageKeyExchange-";
            return retmessage;
        }
        sqlite3_bind_text(stmt, 1, messagesensorid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 2, sharedsecret, DIFFIEHELLMAN_SHAREDSECRETSIZE, SQLITE_STATIC);
        int ret = sqlite3_step(stmt);
        if(ret != SQLITE_DONE)
        {
            std::cout << "Fehler in SQLITE-STEP in -processMessageKeyExchange-";
        }
        if(SQLITE_OK != sqlite3_finalize(stmt))
        {
            std::cout << "Fehler in SQLITE-Finalize in -processMessageKeyExchange-";
        }
        
        //create answer message
        retmessage = std::unique_ptr<MKeyExchange>(new MKeyExchange(message.getSensorID(), publickey, Authenticator::psk, 2));
    }
    return retmessage;
}
/*bool Authenticator::isValid(Message &message)
{
    Authenticator::setSecretOfMessage(message);
    return message.isValid();
}
void Authenticator::setSecretOfMessage(Message &message)
{
    std::lock_guard<std::mutex> lock(dblock);
    
    std::string s;
    
    //MKeyExchange MEssages bekommen den Pre-Shared-Key
    if(typeid(message) == typeid(MKeyExchange))
    {
        s = Authenticator::psk;
    }
    else
    {
        //search the appropriate key for any other message type
        std::string query = std::string("SELECT Secret FROM Secrettable WHERE Sensorid='") + message.getSensorID() + "';";
        sqlite3_stmt *stmt;
        if(SQLITE_OK != sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, NULL))
        {
            std::cout << "Fehler in SQLITE-Prepare in -setSecretOfMessage-";
            return;
        }
        int ret = sqlite3_step(stmt);
        if(ret == SQLITE_ROW)
        {
            const unsigned char *key = static_cast<const unsigned char*>(sqlite3_column_blob(stmt, 0));
            unsigned int bytes = sqlite3_column_bytes(stmt, 0);
            s = (key == nullptr)? "" : std::string(reinterpret_cast<const char*>(key), bytes);
        }
        else if(ret != SQLITE_DONE)
        {
            std::cout << "Fehler in SQLITE-STEP in -setSecretOfMessage-";
        }
        if(SQLITE_OK != sqlite3_finalize(stmt))
        {
            std::cout << "Fehler in SQLITE-Finalize in -setSecretOfMessage-";
        }
    }
    message.setSecret(s);
}*/
std::string Authenticator::getPSK(){return Authenticator::psk;}
std::string Authenticator::getSecretForID(std::string id){
    std::lock_guard<std::mutex> lock(secretdblock);
    std::string s = "";

    //search the appropriate key for any other message type
    std::string query = std::string("SELECT Secret FROM Secrettable WHERE Sensorid='") + id + "';";
    sqlite3_stmt *stmt;
    if(SQLITE_OK != sqlite3_prepare_v2(secretdb, query.c_str(), -1, &stmt, NULL))
    {
        std::cout << "Fehler in SQLITE-Prepare in -getSecretForID-";
        return s;
    }
    int ret = sqlite3_step(stmt);
    if(ret == SQLITE_ROW)
    {
        const unsigned char *key = static_cast<const unsigned char*>(sqlite3_column_blob(stmt, 0));
        unsigned int bytes = sqlite3_column_bytes(stmt, 0);
        s = (key == nullptr)? "" : std::string(reinterpret_cast<const char*>(key), bytes);
    }
    else if(ret != SQLITE_DONE)
    {
        std::cout << "Fehler in SQLITE-STEP in -getSecretForID-";
    }
    if(SQLITE_OK != sqlite3_finalize(stmt))
    {
        std::cout << "Fehler in SQLITE-Finalize in -getSecretForID-";
    }
    return s;
}
