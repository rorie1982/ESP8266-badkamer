#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "IthoCC1101.h"
#include "IthoPacket.h"
#include <DHT.h>
#define min(X, Y) (((X)<(Y))?(X):(Y))
#include <ESP8266HTTPUpdateServer.h>

#define DHTPIN D4     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)

IthoCC1101 rf;
IthoPacket packet;
IPAddress ip(192, 168, 1, 107);
ESP8266WebServer httpServer(80);
DHT dht(DHTPIN, DHTTYPE);
ESP8266HTTPUpdateServer httpUpdater;

String currentState = "Laag";
String currentHostName = "ESP-Badkamer-01";
String currentVersion = "1.0.9";
String currentIpAdres = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
String pilightIpAdres = "192.168.1.108";
boolean currentInAutomaticMode = false;
boolean TimerIsActive = false;
float currentTemperature = 0;
float currentHumidity = 0;
float firstMeasurementMoment = 0;
float secondMeasurementMoment = 0;
float delta =0;
float targetHumidity = 0;
float storedTargetHumidity = 0;
long timer_lastInterval =0;
long dht_lastInterval  = 0;
int itho_counter =0;

void setup() {
  
  const char* username = "Rorie";
  const char* password = "j10HIaueQBKM";
  
  //Initialization dht22 temperature and humidity sensor.
  dht.begin(); 

  //First read temperature and humidity.
  readTemperatureAndHumidity();

  //Initialization of first and second measure moment, to avoid deviding by zero.
  firstMeasurementMoment = currentHumidity;
  secondMeasurementMoment = currentHumidity;

  //Initialization of CC1101 module.
  rf.init();

  //Join command send, this is only needed once.
  //sendRegister();  

  //Connect to the WIFI network
  ConnectToWifi();

  //Set end points of the webserver
  httpServer.on("/", handlePage);
  httpServer.on("/xml",handle_xml);
  httpServer.on("/action",handle_buttonPressed);

  //Set the CC1101 module in receive mode for the first time, this needs to be done everytime the send data was called.
  rf.initReceive();

  //Start webserver  
  httpUpdater.setup(&httpServer, username, password);
  httpServer.begin(); 

  //Synq with external system pilight.
  handle_lowSpeed();
  
}

void loop() {
  
  //Check if there is any other remote pressed in the house.
  //So check if there are new packages received from other remotes.
  if (rf.checkForNewPacket())
  {
    updateCurrentMode();
  }

  //elke 30 seconde
  if (millis() - dht_lastInterval > 30000)
  {
    readTemperatureAndHumidity();
    
    delta = currentHumidity - min(firstMeasurementMoment, secondMeasurementMoment);
    
    storedTargetHumidity = min(secondMeasurementMoment, firstMeasurementMoment) + 3;
    
    secondMeasurementMoment = firstMeasurementMoment; 
    firstMeasurementMoment = currentHumidity;

    if (!currentInAutomaticMode && delta > 3)
    {
       handle_highSpeed();
       currentInAutomaticMode = true;
       TimerIsActive = false;
       targetHumidity = storedTargetHumidity;
    }
    else if (currentInAutomaticMode && currentHumidity <= targetHumidity)
    {
       handle_lowSpeed();
       currentInAutomaticMode = false;
       targetHumidity = 0;
    } 
    dht_lastInterval = millis();
  }

  //If the timer is active and the system is not in automatic mode
  if (TimerIsActive && !currentInAutomaticMode)
  {
      //If timer is set to active and CurrentInAutomaticMode is false then check if 10 minutes are over
      if (millis() - timer_lastInterval > 600000)
      {
        handle_lowSpeed();
        TimerIsActive = false;
      }
  }
  
  httpServer.handleClient();  
}

