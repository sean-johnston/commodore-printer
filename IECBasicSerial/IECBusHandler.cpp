// -----------------------------------------------------------------------------
// Copyright (C) 2023 David Hansel
//
// This implementation is based on the code used in the VICE emulator.
// The corresponding code in VICE (src/serial/serial-iec-device.c) was 
// contributed to VICE by me in 2003.
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

#include "IECBusHandler.h"
#include "IECDevice.h"

#if MAX_DEVICES>16
#error "Maximum allowed number of devices is 16"
#endif

//#define JDEBUG

// ---------------- Arduino 8-bit ATMega (UNO R3/Mega/Mini/Micro/Leonardo...)

#if defined(__AVR__)

#if defined(__AVR_ATmega32U4__)
// Atmega32U4 does not have a second 8-bit timer (first one is used by Arduino millis())
// => use lower 8 bit of 16-bit timer 1
#define timer_init()         { TCCR1A=0; TCCR1B=0; }
#define timer_reset()        TCNT1L=0
#define timer_start()        TCCR1B |= bit(CS11)
#define timer_stop()         TCCR1B &= ~bit(CS11)
#define timer_wait_until(us) while( TCNT1L < ((byte) (2*(us))) )
#else
// use 8-bit timer 2 with /8 prescaler
#define timer_init()         { TCCR2A=0; TCCR2B=0; }
#define timer_reset()        TCNT2=0
#define timer_start()        TCCR2B |= bit(CS21)
#define timer_stop()         TCCR2B &= ~bit(CS21)
#define timer_wait_until(us) while( TCNT2 < ((byte) (2*(us))) )
#endif

//NOTE: Must disable SUPPORT_DOLPHIN, otherwise no pins left for debugging (except Mega)
#ifdef JDEBUG
#define JDEBUGI() DDRD |= 0x80; PORTD &= ~0x80 // PD7 = pin digital 7
#define JDEBUG0() PORTD&=~0x80
#define JDEBUG1() PORTD|=0x80
#endif

// ---------------- Arduino Uno R4

#elif defined(ARDUINO_UNOR4_MINIMA) || defined(ARDUINO_UNOR4_WIFI)
#ifndef ARDUINO_UNOR4
#define ARDUINO_UNOR4
#endif

// NOTE: this assumes the AGT timer is running at the (Arduino default) 3MHz rate
//       and rolling over after 3000 ticks 
static unsigned long timer_start_ticks;
static uint16_t timer_ticks_diff(uint16_t t0, uint16_t t1) { return ((t0 < t1) ? 3000 + t0 : t0) - t1; }
#define timer_init()         while(0)
#define timer_reset()        timer_start_ticks = R_AGT0->AGT
#define timer_start()        timer_start_ticks = R_AGT0->AGT
#define timer_stop()         while(0)
#define timer_wait_until(us) while( timer_ticks_diff(timer_start_ticks, R_AGT0->AGT) < ((int) (us*3)) )

#ifdef JDEBUG
#define JDEBUGI() pinMode(1, OUTPUT)
#define JDEBUG0() R_PORT3->PODR &= ~bit(2);
#define JDEBUG1() R_PORT3->PODR |=  bit(2);
#endif

// ---------------- Arduino Due

#elif defined(__SAM3X8E__)

#define portModeRegister(port) 0

static unsigned long timer_start_ticks;
static uint32_t timer_ticks_diff(uint32_t t0, uint32_t t1) { return ((t0 < t1) ? 84000 + t0 : t0) - t1; }
#define timer_init()         while(0)
#define timer_reset()        timer_start_ticks = SysTick->VAL;
#define timer_start()        timer_start_ticks = SysTick->VAL;
#define timer_stop()         while(0)
#define timer_wait_until(us) while( timer_ticks_diff(timer_start_ticks, SysTick->VAL) < ((int) (us*84)) )

#ifdef JDEBUG
#define JDEBUGI() pinMode(2, OUTPUT)
#define JDEBUG0() digitalWriteFast(2, LOW);
#define JDEBUG1() digitalWriteFast(2, HIGH);
#endif

// ---------------- Raspberry Pi Pico

#elif defined(ARDUINO_ARCH_RP2040)

// note: micros() call on MBED core is SLOW - using time_us_32() instead
static unsigned long timer_start_us;
#define timer_init()         while(0)
#define timer_reset()        timer_start_us = time_us_32()
#define timer_start()        timer_start_us = time_us_32()
#define timer_stop()         while(0)
#define timer_wait_until(us) while( (time_us_32()-timer_start_us) < ((int) (us+0.5)) )

#ifdef JDEBUG
#define JDEBUGI() pinMode(20, OUTPUT)
#define JDEBUG0() gpio_put(20, 0)
#define JDEBUG1() gpio_put(20, 1)
#endif

// ---------------- other (32-bit) platforms

#else

static unsigned long timer_start_us;
#define timer_init()         while(0)
#define timer_reset()        timer_start_us = micros()
#define timer_start()        timer_start_us = micros()
#define timer_stop()         while(0)
#define timer_wait_until(us) while( (micros()-timer_start_us) < ((int) (us+0.5)) )

#if defined(JDEBUG) && defined(ARDUINO_ARCH_ESP32)
#define JDEBUGI() pinMode(22, OUTPUT)
#define JDEBUG0() GPIO.out_w1tc = bit(22)
#define JDEBUG1() GPIO.out_w1ts = bit(22)
#endif

#endif

#ifndef JDEBUG
#define JDEBUGI()
#define JDEBUG0()
#define JDEBUG1()
#endif

#if defined(__SAM3X8E__)
// Arduino Due
#define pinModeFastExt(pin, reg, bit, dir)    { if( (dir)==OUTPUT ) digitalPinToPort(pin)->PIO_OER |= bit; else digitalPinToPort(pin)->PIO_ODR |= bit; }
#define digitalReadFastExt(pin, reg, bit)     (*(reg) & (bit))
#define digitalWriteFastExt(pin, reg, bit, v) { if( v ) *(reg)|=(bit); else (*reg)&=~(bit); }
#elif defined(ARDUINO_ARCH_RP2040)
// Raspberry Pi Pico
#define pinModeFastExt(pin, reg, bit, dir)    gpio_set_dir(pin, (dir)==OUTPUT)
#define digitalReadFastExt(pin, reg, bit)     gpio_get(pin)
#define digitalWriteFastExt(pin, reg, bit, v) gpio_put(pin, v)
#elif defined(__AVR__) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_UNOR4)
// Arduino 8-bit (Uno R3/Mega/...) or ESP32
#define pinModeFastExt(pin, reg, bit, dir)    { if( (dir)==OUTPUT ) *(reg)|=(bit); else *(reg)&=~(bit); }
#define digitalReadFastExt(pin, reg, bit)     (*(reg) & (bit))
#define digitalWriteFastExt(pin, reg, bit, v) { if( v ) *(reg)|=(bit); else (*reg)&=~(bit); }
#else
#warning "No fast digital I/O macros defined for this platform - code will likely run too slow"
#define pinModeFastExt(pin, reg, bit, dir)    pinMode(pin, dir)
#define digitalReadFastExt(pin, reg, bit)     digitalRead(pin)
#define digitalWriteFastExt(pin, reg, bit, v) digitalWrite(pin, v)
#endif

// -----------------------------------------------------------------------------------------

#define P_ATN        0x80
#define P_LISTENING  0x40
#define P_TALKING    0x20
#define P_DONE       0x10
#define P_RESET      0x08

#define S_JIFFY_ENABLED          0x0001  // JiffyDos support is enabled
#define S_JIFFY_DETECTED         0x0002  // Detected JiffyDos request from host
#define S_JIFFY_BLOCK            0x0004  // Detected JiffyDos block transfer request from host
#define S_DOLPHIN_ENABLED        0x0008  // DolphinDos support is enabled
#define S_DOLPHIN_DETECTED       0x0010  // Detected DolphinDos request from host
#define S_DOLPHIN_BURST_ENABLED  0x0020  // DolphinDos burst mode is enabled
#define S_DOLPHIN_BURST_TRANSMIT 0x0040  // Detected DolphinDos burst transmit request from host
#define S_DOLPHIN_BURST_RECEIVE  0x0080  // Detected DolphinDos burst receive request from host
#define S_EPYX_ENABLED           0x0100  // Epyx FastLoad support is enabled
#define S_EPYX_HEADER            0x0200  // Read EPYX FastLoad header (drive code transmission)
#define S_EPYX_LOAD              0x0400  // Detected Epyx "load" request
#define S_EPYX_SECTOROP          0x0800  // Detected Epyx "sector operation" request

IECBusHandler *IECBusHandler::s_bushandler1 = NULL, *IECBusHandler::s_bushandler2 = NULL;


void IECBusHandler::writePinCLK(bool v)
{
  // Emulate open collector behavior: 
  // - switch pin to INPUT  mode (high-Z output) for true
  // - switch pun to OUTPUT mode (LOW output) for false
  pinModeFastExt(m_pinCLK, m_regCLKmode, m_bitCLK, v ? INPUT : OUTPUT);
}


void IECBusHandler::writePinDATA(bool v)
{
  // Emulate open collector behavior: 
  // - switch pin to INPUT  mode (high-Z output) for true
  // - switch pun to OUTPUT mode (LOW output) for false
  pinModeFastExt(m_pinDATA, m_regDATAmode, m_bitDATA, v ? INPUT : OUTPUT);
}


void IECBusHandler::writePinCTRL(bool v)
{
  if( m_pinCTRL!=0xFF )
    digitalWrite(m_pinCTRL, v);
}


bool IECBusHandler::readPinATN()
{
  return digitalReadFastExt(m_pinATN, m_regATNread, m_bitATN)!=0;
}


bool IECBusHandler::readPinCLK()
{
  return digitalReadFastExt(m_pinCLK, m_regCLKread, m_bitCLK)!=0;
}


bool IECBusHandler::readPinDATA()
{
  return digitalReadFastExt(m_pinDATA, m_regDATAread, m_bitDATA)!=0;
}


bool IECBusHandler::readPinRESET()
{
  if( m_pinRESET==0xFF ) return true;
  return digitalReadFastExt(m_pinRESET, m_regRESETread, m_bitRESET)!=0;
}


bool IECBusHandler::waitTimeoutFrom(uint32_t start, uint16_t timeout)
{
  while( (micros()-start)<timeout )
    if( (m_flags & P_ATN)==0 && !readPinATN() )
      return false;
  
  return true;
}


bool IECBusHandler::waitTimeout(uint16_t timeout)
{
  return waitTimeoutFrom(micros(), timeout);
}


