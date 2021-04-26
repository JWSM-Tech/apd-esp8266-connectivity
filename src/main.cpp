#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <string>
#include <stdbool.h>

#define baudRate 115200

#define SSID_SIZE 40
#define PASSWORD_SIZE 40

bool WiFiNotSet = true;

#define pillContainersCount 8

//container max digit count
#define maxDigitCount 2
#define maxPillName 15

#define hourDigits 2
#define minuteDigits 2
#define dayDigits 2
#define monthDigits 2
#define yearDigits 4

//Set Parameters for serial communication
#define setupInfoParam 0 //error avoidance
#define receiveAnalyticsParam 1 //receive analytics
#define receiveNetworkParam 2 // receive network
#define receivePillInfoParam 3 //error avoidance
#define sendAddReminderParam 4 // send reminder
#define receiveAddReminderParam 4 //receive reminder
#define sendRemoveReminderParam 5 // send reminder
#define receiveRemoveReminderParam 5 //receive reminder
#define sendRefillParam 6 //send refill
#define receiveRefillParam 6 //receive refill
#define sendAddPillParam 7 //send add
#define receiveAddParam 7 //receive add
#define sendRemovePillParam 8 //send remove
#define receiveRemovePillParam 8 //receive remove

#define sep_character ' '

#define debug true

const int RX_pin = 13;
const int TX_pin = 15;

int param = 0;

const size_t JSON_SIZE = JSON_OBJECT_SIZE(30);
StaticJsonDocument<JSON_SIZE> doc;
JsonObject object;

String json;

bool updatedFields = false;

void sendRefillUART();
void sendRemoveReminderUART();
void sendRemovePillUART();
void sendAddPillUART();
void sendAddReminderUART();

bool receiveAnalyticsUART();
bool receiveAddRemindersUART();
bool receiveRemoveRemindersUART();
bool receiveAddPillUART();
bool receiveRemovePillUART();
bool receiveRefillUART();


int readnInt(int digitsToRead);
char *readnChar(char stringLength, char *list);
int *readnIntList(char digitCount, int *list, int listLength);

void post_DB(String json);
void Analytics_to_JSON();
void Pill_Status_to_JSON();

char ssid[SSID_SIZE] = "SweetPotatoJam"; //  your network SSID (name)

char password[PASSWORD_SIZE] = "jEVezYP92*BRPiyC8zxhceAF"; // your network password

const char *rest_host = "http://apd-webapp-backend.herokuapp.com/api/analytics";

char str[200];

char temp_original_date[20];
char temp_taken_date[20];

struct alarm //hour, minute, pillQuantities
{

  unsigned char hour;
  unsigned char minute;
  int pillQuantities[pillContainersCount];
  // char pillNames[pillContainersCount][maxPillName]; //remove pillNames from alarm
};

struct refill //pillNames and pillQuantities
{
  int pillQuantities[pillContainersCount];
  char pillNames[pillContainersCount][maxPillName];
};

struct analytic // fields: original_date, taken_date, completed, pill_names, pill_quanitites
{
  unsigned char hour;   //original hour for alarm
  unsigned char minute; // original minute for alarm
  int pillQuantities[pillContainersCount];
  char pillNames[pillContainersCount][maxPillName];
  bool taken;  // true if user was reminded successfully
  char day;    // current day
  char month;  //current month
  int year;    // current year
  char dow;    // current day of week
  char TakenH; // hour when analytic was generated
  char TakenM; // minute when analytic was generated
};

struct alarm schedule[pillContainersCount];
struct analytic analytics;
struct refill fill;

ESP8266WebServer server(80);

// handle server functions
void handleRoot() // handle GET request to apdwifimodule.local
{
  server.send(200, "text/plain", "hello from apdwifimodule.local");
}

