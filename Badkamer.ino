#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "IthoCC1101.h"
#include "IthoPacket.h"
#include <DHT.h>

#define DHTPIN D4     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)

IthoCC1101 rf;
IthoPacket packet;
IPAddress ip;
ESP8266WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);

String CurrentState = "Laag";
String CurrentHostName = "Badkamer";
boolean CurrentInAutomaticMode = false;
boolean TimerIsActive = false;
float CurrentTemperature = 0;
float CurrentHumidity = 0;
long timer_lastInterval =0;
long dht_lastInterval  = 0;
int itho_counter =0;

void setup() {
  dht.begin(); 
  rf.init();
  
  ConnectToWifi();
  
  server.on("/", handlePage);
  server.on("/lowSpeed", handle_lowSpeed); 
  server.on("/MediumSpeed", handle_mediumSpeed); 
  server.on("/HighSpeed", handle_highSpeed); 
  server.on("/Timer", handle_timer); 

  rf.initReceive();
     
  server.begin(); 
}

void loop() {
  
  if (rf.checkForNewPacket())
  {
    updateCurrentMode();
  }

  //elke 20 seconde
  if (millis() - dht_lastInterval > 20000)
  {
    readTemperatureAndHumidity();
    dht_lastInterval = millis();

    if (CurrentHumidity > 50 && !CurrentInAutomaticMode)
    {     
          itho_counter = itho_counter +1;

          if (itho_counter > 2)
          {
          handle_highSpeed();
          CurrentInAutomaticMode = true;
          TimerIsActive = false;
          itho_counter = 0;
          }
    }
    else if (CurrentInAutomaticMode  && CurrentHumidity < 48)
    {
          itho_counter = itho_counter +1;

          if (itho_counter > 2)
          {
            handle_lowSpeed();
            CurrentInAutomaticMode = false;
            TimerIsActive = false;
            itho_counter = 0;
          }
    }
    else
    {
      if (itho_counter != 0)
      {
        itho_counter = 0;
      }
    }
    
  }

  if (TimerIsActive && !CurrentInAutomaticMode)
  {
      //If timer is set to active and CurrentInAutomaticMode is false then check if 10 minutes are over
      if (millis() - timer_lastInterval > 600000)
      {
        handle_lowSpeed();
        TimerIsActive = false;
      }
  }
  
  server.handleClient();
  
}

void updateCurrentMode()
{
  //65-9a-66-95-a5-a9-9a-56 wc
  //66-6a-66-69-a5-69-9a-56 badkamer
  IthoPacket packet;
  packet = rf.getLastPacket();
  String remoteId = rf.getLastIDstr().substring(0,2); 
  String remoteName;

  if (remoteId == "65-9a-66-95-a5-a9-9a-56")
  {
    remoteName = "(WC)";
  }
  else if(remoteId == "66-6a-66-69-a5-69-9a-56")
  {
    remoteName = "(Badkamer)";
  }
  
  if (remoteId == "65-9a-66-95-a5-a9-9a-56" || remoteId == "66-6a-66-69-a5-69-9a-56")
  {
    switch (packet.command) { 
    case IthoUnknown: 
      break; 
    case IthoLow: 
      CurrentState = "Laag " + remoteName;
      CurrentInAutomaticMode = false;   
      break; 
    case IthoMedium: 
      CurrentState = "Medium " + remoteName; 
      activateTimer();
      CurrentInAutomaticMode = false;
      break; 
    case IthoFull: 
      CurrentState = "Hoog " + remoteName; 
      activateTimer();
      CurrentInAutomaticMode = false;
      break; 
    case IthoTimer1: 
      CurrentState = "Timer 10 minuten " + remoteName;
      CurrentInAutomaticMode = false; 
      break; 
    }
  }
}

void handle_lowSpeed() { 
  CurrentState="laag";
  CurrentInAutomaticMode = false;
  sendLowSpeed();
  handlePage();
  rf.initReceive(); 
}

void handle_mediumSpeed() { 
  CurrentState="Medium";
  CurrentInAutomaticMode = false;
  activateTimer();
  sendMediumSpeed();
  handlePage();
  rf.initReceive(); 
}

void handle_highSpeed() { 
  CurrentState="Hoog";
  CurrentInAutomaticMode = false;
  activateTimer();
  sendFullSpeed();
  handlePage();
  rf.initReceive();
}

void handle_timer() { 
  CurrentState="Timer 10 minuten";
  CurrentInAutomaticMode = false;
  sendTimer();
  handlePage();
  rf.initReceive(); 
}