bool IECBusHandler::waitPinDATA(bool state, uint16_t timeout)
{
  // (if timeout is not given it defaults to 1000us)
  // if ATN changes (i.e. our internal ATN state no longer matches the ATN signal line)
  // or the timeout is met then exit with error condition
  if( timeout==0 )
    {
      // timeout is 0 (no timeout), do NOT call micros() as calling micros() may enable
      // interrupts on some platforms
      while( readPinDATA()!=state )
        if( ((m_flags & P_ATN)!=0) == readPinATN() )
          return false;
    }
  else
    {
      uint32_t start = micros();
      while( readPinDATA()!=state )
        if( (((m_flags & P_ATN)!=0) == readPinATN()) || (uint16_t) (micros()-start)>=timeout )
          return false;
    }

  // DATA LOW can only be properly detected if ATN went HIGH->LOW
  // (m_flags&ATN)==0 and readPinATN()==0)
  // since other devices may have pulled DATA LOW
  return state || (m_flags & P_ATN) || readPinATN();
}


bool IECBusHandler::waitPinCLK(bool state, uint16_t timeout)
{
  // (if timeout is not given it defaults to 1000us)
  // if ATN changes (i.e. our internal ATN state no longer matches the ATN signal line)
  // or the timeout is met then exit with error condition
  if( timeout==0 )
    {
      // timeout is 0 (no timeout), do NOT call micros() as calling micros() may enable
      // interrupts on some platforms
      while( readPinCLK()!=state )
        if( ((m_flags & P_ATN)!=0) == readPinATN() )
          return false;
    }
  else
    {
      uint32_t start = micros();
      while( readPinCLK()!=state )
        if( (((m_flags & P_ATN)!=0) == readPinATN()) || (uint16_t) (micros()-start)>=timeout )
          return false;
    }
  
  return true;
}


IECBusHandler::IECBusHandler(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET, byte pinCTRL) :
#if defined(SUPPORT_DOLPHIN)
#if defined(ARDUINO_ARCH_ESP32)
  // ESP32
  m_pinDolphinHandshakeTransmit(4),
  m_pinDolphinHandshakeReceive(36),
  m_pinDolphinParallel{13,14,15,16,17,25,26,27},
#elif defined(ARDUINO_ARCH_RP2040)
  // Raspberry Pi Pico
  m_pinDolphinHandshakeTransmit(6),
  m_pinDolphinHandshakeReceive(15),
  m_pinDolphinParallel{7,8,9,10,11,12,13,14},
#elif defined(__SAM3X8E__)
  // Arduino Due
  m_pinDolphinHandshakeTransmit(52),
  m_pinDolphinHandshakeReceive(53),
  m_pinDolphinParallel{51,50,49,48,47,46,45,44},
#elif defined(__AVR_ATmega328P__) || defined(ARDUINO_UNOR4)
  // Arduino UNO, Pro Mini
  m_pinDolphinHandshakeTransmit(7),
  m_pinDolphinHandshakeReceive(2),
  m_pinDolphinParallel{A0,A1,A2,A3,A4,A5,8,9},
#elif defined(__AVR_ATmega2560__)
  // Arduino Mega 2560
  m_pinDolphinHandshakeTransmit(30),
  m_pinDolphinHandshakeReceive(2),
  m_pinDolphinParallel{22,23,24,25,26,27,28,29},
#endif
#endif
#if defined(SUPPORT_JIFFY) || defined(SUPPORT_EPYX) || defined(SUPPORT_DOLPHIN)
#if IEC_DEFAULT_FASTLOAD_BUFFER_SIZE>0
  m_bufferSize(IEC_DEFAULT_FASTLOAD_BUFFER_SIZE),
#else
  m_buffer(NULL),
  m_bufferSize(0),
#endif
#endif
  m_numDevices(0),
  m_flags(0xFF), // 0xFF means: begin() has not yet been called
  m_inTask(false)
{
  m_pinATN       = pinATN;
  m_pinCLK       = pinCLK;
  m_pinDATA      = pinDATA;
  m_pinRESET     = pinRESET;
  m_pinCTRL      = pinCTRL;

#ifdef IOREG_TYPE
  m_bitRESET     = digitalPinToBitMask(pinRESET);
  m_regRESETread = portInputRegister(digitalPinToPort(pinRESET));
  m_bitATN       = digitalPinToBitMask(pinATN);
  m_regATNread   = portInputRegister(digitalPinToPort(pinATN));
  m_bitCLK       = digitalPinToBitMask(pinCLK);
  m_regCLKread   = portInputRegister(digitalPinToPort(pinCLK));
  m_regCLKwrite  = portOutputRegister(digitalPinToPort(pinCLK));
  m_regCLKmode   = portModeRegister(digitalPinToPort(pinCLK));
  m_bitDATA      = digitalPinToBitMask(pinDATA);
  m_regDATAread  = portInputRegister(digitalPinToPort(pinDATA));
  m_regDATAwrite = portOutputRegister(digitalPinToPort(pinDATA));
  m_regDATAmode  = portModeRegister(digitalPinToPort(pinDATA));
#endif

  m_atnInterrupt = digitalPinToInterrupt(m_pinATN);
}


void IECBusHandler::begin()
{
  JDEBUGI();

  // set pins to output 0 (when in output mode)
  pinMode(m_pinCLK,  OUTPUT); digitalWrite(m_pinCLK, LOW); 
  pinMode(m_pinDATA, OUTPUT); digitalWrite(m_pinDATA, LOW); 

  pinMode(m_pinATN,   INPUT);
  pinMode(m_pinCLK,   INPUT);
  pinMode(m_pinDATA,  INPUT);
  if( m_pinCTRL<0xFF )  pinMode(m_pinCTRL,  OUTPUT);
  if( m_pinRESET<0xFF ) pinMode(m_pinRESET, INPUT);
  m_flags = 0;

  // allow ATN to pull DATA low in hardware
  writePinCTRL(LOW);

  // if the ATN pin is capable of interrupts then use interrupts to detect 
  // ATN requests, otherwise we'll poll the ATN pin in function microTask().
  if( m_atnInterrupt!=NOT_AN_INTERRUPT )
    {
      if( s_bushandler1==NULL )
        {
          s_bushandler1 = this;
          attachInterrupt(m_atnInterrupt, atnInterruptFcn1, FALLING);
        }
      else if( s_bushandler2==NULL )
        {
          s_bushandler2 = this;
          attachInterrupt(m_atnInterrupt, atnInterruptFcn2, FALLING);
        }
    }

  // call begin() function for all attached devices
  for(byte i=0; i<m_numDevices; i++)
    m_devices[i]->begin();
}


bool IECBusHandler::canServeATN() 
{ 
  return (m_pinCTRL!=0xFF) || (m_atnInterrupt != NOT_AN_INTERRUPT); 
}


bool IECBusHandler::attachDevice(IECDevice *dev)
{
  if( m_numDevices<MAX_DEVICES && findDevice(dev->m_devnr)==NULL )
    {
      m_devices[m_numDevices++] = dev;
      dev->m_handler = this;
      dev->m_sflags &= ~(S_JIFFY_DETECTED|S_JIFFY_BLOCK|S_DOLPHIN_DETECTED|S_DOLPHIN_BURST_TRANSMIT|S_DOLPHIN_BURST_RECEIVE|S_EPYX_HEADER|S_EPYX_LOAD|S_EPYX_SECTOROP);
#ifdef SUPPORT_DOLPHIN
      enableParallelPins();
#endif

      // if IECBusHandler::begin() has been called already then call the device's
      // begin() function now, otherwise it will be called in IECBusHandler::begin() 
      if( m_flags!=0xFF ) dev->begin();

      return true;
    }
  else
    return false;
}


bool IECBusHandler::detachDevice(IECDevice *dev)
{
  for(byte i=0; i<m_numDevices; i++)
    if( dev == m_devices[i] )
      {
        dev->m_handler = NULL;
        m_devices[i] = m_devices[m_numDevices-1];
        m_numDevices--;
#ifdef SUPPORT_DOLPHIN
        enableParallelPins();
#endif
        return true;
      }

  return false;
}


IECDevice *IECBusHandler::findDevice(byte devnr)
{
  for(byte i=0; i<m_numDevices; i++)
    if( devnr == m_devices[i]->m_devnr )
      return m_devices[i];

  return NULL;
}


void IECBusHandler::atnInterruptFcn1()
{ 
  if( s_bushandler1!=NULL && !s_bushandler1->m_inTask & ((s_bushandler1->m_flags & P_ATN)==0) )
    s_bushandler1->atnRequest();
}


void IECBusHandler::atnInterruptFcn2()
{ 
  if( s_bushandler2!=NULL && !s_bushandler2->m_inTask & ((s_bushandler2->m_flags & P_ATN)==0) )
    s_bushandler2->atnRequest();
}


#if (defined(SUPPORT_JIFFY) || defined(SUPPORT_DOLPHIN) || defined(SUPPORT_EPYX)) && !defined(IEC_DEFAULT_FASTLOAD_BUFFER_SIZE)
void IECBusHandler::setBuffer(byte *buffer, byte bufferSize)
{
  m_buffer     = buffer;
  m_bufferSize = bufferSize;
}
#endif

#ifdef SUPPORT_JIFFY

// ------------------------------------  JiffyDos support routines  ------------------------------------  


bool IECBusHandler::enableJiffyDosSupport(IECDevice *dev, bool enable)
{
  if( enable && m_buffer!=NULL && m_bufferSize>0 )
    dev->m_sflags |= S_JIFFY_ENABLED;
  else
    dev->m_sflags &= ~S_JIFFY_ENABLED;

  // cancel any current JiffyDos activities
  dev->m_sflags &= ~(S_JIFFY_DETECTED|S_JIFFY_BLOCK);

  return (dev->m_sflags & S_JIFFY_ENABLED)!=0;
}


