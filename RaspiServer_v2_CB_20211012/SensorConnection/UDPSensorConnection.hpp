#ifndef UDPSensorConnection_hpp
#define UDPSensorConnection_hpp

#include <iostream>
#include "SensorConnection.hpp"
#include <atomic>
#include <thread>

class UDPSensorConnection : public SensorConnection
{
public:
    UDPSensorConnection(unsigned short port);
    virtual ~UDPSensorConnection();
    
protected:
    unsigned short port;
    std::atomic<bool> isrunning;
    std::thread T;
    void manageConnection();
};

#endif
