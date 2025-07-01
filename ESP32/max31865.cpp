#include <SPI.h>
#include <Adafruit_MAX31865.h>

// Custom SPI pins
#define MAX31865_CS   15
#define MAX31865_MOSI 12
#define MAX31865_MISO 13
#define MAX31865_SCK  14

// Create software SPI instance with custom pins
Adafruit_MAX31865 maxSensor = Adafruit_MAX31865(MAX31865_CS, MAX31865_MOSI, MAX31865_MISO, MAX31865_SCK);

// Reference resistor value
#define RREF 430.0

void setup() {
  Serial.begin(115200);
  Serial.println("MAX31865 PT100/PT1000 Sensor Test");

  maxSensor.begin(MAX31865_3WIRE);  //  2WIRE, 3WIRE, or 4WIRE
}

void loop() {
  float temperature = maxSensor.temperature(100, RREF);  // For PT100


  if (isnan(temperature)) {
    Serial.println("Failed to read temperature!");
  } else {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
  }
  delay(1000);
}
