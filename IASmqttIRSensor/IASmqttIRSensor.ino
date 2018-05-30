#define APPNAME "IASmqttIRSensor"
#define VERSION "V1.0.1"
#define COMPDATE __DATE__ __TIME__
#define MODEBUTTON 0

#include <IOTAppStory.h>
IOTAppStory IAS(COMPDATE, MODEBUTTON);

// ================================================ EXAMPLE VARS =========================================
//#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>         //MQTT include
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

//unsigned long blinkEntry;

// We want to be able to edit these example variables from the wifi config manager
// Currently only char arrays are supported. (Keep in mind that html form fields always return Strings)
// Use functions like atoi() and atof() to transform the char array to integers or floats
// Use IAS.dPinConv() to convert Dpin numbers to integers (D6 > 14)

boolean running = false;

//setup MQTT client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

#define STOPPED_LED_PIN D1
#define RUNNING_LED_PIN D2

//**************************************************************
#define IR_LED_PIN D5
// An IR detector/demodulator is connected to GPIO pin 14 (D5).
uint16_t IR_RECV_PIN = 12;  //D6

char* device_name = "ir";                 //unique IR identifier
char* mqtt_server = "192.168.1.31";
char* mqtt_port = "1883";
char* OnboardLEDPin = "2";               // The value given here is the default value and can be overwritten by values saved in configuration mode
//char* blinkTime = "1000";
char* irCode = "0x219E10EF";

//MQTT settings
#define MQTT_ALL        "/ir/sub/#"          //subscribed topic for all topics
#define MQTT_ONLINE     "/ir/pub/online"     //topic published when node is online
#define MQTT_STOP       "/ir/sub/stop"       //subscribed topic to stop functionality
#define MQTT_START      "/ir/sub/start"      //subscribed topic to start functionality
#define MQTT_GO         "/ir/sub/go"         //subscribed topic to trigger functionality
#define MQTT_TRIGGERED  "/ir/pub/triggered"  //topic published with IR is triggered

