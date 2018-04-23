#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>           //OTA include
#include <PubSubClient.h>         //MQTT include
#include <ArduinoJson.h>

//needed for library
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

// select which pin will trigger the configuration portal when set to LOW
// ESP-01 users please note: the only pins available (0 and 2), are shared 
// with the bootloader, so always set them HIGH at power-up
#define TRIGGER_PIN D2

//MQTT settings
#define MQTT_READY          "/mqttOTABase/ready"
#define MQTT_STOP           "/mqttOTABase/stop"
#define MQTT_START          "/mqttOTABase/start"
#define MQTT_GO             "/mqttOTABase/go"
IPAddress broker(192,168,1,31);             // Address of the MQTT broker
#define CLIENT_ID "linearServo1"            // Client ID to send to the broker

boolean running = false;

//setup MQTT client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

//--------------------------------------------------------------
//--------------------------------------------------------------
void callback(char* topic, byte* payload, unsigned int length) {
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
  
    // Step 4
    JsonObject& root = jsonBuffer.parseObject(payload);
  
    // Step 5
    if (!root.success()) {
      Serial.println("parseObject() failed");
      return;
    }
  
    // Step 6
    const char* sensor = root["sensor"];
    long time = root["time"];
    double latitude = root["data"][0];
    double longitude = root["data"][1];
  
    Serial.println(sensor);
    Serial.println(time);
    Serial.println(latitude, 6);
    Serial.println(longitude, 6);

    Serial.println("started");
  } else if ((strcmp(topic, MQTT_STOP)==0) && running) {
    running = false;
    Serial.println("stopped");
  } else if ((strcmp(topic, MQTT_GO)==0) && running) {
    Serial.println("triggered");

    //actuate
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(1000);                       // wait for a second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    delay(1000);                       // wait for a second
  } else {
    Serial.println("unknown topic"); 
  }
}

//--------------------------------------------------------------
// Attempt connection to MQTT broker and subscribe to command topic
//--------------------------------------------------------------
void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(CLIENT_ID)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish(MQTT_READY, "mqttOTABase ready");
      // ... and resubscribe
      mqttClient.subscribe(MQTT_START);
      mqttClient.subscribe(MQTT_STOP);
      mqttClient.subscribe(MQTT_GO);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("\n Starting");

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

  pinMode(TRIGGER_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

//  if (WiFi.status() == WL_CONNECTED) {
//    Serial.print("Connected to wifi.  IP address: ");
//    Serial.println(WiFi.localIP());
//  } else {
//    Serial.println("not connected to wifi");
//  }

  /* Prepare MQTT client */
  mqttClient.setServer(broker, 1883);
  mqttClient.setCallback(callback);

//  char json[] = "{\"sensor\":\"gps\",\"time\":1351824120,\"data\":[48.756080,2.302038]}";
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void loop() {
  //Serial.println("in loop");
  // is configuration portal requested?
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    Serial.println("wifimanagaer triggered");
    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    //reset settings - for testing
    //wifiManager.resetSettings();

    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep
    //in seconds
    //wifiManager.setTimeout(120);

    //it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration

    //WITHOUT THIS THE AP DOES NOT SEEM TO WORK PROPERLY WITH SDK 1.5 , update to at least 1.5.1
    //WiFi.mode(WIFI_STA);
    
    if (!wifiManager.startConfigPortal("mqttOTABaseAP")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }

    //if you get here you have connected to the WiFi
    Serial.println("connected...)");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  ArduinoOTA.handle();

  // put your main code here, to run repeatedly:
  //ensure we are connected to MQTT
  if (!mqttClient.connected()) {
    reconnect();
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  //Run MQTT loop
  if (mqttClient.connected())
  {
    mqttClient.loop();
  }
}
