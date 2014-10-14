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
#include "Adafruit_BLE_UART.h"

#define ADAFRUITBLE_REQ 34
#define ADAFRUITBLE_RDY 3
#define ADAFRUITBLE_RST 36

Adafruit_BLE_UART uart = Adafruit_BLE_UART(ADAFRUITBLE_REQ, ADAFRUITBLE_RDY, ADAFRUITBLE_RST);

// GPS Log Shield Setup
#include <Adafruit_GPS.h>
#include <SD.h>
#ifdef __AVR__
  #include <SoftwareSerial.h>
  #include <avr/sleep.h>
  SoftwareSerial mySerial(8, 7);
#else
  #define mySerial Serial1
#endif

Adafruit_GPS GPS(&mySerial);

#define CHIPSELECT 10
#define GPSLEDPIN 13

#define SAVEPATH "MOCKLOG.CSV"

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

/**************************************************************************/
/*!
    This function is called whenever select ACI events happen
*/
/**************************************************************************/
void aciCallback(aci_evt_opcode_t event)
{
  switch(event)
  {
    case ACI_EVT_DEVICE_STARTED:
      Serial.println(F("Advertising started"));
      break;
    case ACI_EVT_CONNECTED:
      Serial.println(F("Connected!"));
      break;
    case ACI_EVT_DISCONNECTED:
      Serial.println(F("Disconnected or advertising timed out"));
      break;
    default:
      break;
  }
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
  
  dtostrf(value, 8, 3, floatBuffer);
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

unsigned int printTimestampField(char* buffer, int value, char suffix){
  char intBuffer[16];
  unsigned int length = 0;
  
  itoa(value, intBuffer, 10);
  length = strlen(intBuffer);
  Serial.print("Int String Length: "); Serial.println(length);
  strncpy(buffer, intBuffer, length);
  buffer[length++] = suffix;
  return length;
}

unsigned int writeISO8601(char* buffer){
  unsigned int length = 0;
  
  // Date
  length += printTimestampField(buffer, GPS.year, '-');
  length += printTimestampField(buffer + length, GPS.month, '-');
  length += printTimestampField(buffer + length, GPS.day, 'T');
  
  // Time
  length += printTimestampField(buffer + length, GPS.hour, ':');
  length += printTimestampField(buffer + length, GPS.minute, ':');
  length += printTimestampField(buffer + length, GPS.seconds, '.');
  length += printTimestampField(buffer + length, GPS.milliseconds, 'Z');
  
  return length;
}

unsigned int writeDataLine(char* buffer){
  unsigned int length = 0;
  
  float data[8] = {20, 30, 6, 138, 139, 250, 260, 270};
  for(int i=0; i < 8; ++i){
    // Add Datum
    float datum = data[i] + random(-300, 300)/(float)100;
    length += floatToTrimmedString(buffer + length, datum);
       
    // End with comma
    buffer[length++] = ',';
  }
  
  if(GPS.fix){
    // Write Timestamp
    length += writeISO8601(buffer+length);
    buffer[length++] = ',';
    
    length += floatToTrimmedString(buffer + length, GPS.lon);
    buffer[length++] = ',';
    
    length += floatToTrimmedString(buffer + length, GPS.lat);
  } else {
    length -= 1; // Remove trailing comma from for loop above
  }
  
  buffer[length++] = '\r';
  buffer[length++] = '\n';
  return length;
}

void sendBlank(){
  unsigned int length = 0;
  unsigned int bytesRemaining;
  
  char sendBuffer[128];
  sendBuffer[length++] = 'C';
  
  float blank[5] = { 20, 30, 6, 138, 139 };
  for(unsigned int i = 0; i < 5; ++i){
    sendBuffer[length++] = ',';
    blank[i] += random(-300, 300)/(float)100;
    length += floatToTrimmedString(sendBuffer + length, blank[i]);
  }
  
  sendBuffer[length++] = '\r';
  sendBuffer[length++] = '\n';
  
  for(unsigned int i = 0; i < length; i+=20){
    bytesRemaining = min(length - i, 20);
    uart.write((uint8_t*)sendBuffer + i, bytesRemaining);
  }
}

void sendData(){
  unsigned int length = 0;
  unsigned int bytesRemaining;
  char dataBuffer[160];
  dataBuffer[length++] = 'D';
  dataBuffer[length++] = ',';
  
  
  length += writeDataLine(dataBuffer + length);
  
  for(unsigned int i = 0; i < length; i+=20){
    bytesRemaining = min(length - i, 20);
    uart.write((uint8_t*)dataBuffer + i, bytesRemaining);
  }
}

void verifySave(){
  char dataBuffer[160];
  unsigned int length = 0;
  
  length = writeDataLine(dataBuffer);
  File dataFile = SD.open("DATA.CSV", FILE_WRITE);
  
  uint8_t* response;
  if(dataFile){
    length = writeDataLine(dataBuffer);
    dataFile.write((uint8_t*)dataBuffer, length);
    uart.print("V,1\r\n");
  } else {
    uart.print("V,0\r\n");
  }
  dataFile.close();
}

#define RXBUFFERMAX 256
static volatile uint8_t rxBuffer[RXBUFFERMAX];
static volatile uint8_t rxLength = 0;
/**************************************************************************/
/*!
    This function is called whenever data arrives on the RX channel
*/
/**************************************************************************/
void rxCallback(uint8_t *buffer, uint8_t len)
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
  if(rxBuffer[rxLength - 1] == '\n'){
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
      case 'S':
        Serial.println(F("Recognized Save Data Command"));
        verifySave();
        break;
      default:
        Serial.println(F("Unrecognized Command!"));
        break;
    }
    
    rxLength = 0;
    rxBuffer[rxLength] = '\0';
  }
}

// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
}

void initGPSShield(){
  pinMode(GPSLEDPIN, OUTPUT);
  
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);
  
  // see if the card is present and can be initialized:
  if (!SD.begin(CHIPSELECT, 11, 12, 13)) {
  //if (!SD.begin(chipSelect)) {      // if you're using an UNO, you can use this line instead
    Serial.println("Card init. failed!");
  }
  
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  GPS.sendCommand(PGCMD_NOANTENNA);
  
  // Setup GPS Read Interrupt
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
}

void initBTLEUART(){
  uart.setRXcallback(rxCallback);
  uart.setACIcallback(aciCallback);
  uart.begin();
  uart.setDeviceName("pH-1");
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

  initGPSShield();
  initBTLEUART();
  randomSeed(analogRead(0));
}

void pollGPS(){
  if (GPS.newNMEAreceived()) {
    Serial.println(GPS.lastNMEA());
    GPS.parse(GPS.lastNMEA());
  }
}

uint32_t timer = millis();
void loop()
{
  // Poll BTLE
  uart.pollACI();
  
  // Poll GPS
  pollGPS();
  
  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis())  timer = millis();
}
