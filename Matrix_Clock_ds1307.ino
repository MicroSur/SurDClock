
//MicroSuR D`Clock
//24h only screen
//MAX7219 dot matrix module 8 dot matrix 2*4 display module
//FC16_HW
//Russian week day font

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "font_clock.h"
#include <MD_DS1307.h>
#include <Wire.h>

uint8_t wd = 0;
uint8_t dd = 0;
uint8_t mm = 0;
uint16_t yy = 2020;
uint8_t h = 0;
uint8_t m = 0;
uint8_t s = 0;

#define PRINTS(s) Serial.print(F(s));
#define PRINT(s, v) { Serial.print(F(s)); Serial.print(v); }

#define NUM_ZONES 2
#define ZONE_SIZE 4
#define MAX_DEVICES (NUM_ZONES * ZONE_SIZE)
#define ZONE_UPPER  0
#define ZONE_LOWER  1
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

// NOTE: These pin numbers may need to be adapted
// Arduino nano (ATmega328P)
#define CLK_PIN   13
#define DATA_PIN  11
#define CS_PIN    10
// DS1307 pins:
// (grey)SCK(SCL) - A5
// (violet)SDA - A4

// Hardware SPI connection
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

#define SPEED_TIME_LOWER 75 // speed of the transition
#define SPEED_TIME_UPPER 40
#define PAUSE_TIME  0

#define MAX_MESG  10

// Global variables
char szMesg[MAX_MESG + 1] = {"00.00.00"};
char szTime[MAX_MESG + 1] = {"00:00"};

String inputString = "";         // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

void getTime(char *psz, bool f = true)
{
  s = RTC.s;
  m = RTC.m;
  h = RTC.h;
  sprintf(psz, "%02d%c%02d", h, (f ? ':' : ' '), m);
  //strcpy(szTime, "00:00");
}

void getDate(char *psz)
{
  dd = RTC.dd;
  mm = RTC.mm;
  //  yy = Clock.getYear();
  wd = RTC.dow;
  sprintf(psz, "%c.%02d.%02d", wd + 60, dd, mm);
  //strcpy(szMesg, "00.00.00");
}

char htoa(uint8_t i)
{
  if (i < 10)
  {
    return (i + '0');
  }
  else if (i < 16)
  {
    return (i - 10 + 'a');
  }
  return ('?');
}

const char *p2dig(uint8_t v, uint8_t mode)
// print 2 digits leading zero
{
  static char c[3] = { "00" };

  switch (mode)
  {
    case HEX:
      {
        c[0] = htoa((v >> 4) & 0xf);
        c[1] = htoa(v & 0xf);
      }
      break;

    case DEC:
      {
        c[0] = ((v / 10) % 10) + '0';
        c[1] = (v % 10) + '0';
      }
      break;
  }

  return (c);
}

const char *dow2String(uint8_t code)
{
  static const char *str[] = { "---", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

  if (code > 7) code = 0;
  return (str[code]);
}

void printTime()
{
  PRINT("\n", RTC.yyyy);
  PRINT("-", p2dig(RTC.mm, DEC));
  PRINT("-", p2dig(RTC.dd, DEC));
  PRINT(" ", p2dig(RTC.h, DEC));
  PRINT(":", p2dig(RTC.m, DEC));
  PRINT(":", p2dig(RTC.s, DEC));
  if (RTC.status(DS1307_12H) == DS1307_ON)
    PRINT(" ", RTC.pm ? "pm" : "am");
  PRINT(" ", dow2String(RTC.dow));
  PRINTS("\n");
}

void usage(void)
{
  PRINTS("\n? - help (this message)");
  PRINTS("\n\nt, T - read the current time");
  PRINTS("\nhXX, mXX, sXX - set hours, minutes, seconds to XX");
  PRINTS("\n\dX, wX - set DoW to X");
  PRINTS("\n\nDxx, Mxx - set day, month to xx");
  PRINTS("\nYxxxx - set year to xxxx");
  PRINTS("\n\nS - get RTC status");
  PRINTS("\n");
}

const char *ctl2String(uint8_t code)
{
  static const char *str[] =
  {
    "CLOCK_HALT",
    "SQW_RUN",
    "SQW_TYPE_ON",
    "SQW_TYPE_OFF",
    "12H MODE"
  };

  return (str[code]);
}

const char *sts2String(uint8_t code)
{
  static const char *str[] =
  {
    "ERROR",
    "ON",
    "OFF",
    "1Hz",
    "4KHz",
    "8KHz",
    "32KHz",
    "HIGH",
    "LOW"
  };

  return (str[code]);
}

void showStatus()
{
  PRINT("\nClock Halt: ", sts2String(RTC.status(DS1307_CLOCK_HALT)));
  PRINT("\nIs running: ", RTC.isRunning());
  PRINT("\nSQW Output: ", sts2String(RTC.status(DS1307_SQW_RUN)));
  PRINT("\nSQW Type (on): ", sts2String(RTC.status(DS1307_SQW_TYPE_ON)));
  PRINT("\nSQW Type (off): ", sts2String(RTC.status(DS1307_SQW_TYPE_OFF)));
  PRINT("\n12h mode: ", sts2String(RTC.status(DS1307_12H)));
  PRINTS("\n");
}

/*
  SerialEvent occurs whenever a new data comes in the hardware serial RX. This
  routine is run between each time loop() runs, so using delay inside loop can
  delay response. Multiple bytes of data may be available.
*/
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag so the main loop can
    // do something about it:
    //PRINT("\nchar: ", inChar);
    if ( (inChar == '\r') || (inChar == '\n') ) {
      stringComplete = true;
    }
  }
}

