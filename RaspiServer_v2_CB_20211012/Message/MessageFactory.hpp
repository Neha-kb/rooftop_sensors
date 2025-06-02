#ifndef MessageFactory_hpp
#define MessageFactory_hpp

#include "Type/Message.hpp"
#include <string>
#include <memory>

class MessageFactory
{
public:
    MessageFactory();
    virtual ~MessageFactory();
    static std::unique_ptr<Message> createMesssage(std::string data);
};

#endif
