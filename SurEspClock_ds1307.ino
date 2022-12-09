//SuR (ESP-12E module) NTP & RTC Clock
//24h only screen, russian week day, 32x16 matrix with two zones

//WiFi AP & EEPROM https://github.com/techiesms/WiFi-Credentials-Saving---Connecting
#include <MD_Parola.h>
#include <MD_MAX72xx.h> //https://github.com/MajicDesigns/MD_MAX72XX
#include <SPI.h>
#include "font_clock.h"
#include <MD_DS1307.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
//https://werner.rothschopf.net/202011_arduino_esp8266_ntp_en.htm
#include <time.h>
#include <coredecls.h> // optional settimeofday_cb() callback to check on server

/* Configuration of NTP */
#define MY_NTP_SERVER "ru.pool.ntp.org"           
#define MY_TZ "MSK-3"  //https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

/* Globals */
time_t now;                         // this is the epoch
tm tm;                              // the structure tm holds time information in a more convient way

//const int led = BUILTIN_LED; //gpio16, gpio2 (setup in Tools Arduino IDE)
const char *ssid     = "xxxxxxxx";
const char *password = "xxxxxxxx";

uint8_t wd = 1;
uint8_t dd = 1;
uint8_t mm = 1;
uint16_t yy = 2022;
uint8_t h = 0;
uint8_t m = 0;
uint8_t s = 0;
String newStr = "";
String lastNTPcheck = "Newer.";
bool ntp_get = false; //for icon
uint8_t BrightnessUp = 1;
uint8_t BrightnessLo = 1;
uint8_t FontUpNum = 2;
uint8_t FontUpNumMax = 3; //available fonts
bool wifi_ok = false;

#define PRINTS(s) Serial.print(F(s));
#define PRINT(s, v) { Serial.print(F(s)); Serial.print(v); }

#define NUM_ZONES 2
#define ZONE_SIZE 4
#define MAX_DEVICES (NUM_ZONES * ZONE_SIZE)
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define ZONE_UPPER  0
#define ZONE_LOWER  1

//#define ONE_HRS 3600000UL
//#define HALF_HRS 1800000UL

// need to be adapted
#define CLK_PIN   D5 // or SCK
#define DATA_PIN  D7 // or MOSI
#define CS_PIN    D8 // or SS

//clock pins (grey)SCK(SCL) - (gpio5)D1, (violet)SDA - (gpio4)D2
//MD_DS1307::MD_DS1307  ( int   sda, int   scl )
MD_DS1307 myRTC;

MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
WiFiUDP ntpUDP;
ESP8266WebServer server(80);

#define SPEED_TIME_LOWER 50 // speed of the transition
#define SPEED_TIME_UPPER 15
#define PAUSE_TIME  2000

#define MAX_MESG  21
char szMesg[MAX_MESG] = {""};
char szTime[MAX_MESG] = {""};
char szMesgNew[MAX_MESG] = {""}; //{"00 00.00"};
char szTimeNew[MAX_MESG] = {""}; //{"00:00"};


//uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000 ()
//{
//  return 60000UL; // 60s
//}
//uint32_t sntp_update_delay_MS_rfc_not_less_than_15000 ()
//{
//  return 12 * 60 * 60 * 1000UL; // 12 hours
//}
//callback function:
void time_is_set() {     // no parameter until 2.7.4
  //Serial.println(F("time was sent!));
  timeCheck(); //update rtc
}
//The optional parameter can be used with ESP8266 Core 3.0.0 or higher
//void time_is_set(bool from_sntp /* <= this optional parameter can be used with ESP8266 Core 3.0.0*/) {
//  Serial.print(F("time was sent! from_sntp=")); Serial.println(from_sntp);
//}

void ReadBrightness ()
{
  BrightnessUp = uint8_t(EEPROM.read(100));
  if (BrightnessUp > 15) BrightnessUp = 2;
  BrightnessLo = uint8_t(EEPROM.read(101));
  if (BrightnessLo > 15) BrightnessLo = 2;
}

void WriteBrightness ()
{
  EEPROM.write(100, BrightnessUp);
  EEPROM.write(101, BrightnessLo);
  EEPROM.commit();
}

void ReadFontUpNum ()
{
  FontUpNum = uint8_t(EEPROM.read(102));
  if (FontUpNum > FontUpNumMax) FontUpNum = 2;
}

void WriteFontUpNum ()
{
  EEPROM.write(102, FontUpNum);
  EEPROM.commit();
}