void handleRefillGET() //Handle Refill GET call from web application and send pill names and quantity data
{
  if (debug)
    Serial.println("Entered handleRefillGET");
  
  Pill_Status_to_JSON();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Max-Age", "10000");
  server.sendHeader("Access-Control-Allow-Methods", "PUT,POST,GET,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(200, "application/json", json);
}

// server POST handler functions
void deserializeJSONtoObject() //base JSON POST request handler for the ESP8266 web server
{
  // json = server.arg("plain");
  if (debug)
    Serial.println("json is " + server.arg("plain"));

  if (server.arg("plain")) // if the POST request has a body
  {
    deserializeJson(doc, server.arg("plain")); // assuming the POST body is JSON formatted
    object = doc.as<JsonObject>();

    updatedFields = true;
  }
}

void handleAddReminderPOST() // handle POST request from reminders to apdwifimodule.local
{
  if (debug)
    Serial.println("Entered handleAddReminderPOST");
  param = sendAddReminderParam;
  sendAddReminderUART();
  deserializeJSONtoObject();

  while(!receiveAddRemindersUART()) delay(1000);

  server.send(201, "text/plain", "the fields have been updated");
}

void handleRemoveReminderPOST() // handle POST for delete request from reminders to apdwifimodule.local
{
  if (debug)
    Serial.println("Entered handleRemoveReminderPOST");
  param = sendRemoveReminderParam;
  sendRemoveReminderUART();
  deserializeJSONtoObject();

  while(!receiveRemoveRemindersUART()) delay(1000);

  server.send(201, "text/plain", "the fields have been updated");
}

void handleAddPillPOST() //Handle Add POST call from web application 
{
  if (debug)
    Serial.println("Entered handleAddPillPOST");
  param = sendAddPillParam;
  sendAddPillUART();
  deserializeJSONtoObject();
  while(!receiveAddPillUART()) delay(1000);

  server.send(201, "text/plain", "the fields have been updated");
}

void handleRemovePillPOST() //Handle Add POST call from web application 
{
  if (debug)
    Serial.println("Entered handleRemovePillPOST");
  param = sendRemovePillParam;
  sendRemovePillUART();
  deserializeJSONtoObject();
  while(!receiveRemovePillUART()) delay(1000);

  server.send(201, "text/plain", "the fields have been updated");
}

void handleRefillPOST() //Handle Refill POST call from web application 
{
  if (debug)
    Serial.println("Entered handleRefillPOST");
  param = sendRefillParam;
  sendRefillUART();
  deserializeJSONtoObject();
  while(!receiveRefillUART()) delay(1000);

  server.send(201, "text/plain", "the fields have been updated");
}

//server NOT FOUND handler functions
void handleNotFound() //Handle NotFound resources in apdwifimodule.local
{
  if (server.method() == HTTP_OPTIONS)
  {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.sendHeader("Access-Control-Allow-Methods", "HEAD,PUT,POST,GET,OPTIONS");
    server.sendHeader("access-control-allow-credentials", "false");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(204);
  }

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";

  if (server.method() == HTTP_GET)
  {
    message += "GET";
  }
  else
  {
    message += "POST";
  }

  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (int i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

// UART TX functions
void sendAddReminderUART() // sends reminder arguments to MCU through UART
{
  Serial.print("param:");
  Serial.print(itoa(sendAddReminderParam, str, 10));
  Serial.print(" ");

  Serial.print("Hour");
  Serial.print(":");
  delay(10);
  Serial.print(object["hour"].as<char *>());
  delay(10);
  Serial.print(" ");

  delay(10);
  Serial.print("Minute");
  Serial.print(":");
  delay(10);
  Serial.print(object["minute"].as<char *>());
  delay(10);
  Serial.print(" ");

  Serial.print("pillQuantities:[");

  for (int i = 0; i < pillContainersCount - 1; i++)
  {
    delay(10);
    Serial.print(object["pillQuantities"][i].as<int>());
    Serial.print(",");
  }
  if (pillContainersCount >= 2)
    Serial.print(object["pillQuantities"][pillContainersCount - 1].as<int>()); //runs if pillContainersCount is at least 2
  Serial.print("]\n");

  updatedFields = false;
}

void sendRemoveReminderUART() // sends reminder arguments to MCU through UART
{
  Serial.print("param:");
  Serial.print(itoa(sendRemoveReminderParam, str, 10));
  Serial.print(" ");

  Serial.print("Hour");
  Serial.print(":");
  delay(10);
  Serial.print(object["hour"].as<char *>());
  delay(10);
  Serial.print(" ");

  delay(10);
  Serial.print("Minute");
  Serial.print(":");
  delay(10);
  Serial.print(object["minute"].as<char *>());
  delay(10);
  Serial.print(" ");

  Serial.print("pillQuantities:[");

  for (int i = 0; i < pillContainersCount - 1; i++)
  {
    delay(10);
    Serial.print(object["pillQuantities"][i].as<int>());
    Serial.print(",");
  }
  if (pillContainersCount >= 2)
    Serial.print(object["pillQuantities"][pillContainersCount - 1].as<int>()); //runs if pillContainersCount is at least 2
  Serial.print("]\n");

  updatedFields = false;
}

void sendRefillUART() // sends refill arguments to MCU through UART
{
  Serial.print("param:");
  Serial.print(itoa(sendRefillParam, str, 10));
  Serial.print(" ");

  Serial.print("pillQuantities:[");

  for (int i = 0; i < pillContainersCount - 1; i++)
  {
    delay(10);
    Serial.print(object["pillQuantities"][i].as<int>());

    if (object["pillQuantities"][i].as<int>() > 0)
    {
      fill.pillQuantities[i] = object["pillQuantities"][i].as<int>();
    }
    Serial.print(",");
  }
  if (pillContainersCount >= 2)
    Serial.print(object["pillQuantities"][pillContainersCount - 1].as<int>()); //runs if pillContainersCount is at least 2
  Serial.print("]\n");

  updatedFields = false;
}

void sendAddPillUART() // sends add arguments to MCU through UART
{                  //param:6 pillName:hola pillQuantity:4 \n
  Serial.print("param:");
  Serial.print(itoa(sendAddPillParam, str, 10));
  Serial.print(" ");

  Serial.print("pillName:");
  delay(10);
  Serial.print(object["pillName"].as<char *>());
  Serial.print(" ");

  Serial.print("pillQuantity:");
  delay(10);
  Serial.print(object["pillQuantity"].as<int>());

  Serial.print(" \n");

  updatedFields = false;
}

void sendRemovePillUART() // sends remove arguments to MCU through UART 
{
    Serial.print("param:");
  Serial.print(itoa(sendAddPillParam, str, 10));
  Serial.print(" ");

  Serial.print("pillName:");
  delay(10);
  Serial.print(object["pillName"].as<char *>());
  Serial.print(" ");

  updatedFields = false;
}

// UART RX functions
bool receiveAnalyticsUART() // receives analytic arguments from MCU through UART
{
  if (debug)
    Serial.print("\nEntered param = 1\n");

  if (Serial.find("Hour:"))
  {
    analytics.hour = readnInt(hourDigits);
    if (debug)
      Serial.printf("Analytics Hour = %d\n", analytics.hour);
  }

  if (Serial.find("Minute:"))
  {
    analytics.minute = readnInt(minuteDigits);
    if (debug)
      Serial.printf("Analytics Minute = %d\n", analytics.minute);
  }

  if (Serial.find("pillNames:["))
  {
    if (debug)
      Serial.print("pillNames are\n");

    for (int i = 0; i < pillContainersCount; i++)
    {
      for (int j = 0; j < maxPillName; j++)
      {

        if (Serial.peek() == ',')
        {
          Serial.read();
          analytics.pillNames[i][j] = '\0';
          j = maxPillName + 1;
        }

        else if (Serial.peek() == ']')
        {

          i = pillContainersCount - 1;
          j = maxPillName;
        }
        else
        {
          analytics.pillNames[i][j] = Serial.read();
        }
      }

      if (debug)
        Serial.printf("analytics.pillNames[%d] = %s\n", i, analytics.pillNames[i]);
    }

    if (debug)
      Serial.print("END PILL NAMES \n");
  }

  if (Serial.find("pillQuantities:["))
  {
    if (debug)
      Serial.print("pillQuantities are \n");
    readnIntList(maxDigitCount, analytics.pillQuantities, pillContainersCount);
    if (debug)
      Serial.println("END PILLQUANTITY");
  }

  // Day
  if (Serial.find("Day:"))
  {
    analytics.day = readnInt(dayDigits); // set analytics day
    if (debug)
      Serial.printf("Analytics day is %d\n", analytics.day);
  }

  //Month
  if (Serial.find("Month:"))
  {
    analytics.month = readnInt(monthDigits); // set analytics month
    if (debug)
      Serial.printf("Analytics month is %d\n", analytics.month);
  }

  //Year
  if (Serial.find("Year:"))
  {
    analytics.year = readnInt(yearDigits); // set analytics year
    if (debug)
      Serial.printf("Analytics year is %d\n", analytics.year);
  }

  //Day of Week
  if (Serial.find("DOW:"))
  {
    analytics.dow = readnInt(1); // set analytics dow
    if (debug)
      Serial.printf("Analytics Day of the Week is %d\n", analytics.dow);
  }

  //Taken Hour
  if (Serial.find("TakenH:"))
  {
    analytics.TakenH = readnInt(hourDigits); // set analytics taken hr
    if (debug)
      Serial.printf("Analytics Taken Hour is %d\n", analytics.TakenH);
  }

  //Taken Minute
  if (Serial.find("TakenM:"))
  {
    analytics.TakenM = readnInt(minuteDigits); // set analytics taken min
    if (debug)
      Serial.printf("Analytics Taken Hour is %d\n", analytics.TakenM);
  }

  //Taken boolean
  if (Serial.find("Taken:"))
  {
    analytics.taken = readnInt(1); // set analytics taken
    if (debug)
      Serial.printf("Analytics Taken is %d\n", analytics.taken);
  }

  Serial.find(sep_character);
  json = "";
  Analytics_to_JSON(); //adds the analytics generated to a json and posts the result to the Database
  post_DB(json);

  return true;
}

bool receiveAddRemindersUART() // receives alarm arguments from MCU through UART
{
  // storeIndex: Hour: Minute: pillQuantities:[0,0,0,0,0,0,0,0]
  if (debug)
    Serial.printf("\nEntered param = %d\n", receiveAddReminderParam);
  int storeIndex = 0;
  if (Serial.find("storeIndex:"))
  { 
    storeIndex = readnInt(1);
    if (debug)
      Serial.printf("Reminders Index = %d\n", storeIndex);
  }
  else
  {
    if(debug) Serial.printf("No storeIndex was found\n");
    return false;
  }

  if (Serial.find("Hour:"))
  {
    schedule[storeIndex].hour = readnInt(hourDigits);
    if (debug)
      Serial.printf("Reminders Hour = %d\n", schedule[storeIndex].hour);
  }
  else
  {
    if(debug) Serial.printf("No Hour was found\n");
    return false;
  }

  if (Serial.find("Minute:"))
  {
    schedule[storeIndex].minute = readnInt(minuteDigits);
    if (debug)
      Serial.printf("Reminders Minute = %d\n", schedule[storeIndex].minute);
  }
  else
  {
    if(debug) Serial.printf("No Minute was found\n");
    return false;
  }
  

  if (Serial.find("pillQuantities:["))
  {
    if (debug)
      Serial.print("pillQuantities are \n");
    readnIntList(maxDigitCount, schedule[storeIndex].pillQuantities, pillContainersCount);
    if (debug)
      Serial.println("END PILLQUANTITIES");
  }
  else
  {
    if(debug) Serial.printf("No pillQuantities was found\n");
    return false;
  }

  Serial.find(sep_character);
  return true;
}

bool receiveRemoveRemindersUART() // receives alarm arguments from MCU through UART
{
  // storeIndex: Hour: Minute: pillQuantities:[0,0,0,0,0,0,0,0]
  if (debug) Serial.printf("\nEntered param = %d\n", receiveRemoveReminderParam);
  int storeIndex = 0;
  if (Serial.find("storeIndex:"))
  { 
    storeIndex = readnInt(1);
    if (debug)
      Serial.printf("Reminders Index = %d\n", storeIndex);
  }
  else
  {
    if(debug) Serial.printf("No storeIndex was found\n");
    return false;
  }

  schedule[storeIndex].hour = 0;
  schedule[storeIndex].minute = 0;
  for(int i=0; i< pillContainersCount; i++) schedule[storeIndex].pillQuantities[i] = 0;
  
  Serial.find(sep_character);
  return true;
}

bool receiveNetworkUART() // receives network arguments from MCU through UART
{
  if (debug) Serial.printf("\nEntered param = %d\n", receiveNetworkParam);
  if(Serial.find("ssid:"))       //char* readnChar(char stringLength, char* list)
    readnChar(SSID_SIZE, ssid); // read SSID from UART
  else
    return false;

  if(Serial.find("password:"))
    readnChar(PASSWORD_SIZE, password); // read password from UART
  else
    return false;
  
  return true;
}

bool receivePillInfoUART() // receives pill information arguments from MCU through UART
{
  if(Serial.find("pillNames:["))
  {

    for (int i = 0; i < pillContainersCount; i++)
    {
      for (int j = 0; j < maxPillName; j++)
      {

        if (Serial.peek() == ',')
        {
          Serial.read();
          fill.pillNames[i][j] = '\0';
          j = maxPillName + 1;
        }

        else if (Serial.peek() == ']')
        {

          i = pillContainersCount - 1;
          j = maxPillName;
        }
        else
        {
          fill.pillNames[i][j] = Serial.read();
        }
      }

      if (debug)
        Serial.printf("fill.pillNames[%d] = %s\n", i, fill.pillNames[i]);
    }

    Serial.find("]");
  }
  else
    return false;
  

  if(Serial.find("pillQuantities:["))
  {
    readnIntList(maxDigitCount, fill.pillQuantities, pillContainersCount); // read quantities from UART
    Serial.find("]");
  }
  else return false;
  
  return true;
}

bool receiveRefillUART() // receives pill quanitites to update
{
  if(Serial.find("pillQuantities:["))
  {
    readnIntList(maxDigitCount, fill.pillQuantities, pillContainersCount); // read quantities from UART
    Serial.find("]");
  }
  else return false;

  return true;
}

bool setupInfoUART() //TODO
{
  //send all required variables at start
  return true;
}

bool receiveAddPillUART() // receives add pill parameters through UART
{ 
  // storeIndex: pillName: pillQuantity:
  if(debug) Serial.print("Entered receiveAddPillUART: ");
  int storeIndex = 0;
  if (Serial.find("storeIndex:"))
  { 
    storeIndex = readnInt(1);
    if (debug)
      Serial.printf("Add Pill Index = %d\n", storeIndex);
  }
  else
  {
    if(debug) Serial.printf("No storeIndex was found\n");
    return false;
  }

  if (Serial.find("pillName:"))
  {
    if (debug) Serial.print("pillName is \n");
    readnChar(maxPillName, fill.pillNames[storeIndex]);
    if (debug) Serial.println("END PILLNAME");
  }
  else
  {
    if(debug) Serial.printf("No pillName was found\n");
    return false;
  }


  if (Serial.find("pillQuantity:"))
  {
    fill.pillQuantities[storeIndex] = readnInt(maxDigitCount);
     if (debug) Serial.printf("pillQuantities are %d\n", fill.pillQuantities[storeIndex]);
  }
  else
  {
    if(debug) Serial.printf("No pillQuantity was found\n");
    return false;
  }

  Serial.find(sep_character);
  return true;
}

bool receiveRemovePillUART() // receives remove pill parameters through UART 
{
    // storeIndex:
  
  int storeIndex = 0;
  if (Serial.find("storeIndex:"))
  { 
    storeIndex = readnInt(1);
    if (debug)
      Serial.printf("Add Pill Index = %d\n", storeIndex);
  }
  else
  {
    if(debug) Serial.printf("No storeIndex was found\n");
    return false;
  }

  if (Serial.find("pillName:"))
  {
    if (debug) Serial.print("pillName is \n");
    readnChar(maxPillName, fill.pillNames[storeIndex]);
    if (debug) Serial.println("END PILLNAME");
  }
  else
  {
    if(debug) Serial.printf("No pillName was found\n");
    return false;
  }


  if (Serial.find("pillQuantity:"))
  {
    if (debug) Serial.print("pillQuantities are \n");
    fill.pillQuantities[storeIndex] = readnInt(maxDigitCount);
    if (debug) Serial.println("END PILLQUANTITY");
  }
  else
  {
    if(debug) Serial.printf("No pillQuantity was found\n");
    return false;
  }

  Serial.find(sep_character);
  return true;
}


//WiFi setting functions
bool WiFiCredentialsReady() // returns true if SSID and PASSWORD are set, otherwise returns false
{
  if (!ssid[0])
    return false;
  if (!password[0])
    return false;
  return true;
}

void WiFi_setup() // Sets up WiFi using SSID and PASSWORD set
{
  if (debug)
    Serial.print("connecting to Wifi \n");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  { // while not Wireless Lan Connected
    delay(500);
    Serial.print(".");
  }

  if (debug)
  {
    Serial.println();
    Serial.println("Connected to WiFi successfully");
  }

  Serial.print("NODEMCU IP Address : ");

  Serial.print(WiFi.localIP());

  if (debug)
    Serial.println("\nsetting up MDNS responder");

  while (!MDNS.begin("apdwifimodule"))
  {

    Serial.print(".");
    delay(1000);
  }
  if (debug)
    Serial.println("\nMDNS has started");

  server.begin();

  if (debug)
    Serial.println("\nHTTP Server has started");

  MDNS.addService("http", "tcp", 80);
}

// read from Serial
int readnInt(int digitsToRead) // reads n digits from Serial port and returns resulting int
{
  int result = 0;

  for (int i = digitsToRead; i >= 0; i--)
  {

    if (Serial.peek() == sep_character) //
      i = -1;

    else
      result += (Serial.read() - '0') * pow(10, i - 1);
  }

  return result;
}

char *readnChar(char stringLength, char *list) // reads n (char stringLength) chars from String (char* list)
{

  if (debug)
    Serial.printf("readnChar function was entered\n");

  for (int i = 0; i < stringLength; i++)
  {

    //        if(debug) Serial.printf("readnChar function - character %c, i=%d\n", Serial.peek(), i);

    if (Serial.peek() == sep_character || Serial.peek() == '\0')
    {
      Serial.read();
      list[i] = '\0';
      i = stringLength + 1;
    }
    else
      list[i] = Serial.read();
  }

  if (debug)
  {
    Serial.printf("readnChar function: list = %s\n", list);

    for (int i = 0; i < stringLength; i++)
    {
      Serial.printf("char at pos %d: %c\n", i, list[i]);
    }

    Serial.printf("readnChar function was finished\n");
  }

  return list;
}

int *readnIntList(char digitCount, int *list, int listLength) // reads n (char stringLength) chars from String (char* list)
{

  if (debug)
    Serial.printf("readnIntList function was entered\n");

  for (int i = 0; i < listLength; i++)
    list[i] = 0; //setting the list to 0

  for (int i = 0; i < listLength; i++)
  {
    for (int j = digitCount; j >= 0; j--)
    {
      if (Serial.peek() == ',' || Serial.peek() == ']')
      {
        Serial.read();
        j = -1;
      }
      else
      {
        list[i] += (Serial.read() - '0') * pow(10, j - 1);
      }
    }
    if (debug)
      Serial.printf("readnIntList function: list[%d] = %d\n", i, list[i]);
  }

  if (debug)
    Serial.printf("readnIntList function was finished\n");

  return list;
}

//UART RX function
void handleUARTRX() // RX function to check params for a UART transmission and react accordingly
{

  if (debug) Serial.print("\nENTERED RX\n");

  if (Serial.find("param:"))
  {

    param = Serial.read() - '0';

    if (debug)
      Serial.printf("\nparam = %d\n", param);
  }

  switch(param)
  {
    // case setupInfoParam: //error avoidance
    // setupInfoUART();
    // break;
  case receiveAnalyticsParam: //receive analytics
    receiveAnalyticsUART();
    break;
  case receiveNetworkParam: // receive network
    receiveNetworkUART();
    break;
  case receivePillInfoParam: //error avoidance
    receivePillInfoUART();
    break;
  case receiveAddReminderParam: //receive reminder
    receiveAddRemindersUART();
    break;
  case receiveRefillParam: //receive refill
    receiveRefillUART();
    break;
  case receiveAddParam: //receive add
    receiveAddPillUART();
    break;
  case receiveRemovePillParam: //receive remove
    receiveRemovePillUART();
    break;
  }

  param = 0;
  if (debug)
    Serial.print("\nFINISHED RX\n");
}

void setup() // put your setup code here, to run once:
{

  Serial.begin(baudRate);
  if (debug)
    Serial.println();

  WiFi.mode(WIFI_STA); //WIFI_STA - Devices that connect to WiFi network are called stations (STA)

  Serial.flush();

  if (WiFiCredentialsReady() && WiFiNotSet)
  {
    if (debug)
      Serial.print("WiFi ssid and password are set and not set up\n");
    WiFi_setup();
    if (debug)
      Serial.print("WiFi ssid and password are set and is set up\n");
    WiFiNotSet = false;
  }

  if (debug)
    Serial.println();

  //setup Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/submit_reminder_data", HTTP_POST, handleAddReminderPOST);
  server.on("/get_pill_status", HTTP_GET, handleRefillGET);
  server.on("/submit_refill_data", HTTP_POST, handleRefillPOST);
  server.on("/submit_add_data", HTTP_POST, handleAddPillPOST);

  server.on("/", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("access-control-allow-credentials", "false");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.sendHeader("access-control-allow-methods", "GET,OPTIONS");
    server.send(204);
  });

  server.on("/get_pill_status", HTTP_OPTIONS, []() {
    // server.sendHeader("Content-Type", "application/json");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.sendHeader("Access-Control-Allow-Methods", "PUT,POST,GET,OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(204, "application/json");
  });

  server.onNotFound(handleNotFound);

  Serial.print("Finished Basic Setup");
}

void post_DB(String json) // posts JSON to REST API running on rest_host
{

  if (debug)
    Serial.println("json is " + json + "\n");

  while (WiFi.status() != WL_CONNECTED)
  { // while not Wireless Lan Connected
    if (debug)
      Serial.println("WiFi is not connected");

    delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED)
  {

    WiFiClient client;

    HTTPClient http;

    if (debug)
      Serial.println("NodeMCU is connected to the internet\nStarting http request to rest_host");

    if (http.begin(client, rest_host))
    { // HTTP
      if (debug)
        Serial.println("[HTTP] POST...");

      http.addHeader("Content-Type", "application/json");

      int httpCode = http.POST(json);

      if (httpCode > 0)
      {
        // HTTP header has been send and Server response header has been handled
        if (debug)
          Serial.printf("[HTTP] POST... code: %d", httpCode);

        // file found at server
        //        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        //
        //        }
      }
      else
      {
        if (debug)
          Serial.printf("[HTTP] GET... failed, error: %s", http.errorToString(httpCode).c_str());
      }

      http.end();
    }
  }

  else
  {
    if (debug)
      Serial.printf("[HTTP} Unable to connect");
  }

  json = "";
}

// set struct to JSON
void Analytics_to_JSON() // generates JSON from analytic struct
{

  //            'original_date': self.original_date,
  //            'taken_date': self.taken_date,
  //            'completed': self.completed,
  //            'pill_names': self.pill_names,
  //            'pill_quantities': self.pill_quantities

  //   char* temp_original_date;
  //   char* temp_taken_date;

  json = "";

  DynamicJsonDocument Analyticsdoc(1024);

  JsonObject jobject = Analyticsdoc.to<JsonObject>();

  if (debug)
    Serial.printf("original_date is %02d-%02d-%02d %02d:%02d:00\n", analytics.year, analytics.month, analytics.day, analytics.hour, analytics.minute);

  sprintf(temp_original_date, "%02d-%02d-%02d %02d:%02d:00", analytics.year, analytics.month, analytics.day, analytics.hour, analytics.minute); //verify if errors
  jobject["original_date"] = temp_original_date;

  if (debug)
    Serial.printf("taken_date is %02d-%02d-%02d %02d:%02d:00\n", analytics.year, analytics.month, analytics.day, analytics.hour, analytics.minute);

  sprintf(temp_taken_date, "%02d-%02d-%02d %02d:%02d:00", analytics.year, analytics.month, analytics.day, analytics.TakenH, analytics.TakenM); //verify if errors
  jobject["taken_date"] = temp_taken_date;

  jobject["completed"] = analytics.taken;

  JsonArray pillNameArr = Analyticsdoc.createNestedArray("pill_names");

  for (int i = 0; i < pillContainersCount; i++)
  {
    // for(int j = 0; j<maxPillName; j++){
    pillNameArr.add(analytics.pillNames[i]);
    // }
  }

  JsonArray pillQuantitiesArr = Analyticsdoc.createNestedArray("pill_quantities");

  for (int i = 0; i < pillContainersCount; i++)
  {
    pillQuantitiesArr.add(analytics.pillQuantities[i]);
  }
  serializeJson(Analyticsdoc, json);
}

void Pill_Status_to_JSON() // generates JSON from refill struct
{
  json = "";
  //            'pillNames': fill.pillNames,
  //            'pillQuantities': fill.pillQuantities

  DynamicJsonDocument Refilldoc(1024);

  JsonArray pillNameArr = Refilldoc.createNestedArray("pillNames");
  JsonArray pillQuantitiesArr = Refilldoc.createNestedArray("pillQuantities");

  for (int i = 0; i < pillContainersCount; i++)
    pillNameArr.add(fill.pillNames[i]);

  for (int i = 0; i < pillContainersCount; i++)
    pillQuantitiesArr.add(fill.pillQuantities[i]);

  serializeJson(Refilldoc, json);
}

void loop() // main code to run repeatedly: 
{

  //  WiFiClient client = server.available();

  if (WiFiCredentialsReady() && WiFiNotSet)
  {
    if (debug)
      Serial.print("\nWiFi ssid and password are set and not set up\n");

    WiFi_setup();

    if (debug)
      Serial.print("\nWiFi ssid and password are set and is set up\n");

    WiFiNotSet = false;
  }

  if (!WiFiNotSet) MDNS.update();
  

  if (Serial.available()) // if UART RX port has data
  {
    handleUARTRX(); // call RX function to handle RX UART inputs
  }

  server.handleClient();

  delay(500);
}