void updateCurrentMode()
{
  String idOfWCRemote = "65-9a-66-95-a5-a9-9a-56";
  String idOfBadkamerRemote = "66-6a-66-69-a5-69-9a-56";
  String lastPressedRemoteId = rf.getLastIDstr(); 
  String remoteName;

  IthoPacket packet;
  packet = rf.getLastPacket();

  if (lastPressedRemoteId == idOfWCRemote)
  {
    remoteName = "(WC)";
  }
  else if(lastPressedRemoteId == idOfBadkamerRemote)
  {
    remoteName = "(Badkamer)";
  }
  if (lastPressedRemoteId == idOfWCRemote || lastPressedRemoteId == idOfBadkamerRemote)
  {
    switch (packet.command) { 
    case IthoUnknown: 
      break; 
    case IthoLow: 
      currentState = "Laag:" + remoteName;
      TimerIsActive = false; 
      currentInAutomaticMode = false;
      break; 
    case IthoMedium: 
      currentState = "Medium:" + remoteName;
      activateTimer();
      break; 
    case IthoFull: 
      currentState = "Hoog:" + remoteName;
      activateTimer();
      break; 
    case IthoTimer1: 
      currentState = "Timer 10 minuten:" + remoteName;
      TimerIsActive = false;
      break; 
    }
  }
}

void handle_lowSpeed() { 
  currentState="Laag";
  sendLowSpeed();
  sendLowSpeed();
  rf.initReceive(); 
}

void handle_mediumSpeed() { 
  currentState="Medium";
  sendMediumSpeed();
  sendMediumSpeed();
  rf.initReceive(); 
}

void handle_highSpeed() { 
  currentState="Hoog";
  sendFullSpeed();
  sendFullSpeed();
  rf.initReceive();
}

void handle_timer() { 
  currentState="Timer 10 minuten";
  TimerIsActive = false;
  sendTimer();
  rf.initReceive(); 
}

void handlePage()
{
  String webPage;

  webPage += "<!DOCTYPE html>";
  webPage += "<html lang=\"en\">";
  webPage += "<head>";
  webPage += "<title>Badkamer</title>";
  webPage += "<meta charset=\"utf-8\">";
  webPage += "<meta name=\"apple-mobile-web-app-capable\" content=\"yes\">";
  webPage += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  webPage += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">";
  webPage += GenerateFavIcon();
  webPage += "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js\"></script>";
  webPage += "<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\"></script>";
  webPage += GenerateJavaScript();
  webPage += "</head>";
  webPage += "<body>";
  webPage += "<div class=\"container\">";
  webPage += "<h2>Badkamer</h2>";
  webPage += "<div id=\"BPanelVentilation\" class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Ventilatie</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class=\"glyphicon glyphicon-menu-hamburger\"></span>&nbsp;";
  webPage += "Ventilatie:<span class='pull-right'>"; 
  webPage += "<button type='button' class='btn btn-default' id='btnLow' onclick='ButtonPressed(this.id)'><span>Laag</span></button>&nbsp;"; 
  webPage += "<button type='button' class='btn btn-default' id='btnMedium' onclick='ButtonPressed(this.id)'><span>Medium</span></button>&nbsp;";
  webPage += "<button type='button' class='btn btn-default' id='btnFast' onclick='ButtonPressed(this.id)'><span>Hoog</span></button>&nbsp; ";
  webPage += "</span>"; 
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-dashboard'></span>&nbsp;"; 
  webPage += "Stand: <span class='pull-right' id='BStand01'></span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-tasks'></span>&nbsp;"; 
  webPage += "Modus: <span class='pull-right' id='BModus01'></span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-time'></span>&nbsp;"; 
  webPage += "Automatische timer: <span class='pull-right' id='BTimer01'></span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "<div class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Klimaat</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-fire'></span>&nbsp;"; 
  webPage += "Temperatuur: <span class='pull-right' id='BTemp01'></span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-tint'></span>&nbsp;";
  webPage += "Luchtvogtigheid: <span class='pull-right' id='BHum01'></span>"; 
  webPage += "</div>"; 

  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-tint'></span>&nbsp;";
  webPage += "Delta: <span class='pull-right' id='delta'></span>"; 
  webPage += "</div>"; 

  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-tint'></span>&nbsp;";
  webPage += "Target: <span class='pull-right' id='target'></span>"; 
  webPage += "</div>"; 
  
  webPage += "</div>"; 
  webPage += "<div class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Algemeen</div>"; 
  webPage += "<div class=\"panel-body\">"; 
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>&nbsp;"; 
  webPage += "Firmware: <span class='pull-right'>" + currentVersion + "</span>";
  webPage += "</div>"; 
  webPage += "<div class=\"panel-body\">"; 
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>&nbsp;"; 
  webPage += "IP-adres: <span class='pull-right'>" + currentIpAdres + "</span>"; 
  webPage += "</div>"; 
  webPage += "<div class=\"panel-body\">"; 
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>&nbsp;"; 
  webPage += "Host naam: <span class='pull-right'>" + currentHostName + "</span>"; 
  webPage += "</div>"; 
  webPage += "</div>"; 
  webPage += "</div>";
  webPage += "</body>";
  webPage += "</html>";

  httpServer.send ( 200, "text/html", webPage);
}

