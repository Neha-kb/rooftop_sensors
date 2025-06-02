#ifndef SENSORCONNECTION_BLUETOOTHSENSORCONNECTION_HPP_
#define SENSORCONNECTION_BLUETOOTHSENSORCONNECTION_HPP_

#include <thread>
#include <atomic>

class BluetoothSensorConnection
{
public:
	BluetoothSensorConnection();
	virtual ~BluetoothSensorConnection();

protected:
	std::thread T;
	std::atomic<bool> isrunning;
	void manageConnection();
};

#endif