bool IECBusHandler::receiveJiffyByte(bool canWriteOk)
{
  byte data = 0;
  JDEBUG1();
  timer_init();
  timer_reset();

  noInterrupts(); 

  // signal "ready" by releasing DATA
  writePinDATA(HIGH);

  // wait (indefinitely) for either CLK high ("ready-to-send") or ATN low
  // NOTE: this must be in a blocking loop since the sender starts transmitting
  // the byte immediately after setting CLK high. If we exit the "task" function then
  // we may not get back here in time to receive.
  while( !digitalReadFastExt(m_pinCLK, m_regCLKread, m_bitCLK) && digitalReadFastExt(m_pinATN, m_regATNread, m_bitATN) );

  // start timer (on AVR, lag from CLK high to timer start is between 700...1700ns)
  timer_start();
  JDEBUG0();

  // abort if ATN low
  if( !readPinATN() )
    { interrupts(); return false; }

  // bits 4+5 are set by sender 11 cycles after CLK HIGH (FC51)
  // wait until 14us after CLK
  timer_wait_until(14);
  
  JDEBUG1();
  if( !readPinCLK()  ) data |= bit(4);
  if( !readPinDATA() ) data |= bit(5);
  JDEBUG0();

  // bits 6+7 are set by sender 24 cycles after CLK HIGH (FC5A)
  // wait until 27us after CLK
  timer_wait_until(27);
  
  JDEBUG1();
  if( !readPinCLK()  ) data |= bit(6);
  if( !readPinDATA() ) data |= bit(7);
  JDEBUG0();

  // bits 3+1 are set by sender 35 cycles after CLK HIGH (FC62)
  // wait until 38us after CLK
  timer_wait_until(38);

  JDEBUG1();
  if( !readPinCLK()  ) data |= bit(3);
  if( !readPinDATA() ) data |= bit(1);
  JDEBUG0();

  // bits 2+0 are set by sender 48 cycles after CLK HIGH (FC6B)
  // wait until 51us after CLK
  timer_wait_until(51);

  JDEBUG1();
  if( !readPinCLK()  ) data |= bit(2);
  if( !readPinDATA() ) data |= bit(0);
  JDEBUG0();

  // sender sets EOI status 61 cycles after CLK HIGH (FC76)
  // wait until 64us after CLK
  timer_wait_until(64);

  // if CLK is high at this point then the sender is signaling EOI
  JDEBUG1();
  bool eoi = readPinCLK();

  // acknowledge receipt
  writePinDATA(LOW);

  // sender reads acknowledgement 80 cycles after CLK HIGH (FC82)
  // wait until 83us after CLK
  timer_wait_until(83);

  JDEBUG0();

  interrupts();

  if( canWriteOk )
    {
      // pass received data on to the device
      m_currentDevice->write(data, eoi);
    }
  else
    {
      // canWrite() reported an error
      return false;
    }
  
  return true;
}


bool IECBusHandler::transmitJiffyByte(byte numData)
{
  byte data = numData>0 ? m_currentDevice->peek() : 0;

  JDEBUG1();
  timer_init();
  timer_reset();

  noInterrupts();

  // signal "READY" by releasing CLK
  writePinCLK(HIGH);
  
  // wait (indefinitely) for either DATA high ("ready-to-receive", FBCB) or ATN low
  // NOTE: this must be in a blocking loop since the receiver receives the data
  // immediately after setting DATA high. If we exit the "task" function then
  // we may not get back here in time to transmit.
  while( !digitalReadFastExt(m_pinDATA, m_regDATAread, m_bitDATA) && digitalReadFastExt(m_pinATN, m_regATNread, m_bitATN) );

  // start timer (on AVR, lag from DATA high to timer start is between 700...1700ns)
  timer_start();
  JDEBUG0();

  // abort if ATN low
  if( !readPinATN() )
    { interrupts(); return false; }

  writePinCLK(data & bit(0));
  writePinDATA(data & bit(1));
  JDEBUG1();
  // bits 0+1 are read by receiver 16 cycles after DATA HIGH (FBD5)

  // wait until 16.5 us after DATA
  timer_wait_until(16.5);
  
  JDEBUG0();
  writePinCLK(data & bit(2));
  writePinDATA(data & bit(3));
  JDEBUG1();
  // bits 2+3 are read by receiver 26 cycles after DATA HIGH (FBDB)

  // wait until 27.5 us after DATA
  timer_wait_until(27.5);

  JDEBUG0();
  writePinCLK(data & bit(4));
  writePinDATA(data & bit(5));
  JDEBUG1();
  // bits 4+5 are read by receiver 37 cycles after DATA HIGH (FBE2)

  // wait until 39 us after DATA
  timer_wait_until(39);

  JDEBUG0();
  writePinCLK(data & bit(6));
  writePinDATA(data & bit(7));
  JDEBUG1();
  // bits 6+7 are read by receiver 48 cycles after DATA HIGH (FBE9)

  // wait until 50 us after DATA
  timer_wait_until(50);
  JDEBUG0();
      
  // numData:
  //   0: no data was available to read (error condition, discard this byte)
  //   1: this was the last byte of data
  //  >1: more data is available after this
  if( numData>1 )
    {
      // CLK=LOW  and DATA=HIGH means "at least one more byte"
      writePinCLK(LOW);
      writePinDATA(HIGH);
    }
  else
    {
      // CLK=HIGH and DATA=LOW  means EOI (this was the last byte)
      // CLK=HIGH and DATA=HIGH means "error"
      writePinCLK(HIGH);
      writePinDATA(numData==0);
    }

  // EOI/error status is read by receiver 59 cycles after DATA HIGH (FBEF)
  interrupts();

  // make sure the DATA line has had time to settle on HIGH
  // receiver sets DATA low 63 cycles after initial DATA HIGH (FBF2)
  timer_wait_until(60);

  // receiver signals "done" by pulling DATA low (FBF2)
  JDEBUG1();
  if( !waitPinDATA(LOW) ) return false;
  JDEBUG0();

  if( numData>0 )
    {
      // success => discard transmitted byte (was previously read via peek())
      m_currentDevice->read();
      return true;
    }
  else
    return false;
}


bool IECBusHandler::transmitJiffyBlock(byte *buffer, byte numBytes)
{
  JDEBUG1();
  timer_init();

  // wait (indefinitely) until receiver is not holding DATA low anymore (FB07)
  // NOTE: this must be in a blocking loop since the receiver starts counting
  // up the EOI timeout immediately after setting DATA HIGH. If we had exited the 
  // "task" function then it might be more than 200us until we get back here
  // to pull CLK low and the receiver will interpret that delay as EOI.
  while( !readPinDATA() )
    if( !readPinATN() )
      { JDEBUG0(); return false; }

  // receiver will be in "new data block" state at this point,
  // waiting for us (FB0C) to release CLK
  if( numBytes==0 )
    {
      // nothing to send => signal EOI by keeping DATA high
      // and pulsing CLK high-low
      writePinDATA(HIGH);
      writePinCLK(HIGH);
      if( !waitTimeout(100) ) return false;
      writePinCLK(LOW);
      if( !waitTimeout(100) ) return false;
      JDEBUG0(); 
      return false;
    }

  // signal "ready to send" by pulling DATA low and releasing CLK
  writePinDATA(LOW);
  writePinCLK(HIGH);

  // delay to make sure receiver has seen DATA=LOW - even though receiver 
  // is in a tight loop (at FB0C), a VIC "bad line" may steal 40us.
  if( !waitTimeout(50) ) return false;

  noInterrupts();

  for(byte i=0; i<numBytes; i++)
    {
      byte data = buffer[i];

      // release DATA
      writePinDATA(HIGH);

      // stop and reset timer
      timer_stop();
      timer_reset();

      // signal READY by releasing CLK
      writePinCLK(HIGH);

      // make sure DATA has settled on HIGH
      // (receiver takes at least 19 cycles between seeing DATA HIGH [at FB3E] and setting DATA LOW [at FB51]
      // so waiting a couple microseconds will not hurt transfer performance)
      delayMicroseconds(2);

      // wait (indefinitely) for either DATA low (FB51) or ATN low
      // NOTE: this must be in a blocking loop since the receiver receives the data
      // immediately after setting DATA high. If we exit the "task" function then
      // we may not get back here in time to transmit.
      while( digitalReadFastExt(m_pinDATA, m_regDATAread, m_bitDATA) && digitalReadFastExt(m_pinATN, m_regATNread, m_bitATN) );

      // start timer (on AVR, lag from DATA low to timer start is between 700...1700ns)
      timer_start();
      JDEBUG0();
      
      // abort if ATN low
      if( !readPinATN() )
        { interrupts(); return false; }

      // receiver expects to see CLK high at 4 cycles after DATA LOW (FB54)
      // wait until 6 us after DATA LOW
      timer_wait_until(6);

      JDEBUG0();
      writePinCLK(data & bit(0));
      writePinDATA(data & bit(1));
      JDEBUG1();
      // bits 0+1 are read by receiver 16 cycles after DATA LOW (FB5D)

      // wait until 17 us after DATA LOW
      timer_wait_until(17);
  
      JDEBUG0();
      writePinCLK(data & bit(2));
      writePinDATA(data & bit(3));
      JDEBUG1();
      // bits 2+3 are read by receiver 26 cycles after DATA LOW (FB63)

      // wait until 27 us after DATA LOW
      timer_wait_until(27);

      JDEBUG0();
      writePinCLK(data & bit(4));
      writePinDATA(data & bit(5));
      JDEBUG1();
      // bits 4+5 are read by receiver 37 cycles after DATA LOW (FB6A)

      // wait until 39 us after DATA LOW
      timer_wait_until(39);

      JDEBUG0();
      writePinCLK(data & bit(6));
      writePinDATA(data & bit(7));
      JDEBUG1();
      // bits 6+7 are read by receiver 48 cycles after DATA LOW (FB71)

      // wait until 50 us after DATA LOW
      timer_wait_until(50);
    }

  // signal "not ready" by pulling CLK LOW
  writePinCLK(LOW);

  // release DATA
  writePinDATA(HIGH);

  interrupts();

  JDEBUG0();

  return true;
}

#endif // !SUPPORT_JIFFY


#ifdef SUPPORT_DOLPHIN

// ------------------------------------  DolphinDos support routines  ------------------------------------  

#define DOLPHIN_PREBUFFER_BYTES 2

#if !defined(__AVR_ATmega328P__) && !defined(__AVR_ATmega2560__)
volatile static bool _handshakeReceived = false;
static void handshakeIRQ() { _handshakeReceived = true; }
#endif


void IECBusHandler::setDolphinDosPins(byte pinHT, byte pinHR,byte pinD0, byte pinD1, byte pinD2, byte pinD3, byte pinD4, byte pinD5, byte pinD6, byte pinD7)
{
  m_pinDolphinHandshakeTransmit = pinHT;
  m_pinDolphinHandshakeReceive  = pinHR;
  m_pinDolphinParallel[0] = pinD0;
  m_pinDolphinParallel[1] = pinD1;
  m_pinDolphinParallel[2] = pinD2;
  m_pinDolphinParallel[3] = pinD3;
  m_pinDolphinParallel[4] = pinD4;
  m_pinDolphinParallel[5] = pinD5;
  m_pinDolphinParallel[6] = pinD6;
  m_pinDolphinParallel[7] = pinD7;
}


bool IECBusHandler::enableDolphinDosSupport(IECDevice *dev, bool enable)
{
  if( enable && m_buffer!=NULL && m_bufferSize>=DOLPHIN_PREBUFFER_BYTES && 
      !isDolphinPin(m_pinATN)   && !isDolphinPin(m_pinCLK) && !isDolphinPin(m_pinDATA) && 
      !isDolphinPin(m_pinRESET) && !isDolphinPin(m_pinCTRL) && 
      m_pinDolphinHandshakeTransmit!=0xFF && m_pinDolphinHandshakeReceive!=0xFF && 
      digitalPinToInterrupt(m_pinDolphinHandshakeReceive)!=NOT_AN_INTERRUPT )
    {
      dev->m_sflags |= S_DOLPHIN_ENABLED|S_DOLPHIN_BURST_ENABLED;
    }
  else
    dev->m_sflags &= ~(S_DOLPHIN_ENABLED|S_DOLPHIN_BURST_ENABLED);

  // cancel any current DolphinDos activities
  dev->m_sflags &= ~(S_DOLPHIN_DETECTED|S_DOLPHIN_BURST_TRANSMIT|S_DOLPHIN_BURST_RECEIVE);

  // make sure pins for parallel cable are enabled/disabled as needed
  enableParallelPins();

  return (dev->m_sflags & S_DOLPHIN_ENABLED)!=0;
}


