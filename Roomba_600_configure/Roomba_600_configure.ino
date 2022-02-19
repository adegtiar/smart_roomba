//This version is for the Roomba 600 Series
//Connect a wire from D4 on the nodeMCU to the BRC pin on the roomba to prevent sleep mode.

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <SimpleTimer.h>
#include <Roomba.h>

#ifndef STASSID
#define STASSID "Fios-tr8JX-IoT"
#define STAPSK  "awn56abeam69nil"
#endif


//USER CONFIGURED SECTION START//
const char* ssid = STASSID;
const char* password = STAPSK;
const char* hostname = "roomba";
const char* mqtt_server = "192.168.1.253";
const int mqtt_port = 1883;
const char *mqtt_user = NULL;
const char *mqtt_pass = NULL;
const char *mqtt_client_name = "Roomba"; // Client connections can't have the same connection name
//USER CONFIGURED SECTION END//

WiFiClient espClient;
PubSubClient client(espClient);
SimpleTimer timer;
Roomba roomba(&Serial, Roomba::Baud115200);

// Variables
bool toggle = true;
const int noSleepPin = 2;
bool boot = true;
long battery_Current_mAh = 0;
long battery_Charging_state = 0;
long battery_Total_mAh = 0;
long battery_percent = 0;
char battery_percent_send[50];
char battery_Current_mAh_send[50];
uint8_t tempBuf[10];

// Topics
const String TOPIC_CHECKIN = "roomba/checkIn";
const String TOPIC_COMMANDS = "roomba/commands";
const String TOPIC_STATUS = "roomba/status";
const String TOPIC_BATTERY = "roomba/battery";
const String TOPIC_CHARGING = "roomba/charging";

//Functions

void log(String msg)
{
  /*
    Serial.print("*****");
    Serial.print(msg);
    Serial.println("*****");
  */
}

void publish(String topic, String msg)
{
  log("Publishing: (" + topic + ", " + msg + ")");
  client.publish(topic.c_str(), msg.c_str());
}

void setup_wifi()
{
  log("Setting up wifi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    log("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname("roomba");
}

void setup_ota()
{
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.onStart([]() {
    log("Start");
  });
  ArduinoOTA.onEnd([]() {
    log("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //logf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) log("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) log("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) log("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) log("Receive Failed");
    else if (error == OTA_END_ERROR) log("End Failed");
  });
  ArduinoOTA.begin();
}

void reconnect()
{
  // Loop until we're reconnected
  int retries = 0;
  while (!client.connected())
  {
    if (retries < 50)
    {
      log("Attempting to reconnect to MQTT");
      // Attempt to connect
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass, TOPIC_STATUS.c_str(), 0, 0, "Dead Somewhere"))
      {
        // Once connected, publish an announcement...
        if (boot == false)
        {
          publish(TOPIC_CHECKIN, "Reconnected");
        }
        if (boot == true)
        {
          publish(TOPIC_CHECKIN, "Rebooted");
          boot = false;
        }
        // ... and resubscribe
        client.subscribe(TOPIC_COMMANDS.c_str());
      }
      else
      {
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    if (retries >= 50)
    {
      log("Exceeded retry attempts. Restarting");
      ESP.restart();
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length)
{
  String newTopic = topic;
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  if (newTopic == TOPIC_COMMANDS)
  {
    if (newPayload == "clean")
    {
      toggleCleaning();
    }
    if (newPayload == "dock")
    {
      returnToDock();
    }
    if (newPayload == "reboot")
    {
      reboot();
    }
    if (newPayload == "uturn")
    {
      turnAround();
    }
  }
}

void toggleCleaning()
{
  log("Sending clean command");
  roomba.start();
  roomba.cover();
  publish(TOPIC_STATUS, "Cleaning");
  log("Done sending cleaning command. Publishing status");
}

void returnToDock()
{
  log("Sending dock command");
  roomba.start();
  roomba.dock();
  publish(TOPIC_STATUS, "Returning");
  log("Done sending dock command. Publishing status");
}

void turnAround()
{
  log("Sending u-turn command");
  roomba.start();
  delay(50);
  // Manually write control flag 130 to put into safe mode (library doesn't support).
  Serial.write(130);
  delay(50);
  roomba.drive(270, Roomba::DriveInPlaceClockwise);
  delay(1450);
  roomba.start();
}

void reboot()
{
  log("Rebooting the esp chip");
  ESP.restart();
}

void sendInfoRoomba()
{
  log("Getting info from roomba sensors");
  roomba.start();
  roomba.getSensors(Roomba::SensorChargingState, tempBuf, 1);
  battery_Charging_state = tempBuf[0];
  delay(50);

  roomba.getSensors(Roomba::SensorBatteryCharge, tempBuf, 2);
  battery_Current_mAh = tempBuf[1] + 256 * tempBuf[0];
  delay(50);

  roomba.getSensors(Roomba::SensorBatteryCapacity, tempBuf, 2);
  battery_Total_mAh = tempBuf[1] + 256 * tempBuf[0];

  if (battery_Total_mAh != 0)
  {
    int nBatPcent = 100 * battery_Current_mAh / battery_Total_mAh;
    String temp_str2 = String(nBatPcent);
    temp_str2.toCharArray(battery_percent_send, temp_str2.length() + 1); //packaging up the data to publish to mqtt
    log("Got battery info. Publishing (" + temp_str2 + "%)");
    publish(TOPIC_BATTERY, battery_percent_send);
  }

  if (battery_Total_mAh == 0)
  {
    log("Failed to get battery info. Publishing error");
    publish(TOPIC_BATTERY, "NO DATA");
  }
  String temp_str = String(battery_Charging_state);
  temp_str.toCharArray(battery_Current_mAh_send, temp_str.length() + 1); //packaging up the data to publish to mqtt
  publish(TOPIC_CHARGING, battery_Current_mAh_send);
}

void stayAwakeLow()
{
  log("Sending stayalive ping on D4");
  digitalWrite(noSleepPin, LOW);
  timer.setTimeout(1000, stayAwakeHigh);
}

void stayAwakeHigh()
{
  digitalWrite(noSleepPin, HIGH);
}


void setup()
{
  // Set up the serial output to baud rate 115200.
  Serial.begin(115200);
  setup_wifi();
  setup_ota();

  // Enable stay-awake pin, or else roomba will go to sleep.
  pinMode(noSleepPin, OUTPUT);
  digitalWrite(noSleepPin, HIGH);

  // Set baud rate to 115200 (baud code 11)
  roomba.start();
  roomba.baud(Roomba::Baud115200);

  log("Start of program");
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Wake up the roomba if it's asleep.
  stayAwakeLow();
  // Send info from sensors every 5 seconds.
  timer.setInterval(5000, sendInfoRoomba);
  // Toggle the stay-awake signal every minute.
  timer.setInterval(60000, stayAwakeLow);
}

void loop()
{
  ArduinoOTA.handle();
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
  timer.run();
}
