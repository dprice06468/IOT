#define APPNAME "IASmqttServo"
#define VERSION "V1.0.0"
#define COMPDATE __DATE__ __TIME__
#define MODEBUTTON 0

#include <IOTAppStory.h>
IOTAppStory IAS(APPNAME, VERSION, COMPDATE, MODEBUTTON);

// ================================================ EXAMPLE VARS =========================================
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>         //MQTT include
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <Servo.h>

unsigned long blinkEntry;

// We want to be able to edit these example variables from the wifi config manager
// Currently only char arrays are supported. (Keep in mind that html form fields always return Strings)
// Use functions like atoi() and atof() to transform the char array to integers or floats
// Use IAS.dPinConv() to convert Dpin numbers to integers (D6 > 14)

//******************
//char* device_name = "servo";            //unique servo identifier
//char* mqtt_server = "192.168.1.31";
//char* mqtt_port = "1883";
//char* LEDpin = "D4";                                // The value given here is the default value and can be overwritten by values saved in configuration mode
//char* servoTop = "50";
//char* servoBottom = "120";
///char* topPause = "2000";
//******************

boolean running = false;

//setup MQTT client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

#define STOPPED_LED_PIN D1
#define RUNNING_LED_PIN D4

//**************************************************************
char* device_name = "servo";            //unique servo identifier
char* mqtt_server = "192.168.1.31";
char* mqtt_port = "1883";
char* LEDpin = "D4";                                // The value given here is the default value and can be overwritten by values saved in configuration mode
char* servoPin = "D1";                                // The value given here is the default value and can be overwritten by values saved in configuration mode
char* servoTop = "50";
char* servoBottom = "120";
char* topPause = "2000";

//MQTT settings
#define MQTT_ALL        "/linearservo1/sub/#"        //subscribed topic for all topics
#define MQTT_ONLINE     "/linearservo1/pub/online"   //topic published when node is online
#define MQTT_STOP       "/linearservo1/sub/stop"     //subscribed topic to stop functionality
#define MQTT_START      "/linearservo1/sub/start"    //subscribed topic to start functionality
#define MQTT_GO         "/linearservo1/sub/go"       //subscribed topic to trigger functionality
#define MQTT_RUNNING    "/linearservo1/pub/running"  //topic published when node actuator is running
#define MQTT_DONE       "/linearservo1/pub/done"     //topic published when node actuator stops

//define servo
Servo myServo;

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
      //actuate the linear servo
      mqttClient.publish(MQTT_RUNNING, device_name);
      upanddown();
      digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(500);                       // wait for a second
      digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
      delay(500);                       // wait for a second
      mqttClient.publish(MQTT_DONE, device_name);
      Serial.println("triggered");
    } else {
      Serial.println("unknown topic"); 
    }
  }
}

//--------------------------------------------------------------
//numvar pushup(void) {
//--------------------------------------------------------------
void pushup(void) {
  Serial.println("going up");
  for (byte pos=atoi(servoBottom); pos>=atoi(servoTop); pos--)     // goes from 180 degrees to 0 degrees 
  {                                
    myServo.write(pos);              // tell servo to go to position in variable 'pos' 
    delay(15);                       // waits 15ms for the servo to reach the position 
  } 
}

//--------------------------------------------------------------
//numvar dropdown(void) {
//--------------------------------------------------------------
void dropdown(void) {
  Serial.println("going down");
  myServo.write(atoi(servoBottom));              // tell servo to go to position in variable 'pos' 
}

//--------------------------------------------------------------
// this function will be implemented in bitlash as the IR function
// for the candy store window.
//--------------------------------------------------------------
void upanddown() {
  Serial.println("going upanddown");
  pushup();
  delay(atoi(topPause));
  dropdown();  
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void setupLogic() {
  myServo.attach(atoi(servoPin));
  myServo.write(atoi(servoBottom));
}

//-------------------------------------------------------------------
//loop routine logic
//-------------------------------------------------------------------
void loopLogic() {
//  if (running) {
      //publish TRIGGERED mqtt msg
//      mqttClient.publish(MQTT_TRIGGERED, device_name);
//  }
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
  IAS.serialdebug(true);                            // 1st parameter: true or false for serial debugging. Default: false | When set to true or false serialdebug can be set from wifi config manager
  //IAS.serialdebug(true,115200);                   // 1st parameter: true or false for serial debugging. Default: false | 2nd parameter: serial speed. Default: 115200
  /* TIP! delete the above lines when not used */

  IAS.addField(device_name, "device_name", "Device Name", 50);             // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.
  IAS.addField(mqtt_server, "mqtt_server", "MQTT Server", 16);             // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.
  IAS.addField(mqtt_port, "mqtt_port", "MQTT Port", 6);             // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.
  IAS.addField(LEDpin, "ledpin", "ledPin", 2);             // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.
//**************************
  IAS.addField(servoPin, "servoPin", "servoPin", 3);             // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.
  IAS.addField(servoTop, "servoTop", "servoTop", 5);
  IAS.addField(servoBottom, "servoBottom", "servoBottom", 12);
  IAS.addField(topPause, "topPause(ms)", "Pause time (ms)", 12);
  // reference to org variable | field name | field label value | max char return
//**************************

  IAS.begin(true);      // 1st parameter: true or false to view BOOT STATISTICS | 2nd parameter: true or false to erase eeprom on first boot of the app


  //-------- Your Setup starts from here ---------------
  pinMode(IAS.dPinConv(LEDpin), OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RUNNING_LED_PIN, OUTPUT);
  pinMode(STOPPED_LED_PIN, OUTPUT);
  
  digitalWrite(RUNNING_LED_PIN, LOW); // turn the LED off by making the voltage LOW
  digitalWrite(STOPPED_LED_PIN, LOW); // turn the LED off by making the voltage LOW

//**************************
//  myServo.attach(servoPin);
//  myServo.write(atoi(servoBottom));
//**************************

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
  if (!mqttClient.connected()) {
    mqttReconnect();

    Serial.print("device_name: "); Serial.println(device_name);
    Serial.print("servoTop: "); Serial.println(servoTop);
    Serial.print("servoBottom: "); Serial.println(servoBottom);
    Serial.print("topPause: "); Serial.println(topPause);
  }

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

