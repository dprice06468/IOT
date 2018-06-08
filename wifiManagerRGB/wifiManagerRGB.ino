//#pragma once

char scene[40] = "candystore";
char device_name[40] = "led";
char mqtt_server[16] = "192.168.1.31";
char mqtt_port[6] = "1883";

#include <FS.h> //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h> //tested with v2.6.0
#include <Ticker.h>

//flag for saving data
bool shouldSaveConfig = false;
const char* CONFIG_FILE = "/config.json";
// Indicates whether ESP has WiFi credentials saved from previous session
bool initialConfig = false;

void LED_RED();
void LED_GREEN();
void LED_BLUE();
void LED_W1();
void LED_W2();
void change_LED();
int convertToInt(char upper, char lower);

#define PWM_VALUE 63
int gamma_table[PWM_VALUE + 1] = {
   0, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 8, 9, 10,
   11, 12, 13, 15, 17, 19, 21, 23, 26, 29, 32, 36, 40, 44, 49, 55,
   61, 68, 76, 85, 94, 105, 117, 131, 146, 162, 181, 202, 225, 250,
   279, 311, 346, 386, 430, 479, 534, 595, 663, 739, 824, 918, 1023
};

// PIN definitions
#define REDPIN              15  //12
#define GREENPIN            13  //15
#define BLUEPIN             12  //13
#define WHITE1PIN           14
#define WHITE2PIN           4
#define onboardGreenLEDPIN  5
#define onboardRedLEDPIN    1
#define TRIGGER_PIN         0   //configuration trigger pin

// note 
// TX GPIO2 @Serial1 (Serial1 ONE)
// RX GPIO3 @Serial1    

#define RedLEDoff digitalWrite(onboardRedLEDPIN,HIGH)
#define RedLEDon digitalWrite(onboardRedLEDPIN,LOW)
#define GreenLEDoff digitalWrite(onboardGreenLEDPIN,HIGH)
#define GreenLEDon digitalWrite(onboardGreenLEDPIN,LOW)

//MQTT settings
#define MQTT_ONLINE   "online"     //topic published when node is online
#define SETRGB        0
#define FADERGB       1
#define SETWHITE      2
#define FADEWHITE     3
#define OFF           4
#define FADEOFF       5
int mqttActionsCount = 6;
String mqttActions[] = {"setrgb", "fadergb", "setwhite", "fadewhite", "off", "fadeoff"};
boolean fadeLED = false;

int led_delay_red = 0;
int led_delay_green = 0;
int led_delay_blue = 0;
int led_delay_w1 = 0;
int led_delay_w2 = 0;
#define time_at_colour 100 
unsigned long TIME_LED_RED = 0;
unsigned long TIME_LED_GREEN = 0;
unsigned long TIME_LED_BLUE = 0;
unsigned long TIME_LED_W1 = 0;
unsigned long TIME_LED_W2 = 0;
int RED, GREEN, BLUE, W1, W2;
int RED_A = 0;
int GREEN_A = 0;
int BLUE_A = 0;
int W1_A = 0;
int W2_A = 0;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

Ticker ticker;

//**************************************************************
//--------------------------------------------------------------
//--------------------------------------------------------------
void setupLogic() {
  Serial1.begin(115200);

  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);
  pinMode(WHITE1PIN, OUTPUT);
  pinMode(WHITE2PIN, OUTPUT);
}

//-------------------------------------------------------------------
//loop routine logic
//-------------------------------------------------------------------
void loopLogic() {
//  RedLEDon;
//  GreenLEDoff;
//  delay(300);
//  RedLEDoff;
//  GreenLEDon;
//  delay(300);
}