void handlePage()
{
  String currentModes;
  String automaticTimer;
  
  if(CurrentInAutomaticMode)
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
  
  
  String webPage;
  String CurrentIpAdres = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  String header = "<html lang='en'><head><title>Itho ECO 2SE control paneel</title><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' href='http://maxcdn.bootstrapcdn.com/bootstrap/3.3.4/css/bootstrap.min.css'><script src='https://ajax.googleapis.com/ajax/libs/jquery/1.11.1/jquery.min.js'></script><script src='http://maxcdn.bootstrapcdn.com/bootstrap/3.3.4/js/bootstrap.min.js'></script></head><body>";
  String navbar = "<nav class='navbar navbar-default'><div class='container-fluid'><div class='navbar-header'><a class='navbar-brand' href='/'>Itho control panel</a></div><div><ul class='nav navbar-nav'><li><a href='/'><span class='glyphicon glyphicon-question-sign'></span> Status</a></li><li class='dropdown'><a class='dropdown-toggle' data-toggle='dropdown' href='#'><span class='glyphicon glyphicon-cog'></span> Tools<span class='caret'></span></a><ul class='dropdown-menu'><li><a href='/updatefwm'>Firmware</a></li><li><a href='/api?action=reset&value=true'>Restart</a></ul></li><li><a href='https://github.com/incmve/Itho-WIFI-remote' target='_blank'><span class='glyphicon glyphicon-question-sign'></span> Help</a></li></ul></div></div></nav>  ";
  String containerStart = "<div class='container'><div class='row'>";
  String IthoInformationTitle = "<div class='col-md-4'><div class='page-header'><h1>Itho ECO 2SE control paneel</h1></div>";
  String IPAddClient = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globeglobe'></span> IP-adres:<span class='pull-right'>" + CurrentIpAdres + "</span></div></div>";
  String ClientName = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globetag'></span> Hostnaam:<span class='pull-right'>" + CurrentHostName + "</span></div></div>";
  String ithoMode = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globeok'></span> Modus:<span class='pull-right'>" + currentModes + "</span></div></div>";
  String State = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globeinfo-sign'></span> Stand:<span class='pull-right'>" + CurrentState + "</span></div></div>";
  String DHTsensor = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globefire'></span> Temperatuur:<span class='pull-right'>" + (String)CurrentTemperature + " °C</span></div><div class='panel-body'><span class='glyphicon glyphicon-tint'></span> Luchtvogtigheid:<span class='pull-right'>" + (String)CurrentHumidity + " %</span></div></div></div>";
  String Timer = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globeinfo-sign'></span> automatische timer:<span class='pull-right'>" + automaticTimer + "</span></div></div>";
  String IthoButtonCommands = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globe'></span> <div class='row'><div class='span6' style='text-align:center'><a href=\"lowSpeed\"><button type='button' class='btn btn-default'>Laag</button></a><a href=\"MediumSpeed\"><button type='button' class='btn btn-default'>Medium</button></a><a href=\"HighSpeed\"><button type='button' class='btn btn-default'> Hoog</button><a href=\"Timer\"><button type='button' class='btn btn-default'>Timer</button></a></a><br></div></span></div></div>";
  String containerEnd = "<div class='clearfix visible-lg'></div></div></div>";
  String siteEnd = "</body></html>";
  webPage = header + navbar + containerStart + IthoInformationTitle + IPAddClient + ClientName + ithoMode + State + DHTsensor + Timer + IthoButtonCommands + containerEnd + siteEnd;
  server.send ( 200, "text/html", webPage);
}

void readTemperatureAndHumidity()
{
  // Reading temperature for humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  CurrentHumidity = dht.readHumidity();          // Read humidity (percent)
  CurrentTemperature = dht.readTemperature();     // Read temperature as Fahrenheit
  // Check if any reads failed and exit early (to try again).
  if (isnan(CurrentHumidity) || isnan(CurrentTemperature)) 
  {
    return;
  }
}

void ConnectToWifi()
{
  char ssid[] = "";
  char pass[] = "";
  int i = 0;
  
  WiFi.hostname(CurrentHostName);
  WiFi.begin(ssid, pass);
  
  if (WiFi.status() != WL_CONNECTED && i >= 30)
  {
    WiFi.disconnect();
    delay(1000);
  }
  else
  {
    delay(5000);
    ip = WiFi.localIP();
  }  
}

void activateTimer()
{
  TimerIsActive = true;
  timer_lastInterval = millis();
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
