#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "IthoCC1101.h"
#include "IthoPacket.h"
#include <DHT.h>
#define min(X, Y) (((X)<(Y))?(X):(Y))

#define DHTPIN D4     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)

IthoCC1101 rf;
IthoPacket packet;
IPAddress ip(192, 168, 1, 107);
ESP8266WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);

String currentState = "Laag";
String currentHostName = "ESP-Badkamer-01";
String currentVersion = "1.0.1";
boolean CurrentInAutomaticMode = false;
boolean TimerIsActive = false;
float CurrentTemperature = 0;
float CurrentHumidity = 0;
float firstMeasurementMoment = 0;
float secondMeasurementMoment = 0;
float delta =0;
float targetHumidity = 0;
float storedTargetHumidity = 0;
long timer_lastInterval =0;
long dht_lastInterval  = 0;
int itho_counter =0;

void setup() {
  Serial.begin(115200);
  
  dht.begin(); 
  readTemperatureAndHumidity();
  firstMeasurementMoment = CurrentHumidity;
  secondMeasurementMoment = CurrentHumidity;
  
  rf.init();

  ConnectToWifi();
  
  server.on("/", handlePage);
  server.on("/xml",handle_xml);
  server.on("/action",handle_buttonPressed);

  rf.initReceive();

  handlePage();   
  server.begin(); 

  UpdatePilightLabel("Handmatig",111);
  UpdatePilightLabel(currentState,112);
  
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
    Serial.println("30 seconden voorbij");
    readTemperatureAndHumidity();
    UpdatePiligtTemperatureAndHumidity(CurrentTemperature,CurrentHumidity,110);
    dht_lastInterval = millis();

    delta = CurrentHumidity - min(firstMeasurementMoment, secondMeasurementMoment);
    Serial.println("First en second measurement values delta is calculated with: ");
    Serial.print(firstMeasurementMoment);
    Serial.print(secondMeasurementMoment);
    Serial.println("Delta: ");
    Serial.print(delta);
    
    storedTargetHumidity = min(secondMeasurementMoment, firstMeasurementMoment) + 2;
    Serial.println("storedTargetHumidity: ");
    Serial.print(storedTargetHumidity);
    
    secondMeasurementMoment = firstMeasurementMoment; 
    firstMeasurementMoment = CurrentHumidity;

    if (!CurrentInAutomaticMode && delta > 3)
    {
       Serial.println("Delta > 3");
       handle_highSpeed();
       CurrentInAutomaticMode = true;
       UpdatePilightLabel("Automatisch",111);
       targetHumidity = storedTargetHumidity;
       Serial.println("Target humidity set to: ");
       Serial.print(targetHumidity);
    }
    else if (CurrentInAutomaticMode && CurrentHumidity <= targetHumidity)
    {
       handle_lowSpeed();
       CurrentInAutomaticMode = false;
       UpdatePilightLabel("Automatisch",111);
       Serial.println("CurrentHumidity <= targetHumidity: ");
    } 
     Serial.print("No action required");
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
  String remoteId = rf.getLastIDstr(); 
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
      currentState = "Laag:" + remoteName;
      TimerIsActive = false;
      UpdatePilightLabel(currentState,112);  
      break; 
    case IthoMedium: 
      currentState = "Medium:" + remoteName;
      UpdatePilightLabel(currentState,112); 
      activateTimer();
      break; 
    case IthoFull: 
      currentState = "Hoog:" + remoteName;
      UpdatePilightLabel(currentState,112); 
      activateTimer();
      break; 
    case IthoTimer1: 
      currentState = "Timer 10 minuten:" + remoteName;
      TimerIsActive = false;
      UpdatePilightLabel(currentState,112);
      break; 
    }
  }
}

void handle_lowSpeed() { 
  currentState="laag";
  TimerIsActive = false;
  UpdatePilightLabel(currentState,112);
  sendLowSpeed();
  handlePage();
  rf.initReceive(); 
}

void handle_mediumSpeed() { 
  currentState="Medium";
  UpdatePilightLabel(currentState,112);
  activateTimer();
  sendMediumSpeed();
  handlePage();
  rf.initReceive(); 
}

void handle_highSpeed() { 
  currentState="Hoog";
  UpdatePilightLabel(currentState,112);
  activateTimer();
  sendFullSpeed();
  handlePage();
  rf.initReceive();
}

void handle_timer() { 
  currentState="Timer 10 minuten";
  TimerIsActive = false;
  UpdatePilightLabel(currentState,112);
  sendTimer();
  handlePage();
  rf.initReceive(); 
}

void handlePage()
{
  String webPage;
  String currentIpAdres = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

  webPage += "<!DOCTYPE html>";
  webPage += "<html lang=\"en\">";
  webPage += "<head>";
  webPage += "<title>Badkamer</title>";
  webPage += "<meta charset=\"utf-8\">";
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
  webPage += "<div class=\"panel-group\">";
  webPage += "<div id=\"BPanelVentilation\" class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Ventilatie</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class=\"glyphicon glyphicon-menu-hamburger\"></span>&nbsp;";
  webPage += "Itho ecofan bediening:<span class='pull-right'>"; 
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
  webPage += "</div>";
  webPage += "</body>";
  webPage += "</html>";

  server.send ( 200, "text/html", webPage);
}