//--------------------------------------------------------------
//routine called when an mqtt message is received
//--------------------------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial1.print("Message arrived [");
  Serial1.print(topic);
  Serial1.print("] ");
  for (int i = 0; i < length; i++) {
    Serial1.print((char)payload[i]);
  }
  Serial1.println();

  if ((String(topic).indexOf("rgb") != -1)) {
    Serial1.println("rgb topic");
    long r = convertToInt((char)payload[0], (char)payload[1]); //red
    long g = convertToInt((char)payload[2], payload[3]); //green
    long b = convertToInt((char)payload[4], (char)payload[5]); //blue

    int red = map(r, 0, 255, 0, PWM_VALUE);
    red = constrain(red, 0, PWM_VALUE);
    int green = map(g, 0, 255, 0, PWM_VALUE);
    green = constrain(green, 0, PWM_VALUE);
    int blue = map(b, 0, 255, 0, PWM_VALUE);
    blue = constrain(blue, 0, PWM_VALUE);

    RED = gamma_table[red];
    GREEN = gamma_table[green];
    BLUE = gamma_table[blue];
    W1 = 0;
    W2 = 0;
  } else if ((String(topic).indexOf("white") != -1)) {
    Serial1.println("white topic");
    long w = convertToInt(payload[0], payload[1]); //w1
    int w1 = map(w, 0, 255, 0, 1023);
    w1 = constrain(w1, 0, 1023);

    RED = 0;
    GREEN = 0;
    BLUE = 0;
    W1 = w1;
    W2 = w1;
  } else if ((String(topic).indexOf("off") != -1)) {
    Serial1.println("off topic");
    RED = 0;
    GREEN = 0;
    BLUE = 0;
    W1 = 0;
    W2 = 0;
  }
  
  Serial1.print("RED: "); Serial1.println(RED);
  Serial1.print("GREEN: "); Serial1.println(GREEN);
  Serial1.print("BLUE: "); Serial1.println(BLUE);
  Serial1.print("W1: "); Serial1.println(W1);
  Serial1.print("W2: "); Serial1.println(W2);

  if ((String(topic).indexOf("fade") != -1)) {
    Serial1.println("fade topic");
    fadeLED = true;
    fade_LED();
  } else {  
    Serial1.println("instant topic");
    fadeLED = false;
    change_LED(); 
  }
}
//**************************************************************

//-------------------------------------------------------------
//get wifi parms from SPIFFS and connect to wifi
//-------------------------------------------------------------
void setup_wifi2() {
  delay(10);
  // Mount the filesystem
  bool result = SPIFFS.begin();
  Serial1.println("SPIFFS opened: " + result);

  if (!readConfigFile()) {
    Serial1.println("Failed to read configuration file, using default values");
  }

  WiFi.printDiag(Serial1); //Remove this line if you do not want to see WiFi password printed

  if (WiFi.SSID() == "") {
    Serial1.println("We haven't got any access point credentials, so get them now");
    initialConfig = true;
  } else {
    GreenLEDoff;  // Turn LED off as we are not in configuration mode.
    //digitalWrite(onboardRedLEDPIN, HIGH); 
    WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
    unsigned long startedAt = millis();
    Serial1.print("After waiting ");
    int connRes = WiFi.waitForConnectResult();
    float waited = (millis()- startedAt);
    Serial1.print(waited/1000);
    Serial1.print(" secs in setup() connection result is ");
    Serial1.println(connRes);
    Serial1.println("");
    delay(1000);
    Serial1.println("WiFi connected");
    Serial1.println("IP address: ");
    Serial1.println(WiFi.localIP());
  }
}

