/****************************************************************************************************************************
   ISR_16_Timers_Array.ino
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

   RPM Measuring uses high frequency hardware timer 1Hz == 1ms) to measure the time from of one rotation, in ms
   then convert to RPM. One rotation is detected by reading the state of a magnetic REED SW or IR LED Sensor
   Asssuming LOW is active.
   For example: Max speed is 600RPM => 10 RPS => minimum 100ms a rotation. We'll use 80ms for debouncing
   If the time between active state is less than 8ms => consider noise.
   RPM = 60000 / (rotation time in ms)

   We use interrupt to detect whenever the SW is active, set a flag then use timer to count the time between active state

   This example will demonstrate the nearly perfect accuracy compared to software timers by printing the actual elapsed millisecs.
   Being ISR-based timers, their executions are not blocked by bad-behaving functions / tasks, such as connecting to WiFi, Internet
   and Blynk services. You can also have many (up to 16) timers to use.
   This non-being-blocked important feature is absolutely necessary for mission-critical tasks.
   You'll see blynkTimer is blocked while connecting to WiFi / Internet / Blynk, and elapsed time is very unaccurate
   In this super simple example, you don't see much different after Blynk is connected, because of no competing task is
   written
*/

#if !(defined(NRF52840_FEATHER) || defined(NRF52832_FEATHER) || defined(NRF52_SERIES) || defined(ARDUINO_NRF52_ADAFRUIT) || \
      defined(NRF52840_FEATHER_SENSE) || defined(NRF52840_ITSYBITSY) || defined(NRF52840_CIRCUITPLAY) || \
      defined(NRF52840_CLUE) || defined(NRF52840_METRO) || defined(NRF52840_PCA10056) || defined(PARTICLE_XENON) \
      || defined(NINA_B302_ublox) || defined(NINA_B112_ublox) )
  #error This code is designed to run on nRF52 platform! Please check your Tools->Board setting.
#endif

// These define's must be placed at the beginning before #include "NRF52TimerInterrupt.h"
// Don't define NRF52_TIMER_INTERRUPT_DEBUG > 0. Only for special ISR debugging only. Can hang the system.
#define NRF52_TIMER_INTERRUPT_DEBUG      1

#include "NRF52TimerInterrupt.h"
#include "NRF52_ISR_Timer.h"

#include <SimpleTimer.h>              // https://github.com/schinken/SimpleTimer

#ifndef LED_BUILTIN
  #define LED_BUILTIN       13
#endif

#ifndef LED_BLUE
  #define LED_BLUE          7
#endif

#ifndef LED_RED
  #define LED_RED           8
#endif

#define HW_TIMER_INTERVAL_US      1000L

volatile uint32_t startMillis = 0;

// Depending on the board, you can select NRF52 Hardware Timer from NRF_TIMER_1-NRF_TIMER_4 (1 to 4)
// If you select the already-used NRF_TIMER_0, it'll be auto modified to use NRF_TIMER_1

// Init NRF52 timer NRF_TIMER1
NRF52Timer ITimer(NRF_TIMER_2);

// Init NRF52_ISR_Timer
// Each NRF52_ISR_Timer can service 16 different ISR-based timers
NRF52_ISR_Timer ISR_Timer;

#define LED_TOGGLE_INTERVAL_MS        2000L

void TimerHandler(void)
{
  static bool toggle  = false;
  static bool started = false;
  static int timeRun  = 0;

  ISR_Timer.run();

  // Toggle LED every LED_TOGGLE_INTERVAL_MS = 2000ms = 2s
  if (++timeRun == ((LED_TOGGLE_INTERVAL_MS * 1000) / HW_TIMER_INTERVAL_US) )
  {
    timeRun = 0;

    if (!started)
    {
      started = true;
      pinMode(LED_BUILTIN, OUTPUT);
    }

    //timer interrupt toggles pin LED_BUILTIN
    digitalWrite(LED_BUILTIN, toggle);
    toggle = !toggle;
  }
}

#define NUMBER_ISR_TIMERS         16

// You can assign any interval for any timer here, in milliseconds
uint32_t TimerInterval[NUMBER_ISR_TIMERS] =
{
  5000L,  10000L,  15000L,  20000L,  25000L,  30000L,  35000L,  40000L,
  45000L, 50000L,  55000L,  60000L,  65000L,  70000L,  75000L,  80000L
};

typedef void (*irqCallback)  (void);

#if (NRF52_TIMER_INTERRUPT_DEBUG > 0)
void printStatus(uint16_t index, unsigned long deltaMillis, unsigned long currentMillis)
{
  Serial.print(TimerInterval[index]/1000);
  Serial.print("s: Delta ms = ");
  Serial.print(deltaMillis);
  Serial.print(", ms = ");
  Serial.println(currentMillis);

  // This Serial.flush() is important. Don't remove on nRF52 or it'll hang
  Serial.flush();
}
#endif

