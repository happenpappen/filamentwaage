#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Q2HX711.h>
#include <FreeMono12pt7b.h>
#include <clickButton.h>
#include <MQTT.h>
#include <MQTT_credentials.h>
#include "WebServer.h"

#define SETTINGS_MAGIC 48
#define SETTINGS_START 1

// my DeviceID:
const String myID = System.deviceID();

bool debug = true;

// Pushbutton vars:
const int TARA_BUTTON = D5;
ClickButton taraButton(TARA_BUTTON, LOW, CLICKBTN_PULLUP);
// Button results
int function = 0;
bool start_calibration = false;
volatile uint32_t msBlockOut;

// MQTT vars:
void mqtt_callback(char *, byte *, unsigned int);
void publishState();
void saveSettings();
void loadSettings();
MQTT client(MQTT_HOST, 1883, mqtt_callback);
Timer PublisherTimer(5000, publishState);

// Display vars:
const int TEXTLINE1_PIXEL = 13;
const int TEXTLINE2_PIXEL = 30;
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = D2;
const int LOADCELL_SCK_PIN = D3;
Q2HX711 waage(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
boolean scale_calibrated = false;
float calibrate_weight = 175.0; // calibrated mass to be added
float tara_weight = 0.0;        // weight to be substracted from current reading
long x1 = 0L;
long x0 = 0L;
float avg_size = 10.0; // amount of averages for each mass measurement

ApplicationWatchdog *wd = NULL;

// Webserver related:
//
#define VERSION_STRING "0.1"

// ROM-based messages used by the application
// These are needed to avoid having the strings use up our limited
//    amount of RAM.

P(Page_start) = "<!DOCTYPE html><html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\"><title>Doorbell settings</title><style> div, fieldset, input, select { padding: 5px; font-size: 1em; } fieldset { background-color: #f2f2f2; } p { margin: 0.5em 0; } input { width: 100%; box-sizing: border-box; -webkit-box-sizing: border-box; -moz-box-sizing: border-box; } input[type=checkbox], input[type=radio] { width: 1em; margin-right: 6px; vertical-align: -1px; } select { width: 100%; } textarea { resize: none; width: 98%; height: 318px; padding: 5px; overflow: auto; } body { text-align: center; font-family: verdana; } td { padding: 0; } button { border: 0; border-radius: 0.3rem; background-color: #1fa3ec; color: #fff; line-height: 2.4rem; font-size: 1.2rem; width: 100%; -webkit-transition-duration: 0.4s; transition-duration: 0.4s; cursor: pointer; } button:hover { background-color: #0e70a4; } .bred { background-color: #d43535; } .bred:hover { background-color: #931f1f; } .bgrn { background-color: #47c266; } .bgrn:hover { background-color: #5aaf6f; } a { text-decoration: none; } .p { float: left; text-align: left; } .q { float: right; text-align: right; } input[type=range] { height: 34px; -webkit-appearance: none; margin: 10px 0; width: 100%; } input[type=range]:focus { outline: none; } input[type=range]::-webkit-slider-runnable-track { width: 100%; height: 12px; cursor: pointer; animate: 0.2s; box-shadow: 1px 1px 1px #002200; background: #205928; border-radius: 1px; border: 1px solid #18D501; } input[type=range]::-webkit-slider-thumb { box-shadow: 3px 3px 3px #00AA00; border: 2px solid #83E584; height: 23px; width: 23px; border-radius: 23px; background: #439643; cursor: pointer; -webkit-appearance: none; margin-top: -7px; } input[type=range]:focus::-webkit-slider-runnable-track { background: #205928; } input[type=range]::-moz-range-track { width: 100%; height: 12px; cursor: pointer; animate: 0.2s; box-shadow: 1px 1px 1px #002200; background: #205928; border-radius: 1px; border: 1px solid #18D501; } input[type=range]::-moz-range-thumb { box-shadow: 3px 3px 3px #00AA00; border: 2px solid #83E584; height: 23px; width: 23px; border-radius: 23px; background: #439643; cursor: pointer; } input[type=range]::-ms-track { width: 100%; height: 12px; cursor: pointer; animate: 0.2s; background: transparent; border-color: transparent; color: transparent; } input[type=range]::-ms-fill-lower { background: #205928; border: 1px solid #18D501; border-radius: 2px; box-shadow: 1px 1px 1px #002200; } input[type=range]::-ms-fill-upper { background: #205928; border: 1px solid #18D501; border-radius: 2px; box-shadow: 1px 1px 1px #002200; } input[type=range]::-ms-thumb { margin-top: 1px; box-shadow: 3px 3px 3px #00AA00; border: 2px solid #83E584; height: 23px; width: 23px; border-radius: 23px; background: #439643; cursor: pointer; } input[type=range]:focus::-ms-fill-lower { background: #205928; } input[type=range]:focus::-ms-fill-upper { background: #205928; } </style></head><body>";
P(Page_end) = "</body></html>";
P(form_start) = "<div style=\"text-align:left;display:inline-block;min-width:340px;\"><div style=\"text-align:center;\"><noscript>JavaScript aktivieren diese Seit benutzen zu können<br/></noscript><h2>Filamentwaage</h2></div><fieldset><legend><b>&nbsp;Einstellungen&nbsp;</b></legend><form action=\"index.html\" oninput=\"x.value=parseInt(volume.value)\" method=\"POST\">";
P(form_end) = "<input type=\"submit\" value=\"Speichern\" class=\"button bgrn\"></form></fieldset></div>";
P(form_play_melody) = "<input type=\"submit\" value=\"Melodie spielen\" class=\"button\">";
P(form_field_melody_start) = "<label for=\"melody\">Melodie:</label><select id=\"melody\" name=\"melody_nr\">";
P(form_field_melody_end) = "</select><p/>";
P(form_field_volume_start) = "<label for=\"volume\">Lautstärke (<output name=\"x\" for=\"volume\">";
P(form_field_volume_end) = "</output>):</label><input id=\"volume\" name=\"volume\" type=\"range\" min=\"0\" max=\"30\" ";
P(form_field_silence_begin_start) = "<label for=\"silence_begin\">Ruhezeit Start:</label><input id=\"silence_begin\" name=\"silence_begin\" type=\"time\" value=\"";
P(form_field_silence_begin_end) = "\"/><p/>";
P(form_field_silence_end_start) = "<label for=\"silence_end\">Ruhezeit Ende:</label><input id=\"silence_end\" name=\"silence_end\" type=\"time\" value=\"";
P(form_field_silence_end_end) = "\"/><p/>";
P(form_field_silence_unchecked) = "<label for=\"silence_enabled\">Ruhezeit aktivieren:</label><input id=\"silence_enabled\" name=\"silence_enabled\" type=\"checkbox\"/><p/>";
P(form_field_silence_checked) = "<label for=\"silence_enabled\">Ruhezeit aktivieren:</label><input id=\"silence_enabled\" name=\"silence_enabled\" type=\"checkbox\" checked /><p/>";
P(form_field_debug_unchecked) = "<label for=\"debug\">Debug:</label><input id=\"debug\" name=\"debug\" type=\"checkbox\"/><p/>";
P(form_field_debug_checked) = "<label for=\"debug\">Debug:</label><input id=\"debug\" name=\"debug\" type=\"checkbox\" checked /><p/>";
P(form_last_visit) = "Letzter Besuch: ";

// Values in POST request:
//
static const char valMelody[] PROGMEM = "melody_nr";
static const char valVolume[] PROGMEM = "volume";
static const char valSilenceBegin[] PROGMEM = "silence_begin";
static const char valSilenceEnd[] PROGMEM = "silence_end";
static const char valSilenceEnabled[] PROGMEM = "silence_enabled";
static const char valDebug[] PROGMEM = "debug";
static const char valPlayMelody[] PROGMEM = "play_melody";

/* This creates an instance of the webserver.  By specifying a prefix
 * of "", all pages will be at the root of the server. */
#define PREFIX ""
WebServer webserver(PREFIX, 80);

/* commands are functions that get called by the webserver framework
 * they can read any posted data from client, and they output to the
 * server to send data back to the web browser. */
void indexCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  // Variables for parsing POST parameters:
  //
  bool repeat = false, debug_parsed = false, silence_enabled_parsed = false;
  char name[20], value[48];
  int int_value = 0,
      valLen = 0;

  String cmd, password, prm;

  /* this line sends the standard "we're all OK" headers back to the
     browser */
  server.httpSuccess();

  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type == WebServer::HEAD)
    return;

  server.printP(Page_start);

  switch (type)
  {
  case WebServer::POST:
    do
    {
      repeat = server.readPOSTparam(name, sizeof(name), value, sizeof(value));
      valLen = strlen(value);
      if (debug)
      {
        Particle.publish("DEBUG: Post-Parameter: '" + String(name) + "' read, Value: " + String(value), PRIVATE);
      }
      if (!strcmp_P(name, valMelody))
      {
        int_value = atoi(value);
      }
      else if (!strcmp_P(name, valVolume))
      {
        int_value = atoi(value);
      }
      else if (!strcmp_P(name, valSilenceEnabled))
      {
      }
      else if (!strcmp_P(name, valSilenceBegin))
      {
      }
      else if (!strcmp_P(name, valSilenceEnd))
      {
      }
      else if (!strcmp_P(name, valDebug))
      {
        if (!strcmp_P("on", value))
        {
          debug = true;
        }
        else
        {
          debug = false;
        }
      }
    } while (repeat);
    saveSettings();
    break;
  default:
    break;
  }

  server.printP(form_start);

  server.printP(form_field_melody_start);
  for (int i = 1; i < 4; i++)
  {
    if (i == 1)
    {
      server.printf("<option selected>%d</option>", i);
    }
    else
    {
      server.printf("<option>%d</option>", i);
    }
  }
  server.printP(form_field_melody_end);

  server.printP(form_field_volume_start);
  server.printf("%d", 20);
  server.printP(form_field_volume_end);
  server.printf("value=\"%d\"/>", 20);

  server.radioButton("silence_enabled", "on", "Ruhezeit aktivieren<p/>", true);
  server.radioButton("silence_enabled", "off", "Ruhezeit deaktivieren<p/>", false);

  server.printP(form_field_silence_begin_start);
  server.printP(form_field_silence_begin_end);

  server.printP(form_field_silence_end_start);
  server.printP(form_field_silence_end_end);

  server.radioButton("debug", "on", "Debug an<p/>", debug ? true : false);
  server.radioButton("debug", "off", "Debug aus<p/>", debug ? false : true);

  server.printP(form_play_melody);

  server.printP(form_end);
  server.printP(Page_end);
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  // handle message arrived - we are only subscribing to one topic so assume all are led related
  String myTopic = String(topic);

  bool stateChanged = false;
  char *myPayload = NULL;

  myPayload = (char *)malloc(length + 1);

  memcpy(myPayload, payload, length);
  myPayload[length] = 0;

  if (!client.isConnected())
  {
    client.connect(myID, MQTT_USER, MQTT_PASSWORD);
  }

  client.publish("/" + myID + "/state/LastPayload", "Last Payload: " + String(myPayload));

  free(myPayload);
  myPayload = NULL;
}