bool IECBusHandler::isDolphinPin(byte pin)
{
  if( pin==m_pinDolphinHandshakeTransmit || pin==m_pinDolphinHandshakeReceive )
    return true;

  for(int i=0; i<8; i++) 
    if( pin==m_pinDolphinParallel[i] )
      return true;

  return false;
}


void IECBusHandler::enableParallelPins()
{
  byte i = 0;
  for(i=0; i<m_numDevices; i++)
    if( m_devices[i]->m_sflags & S_DOLPHIN_ENABLED )
      break;

  if( i<m_numDevices )
    {
      // at least one device has DolphinDos support enabled

#if defined(IOREG_TYPE)
      m_regDolphinHandshakeTransmitMode = portModeRegister(digitalPinToPort(m_pinDolphinHandshakeTransmit));
      m_bitDolphinHandshakeTransmit     = digitalPinToBitMask(m_pinDolphinHandshakeTransmit);
      for(byte i=0; i<8; i++)
        {
          m_regDolphinParallelWrite[i] = portOutputRegister(digitalPinToPort(m_pinDolphinParallel[i]));
          m_regDolphinParallelRead[i]  = portInputRegister(digitalPinToPort(m_pinDolphinParallel[i]));
          m_regDolphinParallelMode[i]  = portModeRegister(digitalPinToPort(m_pinDolphinParallel[i]));
          m_bitDolphinParallel[i]      = digitalPinToBitMask(m_pinDolphinParallel[i]);
        }
#endif
      // initialize handshake transmit (high-Z)
      pinMode(m_pinDolphinHandshakeTransmit, OUTPUT);
      digitalWrite(m_pinDolphinHandshakeTransmit, LOW);
      pinModeFastExt(m_pinDolphinHandshakeTransmit, m_regDolphinHandshakeTransmitMode, m_bitDolphinHandshakeTransmit, INPUT);
      
      // initialize handshake receive
      pinMode(m_pinDolphinHandshakeReceive, INPUT); 

      // For 8-bit AVR platforms (Arduino Uno R3, Arduino Mega) the interrupt latency combined
      // with the comparatively slow clock speed leads to reduced performance during load/save
      // For those platforms we do not use the generic interrupt mechanism but instead directly 
      // access the registers dealing with external interrupts.
      // All other platforms are fast enough so we can use the interrupt mechanism without
      // performance issues.
#if defined(__AVR_ATmega328P__)
      // 
      if( m_pinDolphinHandshakeReceive==2 )
        {
          EIMSK &= ~bit(INT0);  // disable pin change interrupt
          EICRA &= ~bit(ISC00); EICRA |=  bit(ISC01); // enable falling edge detection
          m_handshakeReceivedBit = bit(INTF0);
        }
      else if( m_pinDolphinHandshakeReceive==3 )
        {
          EIMSK &= ~bit(INT1);  // disable pin change interrupt
          EICRA &= ~bit(ISC10); EICRA |=  bit(ISC11); // enable falling edge detection
          m_handshakeReceivedBit = bit(INTF1);
        }
#elif defined(__AVR_ATmega2560__)
      if( m_pinDolphinHandshakeReceive==2 )
        {
          EIMSK &= ~bit(INT4); // disable interrupt
          EICRB &= ~bit(ISC40); EICRB |=  bit(ISC41);  // enable falling edge detection
          m_handshakeReceivedBit = bit(INTF4);
        }
      else if( m_pinDolphinHandshakeReceive==3 )
        {
          EIMSK &= ~bit(INT5); // disable interrupt
          EICRB &= ~bit(ISC50); EICRB |=  bit(ISC51);  // enable falling edge detection
          m_handshakeReceivedBit = bit(INTF5);
        }
      else if( m_pinDolphinHandshakeReceive==18 )
        {
          EIMSK &= ~bit(INT3); // disable interrupt
          EICRA &= ~bit(ISC30); EICRA |=  bit(ISC31);  // enable falling edge detection
          m_handshakeReceivedBit = bit(INTF3);
        }
      else if( m_pinDolphinHandshakeReceive==19 )
        {
          EIMSK &= ~bit(INT2); // disable interrupt
          EICRA &= ~bit(ISC20); EICRA |=  bit(ISC21);  // enable falling edge detection
          m_handshakeReceivedBit = bit(INTF2);
        }
      else if( m_pinDolphinHandshakeReceive==20 )
        {
          EIMSK &= ~bit(INT1); // disable interrupt
          EICRA &= ~bit(ISC10); EICRA |=  bit(ISC11);  // enable falling edge detection
          m_handshakeReceivedBit = bit(INTF1);
        }
      else if( m_pinDolphinHandshakeReceive==21 )
        {
          EIMSK &= ~bit(INT0); // disable interrupt
          EICRA &= ~bit(ISC00); EICRA |=  bit(ISC01);  // enable falling edge detection
          m_handshakeReceivedBit = bit(INTF0);
        }
#else
      attachInterrupt(digitalPinToInterrupt(m_pinDolphinHandshakeReceive), handshakeIRQ, FALLING);
#endif

      // initialize parallel bus pins
      for(int i=0; i<8; i++) pinMode(m_pinDolphinParallel[i], OUTPUT);
      // switch parallel bus to input
      setParallelBusModeInput();
    }
  else
    {
      detachInterrupt(digitalPinToInterrupt(m_pinDolphinHandshakeReceive));
    }
}


bool IECBusHandler::parallelBusHandshakeReceived()
{
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega2560__)
  // see comment in function enableDolphinDosSupport
  if( EIFR & m_handshakeReceivedBit )
    {
      EIFR |= m_handshakeReceivedBit;
      return true;
    }
  else
    return false;
#else
  if( _handshakeReceived )
    {
      _handshakeReceived = false;
      return true;
    }
  else
    return false;
#endif
}


void IECBusHandler::parallelBusHandshakeTransmit()
{
  // Emulate open collector behavior: 
  // - switch pin to INPUT  mode (high-Z output) for true
  // - switch pun to OUTPUT mode (LOW output) for false
  pinModeFastExt(m_pinDolphinHandshakeTransmit, m_regDolphinHandshakeTransmitMode, m_bitDolphinHandshakeTransmit, OUTPUT);
  delayMicroseconds(2);
  pinModeFastExt(m_pinDolphinHandshakeTransmit, m_regDolphinHandshakeTransmitMode, m_bitDolphinHandshakeTransmit, INPUT);
}


byte IECBusHandler::readParallelData()
{
  byte res = 0;
  // loop unrolled for performance
  if( digitalReadFastExt(m_pinDolphinParallel[0], m_regDolphinParallelRead[0], m_bitDolphinParallel[0]) ) res |= 0x01;
  if( digitalReadFastExt(m_pinDolphinParallel[1], m_regDolphinParallelRead[1], m_bitDolphinParallel[1]) ) res |= 0x02;
  if( digitalReadFastExt(m_pinDolphinParallel[2], m_regDolphinParallelRead[2], m_bitDolphinParallel[2]) ) res |= 0x04;
  if( digitalReadFastExt(m_pinDolphinParallel[3], m_regDolphinParallelRead[3], m_bitDolphinParallel[3]) ) res |= 0x08;
  if( digitalReadFastExt(m_pinDolphinParallel[4], m_regDolphinParallelRead[4], m_bitDolphinParallel[4]) ) res |= 0x10;
  if( digitalReadFastExt(m_pinDolphinParallel[5], m_regDolphinParallelRead[5], m_bitDolphinParallel[5]) ) res |= 0x20;
  if( digitalReadFastExt(m_pinDolphinParallel[6], m_regDolphinParallelRead[6], m_bitDolphinParallel[6]) ) res |= 0x40;
  if( digitalReadFastExt(m_pinDolphinParallel[7], m_regDolphinParallelRead[7], m_bitDolphinParallel[7]) ) res |= 0x80;
  return res;
}


void IECBusHandler::writeParallelData(byte data)
{
  // loop unrolled for performance
  digitalWriteFastExt(m_pinDolphinParallel[0], m_regDolphinParallelWrite[0], m_bitDolphinParallel[0], data & 0x01);
  digitalWriteFastExt(m_pinDolphinParallel[1], m_regDolphinParallelWrite[1], m_bitDolphinParallel[1], data & 0x02);
  digitalWriteFastExt(m_pinDolphinParallel[2], m_regDolphinParallelWrite[2], m_bitDolphinParallel[2], data & 0x04);
  digitalWriteFastExt(m_pinDolphinParallel[3], m_regDolphinParallelWrite[3], m_bitDolphinParallel[3], data & 0x08);
  digitalWriteFastExt(m_pinDolphinParallel[4], m_regDolphinParallelWrite[4], m_bitDolphinParallel[4], data & 0x10);
  digitalWriteFastExt(m_pinDolphinParallel[5], m_regDolphinParallelWrite[5], m_bitDolphinParallel[5], data & 0x20);
  digitalWriteFastExt(m_pinDolphinParallel[6], m_regDolphinParallelWrite[6], m_bitDolphinParallel[6], data & 0x40);
  digitalWriteFastExt(m_pinDolphinParallel[7], m_regDolphinParallelWrite[7], m_bitDolphinParallel[7], data & 0x80);
}


void IECBusHandler::setParallelBusModeInput()
{
  // set parallel bus data pins to input mode
  for(int i=0; i<8; i++) 
    pinModeFastExt(m_pinDolphinParallel[i], m_regDolphinParallelMode[i], m_bitDolphinParallel[i], INPUT);
}


void IECBusHandler::setParallelBusModeOutput()
{
  // set parallel bus data pins to output mode
  for(int i=0; i<8; i++) 
    pinModeFastExt(m_pinDolphinParallel[i], m_regDolphinParallelMode[i], m_bitDolphinParallel[i], OUTPUT);
}


bool IECBusHandler::waitParallelBusHandshakeReceived()
{
  while( !parallelBusHandshakeReceived() )
    if( !readPinATN() )
      return false;

  return true;
}


void IECBusHandler::enableDolphinBurstMode(IECDevice *dev, bool enable)
{
  if( enable )
    dev->m_sflags |= S_DOLPHIN_BURST_ENABLED;
  else
    dev->m_sflags &= ~S_DOLPHIN_BURST_ENABLED;

  dev->m_sflags &= ~(S_DOLPHIN_BURST_TRANSMIT|S_DOLPHIN_BURST_RECEIVE);
}

void IECBusHandler::dolphinBurstReceiveRequest(IECDevice *dev)
{
  dev->m_sflags |= S_DOLPHIN_BURST_RECEIVE;
  m_timeoutStart = micros();
}

