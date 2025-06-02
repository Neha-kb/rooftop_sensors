#include <Arduino.h>
#include <EEPROM.h>
#include <Preferences.h>
#include <vector>
#include "Ringbuffer.h"
#include <esp_task_wdt.h>
#include "Sensor.hpp"
#include <chrono>
#include <WiFi.h>
#include <Wire.h>


#include <iostream>
#include <sstream>


extern "C"
{
  #include "message.h"
  #include "diffiehellman.h"
  #include <string.h>
}


//Tasks
TaskHandle_t MainTask; //Build and Send the Messages, some other stuff
TaskHandle_t MeasureIrradiationTask; //Measure the PV Temperature

//Semaphores
//Protect different Ressources with Semaphores
SemaphoreHandle_t I2C_Semaphor;
//SemaphoreHandle_t WIFI_Semaphor;

//Hardware pins and I2C adress of the ADC (MCP3426)
#define ADC_Addr 0x6B//0x68
#define ADC_SCL 22
#define ADC_SDA 21

//Sensor Pins ESP32
//Einstrahlungssensor
float valU = 0;
float valI = 0;
float valIrr = 0;
float UFL;
float IFL;
float IrrFL;
int raw_adc = 0;
//Analogeingang A0
int IrrPin = 34;
int UPin = 35;
int IPin = 32; //Reset dead man switch Pin


//Timetracking stuff
time_t t;
int tSync = 120;                            //every controller should Sync to the same point for measurementcomparison
char updates = 2;                           //Per tSync
char updateperiode = tSync / updates;       //There can be more Updates per tSync
int intervalsecond = random(1, updateperiode-1);
char restartPeriod = random(50, 70);        //restart after 50-70 times the Updateinterval

#define timesynctry 7
bool timesynccheck = false;



//Pairing
const int greenLED = 17;
const int redLED = 16;
//const int onboardLED = 2;
const int PairingButton = 4; //Bei Sensorplatine V1: GPIO2, bei V3, V4: GPIO4
const int BUZZER = 2; //Allgemein GPIO23 Ausnahme: Sensorplatine V3 mit TTGO LoRa, da ist es GPIO39 (geht aber nicht)
static bool PairingButton_pressed = false;
const int buzzChannel = 1;

std::vector<Sensor> sensors;

/***************************************************
* save the Time to EEPROM
****************************************************/
void saveTimeToEEPROM(time_t servertime){
    //save servertime as time_t after sharedsecret into eeprom
    unsigned char* data = reinterpret_cast<unsigned char*>(&servertime);
    for(int i=0; i<sizeof(time_t); i++){
        EEPROM.write(DIFFIEHELLMAN_SHAREDSECRETSIZE + i, data[i]);
    }
    EEPROM.commit();
}

/*keeps track of time after sync with server
returns true every updateinterval at intervalsecond
saves every intervalsecond minute time to EEPROM*/
char tickTack(){  
    char ret = 0;
    static unsigned long previousMillis = millis();
    long seconds = (long) t;           //only affects startup; -3 to let MeasureTask first create some Measurements before trying to send
    int ticker = seconds%tSync - tSync;
    static int sendInterval = (int) updateperiode / sensors.size(); // PASCAL: eventuell so -> static int sendInterval = intervalsecond;
    if (millis() - previousMillis >= 1000)  {
        previousMillis += 1000;      
        ++t;
        if(++ticker == 0){
            ret = 1;
        }
        else if((ticker%updateperiode) == 0){
            ret = 1;
        }
        else if(((++ticker))%sendInterval==0) {
            ret = 2;
        }
    }  
    esp_task_wdt_reset();

    //Serial.print((uint8_t) ret);
    //Serial.print("\n");

    return ret;
}
/*shortens an MAC to save Data
shortendegree default:no changes; 1:getting rid of ':' 2:binary*/
String shortenID(String MAC, char shortendegree = 0){ 
    String ret;
    
    switch(shortendegree){
        case 1:
            for(int i = 0; i < 18; i+=3){
                ret += MAC[i];
                ret += MAC[i+1];
            }
            break;
        case 2:
            char a[2];
            for(int p = 0; p < 18; p+=3){
                for(int i = 0; i < 2; i++){
                    a[i] = MAC[p+i];
                    if(a[i]<65) a[i]-=48;
                    else a[i] -= 55;
                }
                ret += (char) (a[0] * 16 + a[1]);
            }
            break;
        default:
            ret = MAC;
            break;
    }
    return ret;
}