IRrecv irrecv(IR_RECV_PIN);
decode_results results;

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

  payload[length] = '\0';
  String strPayload = String((char*)payload);
  Serial.print("payload: "); Serial.println(strPayload);
  if (strcmp((char*)payload, device_name) == 0) {
    if ((strcmp(topic, MQTT_START)==0) && !running) {
      running = true;
      digitalWrite(RUNNING_LED_PIN, HIGH);
      digitalWrite(STOPPED_LED_PIN, LOW);
      Serial.println("started");
    } else if ((strcmp(topic, MQTT_STOP)==0) && running) {
      running = false;
      digitalWrite(STOPPED_LED_PIN, HIGH);
      digitalWrite(RUNNING_LED_PIN, LOW);
      Serial.println("stopped");
    } else if ((strcmp(topic, MQTT_GO)==0) && running) {
      //publish TRIGGERED mqtt msg
      mqttClient.publish(MQTT_TRIGGERED, device_name);
  
      //actuate the IR action
      digitalWrite(IR_LED_PIN, HIGH);
      delay(2000);
      digitalWrite(IR_LED_PIN, LOW);
      Serial.println("triggered");
    } else {
      Serial.println("unknown topic"); 
    }
  }
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void setupLogic() {
  pinMode(atoi(OnboardLEDPin), OUTPUT);
  pinMode(IR_LED_PIN, OUTPUT);
  digitalWrite(atoi(OnboardLEDPin), LOW);      
  digitalWrite(IR_LED_PIN, LOW);      // turn the LED off by making the voltage LOW
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void loopLogic() {
  digitalWrite(atoi(OnboardLEDPin), HIGH);
  delay(500);
  digitalWrite(atoi(OnboardLEDPin), LOW);
  delay(500);

  if (running && irrecv.decode(&results)) {
    // print() & println() can't handle printing long longs. (uint64_t)
    serialPrintUint64(results.value, HEX);
    Serial.println("");
    if (results.value == 0x219E10EF) {
      //publish TRIGGERED mqtt msg
      mqttClient.publish(MQTT_TRIGGERED, device_name);

      digitalWrite(IR_LED_PIN, HIGH);
      delay(2000);
      digitalWrite(IR_LED_PIN, LOW);
    }
    irrecv.resume();  // Receive the next value
  }
  delay(100);

}
//**************************************************************

//--------------------------------------------------------------
// Attempt connection to MQTT broker and subscribe to command topic
//--------------------------------------------------------------
void mqttReconnect() {
  // Loop until we're reconnected
  if (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(device_name)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.subscribe(MQTT_ALL);
      mqttClient.publish(MQTT_ONLINE, device_name);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// ================================================ SETUP ================================================
void setup() {
  IAS.preSetDeviceName("IASmqttIRSensor");                       // preset deviceName this is also your MDNS responder: http://virginsoil.local
//  IAS.serialdebug(true);                            // 1st parameter: true or false for serial debugging. Default: false | When set to true or false serialdebug can be set from wifi config manager
  //IAS.serialdebug(true,115200);                   // 1st parameter: true or false for serial debugging. Default: false | 2nd parameter: serial speed. Default: 115200
  /* TIP! delete the above lines when not used */

  IAS.addField(device_name, "Device Name", 50);             // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.
  IAS.addField(mqtt_server, "MQTT Server", 16);             // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.
  IAS.addField(mqtt_port, "MQTT Port", 6);             // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.
  IAS.addField(OnboardLEDPin, "Onboard LED Pin", 2);             // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.
  //IAS.addField(blinkTime, "BlinkTime(ms)", 5);
  // reference to org variable | field name | field label value | max char return
  //**************************
  IAS.addField(irCode, "IR Code", 12);
  //**************************

  IAS.onModeButtonShortPress([]() {
    Serial.println(F(" If mode button is released, I will enter in firmware update mode."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
  });

  IAS.onModeButtonLongPress([]() {
    Serial.println(F(" If mode button is released, I will enter in configuration mode."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
  });

//  IAS.begin('L');                                     // Optional parameter: What to do with EEPROM on First boot of the app? 'F' Fully erase | 'P' Partial erase(default) | 'L' Leave intact
  //IAS.setCallHome(true);                              // Set to true to enable calling home frequently (disabled by default)
  //IAS.setCallHomeInterval(60);                        // Call home interval in seconds, use 60s only for development. Please change it to at least 2 hours in production


  //-------- Your Setup starts from here ---------------
  irrecv.enableIRIn();  // Start the receiver

  pinMode(RUNNING_LED_PIN, OUTPUT);
  pinMode(STOPPED_LED_PIN, OUTPUT);

  digitalWrite(RUNNING_LED_PIN, LOW); // turn the LED off by making the voltage LOW
  digitalWrite(STOPPED_LED_PIN, LOW); // turn the LED off by making the voltage LOW

  //**************************
  setupLogic();
  //**************************

  while (!Serial)       // Wait for the serial connection to be establised.
    delay(50);
  Serial.println();
  Serial.print("IRrecvDemo is now running and waiting for IR message on Pin ");
  Serial.println(IR_RECV_PIN);

  //Prepare MQTT client
  Serial.print("server: "); Serial.print(mqtt_server);
  Serial.print(" port: "); Serial.println(atoi(mqtt_port));
  mqttClient.setServer(mqtt_server, atoi(mqtt_port));
  mqttClient.setCallback(mqttCallback);

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ================================================ LOOP =================================================
void loop() {
  IAS.buttonLoop();   // this routine handles the reaction of the Flash button. If short press: update of skethc, long press: Configuration

  //-------- Your Sketch starts from here ---------------
  //ensure we are connected to MQTT
  if (!mqttClient.connected())
    mqttReconnect();

  //Run MQTT loop
  if (mqttClient.connected())
    mqttClient.loop();

  //**************************
  loopLogic();
  //**************************

/*  if (millis() - blinkEntry > atoi(blinkTime)) {
    digitalWrite(IAS.dPinConv(LEDpin), !digitalRead(IAS.dPinConv(LEDpin)));
    blinkEntry = millis();

    // Serial feedback
    Serial.println("");
    Serial.print("device_name: "); Serial.println(device_name);
    Serial.print("mqtt_server: "); Serial.println(mqtt_server);
    Serial.print("mqtt_port: "); Serial.println(mqtt_port);
    Serial.print("blinkEntry: "); Serial.println(blinkEntry);
    Serial.print("LEDpin: "); Serial.println(LEDpin);
    Serial.print("irCode: "); Serial.println(irCode);
  }*/
}