void loadSettings()
{
  uint16_t maxSize = EEPROM.length();
  uint16_t address = 1;
  uint8_t version = 0;

  EEPROM.get(address++, version);

  if (version == SETTINGS_MAGIC)
  { // Valid settings in EEPROM?
    Serial.println("Valid settings found - reading from EEPROM.");
    EEPROM.get(address, scale_calibrated);
    address = address + sizeof(scale_calibrated);
    EEPROM.get(address, calibrate_weight);
    address = address + sizeof(calibrate_weight);
    EEPROM.get(address, tara_weight);
    address = address + sizeof(tara_weight);
  }
  else
  {
    Serial.println("No valid settings found - setting defaults.");
    scale_calibrated = false;
    calibrate_weight = 175.0;
    tara_weight = 0.0;
    saveSettings();
  }
}

void saveSettings()
{
  uint16_t address = SETTINGS_START;
  uint16_t maxlength = EEPROM.length();

  EEPROM.write(address++, SETTINGS_MAGIC);
  EEPROM.put(address, scale_calibrated);
  address = address + sizeof(scale_calibrated);
  EEPROM.put(address, calibrate_weight);
  address = address + sizeof(calibrate_weight);
  EEPROM.put(address, tara_weight);
  address = address + sizeof(tara_weight);
}

