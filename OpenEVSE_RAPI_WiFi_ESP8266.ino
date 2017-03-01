/*
 * Copyright (c) 2015 Chris Howell
 *
 * This file is part of Open EVSE.
 * Open EVSE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * Open EVSE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with Open EVSE; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

//Default SSID and PASSWORD for AP Access Point Mode
const char* ssid = "OpenEVSE";
const char* password = "openevse";
const char* www_username = "admin";
const char* www_password = "openevse";
String st;
String privateKey = "";
String node = "";


//SERVER strings and interfers for OpenEVSE Energy Monotoring
const char* host = "data.openevse.com";
const int httpsPort = 443;
const char* e_url = "/emoncms/input/post.json?node=";
const char* inputID_AMP   = "OpenEVSE_AMP:";
const char* inputID_VOLT   = "OpenEVSE_VOLT:";
const char* inputID_TEMP1   = "OpenEVSE_TEMP1:";
const char* inputID_TEMP2   = "OpenEVSE_TEMP2:";
const char* inputID_TEMP3   = "OpenEVSE_TEMP3:";
const char* inputID_PILOT   = "OpenEVSE_PILOT:";

//Server strings for Ohm Connect 
const char* ohm_host = "login.ohmconnect.com";
const char* ohm_url = "/verify-ohm-hour/";
String ohm_key = "";
const int ohm_httpsPort = 443;
const char* ohm_fingerprint = "6B 39 04 A4 BB E0 87 B2 EB B6 FE 77 CD D5 F6 A7 22 4B 3B ED";
int evse_sleep = 0;

int amp = 0;
int volt = 0;
int temp1 = 0;
int temp2 = 0;
int temp3 = 0;
int pilot = 0;

int wifi_mode = 0; 
int buttonState = 0;
int clientTimeout = 0;
int i = 0;
unsigned long Timer;
unsigned long Timer2;

void ResetEEPROM(){
  //Serial.println("Erasing EEPROM");
  for (int i = 0; i < 512; ++i) { 
    EEPROM.write(i, 0);
    //Serial.print("#"); 
  }
  EEPROM.commit();   
}

// Build the command to display the IP, but right pad it to 
// the full width of a 16 char wide LCD
void buildIPCommand(IPAddress myIP, char *tmpStr) {
  const int maxLen=24;
  sprintf(tmpStr,"$FP 0 1 %d.%d.%d.%d",myIP[0],myIP[1],myIP[2],myIP[3]);
  int len = strlen(tmpStr);
  while (len < maxLen ) {
    tmpStr[len++] = ' ';
  }
  tmpStr[len] = '\0';
}

void setup() {
	delay(1000);
	Serial.begin(115200);
  EEPROM.begin(512);
  pinMode(0, INPUT);
  char tmpStr[40];
  String esid;
  String epass = "";
 
  for (int i = 0; i < 32; ++i){
    esid += char(EEPROM.read(i));
  }
  for (int i = 32; i < 96; ++i){
    epass += char(EEPROM.read(i));
  }
  for (int i = 96; i < 128; ++i){
    privateKey += char(EEPROM.read(i));
  }
  node += char(EEPROM.read(129));
  for (int i = 130; i < 138; ++i){
    ohm_key += char(EEPROM.read(i));
  }
     
  if ( esid != 0 ) { 
    //Serial.println(" ");
    //Serial.print("Connecting as Wifi Client to: ");
    //Serial.println(esid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); 
    WiFi.begin(esid.c_str(), epass.c_str());
    delay(50);
    int t = 0;
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED){
      // test esid
      //Serial.print("#");
      delay(500);
      t++;
      if (t >= 20){
        //Serial.println(" ");
        //Serial.println("Trying Again...");
        delay(2000);
        WiFi.disconnect(); 
        WiFi.begin(esid.c_str(), epass.c_str());
        t = 0;
        attempt++;
        if (attempt >= 5){
          //Serial.println();
          //Serial.print("Configuring access point...");
          WiFi.mode(WIFI_STA);
          WiFi.disconnect();
          delay(100);
          int n = WiFi.scanNetworks();
          //Serial.print(n);
          //Serial.println(" networks found");
          st = "<ul>";
          for (int i = 0; i < n; ++i){
            st += "<li>";
            st += WiFi.SSID(i);
            st += "</li>";
          }
          st += "</ul>";
          delay(100);
          WiFi.softAP(ssid, password);
          IPAddress myIP = WiFi.softAPIP();
          //Serial.print("AP IP address: ");
          //Serial.println(myIP);
          Serial.println("$FP 0 0 SSID...OpenEVSE.");
          delay(100);
          Serial.println("$FP 0 1 PASS...openevse.");
          delay(5000);
          Serial.println("$FP 0 0 IP_Address......");
          delay(100);
          buildIPCommand(myIP, tmpStr);
          Serial.println(tmpStr);
          wifi_mode = 1;
          break;
        }
      }
    }
  }
  else {
    //Serial.println();
    //Serial.print("Configuring access point...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    st = "<ul>";
    for (int i = 0; i < n; ++i){
      st += "<li>";
      st += WiFi.SSID(i);
      st += "</li>";
    }
    st += "</ul>";
    delay(100);
    WiFi.softAP(ssid, password);
    IPAddress myIP = WiFi.softAPIP();
    //Serial.print("AP IP address: ");
    //Serial.println(myIP);
    Serial.println("$FP 0 0 SSID...OpenEVSE.");
    delay(100);
    Serial.println("$FP 0 1 PASS...openevse.");
    delay(5000);
    Serial.println("$FP 0 0 IP_Address......");
    delay(100);
    buildIPCommand(myIP, tmpStr);
    Serial.println(tmpStr);
    
    wifi_mode = 2; //AP mode with no SSID in EEPROM
  }
	
	if (wifi_mode == 0){
    //Serial.println(" ");
    //Serial.println("Connected as a Client");
    IPAddress myAddress = WiFi.localIP();
    //Serial.println(myAddress);
    Serial.println("$FP 0 0 Client-IP.......");
    delay(100);
    buildIPCommand(myAddress, tmpStr);
    Serial.println(tmpStr);
  }

  ArduinoOTA.begin();
 
	server.on("/", [] () {
	  if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication();
    String s;
    s = "<html><font size='20'><font color=006666>Open</font><b>EVSE</b></font><p><b>Open Source Hardware</b><p>Wireless Configuration<p>Networks Found:<p>";
        //s += ipStr;
        s += "<p>";
        s += st;
        s += "<p>";
        s += "<form method='get' action='a'><label><b><i>WiFi SSID:</b></i></label><input name='ssid' length=32><p><label><b><i>Password  :</b></i></label><input name='pass' length=64><p><label><b><i>Device Access Key:</b></i></label><input name='ekey' length=32><p><label><b><i>Node:</b></i></label><select name='node'><option value='0'>0 - Default</option><option value='1'>1</option><option value='2'>2</option><option value='3'>3</option><option value='4'>4</option><option value='5'>5</option><option value='6'>6</option><option value='7'>7</option><option value='8'>8</option><option value='9'>9</option></select><p><label><b><i>Ohm Connect Key - Optional:</b></i></label><input name='ohm' length=64><p><input type='submit'></form>";
        s += "<a href='/status'>Status </a><a href='/rapi'>RAPI </a><br><br>";
        s += "<p><b>Firmware Update</b><p>";
        s += "<iframe style='width:380px; height:50px;' frameborder='0' scrolling='no' marginheight='0' marginwidth='0' src='/update'></iframe>";
        s += "</html>\r\n\r\n";
    server.send(200, "text/html", s);  
  });
	
  server.on("/a", [] () {
    if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication();
    String s;
    String qsid = server.arg("ssid");
    String qpass = server.arg("pass");      
    String qkey = server.arg("ekey");
    String qnode = server.arg("node");
    String qohm = server.arg("ohm");  
    qpass.replace("%21", "!");
//  qpass.replace("%22", '"');
    qpass.replace("%23", "#");
    qpass.replace("%24", "$");
    qpass.replace("%25", "%");
    qpass.replace("%26", "&");
    qpass.replace("%27", "'");
    qpass.replace("%28", "(");
    qpass.replace("%29", ")");
    qpass.replace("%2A", "*");
    qpass.replace("%2B", "+");
    qpass.replace("%2C", ",");
    qpass.replace("%2D", "-");
    qpass.replace("%2E", ".");
    qpass.replace("%2F", "/");
    qpass.replace("%3A", ":");
    qpass.replace("%3B", ";");
    qpass.replace("%3C", "<");
    qpass.replace("%3D", "=");
    qpass.replace("%3E", ">");
    qpass.replace("%3F", "?");
    qpass.replace("%40", "@");
    qpass.replace("%5B", "[");
    qpass.replace("%5C", "'\'");
    qpass.replace("%5D", "]");
    qpass.replace("%5E", "^");
    qpass.replace("%5F", "_");
    qpass.replace("%60", "`");
    qpass.replace("%7B", "{");
    qpass.replace("%7C", "|");
    qpass.replace("%7D", "}");
    qpass.replace("%7E", "~");
    qpass.replace('+', ' ');
    qsid.replace("%21", "!");
//  qsid.replace("%22", '"');
    qsid.replace("%23", "#");
    qsid.replace("%24", "$");
    qsid.replace("%25", "%");
    qsid.replace("%26", "&");
    qsid.replace("%27", "'");
    qsid.replace("%28", "(");
    qsid.replace("%29", ")");
    qsid.replace("%2A", "*");
    qsid.replace("%2B", "+");
    qsid.replace("%2C", ",");
    qsid.replace("%2D", "-");
    qsid.replace("%2E", ".");
    qsid.replace("%2F", "/");
    qsid.replace("%3A", ":");
    qsid.replace("%3B", ";");
    qsid.replace("%3C", "<");
    qsid.replace("%3D", "=");
    qsid.replace("%3E", ">");
    qsid.replace("%3F", "?");
    qsid.replace("%40", "@");
    qsid.replace("%5B", "[");
    qsid.replace("%5C", "'\'");
    qsid.replace("%5D", "]");
    qsid.replace("%5E", "^");
    qsid.replace("%5F", "_");
    qsid.replace("%60", "`");
    qsid.replace("%7B", "{");
    qsid.replace("%7C", "|");
    qsid.replace("%7D", "}");
    qsid.replace("%7E", "~");
    qsid.replace('+', ' ');
    if (qsid != 0){
       ResetEEPROM();
       for (int i = 0; i < qsid.length(); ++i){
        EEPROM.write(i, qsid[i]);
      }
    //Serial.println("Writing Password to Memory:"); 
      for (int i = 0; i < qpass.length(); ++i){
        EEPROM.write(32+i, qpass[i]); 
      }
    //Serial.println("Writing EMON Key to Memory:"); 
      for (int i = 0; i < qkey.length(); ++i){
        EEPROM.write(96+i, qkey[i]); 
      }
      EEPROM.write(129, qnode[i]);
      for (int i = 0; i < qohm.length(); ++i){
        EEPROM.write(130+i, qohm[i]); 
      }
      EEPROM.commit();
      s = "<html><font size='20'><font color=006666>Open</font><b>EVSE</b></font><p><b>Open Source Hardware</b><p>Wireless Configuration<p>SSID and Password<p>";
    //s += req;
      s += "<p>Saved to Memory...<p>Wifi will reset to join your network</html>\r\n\r\n";
      server.send(200, "text/html", s);
      delay(2000);
      WiFi.disconnect();
      ESP.reset(); 
    }
    else {
        s = "<html><font size='20'><font color=006666>Open</font><b>EVSE</b></font><p><b>Open Source Hardware</b><p>Wireless Configuration<p>Networks Found:<p>";
        //s += ipStr;
        s += "<p>";
        s += st;
        s += "<p>";
        s += "<form method='get' action='a'><label><b><i>WiFi SSID:</b></i></label><input name='ssid' length=32><p><font color=FF0000><b>SSID Required<b></font><p></font><label><b><i>Password  :</b></i></label><input name='pass' length=64><p><label><b><i>Device Access Key:</b></i></label><input name='ekey' length=32><p><label><b><i>Node:</b></i></label><select name='node'><option value='0'>0</option><option value='1'>1</option><option value='2'>2</option><option value='3'>3</option><option value='4'>4</option><option value='5'>5</option><option value='6'>6</option><option value='7'>7</option><option value='8'>8</option><option value='9'>9</option></select><p><label><b><i>Ohm Connect Key - Optional:</b></i></label><input name='ohm' length=8><p><input type='submit'></form>";
        s += "</html>\r\n\r\n";
     server.send(200, "text/html", s);
  }
});
  
  server.on("/r", [] () {
    if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication();
    String s;
    String rapiString;
    String rapi = server.arg("rapi");
    rapi.replace("%24", "$");
    rapi.replace("+", " "); 
    Serial.flush();
    Serial.println(rapi);
    delay(100);
       while(Serial.available()) {
         rapiString = Serial.readStringUntil('\r');
       }    
     s = "<html><font size='20'><font color=006666>Open</font><b>EVSE</b></font><p><b>Open Source Hardware</b><p>RAPI Command Sent<p>Common Commands:<p>Set Current - $SC XX<p>Set Service Level - $SL 1 - $SL 2 - $SL A<p>Get Real-time Current - $GG<p>Get Temperatures - $GP<p>";
     s += "<p>";
     s += "<form method='get' action='r'><label><b><i>RAPI Command:</b></i></label><input name='rapi' length=32><p><input type='submit'></form>";
     s += rapi;
     s += "<p>>";
     s += rapiString;
     s += "<p></html>\r\n\r\n";
   server.send(200, "text/html", s);
  }); 
  
  server.on("/reset", [] () {
    if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication();
    String s;
    s = "<html><font size='20'><font color=006666>Open</font><b>EVSE</b></font><p><b>Open Source Hardware</b><p>Wireless Configuration<p>Reset to Defaults:<p>";
    s += "<p><b>Clearing the EEPROM</b><p>";
    s += "</html>\r\n\r\n";
    ResetEEPROM();
    EEPROM.commit();
    server.send(200, "text/html", s);
    WiFi.disconnect();
    delay(1000);
    ESP.reset();
  });
  
  server.on("/status", [] () {
    if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication();
    String s;
    s = "<html><iframe style='width:480px; height:320px;' frameborder='0' scrolling='no' marginheight='0' marginwidth='0' src='http://data.openevse.com/emoncms/vis/rawdata?feedid=1&fill=0&colour=008080&units=W&dp=1&scale=1&embed=1'></iframe>";
    s += "</html>\r\n\r\n";
    server.send(200, "text/html", s);
  });
   
  server.on("/rapi", [] () {
    if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication();
    String s;
    s = "<html><font size='20'><font color=006666>Open</font><b>EVSE</b></font><p><b>Open Source Hardware</b><p>Send RAPI Command<p>Common Commands:<p>Set Current - $SC XX<p>Set Service Level - $SL 1 - $SL 2 - $SL A<p>Get Real-time Current - $GG<p>Get Temperatures - $GP<p>";
    s += "<p>";
    s += "<form method='get' action='r'><label><b><i>RAPI Command:</b></i></label><input name='rapi' length=32><p><input type='submit'></form>";
    s += "</html>\r\n\r\n";
    server.send(200, "text/html", s);
	 });
  
  httpUpdater.setup(&server); 
	server.begin();
  httpUpdater.setup(&server);
	//Serial.println("HTTP server started");
  delay(100);
  Timer = millis();
}



void loop() {
ArduinoOTA.handle();
server.handleClient();
  
int erase = 0;  
buttonState = digitalRead(0);
while (buttonState == LOW) {
  buttonState = digitalRead(0);
  erase++;
  if (erase >= 5000) {
    ResetEEPROM();
    int erase = 0;
    WiFi.disconnect();
    Serial.print("Finished...");
    delay(2000);
    ESP.reset(); 
  } 
}
// Remain in AP mode for 5 Minutes before resetting
if (wifi_mode == 1){
   if ((millis() - Timer) >= 300000){
     ESP.reset();
   }
}   
if (wifi_mode == 0 && ohm_key != 0){
  if ((millis() - Timer2) >= 60000){
     Timer2 = millis();
     WiFiClientSecure client;
     if (!client.connect(ohm_host, ohm_httpsPort)) {
       Serial.println("connection failed");
       return;
     }
     if (client.verify(ohm_fingerprint, ohm_host)) {
       client.print(String("GET ") + ohm_url + ohm_key + " HTTP/1.1\r\n" +
               "Host: " + ohm_host + "\r\n" +
               "User-Agent: OpenEVSE\r\n" +
               "Connection: close\r\n\r\n");
     String line = client.readString();
     if(line.indexOf("False") > 0) {
       //Serial.println("It is not an Ohm Hour");
       if (evse_sleep == 1){
         evse_sleep = 0;
         Serial.println("$FE*AF");
       }
     }
     if(line.indexOf("True") > 0) {
       //Serial.println("Ohm Hour");
       if (evse_sleep == 0){
         evse_sleep = 1;
         Serial.println("$FS*BD");
       }
     }
    //Serial.println(line);
  } 
  else {
    Serial.println("Certificate Invalid");
    }
  }
}
if (wifi_mode == 0 && privateKey != 0){
   if ((millis() - Timer) >= 30000){
     Timer = millis();
     Serial.flush();
     Serial.println("$GE*B0");
     delay(100);
       while(Serial.available()) {
         String rapiString = Serial.readStringUntil('\r');
         if ( rapiString.startsWith("$OK ") ) {
           String qrapi; 
           qrapi = rapiString.substring(rapiString.indexOf(' '));
           pilot = qrapi.toInt();
         }
       }  
  
     delay(100);
     Serial.flush();
     Serial.println("$GG*B2");
     delay(100);
     while(Serial.available()) {
       String rapiString = Serial.readStringUntil('\r');
       if ( rapiString.startsWith("$OK") ) {
         String qrapi; 
         qrapi = rapiString.substring(rapiString.indexOf(' '));
         amp = qrapi.toInt();
         String qrapi1;
         qrapi1 = rapiString.substring(rapiString.lastIndexOf(' '));
         volt = qrapi1.toInt();
       }
    }  
    delay(100);
    Serial.flush(); 
    Serial.println("$GP*BB");
    delay(100);
    while(Serial.available()) {
      String rapiString = Serial.readStringUntil('\r');
      if (rapiString.startsWith("$OK") ) {
        String qrapi; 
        qrapi = rapiString.substring(rapiString.indexOf(' '));
        temp1 = qrapi.toInt();
        String qrapi1;
        int firstRapiCmd = rapiString.indexOf(' ');
        qrapi1 = rapiString.substring(rapiString.indexOf(' ', firstRapiCmd + 1 ));
        temp2 = qrapi1.toInt();
        String qrapi2;
        qrapi2 = rapiString.substring(rapiString.lastIndexOf(' '));
        temp3 = qrapi2.toInt();
      }
    } 
 
// Use WiFiClientSecure to create HTTPS connections
    WiFiClientSecure client;
    if (!client.connect(host, httpsPort)) {
      return;
    }

  
// We now create a URL for OpenEVSE RAPI data upload request
    String url = e_url;
    String url_amp = inputID_AMP;
    url_amp += amp;
    url_amp += ",";
    String url_volt = inputID_VOLT;
    url_volt += volt;
    url_volt += ",";
    String url_temp1 = inputID_TEMP1;
    url_temp1 += temp1;
    url_temp1 += ",";
    String url_temp2 = inputID_TEMP2;
    url_temp2 += temp2;
    url_temp2 += ","; 
    String url_temp3 = inputID_TEMP3;
    url_temp3 += temp3;
    url_temp3 += ","; 
    String url_pilot = inputID_PILOT;
    url_pilot += pilot;
    url += node;
    url += "&json={";
    url += url_amp;
    if (volt <= 0) {
      url += url_volt;
    }
    if (temp1 != 0) {
      url += url_temp1;
    }
    if (temp2 != 0) {
      url += url_temp2;
    }
    if (temp3 != 0) {
      url += url_temp3;
    }
    url += url_pilot;
    url += "}&devicekey=";
    url += privateKey.c_str();
    
// This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "User-Agent: OpenEVSE\r\n" + "Connection: close\r\n\r\n");
    delay(10);
    while(client.available()){
      String line = client.readStringUntil('\r');
    }
    //Serial.println(host);
    //Serial.println(url);
    
  }
}

}