void SetFontUpNum()
{
  switch ( FontUpNum )
  {
    case 1:
    default:
      P.setFont(ZONE_UPPER, NumFontUpper);
      break;

    case 2:
      P.setFont(ZONE_UPPER, NumFontUpperThin);
      break;

    case 3:
      P.setFont(ZONE_UPPER, NumFontUpperBold);
      break;
  }
}

void getTime(char *psz, bool f = true)
{
  s = myRTC.s;
  m = myRTC.m;
  h = myRTC.h;
  sprintf(psz, "%02d%c%02d", h, (f ? ':' : ' '), m);
  //strcpy(szTimeNew, "00:00");
}

void getDate(char *psz)
{
  dd = myRTC.dd;
  mm = myRTC.mm;
  
  if ( ntp_get ) {
    //show ntp update icon
    wd = 8;
    ntp_get = false;
  } else {
    wd = myRTC.dow;
    wd = wd == 0 ? 7 : wd ; // days 1-7
  }
  sprintf(psz, "%c%c%02d/%02d", wd + 60, (wifi_ok ? '.' : ' '), dd, mm);
  //strcpy(szMesgNew, "00 00.00");
}

String DateTimeStr() {
  return  p2dig(dd) + "/" + p2dig(mm) + "/" + String(yy) + " (" + dow2String(wd) + ") " + p2dig(h) + ":" + p2dig(m) + ":" + p2dig(s);
}

void timeCheck() {
 // called from callback every 1h by default
  time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time

  yy = tm.tm_year + 1900;  // years since 1900
  mm = tm.tm_mon + 1;      // January = 0 (!)
  dd = tm.tm_mday;         // day of month
  h = tm.tm_hour;         // hours since midnight  0-23  
  m = tm.tm_min;          // minutes after the hour  0-59
  s = tm.tm_sec;          // seconds after the minute  0-61*
  //* tm_sec is generally 0-59. The extra range is to accommodate for leap seconds in certain systems.
  wd = tm.tm_wday;         // days since Sunday 0-6
  //wd = wd == 0 ? 7 : wd ; // days 1-7
  //(tm.tm_isdst == 1)             // Daylight Saving Time flag

  String yearStr = String(yy);
  String monthStr = mm < 10 ? "0" + String(mm) : String(mm);
  String dayStr = dd < 10 ? "0" + String(dd) : String(dd);
  String weekdayStr = String(wd);
  String hoursStr = h < 10 ? "0" + String(h) : String(h);
  String minuteStr = m < 10 ? "0" + String(m) : String(m);
  String secondStr = s < 10 ? "0" + String(s) : String(s);

  String timeStr = dayStr + "-" + monthStr + "-" + yearStr + " " +
                   hoursStr + ":" + minuteStr + ":" + secondStr + " week day: " + weekdayStr;
  PRINT("\nNTP time > ", timeStr);

  
    //set a time from NTP

    myRTC.yyyy = yy;
    myRTC.mm = mm;
    myRTC.dd = dd;

    myRTC.h = h;
    myRTC.m = m;
    myRTC.s = s;
    myRTC.dow = wd;

    myRTC.writeTime();

    //    newStr = "Get NTP time Ok.";
    //    P.displayClear(ZONE_UPPER);
    //    newStr.toCharArray(szTime, MAX_MESG);
    //    P.displayZoneText(ZONE_UPPER, szTime, PA_CENTER, SPEED_TIME_UPPER, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);

    lastNTPcheck = DateTimeStr();
    ntp_get = true;  //for icon  

}

String ip2Str(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
}

String p2dig(uint8_t v)
// print 2 digits leading zero
{
  static char c[3] = { "00" };
  c[0] = ((v / 10) % 10) + '0';
  c[1] = (v % 10) + '0';
  return (c);
}