void publishState()
{

  if (!client.isConnected())
  {
    client.connect(myID, MQTT_USER, MQTT_PASSWORD);
  }

  if (client.isConnected())
  {
    client.publish("/" + myID + "/state/CalibrateWeight", String(calibrate_weight));
    if (scale_calibrated)
    {
      client.publish("/" + myID + "/state/ScaleCalibrated", "true");
    }
    else
    {
      client.publish("/" + myID + "/state/ScaleCalibrated", "false");
    }
    client.publish("/" + myID + "/state/TaraWeight", String(tara_weight));
    client.publish("/" + myID + "/state/FirmwareVersion", System.version());
  }
}

void display_lines(String line1, String line2)
{
  display.clearDisplay();
  display.setCursor(0, TEXTLINE1_PIXEL);
  display.println(line1);
  display.setCursor(0, TEXTLINE2_PIXEL);
  display.println(line2);
  display.display();
}

void init_display(void)
{

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setFont(&FreeMono12pt7b);
  display_lines("Fil.-Waage", "FW V0.1");
}

void calibrate_scale(void)
{

  unsigned long calib_start = millis();

  display_lines("Kalibr.", "gestartet.");
  Serial.println("Kalibrierung gestartet.");

  delay(1000); // allow load cell and hx711 to settle
  // tare procedure
  for (int ii = 0; ii < int(avg_size); ii++)
  {
    delay(10);
    x0 += waage.read();
  }
  x0 /= long(avg_size);
  display_lines(String::format("%4.2f", calibrate_weight) + "g", "auflegen.");
  Serial.println(String::format("%4.2f", calibrate_weight) + "g Gewicht auflegen.");
  // calibration procedure (mass should be added equal to calibrate_weight)
  int ii = 1;
  while (true)
  {
    if (millis() - calib_start > 30000)
    {
      display_lines("Kalibr.", "abgebrochen.");
      Serial.println("Kalibrierung abgebrochen.");
      return;
    }
    if (waage.read() < x0 + 10000)
    {
    }
    else
    {
      ii++;
      delay(2000);
      for (int jj = 0; jj < int(avg_size); jj++)
      {
        x1 += waage.read();
      }
      x1 /= long(avg_size);
      break;
    }
  }
  display_lines("Kalibr.", "erfolgreich.");
  Serial.println("Kalibrierung abgeschlossen.");
}

