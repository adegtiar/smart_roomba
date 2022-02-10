//This version is for the Roomba 600 Series
//Connect a wire from D4 on the nodeMCU to the BRC pin on the roomba to prevent sleep mode.



#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <SimpleTimer.h>
#include <Roomba.h>


//USER CONFIGURED SECTION START//
const char* ssid = "Fios-tr8JX-IoT";
const char* password = "awn56abeam69nil";
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
long battery_Voltage = 0;
long battery_Total_mAh = 0;
long battery_percent = 0;
char battery_percent_send[50];
char battery_Current_mAh_send[50];
uint8_t tempBuf[10];

// Topics
const String TOPIC_CHECKIN = "checkIn/roomba";
const String TOPIC_COMMANDS = "roomba/commands";
const String TOPIC_STATUS = "roomba/status";
const String TOPIC_BATTERY = "roomba/battery";
const String TOPIC_CHARGING = "roomba/charging";

//Functions

void log(String msg)
{
  Serial.print("*****");
  Serial.print(msg);
  Serial.println("*****");
}

void publish(String topic, String msg)
{
  log("Publishing: (" + topic + ", " + msg + ")");
  client.publish(topic.c_str(), msg.c_str());
}

void setup_wifi() 
{
  log("Setting up wifi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
  }
}

void reconnect() 
{
  // Loop until we're reconnected
  int retries = 0;
  while (!client.connected()) 
  {
    if(retries < 50)
    {
      log("Attempting to reconnect to MQTT");
      // Attempt to connect
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass, TOPIC_STATUS.c_str(), 0, 0, "Dead Somewhere"))
      {
        // Once connected, publish an announcement...
        if(boot == false)
        {
          publish(TOPIC_CHECKIN, "Reconnected"); 
        }
        if(boot == true)
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
    if(retries >= 50)
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
    if (newPayload == "start")
    {
      startCleaning();
    }
    if (newPayload == "stop")
    {
      stopCleaning();
    }
  }
}


void startCleaning()
{
  log("Sending cleaning command");
  Serial.write(128);
  delay(50);
  Serial.write(131);
  delay(50);
  Serial.write(135);
  publish(TOPIC_STATUS, "Cleaning");
  log("Done sending cleaning command. Publishing status");
}

void stopCleaning()
{
  log("Sending stop command");
  Serial.write(128);
  delay(50);
  Serial.write(131);
  delay(50);
  Serial.write(143);
  publish(TOPIC_STATUS, "Returning");
  log("Done sending stop command. Publishing status");
}

void sendInfoRoomba()
{
  log("Getting info from roomba sensors");
  roomba.start(); 
  roomba.getSensors(21, tempBuf, 1);
  battery_Voltage = tempBuf[0];
  delay(50);
  roomba.getSensors(25, tempBuf, 2);
  battery_Current_mAh = tempBuf[1]+256*tempBuf[0];
  delay(50);
  roomba.getSensors(26, tempBuf, 2);
  battery_Total_mAh = tempBuf[1]+256*tempBuf[0];
  if(battery_Total_mAh != 0)
  {
    int nBatPcent = 100*battery_Current_mAh/battery_Total_mAh;
    String temp_str2 = String(nBatPcent);
    temp_str2.toCharArray(battery_percent_send, temp_str2.length() + 1); //packaging up the data to publish to mqtt
    log("Got battery info. Publishing (" + temp_str2 + "%)");
    publish(TOPIC_BATTERY, battery_percent_send);
  }
  if(battery_Total_mAh == 0)
  {  
    log("Failed to get battery info. Publishing error");
    publish(TOPIC_BATTERY, "NO DATA");
  }
  String temp_str = String(battery_Voltage);
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
  pinMode(noSleepPin, OUTPUT);
  digitalWrite(noSleepPin, HIGH);
  Serial.begin(115200);
  log("Start of program");
  Serial.write(129);
  delay(50);
  Serial.write(11);
  delay(50);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  timer.setInterval(5000, sendInfoRoomba);
  timer.setInterval(60000, stayAwakeLow);
}

void loop() 
{
  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();
  timer.run();
}