void resetup(){
    return;             //Function disabled, may be get written in the future
    ledcWriteTone(buzzChannel, 0);
    vTaskDelay(500);
    ledcWriteTone(buzzChannel, 698.456 );
    vTaskDelay(100);
    ledcWriteTone(buzzChannel, 783.991);
    vTaskDelay(100);
    ledcWriteTone(buzzChannel, 880.000);
    vTaskDelay(100);
    ledcWriteTone(buzzChannel, 1396.91);
    vTaskDelay(100);
    ledcWriteTone(buzzChannel, 0);
    //ESP.restart();                //may be an option too
    initConnection();                     
    Serial.println("Asking Server for a new Time.");
    //getServerTime(sensors[0].sensorid, sensors[0].sharedsecret, &t);

    while(!getServerTime(sensors[0].sensorid, sensors[0].sharedsecret, &t)){  
        for(int i = 0; i < 7; i++) tickTack();      //tickTack uses millis() and needs to get updated
        Serial.println("Couldnt reach Server"); 
        vTaskDelay(100);
    }
    ledcWriteTone(buzzChannel, 0);
    vTaskDelay(500);
    ledcWriteTone(buzzChannel, 1396.91);
    vTaskDelay(100);
    ledcWriteTone(buzzChannel, 880.000);
    vTaskDelay(100);
    ledcWriteTone(buzzChannel, 783.991);
    vTaskDelay(100);
    ledcWriteTone(buzzChannel, 698.456 );
    vTaskDelay(100);
    ledcWriteTone(buzzChannel, 0);

}

/* *************************************
 * Authenticates with Server and asks for current time
 * *************************************/
void authenticate(String sensorid, unsigned char* sharedsecret){
    if(authenticateSensor(sensorid, sharedsecret, &t)){
        for(int i=0; i<DIFFIEHELLMAN_SHAREDSECRETSIZE; i++)
        {
            EEPROM.write(i, sharedsecret[i]);
        }
        saveTimeToEEPROM(t); //commit SHOULD be inside this method

        ledcWriteTone(buzzChannel, 880.000);
        vTaskDelay(50);
        ledcWriteTone(buzzChannel, 0);
        vTaskDelay(50);
        ledcWriteTone(buzzChannel, 880.000);
        vTaskDelay(50);
        ledcWriteTone(buzzChannel, 0);
        vTaskDelay(50);

        //Indicate that Pairing is done
        digitalWrite(BUZZER, HIGH);
        delay(500);
        digitalWrite(BUZZER, LOW);
        delay(500);
        digitalWrite(BUZZER, HIGH);
        delay(500);
        digitalWrite(BUZZER, LOW);
        delay(500);
        digitalWrite(BUZZER, HIGH);
        delay(500);
        digitalWrite(BUZZER, LOW);
        delay(500);
        
        Serial.println("Pairing done.");
    }
    else{
        //load old sharedsecret
        for(int i=0; i<DIFFIEHELLMAN_SHAREDSECRETSIZE; i++){
            sharedsecret[i] = EEPROM.read(i);
        }
        Serial.println("Pairing failed.");
    }
    Serial.print("Sharedsecret: ");
    for(int i=0; i<DIFFIEHELLMAN_SHAREDSECRETSIZE; i++) Serial.print(sharedsecret[i], HEX); Serial.print("\n");
    PairingButton_pressed = false;
}

/***************************************************
* Get ADC data of channel no.
****************************************************/
/*int getADC(int channel) {
  unsigned int data[2] = {0, 0};
  
  // Start I2C Transmission
  Wire.beginTransmission(ADC_Addr);
  if (channel == 1) {
     Wire.write(B10000000);
     }
  else {
     Wire.write(B10100000);
  }
  // Select data register
  Wire.endTransmission();

  delay(80);
  Wire.beginTransmission(ADC_Addr);

  Wire.write(0x00);
  // Stop I2C Transmission
  Wire.endTransmission();
  
  // Request 2 bytes of data
  Wire.requestFrom(ADC_Addr, 2);
  
  // Read 2 bytes of data
  // raw_adc msb, raw_adc lsb 
  if(Wire.available() == 2)
  {
    data[0] = Wire.read();
    data[1] = Wire.read();
  }
   
  // Convert the data to 12-bits
  raw_adc = data[0] << 8 | data[1]; 
  return raw_adc;
  /*(data[0] & 0x0F) * 256 + data[1];
  if(raw_adc > 2047)
  {
    raw_adc -= 4095;
  }

}*/

