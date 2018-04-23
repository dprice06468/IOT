#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>           //OTA include
#include <PubSubClient.h>         //MQTT include

//needed for library
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <Servo.h>                //server include

// select which pin will trigger the configuration portal when set to LOW
// ESP-01 users please note: the only pins available (0 and 2), are shared 
// with the bootloader, so always set them HIGH at power-up
#define TRIGGER_PIN D2

//set Candy servo pin
#define CANDY_SERVO_PIN D1
#define CANDY_TOP 50
#define CANDY_BOTTOM 120

//MQTT settings
#define MQTT_LINEAR_SERVO1  "/linearservo1/#"
#define MQTT_READY          "/linearservo1/ready"
#define MQTT_STOP           "/linearservo1/stop"
#define MQTT_START          "/linearservo1/start"
#define MQTT_GO             "/linearservo1/go"
IPAddress broker(192,168,1,31);             // Address of the MQTT broker
#define CLIENT_ID "linearServo1"            // Client ID to send to the broker

boolean running = false;

//define servo
Servo candyServo;

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
    Serial.println("started");
  } else if ((strcmp(topic, MQTT_STOP)==0) && running) {
    running = false;
    Serial.println("stopped");
  } else if ((strcmp(topic, MQTT_GO)==0) && running) {
    Serial.println("triggered");

    //actuate the linear servo
    upanddown();
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(500);                       // wait for a second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    delay(500);                       // wait for a second
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
      mqttClient.publish(MQTT_READY, "LinearServo1 ready");
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
//numvar pushup(void) {
//--------------------------------------------------------------
void pushup(void) {
  Serial.println("basicLinearServo.pushup");
  for (byte pos=CANDY_BOTTOM; pos>=CANDY_TOP; pos--)     // goes from 180 degrees to 0 degrees 
  {                                
    candyServo.write(pos);              // tell servo to go to position in variable 'pos' 
    delay(15);                       // waits 15ms for the servo to reach the position 
  } 
}

//--------------------------------------------------------------
//numvar dropdown(void) {
//--------------------------------------------------------------
void dropdown(void) {
  Serial.println("basicLinearServo.dropdown");
  candyServo.write(CANDY_BOTTOM);              // tell servo to go to position in variable 'pos' 
}

//--------------------------------------------------------------
// this function will be implemented in bitlash as the IR function
// for the candy store window.
//--------------------------------------------------------------
void upanddown() {
  Serial.println("basicLinearServo.upanddown");
  pushup();
  delay(2000);
  dropdown();  
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

  candyServo.attach(CANDY_SERVO_PIN);
  candyServo.write(CANDY_BOTTOM);
  
  /* Prepare MQTT client */
  mqttClient.setServer(broker, 1883);
  mqttClient.setCallback(callback);
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
    
    if (!wifiManager.startConfigPortal("OnDemandAP")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }

    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }

  ArduinoOTA.handle();

  // put your main code here, to run repeatedly:
  //ensure we are connected to MQTT
  if (!mqttClient.connected()) {
    reconnect();
  }

  //Run MQTT loop
  if (mqttClient.connected())
  {
    mqttClient.loop();
  }
}