void button_pressed()
{
  if (millis() - msBlockOut < 30)
    return; // 30ms blockout periode
  msBlockOut = millis();

  if (digitalRead(TARA_BUTTON) == LOW)
  {
    start_calibration = true;
  }
}

void setup(void)
{
  Serial.begin(115200);
  pinMode(TARA_BUTTON, INPUT);
  //attachInterrupt(TARA_BUTTON, button_pressed, FALLING);
  init_display();
  delay(3000);
  loadSettings();

  // Setup button timers (all in milliseconds / ms)
  // (These are default if not set, but changeable for convenience)
  taraButton.debounceTime = 30;    // Debounce timer in ms
  taraButton.multiclickTime = 250; // Time limit for multi clicks
  taraButton.longClickTime = 1000; // time until "held-down clicks" register

  client.connect(myID, MQTT_USER, MQTT_PASSWORD); // uid:pwd based authentication

  if (client.isConnected())
  {
    PublisherTimer.start();
    client.subscribe("/" + myID + "/set/+");
  }

  // Init webserver:
  //
  /* setup our default command that will be run when the user accesses
    * the root page on the server */
  webserver.setDefaultCommand(&indexCmd);

  /* setup our default command that will be run when the user accesses
    * a page NOT on the server */
  webserver.setFailureCommand(&indexCmd);

  /* run the same command if you try to load /index.html, a common
    * default page name */
  webserver.addCommand("index.html", &indexCmd);

  /* start the webserver */
  webserver.begin();

  // Start watchdog. Reset the system after 60 seconds if
  // the application is unresponsive.
  wd = new ApplicationWatchdog(60000, System.reset, 1536);
}

void loop(void)
{
  char buff[128];
  int len = 128;
  long reading = 0;
  float ratio = 0.0,
        ratio_1 = 0.0,
        ratio_2 = 0.0,
        mass = 0.0;

  /*
  if (start_calibration) {
      noInterrupts();
      display_lines("Click", "calibrating");
      Serial.println("Click - calibrating.");
      calibrate_scale();
      saveSettings();
      start_calibration = false;
      interrupts();
  }
*/
  taraButton.Update();

  // Save click codes in "function", as click codes are reset at next Update()
  if (taraButton.clicks != 0)
    function = taraButton.clicks;

  switch (function)
  {
  case 1:
    noInterrupts();
    display_lines("Click", "calibrating");
    Serial.println("Click - calibrating.");
    calibrate_scale();
    saveSettings();
    function = 0;
    interrupts();
    break;
  case -1:
    noInterrupts();
    display_lines("Long click", "setting weight to 0.");
    Serial.println("Long click - setting weight to 0.");
    reading = 0;
    for (int jj = 0; jj < int(avg_size); jj++)
    {
      reading += waage.read();
    }
    reading /= long(avg_size);
    // calculating mass based on calibration and linear fit
    ratio_1 = (float)(reading - x0);
    ratio_2 = (float)(x1 - x0);
    ratio = ratio_1 / ratio_2;
    tara_weight = calibrate_weight * ratio;

    function = 0;
    interrupts();
    break;
  default:
    function = 0;
    break;
  }

  // averaging reading
  reading = 0;

  for (int jj = 0; jj < int(avg_size); jj++)
  {
    reading += waage.read();
  }

  reading /= long(avg_size);
  // calculating mass based on calibration and linear fit
  ratio_1 = (float)(reading - x0);
  ratio_2 = (float)(x1 - x0);
  ratio = ratio_1 / ratio_2;
  mass = calibrate_weight * ratio;
  Serial.print("Raw: ");
  Serial.print(reading);
  Serial.print(", ");
  Serial.println(mass);

  display_lines("Gewicht:", String::format("%4.2f", mass) + "g");

  if (client.isConnected())
  {
    client.loop();
  }
  else
  {
    client.connect(myID, MQTT_USER, MQTT_PASSWORD);
  }

  /* process incoming connections one at a time forever */
  webserver.processConnection(buff, &len);
}