String GenerateFavIcon()
{
  //Generate Favicon for Web, Android, Microsoft, and iOS (iPhone and iPad) Apps.
  //Actual png files that are used are stored on a external apache webserver.
  
  String returnValue;
  
  returnValue += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"57x57\" href=\"http://" + pilightIpAdres + "/apple-icon-57x57.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"60x60\" href=\"http://" + pilightIpAdres + "/apple-icon-60x60.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"72x72\" href=\"http://" + pilightIpAdres + "/apple-icon-72x72.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"76x76\" href=\"http://" + pilightIpAdres + "/apple-icon-76x76.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"114x114\" href=\"http://" + pilightIpAdres + "/apple-icon-114x114.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"120x120\" href=\"http://" + pilightIpAdres + "/apple-icon-120x120.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"144x144\" href=\"http://" + pilightIpAdres + "/apple-icon-144x144.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"152x152\" href=\"http://" + pilightIpAdres + "/apple-icon-152x152.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"180x180\" href=\"http://" + pilightIpAdres + "/apple-icon-180x180.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"192x192\"  href=\"http://" + pilightIpAdres + "/android-icon-192x192.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"32x32\" href=\"http://" + pilightIpAdres + "/favicon-32x32.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"96x96\" href=\"http://" + pilightIpAdres + "/favicon-96x96.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"16x16\" href=\"http://" + pilightIpAdres + "/favicon-16x16.png\">";
  returnValue += "<link rel=\"manifest\" href=\"http://" + pilightIpAdres + "/manifest.json\">";
  returnValue += "<meta name=\"msapplication-TileColor\" content=\"#ffffff\">";
  returnValue += "<meta name=\"msapplication-TileImage\" content=\"http://" + pilightIpAdres + "/ms-icon-144x144.png\">";
  returnValue += "<meta name=\"theme-color\" content=\"#ffffff\">";
  
  return returnValue;
}

void handle_xml()
{
  String returnValue;
  String currentModes;
  String automaticTimer;
  
  if(currentInAutomaticMode)
  {
      currentModes = "Automatisch";
  }
  else
  {
      currentModes = "Handmatig";
  }

   if(TimerIsActive)
  {
      automaticTimer = "Actief";
  }
  else
  {
      automaticTimer = "Niet actief";
  }
  
  returnValue += "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
  returnValue += "<Badkamer>";
  returnValue += "<sensors>";
  returnValue += "<sensor>";
  returnValue += "<id>BTemp01</id>";
  returnValue += "<value>" + String(currentTemperature) + " °C</value>";
  returnValue += "</sensor>";
  returnValue += "<sensor>";
  returnValue += "<id>BHum01</id>";
  returnValue += "<value>" + String(currentHumidity) + " %</value>";
  returnValue += "</sensor>";
  returnValue += "</sensors>";
  returnValue += "<devices>";
  returnValue += "<device>";
  returnValue += "<id>BStand01</id>";
  returnValue += "<value>" + currentState + "</value>";
  returnValue += "</device>";
  returnValue += "<device>";
  returnValue += "<id>BModus01</id>";
  returnValue += "<value>" + currentModes + "</value>";
  returnValue += "</device>";
  returnValue += "<device>";
  returnValue += "<id>BTimer01</id>";
  returnValue += "<value>" + automaticTimer + "</value>";
  returnValue += "</device>";
  returnValue += "<device>";
  returnValue += "<id>target</id>";
  returnValue += "<value>" + String(targetHumidity) + "</value>";
  returnValue += "</device>";
   returnValue += "<device>";
  returnValue += "<id>delta</id>";
  returnValue += "<value>" + String(delta) + "</value>";
  returnValue += "</device>";
  returnValue += "</devices>";
  returnValue += "</Badkamer>";
  
  httpServer.send ( 200, "text/html", returnValue);
}