String GenerateFavIcon()
{
  String ExternalDataSourceIpAdres = "192.168.1.108";
  String returnValue;
  
  returnValue += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"57x57\" href=\"http://" + ExternalDataSourceIpAdres + "/apple-icon-57x57.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"60x60\" href=\"http://" + ExternalDataSourceIpAdres + "/apple-icon-60x60.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"72x72\" href=\"http://" + ExternalDataSourceIpAdres + "/apple-icon-72x72.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"76x76\" href=\"http://" + ExternalDataSourceIpAdres + "/apple-icon-76x76.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"114x114\" href=\"http://" + ExternalDataSourceIpAdres + "/apple-icon-114x114.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"120x120\" href=\"http://" + ExternalDataSourceIpAdres + "/apple-icon-120x120.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"144x144\" href=\"http://" + ExternalDataSourceIpAdres + "/apple-icon-144x144.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"152x152\" href=\"http://" + ExternalDataSourceIpAdres + "/apple-icon-152x152.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"180x180\" href=\"http://" + ExternalDataSourceIpAdres + "/apple-icon-180x180.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"192x192\"  href=\"http://" + ExternalDataSourceIpAdres + "/android-icon-192x192.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"32x32\" href=\"http://" + ExternalDataSourceIpAdres + "/favicon-32x32.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"96x96\" href=\"http://" + ExternalDataSourceIpAdres + "/favicon-96x96.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"16x16\" href=\"http://" + ExternalDataSourceIpAdres + "/favicon-16x16.png\">";
  returnValue += "<link rel=\"manifest\" href=\"http://" + ExternalDataSourceIpAdres + "/manifest.json\">";
  returnValue += "<meta name=\"msapplication-TileColor\" content=\"#ffffff\">";
  returnValue += "<meta name=\"msapplication-TileImage\" content=\"http://" + ExternalDataSourceIpAdres + "/ms-icon-144x144.png\">";
  returnValue += "<meta name=\"theme-color\" content=\"#ffffff\">";
  
  return returnValue;
}

void handle_xml()
{
  String returnValue;
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
  
  returnValue += "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
  returnValue += "<Badkamer>";
  returnValue += "<sensors>";
  returnValue += "<sensor>";
  returnValue += "<id>BTemp01</id>";
  returnValue += "<value>" + String(CurrentTemperature) + " °C</value>";
  returnValue += "</sensor>";
  returnValue += "<sensor>";
  returnValue += "<id>BHum01</id>";
  returnValue += "<value>" + String(CurrentHumidity) + " %</value>";
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
  returnValue += "</devices>";
  returnValue += "</Badkamer>";
  
  server.send ( 200, "text/html", returnValue);
}

String GenerateJavaScript()
{
  String returnValue;
  returnValue += "<script type=\"text/javascript\">";
  returnValue += "function LoadData(){";
  returnValue += "$.ajax({";
  returnValue += "type: \"GET\",";
  returnValue += "url: \"http://192.168.1.109/xml\",";
  returnValue += "cache: false,";
  returnValue += "dataType: \"xml\",";
  returnValue += "success: DisplayData";
  returnValue += "});";
  returnValue += "}";
  
  returnValue += "function ButtonPressed(id){";
  returnValue += "$.post('http://192.168.1.109/action?id=' + id);";
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
  returnValue += "if (statusVentilatie == \"Ingeschakelt\")";
  returnValue += "{";
  returnValue += "$('#BPanelVentilation').removeClass('panel panel-default').addClass('panel panel-success');";
  returnValue += "}";
  returnValue += "else";
  returnValue += "{";
  returnValue += "$('#BPanelVentilation').removeClass('panel panel-success').addClass('panel panel-default');";
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
  if (server.args() > 0)
  {
    if (server.arg(0) == "btnLow")
    {
      //Serial.println("button up pressed");
      handle_lowSpeed();
    }
    else if (server.arg(0) == "btnMedium")
    {
      //Serial.println("button stop pressed");
      handle_mediumSpeed();
    }
    else if (server.arg(0) == "btnFast")
    {
      //Serial.println("button down pressed");
      handle_highSpeed();
    }
  }
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

  IPAddress gateway(192, 168, 1, 1); 
  IPAddress subnet(255, 255, 255, 0); 
  WiFi.config(ip, gateway, subnet);
  
  WiFi.hostname(currentHostName);
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
  if(!CurrentInAutomaticMode)
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

void UpdatePilight(String url)
  {    
    const char* host = "192.168.1.108";
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 5001;
    
    if (client.connect(host, httpPort)) {
      // This will send the request to the server
      client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" + 
                 "Connection: close\r\n\r\n");
            
    }
  }

  void UpdatePiligtTemperatureAndHumidity(float temperature,float humidity, int id)
  {
    String url = "/send?protocol=generic_weather&temperature=";
    url += temperature;
    url += "&humidity=";
    url += humidity;
    url += "&id=";
    url += id;
    
    UpdatePilight(url);
  }

  void UpdatePilightLabel(String LabelTekst, int id)
  {
    String l = "/send?protocol=generic_label&label=";
    l += LabelTekst;
    l += "&color=black&id=";
    l += id;
    UpdatePilight(l);
  }
  