int getADC(int channel) {
  unsigned int data[2] = {0};
  
  // Start I2C Transmission
  Wire.beginTransmission(ADC_Addr);
  if (channel == 1) {
     Wire.write(B10000000);
     }
  else {
     Wire.write(B10100000);
  }
  // Select data register
  Wire.endTransmission();

  vTaskDelay(10);
  //Wire.beginTransmission(ADC_Addr);

  //Wire.write(0x00);
  // Stop I2C Transmission
  Wire.endTransmission();
  
  // Request 2 bytes of data
  Wire.requestFrom(ADC_Addr, 2);
  
  // Read 2 bytes of data
  // raw_adc msb, raw_adc lsb 
  if(Wire.available() == 2)
  {
    data[0] = Wire.read();
    data[1] = Wire.read();
  }
   
  // Convert the data to 12-bits
  raw_adc = data[0] << 8 | data[1]; 
  return raw_adc;
  /*(data[0] & 0x0F) * 256 + data[1];
  if(raw_adc > 2047)
  {
    raw_adc -= 4095;
  }*/

}


/***************************************************
* Get ES data
****************************************************/
float getIrradiation(){ //Einstrahlung

    //Wait until I2C is free to use
    while (xSemaphoreTake( I2C_Semaphor, ( TickType_t ) 5 ) == pdFALSE){
    vTaskDelay(2);
    }
    // Spannungswert einlesen
    valIrr = analogRead(IrrPin);
    
    //Wert in Spannung umrechnen
    //IrrFL = (((1.5* valIrr)/4096)*(1500/1.2))-2; //Bei 1,5V Signal und 12Bit AD-Wandler und 1500W/m² max. : OFFSET laut DB = 2W/m²
    IrrFL = (valIrr/(1862/1500)*0.8711)+136;
    //valUVolt = ((5* valU)/1023)*(1300/5); //Bei 5V Signal und 10Bit AD-Wandler und 1300W/m² max.



    //Release I2C
    xSemaphoreGive( I2C_Semaphor );
    return IrrFL;
}

/***************************************************
* Get Voltage data
****************************************************/
float getVoltage(){ 
    //Wait until I2C is free to use
    while (xSemaphoreTake( I2C_Semaphor, ( TickType_t ) 5 ) == pdFALSE){
    vTaskDelay(2);
    }
    //Spannung
    // Analogwert einlesen
    //valU = analogRead(UPin);
    //Wert in Spannung umrechnen
    //UFL = (((3.293* valU)/4096)*(60/3.293)) + 2.48; //

    //NEW VERSION WITH EXTERNAL ADC
    //Read MCP3245 ADC Value 
    //long ADC_result = mcp3246.ReadADC16(SDA_Pin,SCL_Pin); //Returns a 15bit Value
    
    //NEW
    //MCP3246::Struct ADC_result = mcp3246.ReadADC16(SDA_Pin,SCL_Pin);
    float f = getADC(1);
    
    //Release I2C
    xSemaphoreGive( I2C_Semaphor );
    // Calculate the Voltage out of the ADC reading
    // 60.635V is the max voltage based on the measurement system
    // 32768 is the max value of a 15bit ADC
    
    float ret = ((float)f/1000*29.600696517); //((115k+4,02k)/4,02k) //(ADC_result.channel1Data * 60.635) / 32768; 
    //Serial.print("Spannung: "); Serial.println(ret);
    return ret;
}

/***************************************************
* Get Current data
****************************************************/
float getCurrent(){
    //Wait until I2C is free to use
    while (xSemaphoreTake( I2C_Semaphor, ( TickType_t ) 5 ) == pdFALSE){
    vTaskDelay(2);
    }
    int f = getADC(2);
    //Serial.println(f);

    //Release I2C
    xSemaphoreGive( I2C_Semaphor );
    float ret = ((f * 1.62f / 1000.0f) - 0.329) / 0.264f;           //f MCP3426 mV; 1.62 Spannungsteiler; 0.327 (Vcc * 0.1) zero-current-offset-voltage (ACS725 Datasheet) // 
    //Serial.println(ret);
    if (ret < 0.0)
    {
        ret = 0.00;
    }
    //Serial.println(ret);


    return ret;
}



