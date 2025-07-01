#include <OneWire.h>
#include <DallasTemperature.h>

// GPIO where the DS18B20 data pin is connected
#define ONE_WIRE_BUS 4

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress sensorAddress;

void setup() {
  Serial.begin(115200);
  Serial.println("DS18B20 Temperature Sensor Test");

  sensors.begin();

  // Check for connected devices
  if (!sensors.getAddress(sensorAddress, 0)) {
    Serial.println("No DS18B20 sensor found! Check wiring.");
    while (1); // Stop execution
  }

  // Optional: print the address
  Serial.print("Found DS18B20 sensor at address: ");
  printAddress(sensorAddress);
  Serial.println();
}

void loop() {
  sensors.requestTemperatures();

  float temperatureC = sensors.getTempC(sensorAddress);
  if (temperatureC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: DS18B20 sensor disconnected or not responding!");
  } else {
    Serial.print("Temperature: ");
    Serial.print(temperatureC);
    Serial.println(" °C");
  }

  delay(1000);
}

// Helper function to print device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
