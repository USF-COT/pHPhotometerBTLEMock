/*
 * PHPHOTOMETERBTLEMOCK - A mock program that outputs mock values to aid
 * app development.
 *
 * Based on nRF8001 echoCallback sample code by Kevin Townsend @ Adafruit
 *
 * By: Michael Lindemuth <mlindemu@usf.edu>
 * University of South Florida
 * Center for Ocean Technology
 */

// BTLE Setup
#include <stdlib.h>
#include <SPI.h>
#include <RFduinoBLE.h>
#include "utilities.h"

// read a Hex value and return the decimal equivalent
uint8_t parseHex(char c) {
  if (c < '0')
    return 0;
  if (c <= '9')
    return c - '0';
  if (c < 'A')
    return 0;
  if (c <= 'F')
    return (c - 'A')+10;
}

void RFduinoBLE_onAdvertisement(){
  Serial.println(F("Advertising..."));
}

void RFduinoBLE_onConnect()
{
  Serial.println(F("Connected!"));
}

void RFduinoBLE_onDisconnect(){
  Serial.println(F("Disconnected"));
}

void sendBTLEString(char* sendBuffer, unsigned int length){
  unsigned int bytesRemaining;
  
  for(unsigned int i = 0; i < length; i+=20){
    bytesRemaining = min(length - i, 20);
    RFduinoBLE.send(sendBuffer + i, bytesRemaining);
  }
  
  #ifdef DEBUG
  sendBuffer[length] = '\0';
  Serial.print("BTLE Sent: "); Serial.print(sendBuffer);
  #endif
}

void printReceiveInfo(uint8_t *buffer, uint8_t len){
  Serial.print(F("Received "));
  Serial.print(len);
  Serial.print(F(" bytes: "));
  for(int i=0; i<len; i++)
   Serial.print((char)buffer[i]); 

  Serial.print(F(" ["));

  for(int i=0; i<len; i++)
  {
    Serial.print(" 0x"); Serial.print((char)buffer[i], HEX); 
  }
  Serial.println(F(" ]"));
}

unsigned int floatToTrimmedString(char* dest, float value){
  char floatBuffer[16];
  unsigned int floatLength, offset;
  
  dtostrf(value, 3, floatBuffer);
  floatLength = strlen(floatBuffer);
  offset = 0;
  for(offset=0; offset < 12; ++offset){
    if(floatBuffer[offset] != 0x20){
      break;
    }
  }
    
  floatLength -= offset;
  strncpy(dest, floatBuffer + offset, floatLength);
  
  return floatLength;
}

unsigned int writeDataLine(char* buffer){
  unsigned int length = 0;
  
  float data[11] = {20, 30, 30, 6, 138, 139, 250, 260, 5, 6, 7};
  for(int i=0; i < 11; ++i){
    // Add Datum
    float datum = data[i] + random(-300, 300)/(float)100;
    length += floatToTrimmedString(buffer + length, datum);
       
    // End with comma
    buffer[length++] = ',';
  }
  length -= 1; // Remove trailing comma from for loop above
  
  buffer[length++] = '\r';
  buffer[length++] = '\n';
  return length;
}

void sendBlank(){
  unsigned int length = 0;
  unsigned int bytesRemaining;
  
  char sendBuffer[128];
  sendBuffer[length++] = 'C';
  
  float blank[5] = { 20, 30, 30, 138, 139 };
  for(unsigned int i = 0; i < 5; ++i){
    sendBuffer[length++] = ',';
    blank[i] += random(-300, 300)/(float)100;
    length += floatToTrimmedString(sendBuffer + length, blank[i]);
  }
  
  sendBuffer[length++] = '\r';
  sendBuffer[length++] = '\n';
  
  //delay(20000 + random(-5000, 5000));
  
  sendBTLEString(sendBuffer, length);
}

void sendData(){
  unsigned int length = 0;
  unsigned int bytesRemaining;
  char dataBuffer[160];
  dataBuffer[length++] = 'D';
  dataBuffer[length++] = ',';
  
  
  length += writeDataLine(dataBuffer + length);
  
  //delay(30000 + random(-5000, 5000));
  
  sendBTLEString(dataBuffer, length);
}

#define RXBUFFERMAX 256
static volatile uint8_t rxBuffer[RXBUFFERMAX];
static volatile uint8_t rxLength = 0;
/**************************************************************************/
/*!
    This function is called whenever data arrives on the RX channel
*/
/**************************************************************************/
void RFduinoBLE_onReceive(char *buffer, int len)
{
  // Fill receive buffer
  if((rxLength + len) > RXBUFFERMAX){
    len = RXBUFFERMAX - (rxLength + len);
  }
  strncpy((char *)(rxBuffer + rxLength), (char*)buffer, len);
  rxLength += len;
  
  for(int i=0; i < rxLength; ++i){
    Serial.write(rxBuffer[i]);
  }
  Serial.println();
  Serial.print("Length: "); Serial.println(rxLength);
  
  // If line end detected, process the data.
  if(rxBuffer[rxLength - 1] == '\n' || rxBuffer[rxLength - 1] == 'n'){
    Serial.print(F("Received: ")); Serial.print((char*) rxBuffer);
    switch(rxBuffer[0]){
      case 'B':
        Serial.println(F("Recognized Blank Read Command"));
        sendBlank();
        break;
      case 'R':
        Serial.println(F("Recognized Sample Read Command"));
        sendData();
        break;
      default:
        Serial.println(F("Unrecognized Command!"));
        break;
    }
    
    rxLength = 0;
    rxBuffer[rxLength] = '\0';
  }
}

void initBTLEUART(){
  RFduinoBLE.deviceName = "pH-1";
  RFduinoBLE.txPowerLevel = -8;
  RFduinoBLE.advertisementInterval = 100;
  RFduinoBLE.begin();
}

/**************************************************************************/
/*!
    Configure the Arduino and start advertising with the radio
*/
/**************************************************************************/
void setup(void)
{ 
  Serial.begin(9600);
  while(!Serial); // Leonardo/Micro should wait for serial init
  Serial.println(F("USF COT pH Photometer BTLE Mock Proto"));
  
  initBTLEUART();
  randomSeed(analogRead(6));
}

void loop()
{
}
