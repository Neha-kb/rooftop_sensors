#include "BluetoothSensorConnection.hpp"
extern "C"
{
	//#include <bluetooth/bluetooth.h>
}

BluetoothSensorConnection::BluetoothSensorConnection() : isrunning(true)
{
	T = std::thread(&BluetoothSensorConnection::manageConnection, this);
}

BluetoothSensorConnection::~BluetoothSensorConnection()
{
	isrunning = false;
	T.join();
}
void BluetoothSensorConnection::manageConnection()
{

}

