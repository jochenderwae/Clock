#include <WiFiManager.h>
#include <Time.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <RTClib.h>
#include <Wire.h>
#include <TM1637Display.h>
#include <Adafruit_NeoPixel.h>
#include <DNSServer.h>


#define LED_PIN D3
#define LED_COUNT 60
#define DISPLAY1_CLK D7
#define DISPLAY1_DIO D4
#define DISPLAY2_CLK D7
#define DISPLAY2_DIO D6
#define DISPLAY3_CLK D7
#define DISPLAY3_DIO D0
#define BUZZER D8


DateTime prev;
uint32_t WHITE;
bool first = true;
bool doNTP = false;
bool ntpEnabled = false;


Adafruit_NeoPixel seconds = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
TM1637Display display1(DISPLAY1_CLK, DISPLAY1_DIO);
TM1637Display display2(DISPLAY2_CLK, DISPLAY2_DIO);
TM1637Display display3(DISPLAY3_CLK, DISPLAY3_DIO);
RTC_PCF8523 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "be.pool.ntp.org", 0, 3600000);
WiFiServer server(80);
WiFiManager wifiManager;


time_t eastern, utc;
TimeChangeRule CET = {"CET", Last, Sun, Mar, 2, 120};
TimeChangeRule CEST = {"CEST", Last, Sun, Oct, 2, 60};
Timezone Europe(CET, CEST);


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  seconds.begin();
  seconds.setBrightness(20);
  seconds.show();
  WHITE = seconds.Color(255, 255, 255);

  display1.setBrightness(0xff);
  display2.setBrightness(0xff);
  display3.setBrightness(0xff);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }

  if (! rtc.initialized()) {
    Serial.println("RTC is NOT running!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  prev = rtc.now();
  
  wifiManager.autoConnect("Clock");
  timeClient.begin();

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  ntpEnabled = true;
  doNTP = true;
}

void loop() {
  DateTime now = rtc.now();
  updateTimeDisplay(now);
  doNTPUpdate();
  doServer();
  
  //delay(100);
}

void updateTimeDisplay(DateTime now) {
  if(now.hour() != prev.hour() || first) {
    display1.showNumberDecEx(now.hour(), 0b01000000, true, 2, 0);
    doNTP = true;
  }
  
  if(now.minute() != prev.minute() || first) {
    display1.showNumberDecEx(now.minute(), 0b01000000, true, 2, 2);
  }
  
  if(now.day() != prev.day() || first) {
    display2.showNumberDecEx(now.day(), 0b00000000, true, 2, 0);
  }
  
  if(now.month() != prev.month() || first) {
    display2.showNumberDecEx(now.month(), 0b00000000, true, 2, 2);
  }
  
  if(now.year() != prev.year() || first) {
    display3.showNumberDecEx(now.year(), 0b00000000, true, 4, 0);
  }

  if(now.second() != prev.second() || first) {
    seconds.clear();
    seconds.setPixelColor(now.second(), WHITE);
    seconds.show();
    //tone(D8, 1000, 10);
  }
  
  prev = now;
  first = false;
}

void doNTPUpdate() {
  if(doNTP && ntpEnabled) {
    doNTP = false;
    timeClient.update();
    rtc.adjust(Europe.toLocal(timeClient.getEpochTime()));
  }
}


// Auxiliar variables to store the current output state
String output5State = "off";
String output4State = "off";

void doServer() {
  WiFiClient client = server.available();
  String header;

  if (client) {
    header = doHTTPRead(client);
    if(header.length() > 0) {
      doHTTPWrite(client, header);
    }
    client.stop();
  }
}

String doHTTPRead(WiFiClient client) {
  String header;
  String currentLine = "";
  unsigned long httpCurrentTime = millis();
  unsigned long httpPreviousTime = 0;
  static const long httpTimeoutTime = 2000;
  
  httpCurrentTime = millis();
  httpPreviousTime = httpCurrentTime;
  while (client.connected() && httpCurrentTime - httpPreviousTime <= httpTimeoutTime) {
    httpCurrentTime = millis();
    if (client.available()) {
      char c = client.read();
      header += c;
      if (c == '\n') {
        if (currentLine.length() == 0) {
          return header;
        } else {
          currentLine = "";
        }
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }
  return "";
}

void doHTTPWrite(WiFiClient client, String header) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
  
  if (header.indexOf("GET /5/on") >= 0) {
    Serial.println("GPIO 5 on");
    output5State = "on";
  } else if (header.indexOf("GET /5/off") >= 0) {
    Serial.println("GPIO 5 off");
    output5State = "off";
  } else if (header.indexOf("GET /4/on") >= 0) {
    Serial.println("GPIO 4 on");
    output4State = "on";
  } else if (header.indexOf("GET /4/off") >= 0) {
    Serial.println("GPIO 4 off");
    output4State = "off";
  }
  
  client.println("<!DOCTYPE html><html>");
  client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  client.println("<link rel=\"icon\" href=\"data:,\">");
  client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
  client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
  client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
  client.println(".button2 {background-color: #77878A;}</style></head>");
  
  client.println("<body><h1>ESP8266 Web Server</h1>");
  client.println("<p>GPIO 5 - State " + output5State + "</p>");
  if (output5State=="off") {
    client.println("<p><a href=\"/5/on\"><button class=\"button\">ON</button></a></p>");
  } else {
    client.println("<p><a href=\"/5/off\"><button class=\"button button2\">OFF</button></a></p>");
  } 
     
  client.println("<p>GPIO 4 - State " + output4State + "</p>");
  if (output4State=="off") {
    client.println("<p><a href=\"/4/on\"><button class=\"button\">ON</button></a></p>");
  } else {
    client.println("<p><a href=\"/4/off\"><button class=\"button button2\">OFF</button></a></p>");
  }
  client.println("</body></html>");
  client.println();  
}