void IECBusHandler::dolphinBurstTransmitRequest(IECDevice *dev)
{
  dev->m_sflags |= S_DOLPHIN_BURST_TRANSMIT;
  m_timeoutStart = micros();
}

bool IECBusHandler::receiveDolphinByte(bool canWriteOk)
{
  // NOTE: we only get here if sender has already signaled ready-to-send
  // by releasing CLK
  bool eoi = false;

  // we have buffered bytes (see comment below) that need to be
  // sent on to the higher level handler before we can receive more.
  // There are two ways to get to m_dolphinCtr==2:
  // 1) the host never sends a XZ burst request and just keeps sending data
  // 2) the host sends a burst request but we reject it
  // note that we must wait for the host to be ready to send the next data 
  // byte before we can empty our buffer, otherwise we will already empty
  // it before the host sends the burst (XZ) request
  if( m_secondary==0x61 && m_dolphinCtr > 0 && m_dolphinCtr <= DOLPHIN_PREBUFFER_BYTES )
    {
      // send next buffered byte on to higher level
      m_currentDevice->write(m_buffer[m_dolphinCtr-1], false);
      m_dolphinCtr--;
      return true;
    }

  // signal "ready"
  writePinDATA(HIGH);

  // wait for CLK low
  if( !waitPinCLK(LOW, 100) )
    {
      // exit if waitPinCLK returned because of falling edge on ATN
      if( !readPinATN() ) return false;

      // sender did not set CLK low within 100us after we set DATA high
      // => it is signaling EOI
      // acknowledge we received it by setting DATA low for 60us
      eoi = true;
      writePinDATA(LOW);
      if( !waitTimeout(60) ) return false;
      writePinDATA(HIGH);

      // keep waiting for CLK low
      if( !waitPinCLK(LOW) ) return false;
    }

  // get data
  if( canWriteOk )
    {
      // read data from parallel bus
      byte data = readParallelData();

      // confirm receipt
      writePinDATA(LOW);

      // when executing a SAVE command, DolphinDos first sends two bytes of data,
      // and then the "XZ" burst request. If the transmission happens in burst mode then
      // that data is going to be sent again and the initial data is discarded.
      // (MultiDubTwo actually sends garbage bytes for the initial two bytes)
      // so we can't pass the first two bytes on yet because we don't yet know if this is
      // going to be a burst transmission. If it is NOT a burst then we need to send them
      // later (see beginning of this function). If it is a burst then we discard them.
      // Note that the SAVE command always operates on channel 1 (secondary address 0x61)
      // so we only do the buffering in that case. 
      if( m_secondary==0x61 && m_dolphinCtr > DOLPHIN_PREBUFFER_BYTES )
        {
          m_buffer[m_dolphinCtr-DOLPHIN_PREBUFFER_BYTES-1] = data;
          m_dolphinCtr--;
        }
      else
        {
          // pass received data on to the device
          m_currentDevice->write(data, eoi);
        }

      return true;
    }
  else
    {
      // canWrite reported an error
      return false;
    }
}


bool IECBusHandler::transmitDolphinByte(byte numData)
{
  // Note: receiver starts a 50us timeout after setting DATA high
  // (ready-to-receive) waiting for CLK low (data valid). If we take
  // longer than those 50us the receiver will interpret that as EOI
  // (last byte of data). So we need to take precautions:
  // - disable interrupts between setting CLK high and setting CLK low
  // - get the data byte to send before setting CLK high
  // - wait for DATA high in a blocking loop
  byte data = numData>0 ? m_currentDevice->peek() : 0xFF;

  noInterrupts();

  // signal "ready-to-send" (CLK=1)
  writePinCLK(HIGH);

  // wait for "ready-for-data" (DATA=0)
  if( !waitPinDATA(HIGH, 0) ) { interrupts(); return false; }

  if( numData==0 ) 
    {
      // if we have nothing to send then there was some kind of error 
      // aborting here will signal the error condition to the receiver
      interrupts();
      return false;
    }
  else if( numData==1 )
    {
      // last data byte => keep CLK high (signals EOI) and wait for receiver to 
      // confirm EOI by HIGH->LOW->HIGH pulse on DATA
      bool ok = (waitPinDATA(LOW) && waitPinDATA(HIGH));
      if( !ok ) { interrupts(); return false; }
    }

  // put data byte on parallel bus
  setParallelBusModeOutput();
  writeParallelData(data);

  // set CLK low (signal "data ready")
  writePinCLK(LOW);

  interrupts();

  // discard data byte in device (read by peek() before)
  m_currentDevice->read();

  // remember initial bytes of data sent (see comment in transmitDolphinBurst)
  if( m_secondary==0x60 && m_dolphinCtr<DOLPHIN_PREBUFFER_BYTES ) 
    m_buffer[m_dolphinCtr++] = data;

  // wait for receiver to confirm receipt (must confirm within 1ms)
  bool res = waitPinDATA(LOW, 1000);
  
  // release parallel bus
  setParallelBusModeInput();
  
  return res;
}


bool IECBusHandler::receiveDolphinBurst()
{
  // NOTE: we only get here if sender has already signaled ready-to-send
  // by pulling CLK low
  byte n = 0;

  // clear any previous handshakes
  parallelBusHandshakeReceived();

  // pull DATA low
  writePinDATA(LOW);

  // confirm burst mode transmission
  parallelBusHandshakeTransmit();

  // keep going while CLK is low
  bool eoi = false;
  while( !eoi )
    {
      // wait for "data ready" handshake, return if ATN is asserted (high)
      if( !waitParallelBusHandshakeReceived() ) return false;

      // CLK=high means EOI ("final byte of data coming")
      eoi = readPinCLK();

      // get received data byte
      m_buffer[n++] = readParallelData();

      if( n<m_bufferSize && !eoi )
        {
          // data received and buffered  => send handshake
          parallelBusHandshakeTransmit();
        }
      else if( m_currentDevice->write(m_buffer, n, eoi)==n )
        {
          // data written successfully => send handshake
          parallelBusHandshakeTransmit();
          n = 0;
        }
      else
        {
          // error while writing data => release DATA to signal error condition and exit
          writePinDATA(HIGH);
          return false;
        }
    }

  return true;
}


bool IECBusHandler::transmitDolphinBurst()
{
  // NOTE: we only get here if sender has already signaled ready-to-receive
  // by pulling DATA low

  // send handshake to confirm burst transmission (Dolphin kernal EEDA)
  parallelBusHandshakeTransmit();

  // give the host some time to see our confirmation
  // if we send the next handshake too quickly then the host will see only one,
  // the host will be busy printing the load address after seeing the confirmation
  // so nothing is lost by waiting a good long time before the next handshake
  delayMicroseconds(1000);

  // switch parallel bus to output
  setParallelBusModeOutput();

  // when loading a file, DolphinDos switches to burst mode by sending "XQ" after
  // the transmission has started. The kernal does so after the first two bytes
  // were sent, MultiDubTwo after one byte. After swtiching to burst mode, the 1541
  // then re-transmits the bytes that were already sent.
  for(byte i=0; i<m_dolphinCtr; i++)
    {
      // put data on bus
      writeParallelData(m_buffer[i]);

      // send handshake (see "send handshake" comment below)
      noInterrupts();
      parallelBusHandshakeTransmit();
      parallelBusHandshakeReceived();
      interrupts();

      // wait for received handshake
      if( !waitParallelBusHandshakeReceived() ) { setParallelBusModeInput(); return false; }
    }

  // get data from the device and transmit it
  byte n;
  while( (n=m_currentDevice->read(m_buffer, m_bufferSize))>0 )
    for(byte i=0; i<n; i++)
      {
        // put data on bus
        writeParallelData(m_buffer[i]);
        
        // send handshake
        // sending the handshake can induce a pulse on the receive handhake
        // line so we clear the receive handshake after sending, note that we
        // can't have an interrupt take up time between sending the handshake
        // and clearing the receive handshake
        noInterrupts();
        parallelBusHandshakeTransmit();
        parallelBusHandshakeReceived();
        interrupts();

        // wait for receiver handshake
        while( !parallelBusHandshakeReceived() )
          if( !readPinATN() || readPinDATA() )
            {
              // if receiver released DATA or pulled ATN low then there 
              // was an error => release bus and CLK line and return
              setParallelBusModeInput();
              writePinCLK(HIGH);
              return false;
            }
      }

  // switch parallel bus back to input
  setParallelBusModeInput();

  // signal end-of-data
  writePinCLK(HIGH);

  // wait for receiver to confirm
  if( !waitPinDATA(HIGH) ) return false;

  // send handshake
  parallelBusHandshakeTransmit();

  return true;
}

#endif

#ifdef SUPPORT_EPYX

// ------------------------------------  Epyx FastLoad support routines  ------------------------------------


bool IECBusHandler::enableEpyxFastLoadSupport(IECDevice *dev, bool enable)
{
  if( enable && m_buffer!=NULL && m_bufferSize>=32 )
    dev->m_sflags |= S_EPYX_ENABLED;
  else
    dev->m_sflags &= ~S_EPYX_ENABLED;

  // cancel any current requests
  dev->m_sflags &= ~(S_EPYX_HEADER|S_EPYX_LOAD|S_EPYX_SECTOROP);

  return (dev->m_sflags & S_EPYX_ENABLED)!=0;
}


void IECBusHandler::epyxLoadRequest(IECDevice *dev)
{
  if( dev->m_sflags & S_EPYX_ENABLED )
    dev->m_sflags |= S_EPYX_HEADER;
}


bool IECBusHandler::receiveEpyxByte(byte &data)
{
  bool clk = HIGH;
  for(byte i=0; i<8; i++)
    {
      // wait for next bit ready
      // can't use timeout because interrupts are disabled and (on some platforms) the
      // micros() function does not work in this case
      clk = !clk;
      if( !waitPinCLK(clk, 0) ) return false;

      // read next (inverted) bit
      JDEBUG1();
      data >>= 1;
      if( !readPinDATA() ) data |= 0x80;
      JDEBUG0();
    }

  return true;
}


