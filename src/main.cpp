#include <Arduino.h>
#include "config.h"
#include <HardwareSerial.h>
#include <SPI.h>
#include <SD.h>

HardwareSerial Serial485(2);

byte inByte; //byte to store the serial buffer
byte smlMessage[1000]; //byte to store the parsed message
const byte startSequence[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 }; //start sequence of SML protocol
const byte stopSequence[]  = { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A }; //end sequence of SML protocol
const byte powerSequence[] =       { 0x77, 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF }; //sequence preceeding the current "Real Power" value (2 Bytes)
const byte consumptionSequence[] = { 0x77, 0x07, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF }; //sequence predeecing the current "Total power consumption" value (4 Bytes)
int smlIndex; //index counter within smlMessage array
int startIndex; //start index for start sequence search
int stopIndex; //start index for stop sequence search
int stage; //index to maneuver through cases
byte power[8]; //array that holds the extracted 4 byte "Wirkleistung" value
byte consumption[8]; //array that holds the extracted 4 byte "Gesamtverbrauch" value
unsigned long currentpower; //variable to hold translated "Wirkleistung" value
unsigned long currentconsumption; //variable to hold translated "Gesamtverbrauch" value
float currentconsumptionkWh; //variable to calulate actual "Gesamtverbrauch" in kWh



void SD_test(void)
{
  SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN))
  {
    Serial.println("SDCard MOUNT FAIL");
  }
  else
  {
    uint32_t cardSize = SD.cardSize() / (1024 * 1024);
    String str = "SDCard Size: " + String(cardSize) + "MB";
    Serial.println(str);
  }
}

void setup()
{
  pinMode(RS485_EN_PIN, OUTPUT);
  digitalWrite(RS485_EN_PIN, HIGH);

  pinMode(RS485_SE_PIN, OUTPUT);
  digitalWrite(RS485_SE_PIN, HIGH);

  pinMode(PIN_5V_EN, OUTPUT);
  digitalWrite(PIN_5V_EN, HIGH);

  Serial.begin(9600);
  Serial485.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  delay(5);
  Serial.println("start...");
  
  // put your setup code here, to run once:
  //SD_test();
}






void findStartSequence() {
  while (Serial485.available())
  {
    inByte = Serial485.read(); //read serial buffer into array
    
    if (inByte == startSequence[startIndex]) //in case byte in array matches the start sequence at position 0,1,2...
    {
      smlMessage[startIndex] = inByte; //set smlMessage element at position 0,1,2 to inByte value
      startIndex++;
      if (startIndex == sizeof(startSequence)) //all start sequence values have been identified
      {
        #ifdef _debug_msg
        Serial.println("Match found - Start Sequence");
        #endif
        stage = 1; //go to next case
        smlIndex = startIndex; //set start index to last position to avoid rerunning the first numbers in end sequence search
        startIndex = 0;
      }
    }
    else {
      startIndex = 0;
    }
  }
}



void findStopSequence() {
  while (Serial485.available())
  {
    inByte = Serial485.read();
    smlMessage[smlIndex] = inByte;
    smlIndex++;

    if (inByte == stopSequence[stopIndex])
    {
      stopIndex++;
      if (stopIndex == sizeof(stopSequence))
      {
        #ifdef _debug_msg
        Serial.println("Match found - Stop Sequence");
        #endif
        stage = 2;
        stopIndex = 0;
      }
    }
    else {
      stopIndex = 0;
    }
  }
}

void findPowerSequence() {
  byte temp; //temp variable to store loop search data
 startIndex = 0; //start at position 0 of exctracted SML message
 
for(int x = 0; x < sizeof(smlMessage); x++){ //for as long there are element in the exctracted SML message
    temp = smlMessage[x]; //set temp variable to 0,1,2 element in extracted SML message
    if (temp == powerSequence[startIndex]) //compare with power sequence
    {
      startIndex++;
      if (startIndex == sizeof(powerSequence)) //in complete sequence is found
      {
        for(int y = 0; y< 3; y++){ //read the next 2 bytes (the actual power value)
          power[y] = smlMessage[x+y+8]; //store into power array
          #ifdef _debug_msg
          Serial.print(String(power[y], HEX));
          Serial.print(" ");
          #endif        
        }
        #ifdef _debug_msg
        Serial.println();
        #endif
        stage = 3; // go to stage 3
        startIndex = 0;
      }
    }
    else {
      startIndex = 0;
    }
  }
   currentpower = (power[0] << 8 | power[1] << 0); //merge 2 bytes into single variable to calculate power value
}


void findConsumptionSequence() {
  byte temp;
 
  startIndex = 0;
for(int x = 0; x < sizeof(smlMessage); x++){
    temp = smlMessage[x];
    if (temp == consumptionSequence[startIndex])
    {
      startIndex++;
      if (startIndex == sizeof(consumptionSequence))
      {
        for(int y = 0; y < 4; y++){
          //hier muss für die folgenden 4 Bytes hoch gezählt werden
          consumption[y] = smlMessage[x+y+16];
          #ifdef _debug_msg
          Serial.print(String(consumption[y], HEX));
          Serial.print(" ");
          #endif
        }
        #ifdef _debug_msg
        Serial.println();
        #endif
        stage = 4;
        startIndex = 0;
      }
    }
    else {
      startIndex = 0;
    }
  }
 
   currentconsumption = consumption[0];
   currentconsumption <<= 8;
   currentconsumption += consumption[1];
   currentconsumption <<= 8;
   currentconsumption += consumption[2];
   currentconsumption <<= 8;
   currentconsumption += consumption[3];

   currentconsumptionkWh = (float)currentconsumption / 10000; // 10.000 impulses per kWh
}


void publishMessage() {
//Wirkleistung
Serial.print("Real Power: ");
Serial.print(currentpower);
Serial.println(" W");
// Gesamtverbrauch
Serial.print("Total Consumption: ");
Serial.print(currentconsumptionkWh);
Serial.println(" kWh");

// clear the buffers
  memset(smlMessage, 0, sizeof(smlMessage));
  memset(power, 0, sizeof(power));
  memset(consumption, 0, sizeof(consumption));
//reset case
  smlIndex = 0;
  stage = 0; // start over
}



void loop() {
  switch (stage) {
    case 0:
      findStartSequence(); // look for start sequence
      break;
    case 1:
      findStopSequence(); // look for stop sequence
      break;
    case 2:
      findPowerSequence(); //look for power sequence and extract
      break;
    case 3:
      findConsumptionSequence(); //look for consumption sequence and exctract
      break;
    case 4:
      publishMessage(); // do something with the result
      break;   
  }
}