String GenerateJavaScript()
{
  String returnValue;
  returnValue += "<script type=\"text/javascript\">";
  returnValue += "function LoadData(){";
  returnValue += "$.ajax({";
  returnValue += "type: \"GET\",";
  returnValue += "url: \"http://" + currentIpAdres + "/xml\",";
  returnValue += "cache: false,";
  returnValue += "dataType: \"xml\",";
  returnValue += "success: DisplayData";
  returnValue += "});";
  returnValue += "}";
  
  returnValue += "function ButtonPressed(id){";
  returnValue += "$.post('http://" + currentIpAdres + "/action?id=' + id);";
  returnValue += "LoadData();";
  returnValue += "}";
  
  returnValue += "function DisplayData(result){";
  returnValue += "$(result).find('sensor').each(function(){";
  returnValue += "var id = $(this).find(\"id\").text();";
  returnValue += "var value = $(this).find(\"value\").text();";
  returnValue += "$('#' + id).text(value);";
  returnValue += "});";
  returnValue += "$(result).find('device').each(function(){";
  returnValue += "var id = $(this).find(\"id\").text();";
  returnValue += "var value = $(this).find(\"value\").text();";
  returnValue += "$('#' + id).text(value);";
  returnValue += "});";
  returnValue += "UpdatePanels();";
  returnValue += "}";

  returnValue += "function UpdatePanels(){";
  returnValue += "var statusVentilatie = $('#BStand01').html();";
  returnValue += "if(statusVentilatie.includes(\"Laag\"))";
  returnValue += "{";
  returnValue += "$('#BPanelVentilation').removeClass('panel panel-success').addClass('panel panel-default');";;
  returnValue += "}";
  returnValue += "else";
  returnValue += "{";
  returnValue += "$('#BPanelVentilation').removeClass('panel panel-default').addClass('panel panel-success');";
  returnValue += "}";
  returnValue += "}";
  
  returnValue += "$(document).ready(function (){";
  returnValue += "LoadData();";
  returnValue += "setInterval(LoadData,3000);";
  returnValue += "})";
  returnValue += "</script>";
  
  return returnValue;
}

void handle_buttonPressed()
{
  if (httpServer.args() > 0)
  {
    if (httpServer.arg(0) == "btnLow")
    {
      handle_lowSpeed();
      TimerIsActive = false;
      currentInAutomaticMode = false;
    }
    else if (httpServer.arg(0) == "btnMedium")
    {
      handle_mediumSpeed();
      activateTimer();
    }
    else if (httpServer.arg(0) == "btnFast")
    {
      handle_highSpeed();
      activateTimer();
    }
  }
}

void readTemperatureAndHumidity()
{
  // Reading temperature for humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  float h = dht.readHumidity();          // Read humidity (percent)
  float t = dht.readTemperature();     // Read temperature as Fahrenheit
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) 
  {
    return;
  }

  currentHumidity = h;          // Read humidity (percent)
  currentTemperature = t;     // Read temperature as Fahrenheit
}

void ConnectToWifi()
{
  char ssid[] = "Rorie";
  char pass[] = "BIAW7W9H7Y8D";
  int i = 0;

  IPAddress gateway(192, 168, 1, 1); 
  IPAddress subnet(255, 255, 255, 0); 
  WiFi.config(ip, gateway, subnet);
  
  WiFi.hostname(currentHostName);
  WiFi.begin(ssid, pass);
  WiFi.mode(WIFI_STA);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void activateTimer()
{
  if(!currentInAutomaticMode)
  {
    TimerIsActive = true;
    timer_lastInterval = millis();
  }
}

void sendRegister() {
   rf.sendCommand(IthoJoin);
}

void sendLowSpeed() {
   rf.sendCommand(IthoLow);
}

void sendMediumSpeed() {
   rf.sendCommand(IthoMedium);
}

void sendFullSpeed() {
   rf.sendCommand(IthoFull);
}

void sendTimer() {
   rf.sendCommand(IthoTimer1);
}
  