bool IECBusHandler::transmitEpyxByte(byte data)
{
  // receiver expects all data bits to be inverted
  data = ~data;

  // prepare timer
  timer_init();
  timer_reset();

  // wait (indefinitely) for either DATA high ("ready-to-send") or ATN low
  // NOTE: this must be in a blocking loop since the sender starts transmitting
  // the byte immediately after setting CLK high. If we exit the "task" function then
  // we may not get back here in time to receive.
  while( !digitalReadFastExt(m_pinDATA, m_regDATAread, m_bitDATA) && digitalReadFastExt(m_pinATN, m_regATNread, m_bitATN) );
  
  // start timer
  timer_start();
  JDEBUG1();

  // abort if ATN low
  if( !readPinATN() ) { JDEBUG0(); return false; }

  JDEBUG0();
  writePinCLK(data & bit(7));
  writePinDATA(data & bit(5));
  JDEBUG1();
  // bits 5+7 are read by receiver 15 cycles after DATA HIGH

  // wait until 17 us after DATA
  timer_wait_until(17);

  JDEBUG0();
  writePinCLK(data & bit(6));
  writePinDATA(data & bit(4));
  JDEBUG1();
  // bits 4+6 are read by receiver 25 cycles after DATA HIGH

  // wait until 27 us after DATA
  timer_wait_until(27);

  JDEBUG0();
  writePinCLK(data & bit(3));
  writePinDATA(data & bit(1));
  JDEBUG1();
  // bits 1+3 are read by receiver 35 cycles after DATA HIGH

  // wait until 37 us after DATA
  timer_wait_until(37);

  JDEBUG0();
  writePinCLK(data & bit(2));
  writePinDATA(data & bit(0));
  JDEBUG1();
  // bits 0+2 are read by receiver 45 cycles after DATA HIGH

  // wait until 47 us after DATA
  timer_wait_until(47);

  // release DATA and give it time to stabilize
  writePinDATA(HIGH);
  timer_wait_until(49);

  // wait for DATA low, receiver signaling "not ready"
  if( !waitPinDATA(LOW, 0) ) return false;

  JDEBUG0();
  return true;
}


#ifdef SUPPORT_EPYX_SECTOROPS

// NOTE: most calls to waitPinXXX() within this code happen while
// interrupts are disabled and therefore must use the ",0" (no timeout)
// form of the call - timeouts are dealt with using the micros() function
// which does not work properly when interrupts are disabled.

bool IECBusHandler::startEpyxSectorCommand(byte command)
{
  // interrupts are assumed to be disabled when we get here
  // and will be re-enabled before we exit
  // both CLK and DATA must be released (HIGH) before entering
  byte track, sector, data;

  if( command==0x81 )
    {
      // V1 sector write
      // wait for DATA low (no timeout), however we exit if ATN goes low,
      // interrupts are enabled while waiting (same as in 1541 code)
      interrupts();
      if( !waitPinDATA(LOW, 0) ) return false;
      noInterrupts();

      // release CLK
      writePinCLK(HIGH);
    }

  // receive track and sector
  // (command==1 means write sector, otherwise read sector)
  if( !receiveEpyxByte(track) )   { interrupts(); return false; }
  if( !receiveEpyxByte(sector) )  { interrupts(); return false; }

  // V1 of the cartridge has two different uploads for read and write
  // and therefore does not send the command separately
  if( command==0 && !receiveEpyxByte(command) ) { interrupts(); return false; }

  if( (command&0x7f)==1 )
    {
      // sector write operation => receive data
      for(int i=0; i<256; i++)
        if( !receiveEpyxByte(m_buffer[i]) )
          { interrupts(); return false; }
    }

  // pull CLK low to signal "not ready"
  writePinCLK(LOW);

  // we can allow interrupts again
  interrupts();

  // pass data on to the device
  if( (command&0x7f)==1 )
    if( !m_currentDevice->epyxWriteSector(track, sector, m_buffer) )
      { interrupts(); return false; }

  // m_buffer size is guaranteed to be >=32
  m_buffer[0] = command;
  m_buffer[1] = track;
  m_buffer[2] = sector;

  m_currentDevice->m_sflags |= S_EPYX_SECTOROP;
  return true;
}


bool IECBusHandler::finishEpyxSectorCommand()
{
  // this was set in receiveEpyxSectorCommand
  byte command = m_buffer[0];
  byte track   = m_buffer[1];
  byte sector  = m_buffer[2];

  // receive data from the device
  if( (command&0x7f)!=1 )
    if( !m_currentDevice->epyxReadSector(track, sector, m_buffer) )
      return false;

  // all timing is clocked by the computer so we can't afford
  // interrupts to delay execution as long as we are signaling "ready"
  noInterrupts();

  // release CLK to signal "ready"
  writePinCLK(HIGH);

  if( command==0x81 )
    {
      // V1 sector write => receive new track/sector
      return startEpyxSectorCommand(0x81); // startEpyxSectorCommand() re-enables interrupts
    }
  else
    {
      // V1 sector read or V2/V3 read/write => release CLK to signal "ready"
      if( (command&0x7f)!=1 )
        {
          // sector read operation => send data
          for(int i=0; i<256; i++)
            if( !transmitEpyxByte(m_buffer[i]) )
              { interrupts(); return false; }
        }
      else
        {
          // release DATA and wait for computer to pull it LOW
          writePinDATA(HIGH);
          if( !waitPinDATA(LOW, 0) ) { interrupts(); return false; }
        }

      // release DATA and toggle CLK until DATA goes high or ATN goes low.
      // This provides a "heartbeat" for the computer so it knows we're still running
      // the EPYX sector command code. If the computer does not see this heartbeat
      // it will re-upload the code when it needs it.
      // The EPYX code running on a real 1541 drive does not have this timeout but
      // we need it because otherwise we're stuck in an endless loop with interrupts
      // disabled until the computer either pulls ATN low or releases DATA
      // We can not enable interrupts because the time between DATA high
      // and the start of transmission for the next track/sector/command block
      // is <400us without any chance for us to signal "not ready.
      // A (not very nice) interrupt routing may take longer than that.
      // We could just always quit and never send the heartbeat but then operations
      // like "copy disk" would have to re-upload the code for ever single sector.
      // Wait for DATA high, time out after 30000 * ~16us (~500ms)
      timer_init();
      timer_reset();
      timer_start();
      for(unsigned int i=0; i<30000; i++)
        {
          writePinCLK(LOW);
          if( !readPinATN() ) break;
          interrupts();
          timer_wait_until(8);
          noInterrupts();
          writePinCLK(HIGH);
          if( readPinDATA() ) break;
          timer_wait_until(16);
          timer_reset();
        }

      // abort if we timed out (DATA still low) or ATN is pulled
      if( !readPinDATA() || !readPinATN() ) { interrupts(); return false; }

      // wait (DATA high pulse from sender can be up to 90us)
      if( !waitTimeout(100) ) { interrupts(); return false; }

      // if DATA is still high (or ATN is low) then done, otherwise repeat for another sector
      if( readPinDATA() || !readPinATN() )
        { interrupts(); return false; }
      else
        return startEpyxSectorCommand((command&0x80) ? command : 0); // startEpyxSectorCommand() re-enables interrupts
    }
}

#endif

bool IECBusHandler::receiveEpyxHeader()
{
  // all timing is clocked by the computer so we can't afford
  // interrupts to delay execution as long as we are signaling "ready"
  noInterrupts();

  // pull CLK low to signal "ready for header"
  writePinCLK(LOW);

  // wait for sender to set DATA low, signaling "ready"
  if( !waitPinDATA(LOW, 0) ) { interrupts(); return false; }

  // release CLK line
  writePinCLK(HIGH);

  // receive fastload routine upload (256 bytes) and compute checksum
  byte data, checksum = 0;
  for(int i=0; i<256; i++)
    {
      if( !receiveEpyxByte(data) ) { interrupts(); return false; }
      checksum += data;
    }

  if( checksum==0x26 /* V1 load file */ ||
      checksum==0x86 /* V2 load file */ ||
      checksum==0xAA /* V3 load file */ )
    {
      // LOAD FILE operation
      // receive file name and open file
      byte n;
      if( receiveEpyxByte(n) && n>0 && n<=32 )
        {
          // file name arrives in reverse order
          for(byte i=n; i>0; i--)
            if( !receiveEpyxByte(m_buffer[i-1]) )
              { interrupts(); return false; }

          // pull CLK low to signal "not ready"
          writePinCLK(LOW);

          // can allow interrupts again
          interrupts();

          // initiate DOS OPEN command in the device (open channel #0)
          m_currentDevice->listen(0xF0);

          // send file name (in proper order) to the device
          for(byte i=0; i<n; i++)
            {
              // make sure the device can accept data
              int8_t ok;
              while( (ok = m_currentDevice->canWrite())<0 )
                if( !readPinATN() )
                  return false;

              // fail if it can not
              if( ok==0 ) return false;

              // send next file name character
              m_currentDevice->write(m_buffer[i], i<n-1);
            }

          // finish DOS OPEN command in the device
          m_currentDevice->unlisten();

          m_currentDevice->m_sflags |= S_EPYX_LOAD;
          return true;
        }
    }
#ifdef SUPPORT_EPYX_SECTOROPS
  else if( checksum==0x0B /* V1 sector read */ )
    return startEpyxSectorCommand(0x82); // startEpyxSectorCommand re-enables interrupts
  else if( checksum==0xBA /* V1 sector write */ )
    return startEpyxSectorCommand(0x81); // startEpyxSectorCommand re-enables interrupts
  else if( checksum==0xB8 /* V2 and V3 sector read or write */ )
    return startEpyxSectorCommand(0); // startEpyxSectorCommand re-enables interrupts
#endif
#if 0
  else if( Serial )
    {
      interrupts();
      Serial.print(F("Unknown EPYX fastload routine, checksum is 0x"));
      Serial.println(checksum, HEX);
    }
#endif

  interrupts();
  return false;
}


bool IECBusHandler::transmitEpyxBlock()
{
  byte n = m_currentDevice->read(m_buffer, m_bufferSize);

  noInterrupts();

  // release CLK to signal "ready"
  writePinCLK(HIGH);

  // transmit length of this data block
  if( !transmitEpyxByte(n) ) { interrupts(); return false; }

  // transmit the data block
  for(byte i=0; i<n; i++)
    if( !transmitEpyxByte(m_buffer[i]) )
      { interrupts(); return false; }

  // pull CLK low to signal "not ready"
  writePinCLK(LOW);

  interrupts();

  // the "end transmission" condition for the receiver is receiving
  // a "0" length byte so we keep sending block until we have
  // transmitted a 0-length block (i.e. end-of-file)
  return n>0;
}


#endif

// ------------------------------------  IEC protocol support routines  ------------------------------------  