void MainTaskCode( void * pvParameters ){
    bool wakeUp = true;
    int MeasurementCounter = 0;
    char update = 0;

    esp_task_wdt_add(NULL);
    //Infinite Loop
    for(;;){
        do{//wait for updateinterval to pass
            update = tickTack();
            if(PairingButton_pressed){
                Serial.println("Start Authentication!");
                authenticate(sensors[0].sensorid, sensors[0].sharedsecret);
                PairingButton_pressed = false;
            }
            vTaskDelay(50);
            gatewayIdle();  //instead of only waiting, gateway function call can be accessed.
        }while(update == 0);

        if(update == 1){            
            for(int i = 0; i < sensors.size(); i++){
                sensors[i].createSensorvalue(t);
            }
            saveTimeToEEPROM(t);
        }
        if(update == 2){
            ledcWriteTone(buzzChannel, 4000.000);
            vTaskDelay(50);
            ledcWriteTone(buzzChannel, 0);

            // Time to wait
            vTaskDelay(50);uint32_t waitmillistosend = intervalsecond; // milliseconds to wait 
            //Serial.println(waitmillis);

            const TickType_t xDelaymsToSend = pdMS_TO_TICKS(waitmillistosend); // Get amount of FreeRtos ticks from milliseconds (time to tick conversion)
            vTaskDelay(xDelaymsToSend);  // waits the amount of ticks that are converted from the milliseconds 



            // //Serial.println(WiFi.RSSI());
        //for(int i = 0; i < sensors.size(); i++){                          //its highly preferable to send one sensor at a time with LoRa #collision #dutyCycle
            if(!sensors[MeasurementCounter%sensors.size()].sendSensorvalue(wakeUp)){
                ledcWriteTone(buzzChannel, 1880.000);
                vTaskDelay(100);
                ledcWriteTone(buzzChannel, 0);
                vTaskDelay(50);
                ledcWriteTone(buzzChannel, 880.000);
                vTaskDelay(100);
                ledcWriteTone(buzzChannel, 0);
                vTaskDelay(50);

                intervalsecond = random(2,(((int) updateperiode / sensors.size() )-3)*1000);  //intervalsecond = (random(2, (int)updateperiode/sensors.size())-1);         //change Interval in case of collision
                
            }
                
            if(++MeasurementCounter == restartPeriod*sensors.size()) {
                //Serial.println("Resetting now...");
                //resetup();                      
            }
            
            if(MeasurementCounter > sensors.size() - 1) wakeUp = false;                         //"wakeUp" is done after every sensor has sent his information once
            //if(!(MeasurementCounter%(3*sensors.size()-1))) wakeUp = true;                       //idea of sending non data saving packets in an alternating matter to achive sync without restarting; every package shoul get a round without data saving
        
        }
    }
}



void MeasureIrradiationTaskCode( void * pvParameters ){
    TickType_t xLastWakeTime;
    esp_task_wdt_add(NULL);

    //Infinite Loop
    for(;;){    

        // Initialise the xLastWakeTime variable with the current time.
        xLastWakeTime = xTaskGetTickCount();
        digitalWrite(greenLED, HIGH);
        for(int i = 0; i < sensors.size(); i++){
            sensors[i].measure();
        }
        digitalWrite(greenLED, LOW);
        esp_task_wdt_reset();
        //Wait 1000ms until the next measurement
        vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
    }
}