void ReadConsole() {
  // print the string when a newline arrives:
  if (stringComplete) {
    //    Serial.println(inputString);
    //    Serial.println(inputString.charAt(0));
    //    Serial.println(inputString.substring(1).toInt());

    switch (inputString.charAt(0))
    {
      case '?':
      default:
        usage();
        break;
      case 'h':
        RTC.h = inputString.substring(1).toInt();
        RTC.writeTime();
        PRINT("\nSet hours to: ", RTC.h);
        PRINTS("\n");
        break;

      case 'm':
        RTC.m = inputString.substring(1).toInt();
        RTC.writeTime();
        PRINT("\nSet minutes to: ", RTC.m);
        PRINTS("\n");
        break;

      case 's':
        RTC.s = inputString.substring(1).toInt();
        RTC.writeTime();
        PRINT("\nSet seconds to: ", RTC.s);
        PRINTS("\n");
        break;

      case 'd':
      case 'w':
        RTC.dow = inputString.substring(1).toInt();
        RTC.writeTime();
        PRINT("\nSet DoW to: ", RTC.dow);
        PRINTS("\n");
        break;

      case 'D':
        RTC.dd = inputString.substring(1).toInt();
        RTC.writeTime();
        PRINT("\nSet day to: ", RTC.dd);
        PRINTS("\n");
        break;

      case 'M':
        RTC.mm = inputString.substring(1).toInt();
        RTC.writeTime();
        PRINT("\nSet month to: ", RTC.mm);
        PRINTS("\n");
        break;

      case 'Y':
        RTC.yyyy = inputString.substring(1).toInt();
        RTC.writeTime();
        PRINT("\nSet year to: ", RTC.yyyy);
        PRINTS("\n");
        break;

      case 'S':
        showStatus();
        break;

      case 't':
      case 'T':
        printTime();
        break;
    }

    // clear the string:
    inputString = "";
    stringComplete = false;
  }

}

void setup(void)
{
  Serial.begin(9600);
  PRINTS("[SuR clock setup]");
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);

  Wire.begin();

  // Ensure the clock is running
  if (!RTC.isRunning())
    RTC.control(DS1307_CLOCK_HALT, DS1307_OFF);

  //RTC.control(DS1307_CLOCK_HALT, DS1307_OFF);
  //RTC.control(DS1307_12H, DS1307_OFF);
  //RTC.readTime();

  P.begin(NUM_ZONES);
  P.setInvert(false);

  P.setZone(ZONE_UPPER, ZONE_SIZE, MAX_DEVICES - 1);
  P.setZone(ZONE_LOWER, 0, ZONE_SIZE - 1);

  P.setFont(ZONE_UPPER, NumFontUpper);
  P.setFont(ZONE_LOWER, NumFontLower);
  P.setCharSpacing(ZONE_UPPER, 1);
  P.setCharSpacing(ZONE_LOWER, 1);

  P.displayZoneText(ZONE_UPPER, szTime, PA_CENTER, SPEED_TIME_UPPER, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(ZONE_LOWER, szMesg, PA_CENTER, SPEED_TIME_LOWER, 0, PA_PRINT, PA_NO_EFFECT);

  P.displayAnimate();
  //P.displayReset();
  //P.displayClear();
  // synchronise the start
  //P.synchZoneStart();

  //  RTC.yyyy = 2020;
  //  RTC.mm = 7;
  //  RTC.dd = 9;
  //
  //  RTC.h = 17;
  //  RTC.m = 45;
  //  RTC.s = 30;
  //
  //  RTC.dow = 4;
  //  RTC.writeTime();

}

void loop(void)
{
  static uint32_t lastTime = 0; // millis() memory
  static bool flasher = true;  // seconds passing flasher

  P.displayAnimate();

  if (P.getZoneStatus(ZONE_UPPER))
  {

    P.setTextEffect(ZONE_UPPER, PA_PRINT, PA_NO_EFFECT);
    if ( h > 19 || h < 10 )
    {
      P.setIntensity(0);
    }
    else
    {
      P.setIntensity(1); //Set display brightness 0-15
    }

    if (millis() - lastTime >= 500)
    {
      RTC.readTime();
      lastTime = millis();
      getTime(szTime, flasher);
      getDate(szMesg);
      if (RTC.status(DS1307_CLOCK_HALT) != DS1307_ON)
      {
        flasher = !flasher; //":"
      }
      ReadConsole();
    }

    //    if ( s > 58 )
    //    {
    //      P.setTextEffect(ZONE_UPPER, PA_PRINT, PA_SCROLL_LEFT);
    //    }

    P.displayReset(ZONE_UPPER);
  }

  if (P.getZoneStatus(ZONE_LOWER))
  {
    //P.setTextEffect(ZONE_LOWER, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    P.displayReset(ZONE_LOWER);
  }

  //P.displayReset();
  //synchronise the start
  //P.synchZoneStart();

} //END