//--------------------------------------------------------------
// Attempt connection to MQTT broker and subscribe to command topic
//--------------------------------------------------------------
void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial1.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("device_name")) {
      Serial1.println("connected");

      String mqttTopic = String("");
      char __mqttTopic[100];

      for (int i=0; i<mqttActionsCount; i++) {
        mqttTopic = String(scene) + "/" + String(device_name) + "/" + mqttActions[i] + "/";
        mqttTopic.toCharArray(__mqttTopic, mqttTopic.length());
        mqttClient.subscribe(__mqttTopic);
        Serial1.print("subscribing to mqttTopic: "); Serial1.println(mqttTopic);
      
        mqttTopic = "allscenes/" + String(device_name) + "/" + mqttActions[i] + "/";
        mqttTopic.toCharArray(__mqttTopic, mqttTopic.length());
        mqttClient.subscribe(__mqttTopic);
        Serial1.print("subscribing to mqttTopic: "); Serial1.println(mqttTopic);

        mqttTopic = String(scene) + "/alldevices/" + mqttActions[i] + "/";
        mqttTopic.toCharArray(__mqttTopic, mqttTopic.length());
        mqttClient.subscribe(__mqttTopic);
        Serial1.print("subscribing to mqttTopic: "); Serial1.println(mqttTopic);

        mqttTopic = "allscenes/alldevices/" + mqttActions[i] + "/";
        mqttTopic.toCharArray(__mqttTopic, mqttTopic.length());
        mqttClient.subscribe(__mqttTopic);
        Serial1.print("subscribing to mqttTopic: "); Serial1.println(mqttTopic);
      }

      // Once connected, publish an announcement...
      mqttTopic = String(scene) + "/" + String(device_name) + "/" + MQTT_ONLINE;
      mqttTopic.toCharArray(__mqttTopic, mqttTopic.length()+1);
      Serial1.print("publishing: "); Serial1.println(__mqttTopic);
      mqttClient.publish(__mqttTopic, device_name);
    }
    else {
      Serial1.print("failed, rc=");
      Serial1.print(mqttClient.state());
      Serial1.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//--------------------------------------------------------------
//callback notifying us of the need to save config
//--------------------------------------------------------------
void saveConfigCallback () {
  Serial1.println("Should save config");
  shouldSaveConfig = true;
}

//-------------------------------------------------------------
//-------------------------------------------------------------
void change_LED() {
  analogWrite(REDPIN, RED);
  analogWrite(GREENPIN, GREEN);
  analogWrite(BLUEPIN, BLUE);
  analogWrite(WHITE1PIN, W1);
  analogWrite(WHITE2PIN, W1);
}

//-------------------------------------------------------------
//-------------------------------------------------------------
void fade_LED()
{
  int diff_red = abs(RED - RED_A);
  if (diff_red > 0)
  {
    led_delay_red = time_at_colour / abs(RED - RED_A);
  }
  else
  {
    led_delay_red = time_at_colour / 1023;
  }

  int diff_green = abs(GREEN - GREEN_A);
  if (diff_green > 0)
  {
    led_delay_green = time_at_colour / abs(GREEN - GREEN_A);
  }
  else
  {
    led_delay_green = time_at_colour / 1023;
  }

  int diff_blue = abs(BLUE - BLUE_A);
  if (diff_blue > 0)
  {
    led_delay_blue = time_at_colour / abs(BLUE - BLUE_A);
  }
  else
  {
    led_delay_blue = time_at_colour / 1023;
  }

  int diff_w1 = abs(W1 - W1_A);
  if (diff_w1 > 0)
  {
    led_delay_w1 = time_at_colour / abs(W1 - W1_A);
  }
  else
  {
    led_delay_w1 = time_at_colour / 1023;
  }

  int diff_w2 = abs(W2 - W2_A);
  if (diff_w2 > 0)
  {
    led_delay_w2 = time_at_colour / abs(W2 - W2_A);
  }
  else
  {
    led_delay_w2 = time_at_colour / 1023;
  }
}

//-------------------------------------------------------------
//-------------------------------------------------------------
void LED_RED()
{
  if (RED != RED_A) {
    if (RED_A > RED) RED_A = RED_A - 1;
    if (RED_A < RED) RED_A++;
    analogWrite(REDPIN, RED_A);
//Serial1.print(millis()); Serial1.print(" - red: "); Serial1.print(RED); Serial1.print("red_a: "); Serial1.println(RED_A);
  }
}

//-------------------------------------------------------------
//-------------------------------------------------------------
void LED_GREEN()
{
  if (GREEN != GREEN_A) {
    if (GREEN_A > GREEN) GREEN_A = GREEN_A - 1;
    if (GREEN_A < GREEN) GREEN_A++;
    analogWrite(GREENPIN, GREEN_A);
  }
}

//-------------------------------------------------------------
//-------------------------------------------------------------
void LED_BLUE()
{
  if (BLUE != BLUE_A) {
    if (BLUE_A > BLUE) BLUE_A = BLUE_A - 1;
    if (BLUE_A < BLUE) BLUE_A++;
    analogWrite(BLUEPIN, BLUE_A);
  }
}

//-------------------------------------------------------------
//-------------------------------------------------------------
void LED_W1()
{
  if (W1 != W1_A) {
    if (W1_A > W1) W1_A = W1_A - 1;
    if (W1_A < W1) W1_A++;
    analogWrite(WHITE1PIN, W1_A);
  }
}

//-------------------------------------------------------------
//-------------------------------------------------------------
void LED_W2()
{
  if (W2 != W2_A) {
    if (W2_A > W2) W2_A = W2_A - 1;
    if (W2_A < W2) W2_A++;
    analogWrite(WHITE2PIN, W2_A);
  }
}

//-------------------------------------------------------------
//-------------------------------------------------------------
int convertToInt(char upper, char lower)
{
  int uVal = (int)upper;
  int lVal = (int)lower;
  uVal = uVal > 64 ? uVal - 55 : uVal - 48;
  uVal = uVal << 4;
  lVal = lVal > 64 ? lVal - 55 : lVal - 48;
  return uVal + lVal;
}

//-------------------------------------------------------------
//-------------------------------------------------------------
void Tick()
{
  if (fadeLED) {
    if (millis() - TIME_LED_RED >= led_delay_red) {
      TIME_LED_RED = millis();
      LED_RED();
    }
  
    if (millis() - TIME_LED_GREEN >= led_delay_green) {
      TIME_LED_GREEN = millis();
      LED_GREEN();
    }
  
    if (millis() - TIME_LED_BLUE >= led_delay_blue) {
      TIME_LED_BLUE = millis();
      LED_BLUE();
    }
  
    if (millis() - TIME_LED_W1 >= led_delay_w1) {
      TIME_LED_W1 = millis();
      LED_W1();
    }
  
    if (millis() - TIME_LED_W2 >= led_delay_w2) {
      TIME_LED_W2 = millis();
      LED_W2();
    }
  }
}

//-------------------------------------------------------------
//-------------------------------------------------------------
void setup()
{
  pinMode(onboardGreenLEDPIN, OUTPUT);
  pinMode(onboardRedLEDPIN, OUTPUT);

  // Setup console
  Serial1.begin(115200);
  delay(10);
  Serial1.println();
  Serial1.println();

  RedLEDoff;  //turn green led off until connected

  //get wifi parms from SPIFFS and connect to wifi
  setup_wifi2();

  while (WiFi.status() != WL_CONNECTED) {
    GreenLEDoff;
    delay(500);
    Serial1.print(".");
    GreenLEDon;
  }

  Serial1.println("");

  RedLEDon;   //turn green led on indicating we are connected

  Serial1.println("WiFi connected");
  Serial1.println("IP address: ");
  Serial1.println(WiFi.localIP());

  Serial1.println("");

  Serial1.print("WiFi.status: "); Serial1.println(WiFi.status());
  if (WiFi.status()!=WL_CONNECTED){
    Serial1.println("Failed to connect, finishing setup anyway");
  } else{
    //Prepare MQTT client
    Serial1.print("mqtt server: "); Serial1.println(mqtt_server);
    Serial1.print("mqtt port: "); Serial1.println(atoi(mqtt_port));
    mqttClient.setServer(mqtt_server, atoi(mqtt_port));
    mqttClient.setCallback(mqttCallback);

    Serial1.print("Local ip: ");
    Serial1.println(WiFi.localIP());
  }

  ticker.attach(0.01, Tick);

  setupLogic();
}

//-------------------------------------------------------------
//-------------------------------------------------------------
void loop()
{
  // is configuration portal requested?
  if ((digitalRead(TRIGGER_PIN) == LOW) || (initialConfig)) {
     Serial1.println("Configuration portal requested");
     GreenLEDon;  // turn the LED on indicating we are in configuration mode.
     //digitalWrite(onboardRedLEDPIN, LOW); 
    //Local intialization. Once its business is done, there is no need to keep it around

    // Extra parameters to be configured
    // After connecting, parameter.getValue() will get you the configured value
    // Format: <ID> <Placeholder text> <default value> <length> <custom HTML> <label placement>
    WiFiManagerParameter custom_scene("scene", "scene", scene, 40);
    WiFiManagerParameter custom_device("device_name", "device_name", device_name, 40);
    WiFiManagerParameter custom_mqtt_server("mqttserver", "mqtt server", mqtt_server, 16);
    WiFiManagerParameter custom_mqtt_port("mqttport", "mqtt port", mqtt_port, 6);

    // Just a quick hint
    WiFiManagerParameter p_hint("<small>*Hint: if you want to reuse the currently active WiFi credentials, leave SSID and Password fields empty</small>");
    
    // Initialize WiFIManager
    WiFiManager wifiManager;
    
    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    
    //add all parameters here  
    wifiManager.addParameter(&p_hint);
    wifiManager.addParameter(&custom_scene);
    wifiManager.addParameter(&custom_device);
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);

    // Sets timeout in seconds until configuration portal gets turned off.
    // If not specified device will remain in configuration mode until
    // switched off via webserver or device is restarted.
    // wifiManager.setConfigPortalTimeout(600);

    // It starts an access point 
    // and goes into a blocking loop awaiting configuration.
    // Once the user leaves the portal with the exit button
    // processing will continue
    if (!wifiManager.startConfigPortal()) {
      Serial1.println("Not connected to WiFi but continuing anyway.");
    } else {
      // If you get here you have connected to the WiFi
      Serial1.println("Connected...yay :)");
    }

    // Getting posted form values and overriding local variables parameters
    // Config file is written regardless the connection state
    strcpy(scene, custom_scene.getValue());
    strcpy(device_name, custom_device.getValue());
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());

    // Writing JSON config file to flash for next boot
    if (shouldSaveConfig)
      writeConfigFile();

    GreenLEDoff;  // Turn LED off indicating we are not in configuration mode.
    //digitalWrite(onboardRedLEDPIN, HIGH); 

    ESP.reset(); // This is a bit crude. For some unknown reason webserver can only be started once per boot up 
    // so resetting the device allows to go back into config mode again when it reboots.
    delay(5000);
  }

  //ensure we are connected to MQTT
  if (!mqttClient.connected()) {
    mqttReconnect();

    Serial1.print(device_name); Serial1.println(" connected to mqtt");
  }

  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  loopLogic();
}

