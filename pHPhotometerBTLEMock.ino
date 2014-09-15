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
  unsigned int offset = 0;
  
  // Date
  offset += printTimestampField(buffer, GPS.year, '-');
  offset += printTimestampField(buffer + offset, GPS.month, '-');
  offset += printTimestampField(buffer + offset, GPS.day, 'T');
  
  // Time
  offset += printTimestampField(buffer + offset, GPS.hour, ':');
  offset += printTimestampField(buffer + offset, GPS.minute, ':');
  offset += printTimestampField(buffer + offset, GPS.seconds, '.');
  offset += printTimestampField(buffer + offset, GPS.milliseconds, 'Z');
  
  return offset;
}
  
void sendData(){
  char sendBuffer[128];
  sendBuffer[0] = 'D';
  unsigned int offset = 1;
  
  float data[8] = {20, 30, 6, 138, 139, 250, 260, 270};
  for(int i=0; i < 8; ++i){
    // Prefix with comma
    sendBuffer[offset++] = ',';
    
    // Add Datum
    float datum = data[i] + random(-300, 300)/(float)100;
    dtostrf(datum, 8, 3, sendBuffer + offset);
    offset += 8;
  }
  
  // Write Timestamp'
  sendBuffer[offset++] = ',';
  offset += writeISO8601(sendBuffer+offset);
  
  sendBuffer[offset++] = ',';
  dtostrf(GPS.lon, 8, 3, sendBuffer + offset);
  offset += 8;
  
  sendBuffer[offset++] = ',';
  dtostrf(GPS.lat, 8, 3, sendBuffer + offset);
  offset += 8;
  
  sendBuffer[offset++] = '\r';
  sendBuffer[offset++] = '\n';
  uart.write((uint8_t*)sendBuffer, offset);
}

/**************************************************************************/
/*!
    This function is called whenever data arrives on the RX channel
*/
/**************************************************************************/
void rxCallback(uint8_t *buffer, uint8_t len)
{
  printReceiveInfo(buffer, len);
  switch(buffer[0]){
    case 'R':
      Serial.println(F("Recognized Request Data Command"));
      sendData();
      break;
    case 'S':
      Serial.println(F("Recognized Save Data Command"));
      //verifySave();
      break;
    default:
      Serial.println(F("Unrecognized Command!"));
      break;
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