String dow2String(uint8_t code)
{
  static const char *str[] = { "---", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
  if (code > 7) code = 0;
  return (str[code]);
}

void PrintIP()
{
  // scroll local IP in ZONE_LOWER
  newStr = ip2Str(WiFi.localIP());
  PRINT("\nWiFi Local IP > ", newStr);
  newStr.toCharArray(szMesg, MAX_MESG); //newStr.length() + 1);
  P.displayClear(ZONE_LOWER);
  P.displayReset(ZONE_LOWER);
  P.setTextEffect(ZONE_LOWER, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

String SendHTML()
{
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  
  ptr += "<title>SuR DClock setup</title>\n";

  ptr += "<style>html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #333333;}\n";
  ptr += "body{margin-top: 50px;}\n";
  ptr += "h1 {margin: 50px auto 30px;}\n";
  ptr += ".side-by-side{display: inline-block;vertical-align: middle;position: relative;}\n";
  ptr += ".superscript{font-size: 17px;font-weight: 600;position: absolute;right: -20px;top: 15px;}\n";
  ptr += ".data{padding: 10px;}\n";
  ptr += "</style>\n";

  ptr += "</head>\n";
  ptr += "<body>\n";

  ptr += "<div id=\"webpage\">\n";

  ptr += "<h1>SuR DClock</h1>\n";
  ptr += "<h3>" + DateTimeStr() + "</h3>\n";
  ptr += "Last NTP check: " + lastNTPcheck + "<br><br>\n";

  // set matrix intensity
  ptr += "<form action='/Brightness' method='GET'> Up: <input type='number' name='BrightnessUp' min='0' max='15' length='1' value=";
  ptr +=  String(BrightnessUp) + "> Lo: <input type='number' name='BrightnessLo' min='0' max='15' length='1' value=";
  ptr +=  String(BrightnessLo) + "><input type='submit' value='Set brightness'></form><br>\n";

  // set upper font
  ptr += "<form action='/Font' method='GET'> Upper font (1-3): <input type='number' name='FontUpNum' min='1' max='3' length='1' value=";
  ptr +=  String(FontUpNum) + "><input type='submit' value='Set font'></form><br>\n";

  // set time button
  //ptr += "<form action='/SetTime' method='GET'><input type='submit' value='Set time'></form><br>\n";

  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}

void handleRoot() {
  //server.send(200, "text/html", "<form action=\"/LED\" method=\"POST\"><input type=\"submit\" value=\"Toggle LED\"></form>");
  server.send(200, "text/html", SendHTML());
}

void handleBrightness() {

  BrightnessUp = server.arg("BrightnessUp").toInt();
  BrightnessLo = server.arg("BrightnessLo").toInt();
  WriteBrightness(); // to EEPROM
  P.setIntensity(ZONE_UPPER, BrightnessUp);
  P.setIntensity(ZONE_LOWER, BrightnessLo);

  PRINT("\nSet Brightness Up to: ", BrightnessUp);
  PRINT("\nSet Brightness Lo to: ", BrightnessLo);

  server.sendHeader("Location", "/");       // Add a header to respond with a new location for the browser to go to the home page again
  server.send(303);                         // Send it back to the browser with an HTTP status 303 (See Other) to redirect
}

void handleFontNum() {

  FontUpNum = server.arg("FontUpNum").toInt();

  SetFontUpNum();

  WriteFontUpNum(); // to EEPROM

  PRINT("\nSet Upper font to: ", FontUpNum);

  server.sendHeader("Location", "/");       // Add a header to respond with a new location for the browser to go to the home page again
  server.send(303);                         // Send it back to the browser with an HTTP status 303 (See Other) to redirect
}

//void handleSetTime() {
//
//  ntp_get = true;
//  timeCheck(); //from NTP
//
//  PRINT("\nSet time to: ", DateTimeStr());
//
//  server.sendHeader("Location", "/");       // Add a header to respond with a new location for the browser to go to the home page again
//  server.send(303);                         // Send it back to the browser with an HTTP status 303 (See Other) to redirect
//}


void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
}

void setup(void)
{
  Serial.begin(115200);
  Wire.begin();
  //pinMode(BUILTIN_LED, OUTPUT); //16, D0, LED_BUILTIN, BUILTIN_LED
  //pinMode(PIN_TONE, OUTPUT);

  //bytes 100, 101 - for upper & lower zones brightness
  //102 - FontUpNum
  //0 to 96 - for wifi ssid & pass (not implemented)
  EEPROM.begin(512); //Initialasing EEPROM
  
  // Ensure the clock is running
  if (!myRTC.isRunning())
    myRTC.control(DS1307_CLOCK_HALT, DS1307_OFF);
    
  //myRTC.control(DS1307_CLOCK_HALT, DS1307_OFF);
  //myRTC.control(DS1307_12H, DS1307_OFF);
  //myRTC.readTime();

  //  myRTC.yyyy = 2020;
  //  myRTC.mm = 7;
  //  myRTC.dd = 9;
  //
  //  myRTC.h = 17;
  //  myRTC.m = 45;
  //  myRTC.s = 30;
  //
  //  myRTC.dow = 4;
  //  myRTC.writeTime();
     
  P.begin(NUM_ZONES);
  P.displaySuspend(false);
  P.setInvert(false);

  P.setZone(ZONE_UPPER, ZONE_SIZE, MAX_DEVICES - 1);
  P.setZone(ZONE_LOWER, 0, ZONE_SIZE - 1);

  //P.setIntensity(Brightness);
  ReadBrightness(); // from EEPROM
  P.setIntensity(ZONE_UPPER, BrightnessUp);
  P.setIntensity(ZONE_LOWER, BrightnessLo);

  //P.setFont(ZONE_UPPER, NumFontUpper);
  //P.setFont(ZONE_LOWER, NumFontLower);
  P.setCharSpacing(ZONE_UPPER, 1);
  P.setCharSpacing(ZONE_LOWER, 1);

  configTime(MY_TZ, MY_NTP_SERVER); // Here is the IMPORTANT ONE LINE for NTP
  //activate the callback
  settimeofday_cb(time_is_set); // optional: callback if time was sent

  WiFi.begin(ssid, password);
  delay ( 500 );

  PRINT("\nConnecting to the WiFi Network named > ", ssid);

  P.setFont(ZONE_UPPER, NumFontUpper); //with abc
  P.setFont(ZONE_LOWER, NumFontUpper); //temporary

  P.displayZoneText(ZONE_UPPER, "Wait", PA_CENTER, SPEED_TIME_UPPER, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(ZONE_LOWER, "WiFi...", PA_CENTER, SPEED_TIME_LOWER, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayAnimate();

  WiFi.softAPdisconnect (true); //no Access point

  wifi_ok = true;
  uint32_t lastWifi = 0;
  while ( wifi_ok && WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    if ( (millis() - lastWifi) >= 10000 ) {
      wifi_ok = false;
    }
    PRINTS( "." );
  }

  ReadFontUpNum(); //from EEPROM
  SetFontUpNum();
  P.setFont(ZONE_LOWER, NumFontLower);
  P.displayZoneText(ZONE_LOWER, szMesg, PA_CENTER, SPEED_TIME_LOWER, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(ZONE_UPPER, szTime, PA_CENTER, SPEED_TIME_UPPER, 0, PA_PRINT, PA_NO_EFFECT);

  P.displayClear();
  PrintIP();

  //  if (MDNS.begin("clock")) {              // Start the mDNS responder for clock.local
  //    PRINTS("\nmDNS responder started");
  //  } else {
  //    PRINTS("\nError setting up MDNS responder!");
  //  }
  server.on("/", HTTP_GET, handleRoot);     // Call the 'handleRoot' function when a client requests URI "/"
  server.on("/Brightness", HTTP_GET, handleBrightness);
  server.on("/Font", HTTP_GET, handleFontNum);
  //server.on("/SetTime", HTTP_GET, handleSetTime);
  server.onNotFound(handleNotFound);
  server.begin();
  PRINTS("\nHTTP server started");

}

void loop(void)
{
  static uint32_t lastTime = 0; // millis() memory
  static uint32_t lastNTP = 0;
  static bool flasher = true;  // seconds passing flasher

  P.displayAnimate();

  if (P.getZoneStatus(ZONE_UPPER))
  {
    P.setTextEffect(ZONE_UPPER, PA_PRINT, PA_NO_EFFECT);

    strcpy(szTime, szTimeNew);

    if ( m == 0 && s == 0 ) {
      P.setTextEffect(ZONE_UPPER, PA_RANDOM, PA_NO_EFFECT);
    }

    P.displayReset(ZONE_UPPER);
  }

  if (P.getZoneStatus(ZONE_LOWER))
  {
    P.setTextEffect(ZONE_LOWER, PA_PRINT, PA_NO_EFFECT);
    strcpy(szMesg, szMesgNew);
    P.displayReset(ZONE_LOWER);
  }

  //get date time from RTC
  if (millis() - lastTime >= 1000) {

    myRTC.readTime();
    lastTime = millis();
    getDate(szMesgNew);
    getTime(szTimeNew, flasher);
    flasher = !flasher;

    //Set display brightness to (0-15)
    //    if ( h > 19 || h < 10 ) {
    //      BrightnessUp = 2;
    //      BrightnessLo = 2;
    //    }
    //    else {
    //      BrightnessUp = 4;
    //      BrightnessLo = 4;
    //    }

    //PRINT("\n WiFi status: ", WiFi.status());
    if (!wifi_ok && WiFi.status() == WL_CONNECTED)
    {
      wifi_ok = true;
      PrintIP();
    } else if (wifi_ok && WiFi.status() != WL_CONNECTED)
    {
      wifi_ok = false;
      PrintIP();
    }
  }

  server.handleClient();                    // Listen for HTTP requests from clients

} //END