void setup(){
    
    //ledcSetup(0, 500,8);
    Serial.begin(9600);
    Serial.println();

    int timesynccounter = 0;
    
    pinMode(greenLED,OUTPUT);
    pinMode(redLED,OUTPUT);
    digitalWrite(redLED, HIGH);
    pinMode(BUZZER,OUTPUT);

    ledcSetup(buzzChannel, 5000, 8);
    ledcAttachPin(BUZZER, buzzChannel);

    ledcWriteTone(buzzChannel, 0);
    delay(500);
    ledcWriteTone(buzzChannel, 698.456 );
    delay(100);
    ledcWriteTone(buzzChannel, 783.991);
    delay(100);
    ledcWriteTone(buzzChannel, 880.000);
    delay(100);
    ledcWriteTone(buzzChannel, 1396.91);
    delay(100);
    ledcWriteTone(buzzChannel, 0);
    //initialize EEPROM region on flash memory, 128 bytes should be enough
    if (!EEPROM.begin(128))
    {
        Serial.println("failed to initialise EEPROM");
    }

    //Give the Power Supply a litte time to settle befor starting intensive Wifi functions
    delay(4000);

    //Configure the digital GPIOs
    pinMode(PairingButton,INPUT);
    //pinMode(PairingButton, INPUT_PULLUP);
    //digitalWrite(PairingButton, LOW);  //disables internal PullUp (doesn't seem to work for TTGO LoRa)

    initConnection();                       //inits Connection to WiFi etc.


    vTaskDelay(100);

    // Set dead man switch pin to high
    pinMode(IPin,OUTPUT);
    digitalWrite(IPin,HIGH);

    //Setup Semaphors
    I2C_Semaphor = xSemaphoreCreateMutex();
    xSemaphoreGive( ( I2C_Semaphor ) );

    unsigned char sharedsecret[DIFFIEHELLMAN_SHAREDSECRETSIZE];
    
    //load data from persistant storage
    //shared secret
    for(int i=0; i<DIFFIEHELLMAN_SHAREDSECRETSIZE; i++){
        sharedsecret[i] = EEPROM.read(i);
    }
    Serial.print("Sharedsecret from persistent Storage: ");
    for(int i = 0; i < DIFFIEHELLMAN_SHAREDSECRETSIZE; i++) Serial.print(sharedsecret[i], HEX); Serial.println(); //print Key to Serial
    
    //last saved servertime
    unsigned char data[sizeof(time_t)];
    for(int i = 0; i<sizeof(time_t); i++){
        data[i] = EEPROM.read(DIFFIEHELLMAN_SHAREDSECRETSIZE + i);
    }
    memcpy(&t, data, sizeof(time_t));

    
    //sensor properties
    String sensorid = shortenID(WiFi.macAddress(), 1);
    Serial.print("Sensor ID: "); for(int i  = 0; i < sensorid.length(); i++){Serial.print(sensorid[i]); Serial.print(" ");} Serial.println();
    
    //                       sensorid , sharedsecret, Sensortype, sensordescription, unit, acc, value
    sensors.push_back(Sensor(sensorid, sharedsecret, "U", "Spannung Solon Modul 8", "[V]", 0.023, &getVoltage)); //Spannung Solon Modul 7
    sensors.push_back(Sensor(sensorid, sharedsecret, "I", "Strom Solon Modul 8", "[A]", 0.026, &getCurrent)); //Strom Solon Modul 7

    vTaskDelay(50);

    //#intervalsecond = random(2,(((int) updateperiode / sensors.size() )-3)*1000);

    
    do{
        if (timesynccheck == false){

            uint32_t waitmillis = random(1,999); // random amount of milliseconds to wait (between 1ms and 999ms)
            //Serial.println(waitmillis);

            const TickType_t xDelayXms = pdMS_TO_TICKS(waitmillis); // Get amount of FreeRtos ticks from milliseconds (time to tick conversion)
            vTaskDelay(xDelayXms);  // waits the amount of ticks that are converted from the milliseconds 
            //Serial.println(xDelayXms);

            Serial.println("Asking Server for a new Time.");
            timesynccheck = getServerTime(sensorid, sharedsecret, &t); //timesynccheck is true if sensore received servertime message back, otherwise it remains false

            timesynccounter +=1;
        }
        else{
            timesynccounter +=1;
            continue;
        }
        
        //Serial.println(timesynccounter);
    }while(timesynccounter <= timesynctry);
    
    timesynccounter = 0; // reset timesynccounter

    vTaskDelay(100);

    //getServerTime(sensorid, sharedsecret, &t);
    saveTimeToEEPROM(t);

    // For printing Server time (UTC or local time???)
    std::stringstream stream;
    stream << t;
    Serial.printf(stream.str().c_str());

    
    // while(!getServerTime(sensorid, sharedsecret, &t)){  
    //     Serial.println("Couldnt reach Server"); 
    //     delay(10000);   
    // }
    tickTack();
    ledcWriteTone(buzzChannel, 0);
    delay(500);
    ledcWriteTone(buzzChannel, 1396.91);
    delay(100);
    ledcWriteTone(buzzChannel, 880.000);
    delay(100);
    ledcWriteTone(buzzChannel, 783.991);
    delay(100);
    ledcWriteTone(buzzChannel, 698.456 );
    delay(100);
    ledcWriteTone(buzzChannel, 0);
    
    esp_task_wdt_init(15000, false);
    esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(0));
    esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(1));

    

    Wire.begin(21,22);

    


    //Setup all the Tasks
    xTaskCreate(
        MainTaskCode,   /* Task function. */
        "MainTask",     /* name of task. */
        10000,       /* Stack size of task */
        NULL,        /* parameter of the task */
        9,           /* priority of the task */
        &MainTask     /* Task handle to keep track of created task */
    );

    xTaskCreate(
        MeasureIrradiationTaskCode,   /* Task function. */
        "MeasureIrradiationTask",     /* name of task. */
        10000,       /* Stack size of task */
        NULL,        /* parameter of the task */
        10,           /* priority of the task */
        &MeasureIrradiationTask     /* Task handle to keep track of created task */
    );


}



void loop(){

    //Check if the Pairing Button is pressed
    if (digitalRead(PairingButton)){

        //Remember that the Button is pressed until the Pairing is done in MainTask
        PairingButton_pressed = true; 

        //Wait 10s to prevent redundant Pairing
        vTaskDelay(10000);
    }else{
        //Serial.println("Pairing Button State: ");
        //Serial.println(digitalRead(PairingButton));
    }
    // //Wait 1s
    vTaskDelay(1000);
}
