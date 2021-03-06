/****************************************************************************************************************************
   TimerInterruptTest.ino
   For NRF52 boards
   Written by Khoi Hoang

   Built by Khoi Hoang https://github.com/khoih-prog/NRF52_TimerInterrupt
   Licensed under MIT license

   Now even you use all these new 16 ISR-based timers,with their maximum interval practically unlimited (limited only by
   unsigned long miliseconds), you just consume only one NRF52 timer and avoid conflicting with other cores' tasks.
   The accuracy is nearly perfect compared to software timers. The most important feature is they're ISR-based timers
   Therefore, their executions are not blocked by bad-behaving functions / tasks.
   This important feature is absolutely necessary for mission-critical tasks.

   Based on SimpleTimer - A timer library for Arduino.
   Author: mromani@ottotecnica.com
   Copyright (c) 2010 OTTOTECNICA Italy

   Based on BlynkTimer.h
   Author: Volodymyr Shymanskyy

   Version: 1.0.1

   Version Modified By   Date      Comments
   ------- -----------  ---------- -----------
   1.0.0   K Hoang      02/11/2020 Initial coding
   1.0.1   K Hoang      06/11/2020 Add complicated example ISR_16_Timers_Array using all 16 independent ISR Timers.
*****************************************************************************************************************************/
/*
   Notes:
   Special design is necessary to share data between interrupt code and the rest of your program.
   Variables usually need to be "volatile" types. Volatile tells the compiler to avoid optimizations that assume
   variable can not spontaneously change. Because your function may change variables while your program is using them,
   the compiler needs this hint. But volatile alone is often not enough.
   When accessing shared variables, usually interrupts must be disabled. Even with volatile,
   if the interrupt changes a multi-byte variable between a sequence of instructions, it can be read incorrectly.
   If your data is multiple variables, such as an array and a count, usually interrupts need to be disabled
   or the entire sequence of your code which accesses the data.
*/

#if !(defined(NRF52840_FEATHER) || defined(NRF52832_FEATHER) || defined(NRF52_SERIES) || defined(ARDUINO_NRF52_ADAFRUIT) || \
      defined(NRF52840_FEATHER_SENSE) || defined(NRF52840_ITSYBITSY) || defined(NRF52840_CIRCUITPLAY) || \
      defined(NRF52840_CLUE) || defined(NRF52840_METRO) || defined(NRF52840_PCA10056) || defined(PARTICLE_XENON) \
      || defined(NINA_B302_ublox) || defined(NINA_B112_ublox) )
  #error This code is designed to run on nRF52 platform! Please check your Tools->Board setting.
#endif

// These define's must be placed at the beginning before #include "NRF52TimerInterrupt.h"
// Don't define NRF52_TIMER_INTERRUPT_DEBUG > 2. Only for special ISR debugging only. Can hang the system.
#define NRF52_TIMER_INTERRUPT_DEBUG      1

#include "NRF52TimerInterrupt.h"

//#ifndef LED_BUILTIN
//  #define LED_BUILTIN         3
//#endif

#ifndef LED_BLUE_PIN
  #define LED_BLUE_PIN          7
#endif

#ifndef LED_RED
  #define LED_RED               8
#endif

unsigned int SWPin = 11;

#define TIMER0_INTERVAL_MS        1000
#define TIMER0_DURATION_MS        5000

#define TIMER1_INTERVAL_MS        3000
#define TIMER1_DURATION_MS        15000

volatile uint32_t preMillisTimer0 = 0;
volatile uint32_t preMillisTimer1 = 0;

// Depending on the board, you can select NRF52 Hardware Timer from NRF_TIMER_1-NRF_TIMER_4 (1 to 4)
// If you select the already-used NRF_TIMER_0, it'll be auto modified to use NRF_TIMER_1

// Init NRF52 timer NRF_TIMER1
NRF52Timer ITimer0(NRF_TIMER_1);

void TimerHandler0(void)
{
  static bool toggle0 = false;
  static bool started = false;
  static uint32_t curMillis = 0;

  if (!started)
  {
    started = true;
    pinMode(LED_BUILTIN, OUTPUT);
  }
 
#if (NRF52_TIMER_INTERRUPT_DEBUG > 0)
    curMillis = millis();
    
    if (curMillis > TIMER0_INTERVAL_MS)
    {
      Serial.println("ITimer0: millis() = " + String(curMillis) + ", delta = " + String(curMillis - preMillisTimer0));
    }
    
    preMillisTimer0 = curMillis;
#endif

  //timer interrupt toggles pin LED_BUILTIN
  digitalWrite(LED_BUILTIN, toggle0);
  toggle0 = !toggle0;
}

// Init NRF52 timer NRF_TIMER2
NRF52Timer ITimer1(NRF_TIMER_2);

void TimerHandler1(void)
{
  static bool toggle1 = false;
  static bool started = false;
  static uint32_t curMillis = 0;

  if (!started)
  {
    started = true;
    pinMode(LED_BLUE, OUTPUT);
  }
  
#if (NRF52_TIMER_INTERRUPT_DEBUG > 0)
    curMillis = millis();

    if (curMillis > TIMER1_INTERVAL_MS)
    {
      Serial.println("ITimer1: millis() = " + String(curMillis) + ", delta = " + String(curMillis - preMillisTimer1));
    }
    
    preMillisTimer1 = curMillis;
#endif

  //timer interrupt toggles outputPin
  digitalWrite(LED_BLUE, toggle1);
  toggle1 = !toggle1;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial);
  
  delay(100);
  
  Serial.println("\nStarting TimerInterruptTest on " + String(BOARD_NAME));
  Serial.println("Version : " + String(NRF52_TIMER_INTERRUPT_VERSION));

  // Interval in microsecs
  if (ITimer0.attachInterruptInterval(TIMER0_INTERVAL_MS * 1000, TimerHandler0))
  {
    preMillisTimer0 = millis();
    Serial.println("Starting  ITimer1 OK, millis() = " + String(preMillisTimer0));
  }
  else
    Serial.println("Can't set ITimer0. Select another freq. or timer");

  // Interval in microsecs
  if (ITimer1.attachInterruptInterval(TIMER1_INTERVAL_MS * 1000, TimerHandler1))
  {
    preMillisTimer1 = millis();
    Serial.println("Starting  ITimer1 OK, millis() = " + String(preMillisTimer1));
  }
  else
    Serial.println("Can't set ITimer1. Select another freq. or timer");
}

void loop()
{
  static unsigned long lastTimer0   = 0; 
  static bool timer0Stopped         = false;
  
  static unsigned long lastTimer1   = 0;
  static bool timer1Stopped         = false;
  

  if (millis() - lastTimer0 > TIMER0_DURATION_MS)
  {
    lastTimer0 = millis();

    if (timer0Stopped)
    {
      preMillisTimer0 = millis();
      Serial.println("Start ITimer0, millis() = " + String(preMillisTimer0));
      ITimer0.restartTimer();
    }
    else
    {
      Serial.println("Stop ITimer0, millis() = " + String(millis()));
      ITimer0.stopTimer();
    }
    timer0Stopped = !timer0Stopped;
  }

  if (millis() - lastTimer1 > TIMER1_DURATION_MS)
  {
    lastTimer1 = millis();

    if (timer1Stopped)
    {
      preMillisTimer1 = millis();
      Serial.println("Start ITimer1, millis() = " + String(preMillisTimer1));
      ITimer1.restartTimer();
    }
    else
    {
      Serial.println("Stop ITimer1, millis() = " + String(millis()));
      ITimer1.stopTimer();
    }
    
    timer1Stopped = !timer1Stopped;
  }
}