bool IECBusHandler::receiveIECByte(bool canWriteOk)
{
  // NOTE: we only get here if sender has already signaled ready-to-send
  // by releasing CLK
  bool eoi = false;

  // release DATA ("ready-for-data")
  writePinDATA(HIGH);

  // if we are under attention then wait until all other devices have also
  // released DATA, otherwise we may incorrectly detect EOI
  if( (m_flags & P_ATN)!=0 && !waitPinDATA(HIGH) ) return false;

  // wait for sender to set CLK=0 ("ready-to-send")
  if( !waitPinCLK(LOW, 200) )
    {
      // exit if waitPinCLK returned because of falling edge on ATN
      if( (m_flags & P_ATN)==0 && !readPinATN() ) return false;

      // sender did not set CLK=0 within 200us after we set DATA=1
      // => it is signaling EOI (not so if we are under ATN)
      // acknowledge we received it by setting DATA=0 for 80us
      eoi = true;
      writePinDATA(LOW);
      if( !waitTimeout(80) ) return false;
      writePinDATA(HIGH);

      // keep waiting for CLK=0
      if( !waitPinCLK(LOW) ) return false;
    }

  byte data = 0;
  for(byte i=0; i<8; i++)
    {
      // wait for CLK=1, signaling data is ready
#ifdef SUPPORT_JIFFY
      if( !waitPinCLK(HIGH, 200) )
        {
          IECDevice *dev = findDevice((data>>1)&0x0F);

          if( (m_flags & P_ATN)==0 && !readPinATN() )
            return false;
          else if( (m_flags & P_ATN) && (m_primary==0) && (i==7) && (dev!=NULL) && (dev->m_sflags&S_JIFFY_ENABLED) )
            {
              // when sending final bit of primary address byte under ATN, host
              // delayed CLK=1 by more than 200us => JiffyDOS protocol detection
              // => if JiffyDOS support is enabled and we are being addressed then
              // respond that we support the protocol by pulling DATA low for 80us
              dev->m_sflags |= S_JIFFY_DETECTED;
              writePinDATA(LOW);
              if( !waitTimeout(80) ) return false;
              writePinDATA(HIGH);
            }
          
          // keep waiting fom CLK=1
          if( !waitPinCLK(HIGH) ) return false;
        }
#else
      if( !waitPinCLK(HIGH) ) return false;
#endif

      // read DATA bit
      data >>= 1;
      if( readPinDATA() ) data |= 0x80;

      // wait for CLK=0, signaling "data not ready"
      if( !waitPinCLK(LOW) ) return false;
    }

  if( m_flags & P_ATN )
    {
      // We are currently receiving under ATN.  Store first two 
      // bytes received(contain primary and secondary address)
      if( m_primary == 0 ) {
        m_primary = data;
        m_currentDevice = findDevice(m_primary & 0x0f);
        if (m_currentDevice) {
          m_currentDevice->primary_address(m_primary);
        }
      }
      else if( m_secondary == 0 ) {
        m_secondary = data;
        m_currentDevice->secondary_address(m_secondary);
      }

      if( (m_primary != 0x3f) && (m_primary != 0x5f) && findDevice((unsigned int) m_primary & 0x1f)==NULL )
        {
          // This is NOT an UNLISTEN (0x3f) or UNTALK (0x5f)
          // command and the primary address is not ours =>
          // Do not acknowledge the frame and stop listening.
          // If all devices on the bus do this, the bus master
          // knows that "Device not present"
          return false;
        }
      else
        {
          // Acknowledge receipt by pulling DATA low
          writePinDATA(LOW);

#if defined(SUPPORT_DOLPHIN)
          // DolphinDos parallel cable detection:
          // wait for either:
          //  HIGH->LOW edge (1us pulse) on incoming parallel handshake signal, 
          //      if received pull outgoing parallel handshake signal LOW to confirm
          //  LOW->HIGH edge on ATN, 
          //      if so then timeout, host does not support DolphinDos
          IECDevice *dev = findDevice(m_primary & 0x0F);
          if( dev!=NULL && (dev->m_sflags & S_DOLPHIN_ENABLED) && m_secondary!=0 )
            {
              // clear any previous handshakes
              parallelBusHandshakeReceived();

              // wait for handshake
              while( !readPinATN() )
                if( parallelBusHandshakeReceived() )
                  {
                    dev->m_sflags |= S_DOLPHIN_DETECTED;
                    parallelBusHandshakeTransmit(); 
                    break;
                  }
            }
#endif
          return true;
        }
    }
  else if( canWriteOk )
    {
      // acknowledge receipt by pulling DATA low
      writePinDATA(LOW);

      // pass received data on to the device
      m_currentDevice->write(data, eoi);
      return true;
    }
  else
    {
      // canWrite() reported an error
      return false;
    }
}


bool IECBusHandler::transmitIECByte(byte numData)
{
  // check whether ready-to-receive was already signaled by the 
  // receiver before we signaled ready-to-send. The 1541 ROM 
  // disassembly (E919-E924) suggests that this signals a "verify error" 
  // condition and we should send EOI. Note that the C64 kernal does not
  // actually do this signaling during a "verify" operation so I don't
  // know whether my interpretation here is correct. However, some 
  // programs (e.g. "copy 190") lock up if we don't handle this case.
  bool verifyError = readPinDATA();

  // signal "ready-to-send" (CLK=1)
  writePinCLK(HIGH);
  
  // wait (indefinitely, no timeout) for DATA HIGH ("ready-to-receive")
  // NOTE: this must be in a blocking loop since the receiver starts counting
  // up the EOI timeout immediately after setting DATA HIGH. If we had exited the 
  // "task" function then it might be more than 200us until we get back here
  // to pull CLK low and the receiver will interpret that delay as EOI.
  if( !waitPinDATA(HIGH, 0) ) return false;
  
  if( numData==1 || verifyError )
    {
      // only this byte left to send => signal EOI by keeping CLK=1
      // wait for receiver to acknowledge EOI by setting DATA=0 then DATA=1
      // if we got here by "verifyError" then wait indefinitely because we
      // didn't enter the "wait for DATA high" state above
      if( !waitPinDATA(LOW, verifyError ? 0 : 1000) ) return false;
      if( !waitPinDATA(HIGH) ) return false;
    }

  // if we have nothing to send then there was some kind of error 
  // => aborting at this stage will signal the error condition to the receiver
  //    (e.g. "File not found" for LOAD)
  if( numData==0 ) return false;

  // signal "data not valid" (CLK=0)
  writePinCLK(LOW);

  // get the data byte from the device
  byte data = m_currentDevice->read();

  // transmit the byte
  for(byte i=0; i<8; i++)
    {
      // signal "data not valid" (CLK=0)
      writePinCLK(LOW);

      // set bit on DATA line
      writePinDATA((data & 1)!=0);

      // hold for 80us
      if( !waitTimeout(80) ) return false;
      
      // signal "data valid" (CLK=1)
      writePinCLK(HIGH);

      // hold for 60us
      if( !waitTimeout(60) ) return false;

      // next bit
      data >>= 1;
    }

  // pull CLK=0 and release DATA=1 to signal "busy"
  writePinCLK(LOW);
  writePinDATA(HIGH);

  // wait for receiver to signal "busy"
  if( !waitPinDATA(LOW) ) return false;
  
  return true;
}


// called when a falling edge on ATN is detected, either by the pin change
// interrupt handler or by polling within the microTask function
void IECBusHandler::atnRequest()
{
  // falling edge on ATN detected (bus master addressing all devices)
  m_flags |= P_ATN;
  m_flags &= ~P_DONE;
  m_currentDevice = NULL;
  m_primary = 0;
  m_secondary = 0;

  // ignore anything for 100us after ATN falling edge
  m_timeoutStart = micros();

  // release CLK (in case we were holding it LOW before)
  writePinCLK(HIGH);
  
  // set DATA=0 ("I am here").  If nobody on the bus does this within 1ms,
  // busmaster will assume that "Device not present" 
  writePinDATA(LOW);

  // disable the hardware that allows ATN to pull DATA low
  writePinCTRL(HIGH);

#ifdef SUPPORT_JIFFY
  for(byte i=0; i<m_numDevices; i++)
    m_devices[i]->m_sflags &= ~(S_JIFFY_DETECTED|S_JIFFY_BLOCK);
#endif
#ifdef SUPPORT_DOLPHIN
  for(byte i=0; i<m_numDevices; i++) 
    m_devices[i]->m_sflags &= ~(S_DOLPHIN_BURST_TRANSMIT|S_DOLPHIN_BURST_RECEIVE|S_DOLPHIN_DETECTED);
#endif
#ifdef SUPPORT_EPYX
  for(byte i=0; i<m_numDevices; i++) 
    m_devices[i]->m_sflags &= ~(S_EPYX_HEADER|S_EPYX_LOAD|S_EPYX_SECTOROP);
#endif
}


