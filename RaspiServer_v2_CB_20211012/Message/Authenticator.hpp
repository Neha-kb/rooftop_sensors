#ifndef Authenticator_hpp
#define Authenticator_hpp

#include "Type/Message.hpp"
#include "Type/MKeyExchange.hpp"
#include "Utility/sqlite3.h"
#include <memory>
#include <unordered_map>
#include <mutex>

class Authenticator
{
public:
    static Authenticator& getInstance();
    virtual ~Authenticator();
    std::unique_ptr<MKeyExchange> processMessageKeyExchange(MKeyExchange &message);
    //bool isValid(Message &message);
    //void setSecretOfMessage(Message &message);
    std::string getPSK();
    std::string getSecretForID(std::string id);
    
protected:
    Authenticator();
    static const std::string psk;
    static const std::string secrettable;
    static const unsigned int privatekeysize;
    std::mutex secretdblock;
    sqlite3 *secretdb;
};

#endif