//------------------------------------------------------------
bool readConfigFile() {
  // this opens the config file in read-mode
  File f = SPIFFS.open(CONFIG_FILE, "r");
  
  if (!f) {
    Serial1.println("Configuration file not found");
    return false;
  } else {
    // we could open the file
    size_t size = f.size();
    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size]);

    // Read and store file contents in buf
    f.readBytes(buf.get(), size);
    // Closing file
    f.close();
    // Using dynamic JSON buffer which is not the recommended memory model, but anyway
    // See https://github.com/bblanchon/ArduinoJson/wiki/Memory%20model
    DynamicJsonBuffer jsonBuffer;
    // Parse JSON string
    JsonObject& json = jsonBuffer.parseObject(buf.get());
    // Test if parsing succeeds.
    if (!json.success()) {
      Serial1.println("JSON parseObject() failed");
      return false;
    }
    json.printTo(Serial1);

    // Parse all config file parameters, override 
    // local config variables with parsed values
    if (json.containsKey("scene")) {
      strcpy(scene, json["scene"]);      
    }
    
    if (json.containsKey("device_name")) {
      strcpy(device_name, json["device_name"]);
    }

    if (json.containsKey("mqtt_server")) {
      strcpy(mqtt_server, json["mqtt_server"]);      
    }
    
    if (json.containsKey("mqtt_port")) {
      strcpy(mqtt_port, json["mqtt_port"]);
    }
  }

  Serial1.println("\nConfig file was successfully parsed");
  return true;
}

//------------------------------------------------------------
bool writeConfigFile() {
  Serial1.println("Saving config file");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  // JSONify local configuration parameters
  json["scene"] = scene;
  json["device_name"] = device_name;
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;

  // Open file for writing
  File f = SPIFFS.open(CONFIG_FILE, "w");
  if (!f) {
    Serial1.println("Failed to open config file for writing");
    return false;
  }

  json.prettyPrintTo(Serial1);
  // Write data to file and close it
  json.printTo(f);
  f.close();

  Serial1.println("\nConfig file was successfully saved");
  return true;
}

