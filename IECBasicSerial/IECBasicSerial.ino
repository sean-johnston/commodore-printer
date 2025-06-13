// -----------------------------------------------------------------------------
// Copyright (C) 2023 David Hansel
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include "IECDevice.h"
#include "IECBusHandler.h"

#define DEVICE_NUMBER 4

#if defined(__AVR__) || defined(__SAM3X8E__) || defined(ARDUINO_UNOR4_MINIMA) || defined(ARDUINO_UNOR4_WIFI)

#define PIN_ATN   3
#define PIN_CLK   4
#define PIN_DATA  5
#define PIN_LED   13

#elif defined(ARDUINO_ARCH_RP2040)

#define PIN_ATN   2
#define PIN_CLK   3
#define PIN_DATA  4
#define PIN_LED   25

#elif defined(ARDUINO_WT32_ETH01)

#define PIN_ATN   IO35
#define PIN_CLK   IO14
#define PIN_DATA  IO15
#define PIN_LED   0       // Not sure of the LED pin

#elif defined(ARDUINO_ARCH_ESP32)

#define PIN_ATN   34
#define PIN_CLK   32
#define PIN_DATA  33
#define PIN_LED   5

#endif

#define LED_TIMEOUT 200 // How long the LED is on after data is transmitted

unsigned long led_timeout = 0; // LED timeout value

// -----------------------------------------------------------------------------
// IECBasicSerial class implements a very basic IEC-to-serial converter
// -----------------------------------------------------------------------------


class IECBasicSerial : public IECDevice
{
 public:
  IECBasicSerial(byte devnum) : IECDevice(devnum) {}

  virtual int8_t canRead();
  virtual byte   read();

  virtual int8_t canWrite();
  virtual void   write(byte data, bool eoi);

  virtual void   primary_address(byte primary);
  virtual void   secondary_address(byte secondary);
};


int8_t IECBasicSerial::canWrite()
{
  // Return -1 if we can't receive IEC bus data right now which will cause this
  // to be called again until we are ready and return 1.
  // Alternatively we could just wait within this function until we are ready.
  return Serial.availableForWrite()>0 ? 1 : -1;
}


void IECBasicSerial::write(byte data, bool eoi)
{
  // Turn on LED when data is transmitted, and set the timeout
  if (PIN_LED != 0) { 
    digitalWrite(PIN_LED, HIGH);
    led_timeout = millis() + LED_TIMEOUT;
  }
 
  // write() will only be called if canWrite() returned >0.
  
  Serial.write(data);
  // If it is escape, escape it. Escape is used for the secondary address, -
  if (data == 27) Serial.write(data);
  if (eoi) {
    Serial.write(27);
    Serial.write(0x3f);
  }
}


int8_t IECBasicSerial::canRead()
{
  // Return 0 if we have nothing to send. This will indicate a "nothing to send"
  // (error) condition on the bus. If we returned -1 instead then canRead()
  // would be called repeatedly, blocking the bus, until we have something to send.
  // That would prevent us from receiving incoming data on the bus.
  byte n = Serial.available();
  return n>1 ? 2 : n;
}


byte IECBasicSerial::read()
{ 
  // read() will only be called if canRead() returned >0.
  return Serial.read();
}


void IECBasicSerial::primary_address(byte primary){
}

void IECBasicSerial::secondary_address(byte secondary){
  // If was sent with the output data, send it with an escape
  // 0x60 - Output data
  // 0xE0 - On close
  //if ((secondary & 0xf0) == 0x60) {
    Serial.write(27);
    Serial.write(secondary & 0x0f);
  //}
}

// -----------------------------------------------------------------------------


IECBasicSerial iecSerial(DEVICE_NUMBER);
IECBusHandler  iecBus(PIN_ATN, PIN_CLK, PIN_DATA);

void setup()
{
  // initialize serial communication
  Serial.begin(115200);

  // Set LED pin to output, if it is not 0
  if (PIN_LED != 0) {
    pinMode(PIN_LED, OUTPUT);
  }

  // attach serial device to IEC bus
  iecBus.attachDevice(&iecSerial);

  // initialize IEC bus
  iecBus.begin();
}


void loop()
{
  // Turn off LED for data transmission
  if (led_timeout <= millis()) {
    if (PIN_LED != 0) digitalWrite(PIN_LED, LOW);
    led_timeout = 0;
  }

  // handle IEC bus communication (this will call the read and write functions above)
  iecBus.task();
}
