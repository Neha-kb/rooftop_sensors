#ifndef MessageHandler_hpp
#define MessageHandler_hpp

#include "Type/Message.hpp"
#include <vector>
#include <memory>

class MessageHandler
{
public:
    MessageHandler();
    virtual ~MessageHandler();
    static std::vector<std::unique_ptr<Message>> processMessage(std::unique_ptr<Message> message);
};

#endif
