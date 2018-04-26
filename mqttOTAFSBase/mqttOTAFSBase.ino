#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//#include <DNSServer.h>
//#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
//#include <WiFiUdp.h>
#include <ArduinoOTA.h>           //OTA include
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>         //MQTT include

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "1883";
//char blynk_token[34] = "YOUR_BLYNK_TOKEN";

//flag for saving data
bool shouldSaveConfig = false;

boolean running = false;

//setup MQTT client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

//********************************************************************
// select which pin will trigger the configuration portal when set to LOW
// ESP-01 users please note: the only pins available (0 and 2), are shared 
// with the bootloader, so always set them HIGH at power-up
#define TRIGGER_PIN D2

//MQTT settings
#define MQTT_CLIENT_ID  "mqttOTAFSBase"           // Client ID to send to the broker
#define MQTT_ALL        "/mqttOTAFSBase/#"        //subscribed topic for all topics
#define MQTT_ONLINE     "/mqttOTAFSBase/online"   //topic published when node is online
#define MQTT_STOP       "/mqttOTAFSBase/stop"     //subscribed topic to stop functionality
#define MQTT_START      "/mqttOTAFSBase/start"    //subscribed topic to start functionality
#define MQTT_GO         "/mqttOTAFSBase/go"       //subscribed topic to trigger functionality

//--------------------------------------------------------------
//routine called when an mqtt message is received
//--------------------------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if ((strcmp(topic, MQTT_START)==0) && !running) {
    running = true;
    StaticJsonBuffer<200> jsonBuffer;
  
    JsonObject& root = jsonBuffer.parseObject(payload);
  
    if (!root.success()) {
      Serial.println("parseObject() failed");
      return;
    }
  
    //todo: customize to pull data from json payload
    const char* sensor = root["sensor"];
    long ltime = root["time"];
    double latitude = root["data"][0];
    double longitude = root["data"][1];
    long top = root["top"];
    long bottom = root["bottom"];
  
    Serial.println(sensor);
    Serial.println(ltime);
    Serial.println(latitude, 6);
    Serial.println(longitude, 6);
    Serial.println(top);
    Serial.println(bottom);

    Serial.println("started");
  } else if ((strcmp(topic, MQTT_STOP)==0) && running) {
    running = false;
    Serial.println("stopped");
  } else if ((strcmp(topic, MQTT_GO)==0) && running) {
    Serial.println("triggered");

    //todo: actuate once
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(200);                       // wait for a second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    delay(200);                       // wait for a second
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(200);                       // wait for a second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    delay(200);                       // wait for a second
  } else {
    Serial.println("unknown topic"); 
  }
}

//-------------------------------------------------------------------
//loop routine logic
//-------------------------------------------------------------------
void loopFunction() {
  if (running) {
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(500);                       // wait for a second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    delay(500);                       // wait for a second
  }
}
//********************************************************************

//-------------------------------------------------------------------
//callback notifying us of the need to save config
//-------------------------------------------------------------------
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//-------------------------------------------------------------------
//called to establish wifi connection using wifimanager library
//-------------------------------------------------------------------
void connectToWifi(boolean resetWifiSettings) {
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  //WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 32);

  //WiFiManager Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
//  wifiManager.addParameter(&custom_blynk_token);

  //reset settings - for testing
  if (resetWifiSettings) {
    wifiManager.resetSettings();
    shouldSaveConfig = true;
  }

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP" and goes into a blocking loop awaiting configuration
//  if (!wifiManager.autoConnect("mqttOTAFSBaseAP", "password")) {
  char nameAP[100];
  strcpy(nameAP, MQTT_CLIENT_ID);
  strcpy(nameAP, "_AP");
  if (!wifiManager.autoConnect(nameAP)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yay :)");
  
  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
//  strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
//    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }  
}

//--------------------------------------------------------------
// Attempt connection to MQTT broker and subscribe to command topic
//--------------------------------------------------------------
void mqttReconnect() {
  // Loop until we're reconnected
  if (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
//      mqttClient.publish(MQTT_READY, "mqttOTAFSBase ready");
      char msg[100];
      strcpy(msg, MQTT_CLIENT_ID);
      strcpy(msg, " online");
      mqttClient.publish(MQTT_ONLINE, msg);
      // ... and resubscribe
      mqttClient.subscribe(MQTT_ALL);
//      mqttClient.subscribe(MQTT_START);
//      mqttClient.subscribe(MQTT_STOP);
//      mqttClient.subscribe(MQTT_GO);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//-------------------------------------------------------------------
//setup OTA programmability
//-------------------------------------------------------------------
void setupOTA() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");
  
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR   ) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR  ) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR    ) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

//-------------------------------------------------------------------
//get configuration settings from device SPIFFS
//-------------------------------------------------------------------
void getSPIFFS() {
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          //*********************************************************
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
//          strcpy(blynk_token, json["blynk_token"]);
          //*********************************************************
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  //Serial.print("mqtt1: ");  Serial.print(mqtt_server); Serial.print(":"); Serial.println(mqtt_port);
}

//-------------------------------------------------------------------
//setup routine run once
//-------------------------------------------------------------------
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("\n Starting");

  //get SPIFFS configuration settings
  getSPIFFS();
  
  //connect to wifi based on configuration in SPIFFS
  connectToWifi(false);

  //setup for OTA updating
  setupOTA();

  //*********************************************************
  //set pin modes
  pinMode(TRIGGER_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  //*********************************************************

  //Prepare MQTT client
  Serial.print("server: "); Serial.print(mqtt_server);
  Serial.print(" port: "); Serial.println(atoi(mqtt_port));
  mqttClient.setServer(mqtt_server, atoi(mqtt_port));
  mqttClient.setCallback(mqttCallback);

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

//  char json[] = "{\"sensor\":\"gps\",\"blinkrate\":500,\"data\":[48.756080,2.302038]}";
}

//-------------------------------------------------------------------
//repetative loop where functionality goes
//-------------------------------------------------------------------
void loop() {
  // put your main code here, to run repeatedly:
  // is configuration portal requested?
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    Serial.println("wifimanagaer triggered");

    //reconnect to wifi after resetting connection settings
    connectToWifi(true);
  }

  //handle any OTA requests
  ArduinoOTA.handle();

  //ensure we are connected to MQTT
  if (!mqttClient.connected())
    mqttReconnect();

  //Run MQTT loop
  if (mqttClient.connected())
    mqttClient.loop();

  //do repeat loop
  loopFunction();  
}