// In NRF52, avoid doing something fancy in ISR, for example complex Serial.print with String() argument
// The pure simple Serial.prints here are just for demonstration and testing. Must be eliminate in working environment
// Or you can get this run-time error / crash
void doingSomething0()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 0)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(0, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething1()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(1, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething2()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(2, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething3()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(3, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething4()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(4, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething5()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(5, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething6()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(6, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething7()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(7, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething8()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(8, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething9()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(9, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething10()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(10, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

// In NRF52, avoid doing something fancy in ISR, for example complex Serial.print with String() argument
// The pure simple Serial.prints here are just for demonstration and testing. Must be eliminate in working environment
// Or you can get this run-time error / crash
void doingSomething11()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(11, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

// In NRF52, avoid doing something fancy in ISR, for example complex Serial.print with String() argument
// The pure simple Serial.prints here are just for demonstration and testing. Must be eliminate in working environment
// Or you can get this run-time error / crash
void doingSomething12()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(12, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething13()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(13, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething14()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(14, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

void doingSomething15()
{
#if (NRF52_TIMER_INTERRUPT_DEBUG > 1)
  static unsigned long previousMillis = startMillis;
  
  unsigned long currentMillis = millis();
  unsigned long deltaMillis   = currentMillis - previousMillis;

  printStatus(15, deltaMillis, currentMillis);

  previousMillis = currentMillis;
#endif
}

irqCallback irqCallbackFunc[NUMBER_ISR_TIMERS] =
{
  doingSomething0,  doingSomething1,  doingSomething2,  doingSomething3, 
  doingSomething4,  doingSomething5,  doingSomething6,  doingSomething7, 
  doingSomething8,  doingSomething9,  doingSomething10, doingSomething11,
  doingSomething12, doingSomething13, doingSomething14, doingSomething15
};

////////////////////////////////////////////////


#define SIMPLE_TIMER_MS        2000L

// Init SimpleTimer
SimpleTimer simpleTimer;

// Here is software Timer, you can do somewhat fancy stuffs without many issues.
// But always avoid
// 1. Long delay() it just doing nothing and pain-without-gain wasting CPU power.Plan and design your code / strategy ahead
// 2. Very long "do", "while", "for" loops without predetermined exit time.
void simpleTimerDoingSomething2s()
{
  static unsigned long previousMillis = startMillis;

  unsigned long currMillis = millis();

  Serial.print("simpleTimer");
  Serial.print(SIMPLE_TIMER_MS/1000);
  Serial.print("s:Dms=");
  Serial.println(currMillis - previousMillis);
  Serial.flush();

  previousMillis =currMillis;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial);
  
  Serial.println("\nStarting ISR_16_Timers_Array on " + String(BOARD_NAME));
  Serial.println("Version : " + String(NRF52_TIMER_INTERRUPT_VERSION));
  Serial.println("CPU Frequency = " + String(F_CPU / 1000000) + " MHz");

  // Interval in microsecs
  if (ITimer.attachInterruptInterval(HW_TIMER_INTERVAL_US, TimerHandler))
  {
    startMillis = millis();
    Serial.println("Starting  ITimer OK, millis() = " + String(startMillis));
  }
  else
    Serial.println("Can't set ITimer correctly. Select another freq. or interval");

  // Just to demonstrate, don't use too many ISR Timers if not absolutely necessary
  // You can use up to 16 timer for each ISR_Timer
  for (int i = 0; i < NUMBER_ISR_TIMERS; i++)
  {
    ISR_Timer.setInterval(TimerInterval[i], irqCallbackFunc[i]); 
  }

  // You need this timer for non-critical tasks. Avoid abusing ISR if not absolutely necessary.
  simpleTimer.setInterval(SIMPLE_TIMER_MS, simpleTimerDoingSomething2s);
}

#define BLOCKING_TIME_MS      10000L

void loop()
{
  // This unadvised blocking task is used to demonstrate the blocking effects onto the execution and accuracy to Software timer
  // You see the time elapse of ISR_Timer still accurate, whereas very unaccurate for Software Timer
  // The time elapse for 2000ms software timer now becomes 3000ms (BLOCKING_TIME_MS)
  // While that of ISR_Timer is still prefect.
  delay(BLOCKING_TIME_MS);

  // You need this Software timer for non-critical tasks. Avoid abusing ISR if not absolutely necessary
  // You don't need to and never call ISR_Timer.run() here in the loop(). It's already handled by ISR timer.
  simpleTimer.run();
}