void IECBusHandler::task()
{
  // don't do anything if begin() hasn't been called yet
  if( m_flags==0xFF ) return;

  // prevent interrupt handler from calling atnRequest()
  m_inTask = true;

  // ------------------ check for activity on RESET pin -------------------

  if( readPinRESET() )
    m_flags |= P_RESET;
  else if( (m_flags & P_RESET)!=0 )
    { 
      // falling edge on RESET pin
      m_flags = 0;
      
      // release CLK and DATA, allow ATN to pull DATA low in hardware
      writePinCLK(HIGH);
      writePinDATA(HIGH);
      writePinCTRL(LOW);

      // call "reset" function for attached devices
      for(byte i=0; i<m_numDevices; i++)
        m_devices[i]->reset(); 
    }

  // ------------------ check for activity on ATN pin -------------------

  if( !(m_flags & P_ATN) && !readPinATN() ) 
    {
      // falling edge on ATN (bus master addressing all devices)
      atnRequest();
    } 
  else if( (m_flags & P_ATN) && readPinATN() )
    {
      // rising edge on ATN (bus master finished addressing all devices)
      m_flags &= ~P_ATN;
      
      // allow ATN to pull DATA low in hardware
      writePinCTRL(LOW);
      
      if( (m_primary & 0xE0)==0x20 && (m_currentDevice = findDevice(m_primary & 0x1F))!=NULL )
        {
          // we were told to listen
          m_currentDevice->listen(m_secondary);
          m_flags &= ~P_TALKING;
          m_flags |= P_LISTENING;
#ifdef SUPPORT_DOLPHIN
          // see comments in function receiveDolphinByte
          if( m_secondary==0x61 ) m_dolphinCtr = 2*DOLPHIN_PREBUFFER_BYTES;
#endif
          // set DATA=0 ("I am here")
          writePinDATA(LOW);
        } 
      else if( (m_primary & 0xE0)==0x40 && (m_currentDevice = findDevice(m_primary & 0x1F))!=NULL )
        {
          // we were told to talk
#ifdef SUPPORT_JIFFY
          if( (m_currentDevice->m_sflags & S_JIFFY_DETECTED)!=0 && m_secondary==0x61 )
            { 
              // in JiffyDOS, secondary 0x61 when talking enables "block transfer" mode
              m_secondary = 0x60; 
              m_currentDevice->m_sflags |= S_JIFFY_BLOCK; 
            }
#endif        
          m_currentDevice->talk(m_secondary);
          m_flags &= ~P_LISTENING;
          m_flags |= P_TALKING;
#ifdef SUPPORT_DOLPHIN
          // see comments in function transmitDolphinByte
          if( m_secondary==0x60 ) m_dolphinCtr = 0;
#endif
          // wait for bus master to set CLK=1 (and DATA=0) for role reversal
          if( waitPinCLK(HIGH) )
            {
              // now set CLK=0 and DATA=1
              writePinCLK(LOW);
              writePinDATA(HIGH);

              // wait 80us before transmitting first byte of data
              m_timeoutStart = micros();
              m_timeoutDuration = 80;
            }
        }
      else if( (m_primary == 0x3f) && (m_flags & P_LISTENING) )
        {
          // all devices were told to stop listening
          m_flags &= ~P_LISTENING;
          for(byte i=0; i<m_numDevices; i++)
            m_devices[i]->unlisten();
        }
      else if( m_primary == 0x5f && (m_flags & P_TALKING) )
        {
          // all devices were told to stop talking
          m_flags &= ~P_TALKING;
          for(byte i=0; i<m_numDevices; i++)
            m_devices[i]->untalk();
        }

      if( !(m_flags & (P_LISTENING | P_TALKING)) )
        {
          // we're neither listening nor talking => make sure we're not
          // holding the DATA or CLOCK line to 0
          writePinCLK(HIGH);
          writePinDATA(HIGH);
        }
    }

#ifdef SUPPORT_DOLPHIN
  // ------------------ DolphinDos burst transfer handling -------------------

  for(byte devidx=0; devidx<m_numDevices; devidx++)
  if( (m_devices[devidx]->m_sflags & S_DOLPHIN_BURST_TRANSMIT)!=0 && (micros()-m_timeoutStart)>200 && !readPinDATA() )
    {
      // if we are in burst transmit mode, give other devices 200us to release
      // the DATA line and wait for the host to pull DATA LOW

      // pull CLK line LOW (host should have released it by now)
      writePinCLK(LOW);
      
      m_currentDevice = m_devices[devidx];
      if( m_currentDevice->m_sflags & S_DOLPHIN_BURST_ENABLED )
        {
          // transmit data in burst mode
          transmitDolphinBurst();
          
          // close the file (usually the host sends these but not in burst mode)
          m_currentDevice->listen(0xE0);
          m_currentDevice->unlisten();

          // check whether ATN has been asserted and handle if necessary
          if( !readPinATN() ) atnRequest();
        }
      else
        {
          // switch to regular transmit mode
          m_flags = P_TALKING;
          m_currentDevice->m_sflags |= S_DOLPHIN_DETECTED;
          m_secondary = 0x60;
        }

      m_currentDevice->m_sflags &= ~S_DOLPHIN_BURST_TRANSMIT;
    }
  else if( (m_devices[devidx]->m_sflags&S_DOLPHIN_BURST_RECEIVE)!=0 && (micros()-m_timeoutStart)>500 && !readPinCLK() )
    {
      // if we are in burst receive mode, wait 500us to make sure host has released CLK after 
      // sending "XZ" burst request (Dolphin kernal ef82), and wait for it to pull CLK low again
      // (if we don't wait at first then we may read CLK=0 already before the host has released it)

      m_currentDevice = m_devices[devidx];
      if( m_currentDevice->m_sflags & S_DOLPHIN_BURST_ENABLED )
        {
          // transmit data in burst mode
          receiveDolphinBurst();
          
          // check whether ATN has been asserted and handle if necessary
          if( !readPinATN() ) atnRequest();
        }
      else
        {
          // switch to regular receive mode
          m_flags = P_LISTENING;
          m_currentDevice->m_sflags |= S_DOLPHIN_DETECTED;
          m_secondary = 0x61;

          // see comment in function receiveDolphinByte
          m_dolphinCtr = (2*DOLPHIN_PREBUFFER_BYTES)-m_dolphinCtr;

          // signal NOT ready to receive
          writePinDATA(LOW);
        }

      m_currentDevice->m_sflags &= ~S_DOLPHIN_BURST_RECEIVE;
    }
#endif

#ifdef SUPPORT_EPYX
  // ------------------ Epyx FastLoad transfer handling -------------------

  for(byte devidx=0; devidx<m_numDevices; devidx++)
  if( (m_devices[devidx]->m_sflags & S_EPYX_HEADER) && readPinDATA() )
    {
      m_currentDevice = m_devices[devidx];
      m_currentDevice->m_sflags &= ~S_EPYX_HEADER;
      if( !receiveEpyxHeader() )
        {
          // transmission error
          writePinCLK(HIGH);
          writePinDATA(HIGH);
        }
    }
  else if( m_devices[devidx]->m_sflags & S_EPYX_LOAD )
    {
      m_currentDevice = m_devices[devidx];
      if( !transmitEpyxBlock() )
        {
          // either end-of-data or transmission error => we are done
          writePinCLK(HIGH);
          writePinDATA(HIGH);

          // close the file (was opened in receiveEpyxHeader)
          m_currentDevice->listen(0xE0);
          m_currentDevice->unlisten();

          // no more data to send
          m_currentDevice->m_sflags &= ~S_EPYX_LOAD;
        }
    }
#ifdef SUPPORT_EPYX_SECTOROPS
  else if( m_devices[devidx]->m_sflags & S_EPYX_SECTOROP )
    {
      m_currentDevice = m_devices[devidx];
      if( !finishEpyxSectorCommand() )
        {
          // either no more operations or transmission error => we are done
          writePinCLK(HIGH);
          writePinDATA(HIGH);

          // no more sector operations
          m_currentDevice->m_sflags &= ~S_EPYX_SECTOROP;
        }
    }
#endif
#endif

  // ------------------ receiving data -------------------

  if( (m_flags & (P_ATN | P_LISTENING))!=0 && (m_flags & P_DONE)==0 )
    {
      // we are either under ATN or in "listening" mode and not yet done with the transaction

      // check if we can write (also gives devices a chance to
      // execute time-consuming tasks while bus master waits for ready-for-data)
      // (if we are receiving under ATN then m_currentDevice==NULL)
      m_inTask = false;
      int8_t numData = m_currentDevice ? m_currentDevice->canWrite() : 0;
      m_inTask = true;

      if( !(m_flags & P_ATN) && !readPinATN() )
        {
          // a falling edge on ATN happened while we were stuck in "canWrite"
          atnRequest();
        }
      else if( (m_flags & P_ATN) && (micros()-m_timeoutStart)<100 )
        {
          // ignore anything that happens during first 100us after falling
          // edge on ATN (other devices may have been sending and need
          // some time to set CLK=1). m_timeoutStart is set in atnRequest()
        }
#ifdef SUPPORT_JIFFY
      else if( !(m_flags & P_ATN) && (m_currentDevice->m_sflags & S_JIFFY_DETECTED)!=0 && numData>=0 )
        {
          // receiving under JiffyDOS protocol
          if( !receiveJiffyByte(numData>0) )
            {
              // receive failed => release DATA 
              // and stop listening.  This will signal
              // an error condition to the sender
              writePinDATA(HIGH);
              m_flags |= P_DONE;
            }
          }
#endif
#ifdef SUPPORT_DOLPHIN
      else if( (m_currentDevice->m_sflags & S_DOLPHIN_DETECTED)!=0 && numData>=0 && !(m_flags & P_ATN) )
        {
          // receiving under DolphinDOS protocol
          if( !readPinCLK() )
            { /* CLK is still low => sender is not ready yet */ }
          else if( !receiveDolphinByte(numData>0) )
            {
              // receive failed => release DATA 
              // and stop listening.  This will signal
              // an error condition to the sender
              writePinDATA(HIGH);
              m_flags |= P_DONE;
            }
        }
#endif
      else if( ((m_flags & P_ATN) || numData>=0) && readPinCLK() ) 
        {
          // either under ATN (in which case we always accept data)
          // or canWrite() result was non-negative
          // CLK high signals sender is ready to transmit
          if( !receiveIECByte(numData>0) )
            {
              // receive failed => transaction is done
              writePinDATA(HIGH);
              m_flags |= P_DONE;
            }
        }
    }

  // ------------------ transmitting data -------------------

  if( (m_flags & (P_ATN|P_TALKING|P_DONE))==P_TALKING )
   {
     // we are not under ATN, are in "talking" mode and not done with the transaction

#ifdef SUPPORT_JIFFY
     if( (m_currentDevice->m_sflags & S_JIFFY_BLOCK)!=0 )
       {
         // JiffyDOS block transfer mode
         byte numData = m_currentDevice->read(m_buffer, m_bufferSize);

         // delay to make sure receiver sees our CLK LOW and enters "new data block" state.
         // due possible VIC "bad line" it may take receiver up to 120us after
         // reading bits 6+7 (at FB71) to checking for CLK low (at FB54).
         // If we make it back into transmitJiffyBlock() during that time period
         // then we may already set CLK HIGH again before receiver sees the CLK LOW, 
         // preventing the receiver from going into "new data block" state
         if( !waitTimeoutFrom(m_timeoutStart, 150) || !transmitJiffyBlock(m_buffer, numData) )
           {
             // either a transmission error, no more data to send or falling edge on ATN
             m_flags |= P_DONE;
           }
         else
           {
             // remember time when previous transmission finished
             m_timeoutStart = micros();
           }
       }
     else
#endif
       {
         // check if we can read (also gives devices a chance to
         // execute time-consuming tasks while bus master waits for ready-to-send)
        m_inTask = false;
        int8_t numData = m_currentDevice->canRead();
        m_inTask = true;

        if( !readPinATN() )
          {
            // a falling edge on ATN happened while we were stuck in "canRead"
            atnRequest();
          }
        else if( (micros()-m_timeoutStart)<m_timeoutDuration || numData<0 )
          {
            // either timeout not yet met or canRead() returned a negative value => do nothing
          }
#ifdef SUPPORT_JIFFY
        else if( (m_currentDevice->m_sflags & S_JIFFY_DETECTED)!=0 )
          {
            // JiffyDOS byte-by-byte transfer mode
            if( !transmitJiffyByte(numData) )
              {
                // either a transmission error, no more data to send or falling edge on ATN
                m_flags |= P_DONE;
              }
          }
#endif
#ifdef SUPPORT_DOLPHIN
        else if( (m_currentDevice->m_sflags & S_DOLPHIN_DETECTED)!=0 )
          {
            // DolphinDOS byte-by-byte transfer mode
            if( !transmitDolphinByte(numData) )
              {
                // either a transmission error, no more data to send or falling edge on ATN
                writePinCLK(HIGH);
                m_flags |= P_DONE;
              }
          }
#endif
        else
          {
            // regular IEC transfer
            if( transmitIECByte(numData) )
              {
                // delay before next transmission ("between bytes time")
                m_timeoutStart = micros();
                m_timeoutDuration = 200;
              }
            else
              {
                // either a transmission error, no more data to send or falling edge on ATN
                m_flags |= P_DONE;
              }
          }
       }
   }

  // allow the interrupt handler to call atnRequest() again
  m_inTask = false;

  // if ATN is low and we don't have P_ATN then we missed the falling edge,
  // make sure to process it before we leave
  if( m_atnInterrupt!=NOT_AN_INTERRUPT && !readPinATN() && !(m_flags & P_ATN) ) { noInterrupts(); atnRequest(); interrupts(); }

  // call "task" function for attached devices
  for(byte i=0; i<m_numDevices; i++)
    m_devices[i]->task(); 
}
