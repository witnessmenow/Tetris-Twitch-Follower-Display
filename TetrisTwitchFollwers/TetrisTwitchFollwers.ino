/*******************************************************************
    Displaying Twtich followers on an RGB LED Matrix
    using falling tetris blocks
 *                                                                 *
    Built using an ESP32 and using my own ESP32 Matrix Shield
    https://www.tindie.com/products/brianlough/esp32-matrix-shield-mini-32/

    Written by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

// ----------------------------
// Standard Libraries - Already Installed if you have ESP32 set up
// ----------------------------

#include <Ticker.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <WiFiManager.h>
// Library for allowing users to configure their own
// Wifi details.
//
// More details here: https://www.youtube.com/watch?v=A-P20vC7zq4&list=PLbd5_U5QzQgZ8Ni8F48G2gnMgCkN6saUT
//
// NOTE: Need to install the development branch from github
// for ESP32 support
// https://github.com/tzapu/WiFiManager/tree/development

#include <TwitchApi.h>
// Library for fetching the data from the Twtich API
//
// Install from the library manager
// https://github.com/witnessmenow/arduino_twitch_api

#include <ArduinoJson.h>
// JSON parsing library required by the Twitch API library
// NOTE: You must install 5.X, as the Twitch library doesnt
// support V6
//
// Install from the library manager (use the drop down to change version)
// https://github.com/bblanchon/ArduinoJson

#define double_buffer // this must be enabled to stop flickering
#include <PxMatrix.h>
// The library for controlling the LED Matrix
//
// Install from the library manager
// https://github.com/2dom/PxMatrix

// Adafruit GFX library is a dependancy for the PxMatrix Library
// Can be installed from the library manager
// https://github.com/adafruit/Adafruit-GFX-Library

#include <TetrisMatrixDraw.h>
// This library draws out characters using a tetris block
// amimation
// Can be installed from the library manager
// https://github.com/toblum/TetrisAnimation

// ----------------------------
// Config - Replace These
// ----------------------------

// Create a new application on https://dev.twitch.tv/
#define TWITCH_CLIENT_ID "1234567890654rfscgthyhuj"

// Username of who you are getting the data for (e.g. "shroud")
#define TWITCH_LOGIN "mrlukesgames"

// You can use the twitch library to get this ID
// Use the "userAndFollowerData" example to get it
#define TWITCH_ID "55551428"

// ----------------------------
// Wiring and Display setup
// ----------------------------

#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 2
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// This defines the 'on' time of the display is us. The larger this number,
// the brighter the display. If too large the ESP will crash
uint8_t display_draw_time = 10; //10-50 is usually fine


#define SCREEN_HEIGHT 32
#define SCREEN_WIDTH 64

PxMATRIX display(SCREEN_WIDTH, SCREEN_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);

TetrisMatrixDraw tetris(display); // Counter

// Some standard colors
uint16_t myRED = display.color565(255, 0, 0);
uint16_t myGREEN = display.color565(0, 255, 0);
uint16_t myBLUE = display.color565(0, 0, 255);
uint16_t myWHITE = display.color565(255, 255, 255);
uint16_t myYELLOW = display.color565(255, 255, 0);
uint16_t myCYAN = display.color565(0, 255, 255);
uint16_t myMAGENTA = display.color565(255, 0, 255);
uint16_t myBLACK = display.color565(0, 0, 0);

Ticker animation_ticker;
bool finishedAnimating = false;
unsigned long resetAnimationDue = 0;

WiFiClientSecure client;
TwitchApi twitch(client, TWITCH_CLIENT_ID);

unsigned long timeBetweenRequests = 10000; // 10 seconds
unsigned long apiRequestDue = 0; // When API request is next Due

String followerCountStr = "";
long followerCount = 0;

void IRAM_ATTR display_updater() {
  display.display(display_draw_time);
}


void display_update_enable(bool is_enable)
{
  if (is_enable)
  {
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &display_updater, true);
    timerAlarmWrite(timer, 2000, true);
    timerAlarmEnable(timer);
  }
  else
  {
    timerDetachInterrupt(timer);
    timerAlarmDisable(timer);
  }
}

// Will center the given text
void displayText(String text, int yPos) {
  int16_t  x1, y1;
  uint16_t w, h;
  display.setTextSize(2);
  char charBuf[text.length() + 1];
  text.toCharArray(charBuf, text.length() + 1);
  display.setTextSize(1);
  display.getTextBounds(charBuf, 0, yPos, &x1, &y1, &w, &h);
  int startingX = 33 - (w / 2);
  display.setTextSize(1);
  display.setCursor(startingX, yPos);
  Serial.println(startingX);
  Serial.println(yPos);
  display.print(text);
}

void drawStuff()
{
  // Not clearing the display and redrawing it when you
  // dont need to improves how the refresh rate appears
  if (!finishedAnimating) {

    // Step 1: Clear the display
    display.clearDisplay();

    // Step 2: draw tetris
    if (followerCount != 0) {
      int textLength = ((followerCountStr.length() * 7) * tetris.scale) - 1;
      int xPos = SCREEN_WIDTH / 2 - textLength / 2;

      int ypos = (tetris.scale == 2) ? 28 : 22;
      finishedAnimating = tetris.drawNumbers(xPos, ypos);
    }

    // Step 3: Display the buffer
    display.showBuffer();
  }
}

void setup() {
   WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

    // put your setup code here, to run once:
    Serial.begin(115200);
    
    // WiFi.mode(WiFi_STA); // it is a good practice to make sure your code sets wifi mode how you want it.

    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    //reset settings - wipe credentials for testing
    //wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect("MrLukesGames","lukelucan"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }


  // Define your display layout here, e.g. 1/8 step
  display.begin(16);

  // Define your scan pattern here {LINE, ZIGZAG, ZAGGIZ, WZAGZIG, VZAG} (default is LINE)
  //display.setScanPattern(LINE);

  // Define multiplex implemention here {BINARY, STRAIGHT} (default is BINARY)
  //display.setMuxPattern(BINARY);

  // -----------
  // A splash screen, only shown at the start
  // -----------

  display.setFastUpdate(true);
  display.clearDisplay();
  display.setTextColor(myCYAN);
  display.setCursor(2, 0);
  display.print("Mr");
  display.setTextColor(myMAGENTA);
  display.setCursor(2, 8);
  display.print("Lukes");
  display.setTextColor(myCYAN);
  display.setCursor(2, 16);
  display.print("Games");
  display.setTextColor(myMAGENTA);
  display.setCursor(2, 24);
  display_update_enable(true);
  display.showBuffer();

  delay(3000);

  animation_ticker.attach(0.10, drawStuff);
}

String getCommas(String number) {
  int commaCount = (number.length() - 1) / 3;
  String numberWithCommas = "";
  numberWithCommas.reserve(50);
  int commaOffsetIndex = 0;
  for (int j = 0; j < commaCount; j++) {
    commaOffsetIndex = number.length() - 3;
    numberWithCommas = "," + number.substring(commaOffsetIndex) + numberWithCommas;
    number.remove(commaOffsetIndex);
  }

  numberWithCommas = number + numberWithCommas;

  return numberWithCommas;
}


void loop() {
  if (millis() > apiRequestDue)  {
    FollowerData followerData = twitch.getFollowerData(TWITCH_ID);
    if (!followerData.error) {
      Serial.println("---------Follower Data ---------");

      Serial.print("Number of Followers: ");
      Serial.println(followerData.total);

      followerCountStr = String(followerData.total);
      followerCount = followerData.total;
      Serial.print("Last Follower Id: ");
      Serial.println(followerData.fromId);

      Serial.print("Last Follower Name: ");
      Serial.println(followerData.fromName);

      Serial.print("Last Follower to Id: ");
      Serial.println(followerData.toId);

      Serial.print("Last Follower to Name: ");
      Serial.println(followerData.toName);

      Serial.print("Last Follower at: ");
      Serial.println(followerData.followedAt);

      Serial.println("------------------------");

      // Only 4 digits will fit when scaled to 2
      if (followerCount < 10000) {
        tetris.scale = 2;
      } else {
        tetris.scale = 1;
      }
      tetris.setNumbers(followerCount);
      finishedAnimating = false;
      apiRequestDue = millis() + (timeBetweenRequests);
    }
    else {
      Serial.println("Twitch data fetch failed");

      //Shorter request time if it failed.
      apiRequestDue = millis() + (timeBetweenRequests / 10);
    }
  }
}
