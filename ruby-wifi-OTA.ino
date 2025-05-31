/*
 *  Ruby hamster wheel / WEMOS
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "HTTPSRedirect.h"
#include <ArduinoOTA.h>

#include <RTClib.h>
RTC_Millis rtc;
byte GMT = 1;

DNSServer dnsServer;
ESP8266WebServer webServer(80);

struct settings {
  char ssid[30];
  char password[30];
  char vars[255];
} userdata = {};



#include "SSD1306.h" 
SSD1306  display(0x3c, D2, D1);

int reedSensor = D7;  // MINI REED SENSOR CONNECTED ON A2

float r;          // wheel radius in cm
float p;          // wheel circumference in m
float vmax = 0;
float vmed = 0;

const int led = LED_BUILTIN;
  
int val = LOW;
int preval = 0;
int c = 0;
int q = 0;
int c0 = 0;
int deltat = 2;

unsigned long tim=0;
unsigned long tim15=0;
unsigned long timr=0;
byte flagr = 0;

float v,d,dt;


byte flag = 0;

float totS = 0;
unsigned long lastSec=0;
unsigned long debouncetimer = 0;


#define MAXROWS 24
float queue[6][MAXROWS];
unsigned long queueUL[1][MAXROWS];
float queuePREV[6] = {0,0,0,0,0,0};
int queuePos = 0;
void resetQueue() {
  for (int i=0; i<MAXROWS; i++) {
    for (int k=0; k<6; k++) {
    queue[k][i] = 0; queueUL[0][i] = 0;
  }
  }
  queuePos = 0;
}
void addToQueue(int sheet, unsigned long ts, int c, float dt, float totS, float vmax, float vmed) {
    queueUL[0][queuePos] = ts;  // timestamp
    queue[0][queuePos] = sheet; // Sheet number
    queue[1][queuePos] = c;     // counter
    queue[2][queuePos] = dt;    // 
    queue[3][queuePos] = totS;  // total running seconds
    queue[4][queuePos] = vmax;  // max speed
    queue[5][queuePos] = vmed;  // avarage speed
    queuePos++; 
}




//unsigned long gostandby

byte lastDay = 0;

void setup() {
  //delay(1000);

  resetQueue();


  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);

  // SERIAL MONITOR
  Serial.begin(115200);
  delay(1000);
  Serial.println("start... ");

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);


  readUserData();


 
  Serial.println(userdata.ssid);
  Serial.println(userdata.password);
  printy(String(userdata.ssid) + " ...");



  boolean wifi = false;
  int i =0;
  while(i<3 && wifi==false) {
    wifi = tryWifi(30);
    i++;
  }
  if(!wifi) {
    printy("Access point");
    setupPortal();
  } else {

    if (MDNS.begin("rubywifi")) {
      printy("MDNS responder started");
    }

  }
  
  // ---------------------


  Serial.println("WIFI OK");  
  printy("WIFI OK");

  




  pinMode(reedSensor, INPUT);


  r = 6.0;

  // calculate p
  p = 2.0 * (r / 100) * 3.1415;



  setOra();
  printOra();





  // -----------------------------------------------
  // OVER THE AIR
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  // -----------------------------------------------


  
}

int screen = 0;


long unsigned ledtimer = 0;


long unsigned inviaDebugTimer = 0;

void sendQueuedData() {

  for(int i=0;i<queuePos;i++) {
    if(queue[0][i] == 1) {
      // Sheet1: daily data
      sendDataToCloud(queue[0][i], queueUL[0][i], queue[1][i], queue[2][i], queue[3][i], queue[4][i], queue[5][i]);  
    } else {
      // Sheet2: half hour data (incremental)
      // sendDataToCloud(1, tcoda.unixtime(), c, dt, totS, vmax, vmed);
      float q1 = 0 > queue[1][i] - queuePREV[1] ? 0 : queue[1][i] - queuePREV[1];
      float q2 = 0 > queue[2][i] - queuePREV[2] ? 0 : queue[2][i] - queuePREV[2];
      float q3 = 0 > queue[3][i] - queuePREV[3] ? 0 : queue[3][i] - queuePREV[3];
      sendDataToCloud(queue[0][i], queueUL[0][i], q1, q2, q3, queue[4][i], queue[5][i]);  
      queuePREV[1] = queue[1][i];
      queuePREV[2] = queue[2][i];
      queuePREV[3] = queue[3][i];
    }
  }
  resetQueue();
  
}

void loop() {

  
  ArduinoOTA.handle();
  
  
  //Serial.println("loop");


  //
  // ogni deltat vedo quanti giri ha fatto e calcolo velocità
  flag=1;
  if ( millis() > tim ) {
    tim = millis() + deltat * 1000  ;
    //lcd.setCursor(15,0); lcd.print("*"); 
    d = p * q;                 // distanza percorsa
    v = 3.6 * d  / deltat ;    // velocità
    q = 0;                     // azzero contatore parziale numero di giri in deltat
    if(v>vmax) vmax=v;
    flag=0;

    
  }

    if (v > 0) { // conto i secondi di corsa
      if( millis() > lastSec) {
       lastSec = millis() + 500;
       totS += .5;     // tempo di corsa
      }
    }



  int hh=0;
  int mm=0;
  int ss=0;
  getOra(&hh,&mm,&ss);

  int gi=0;
  int me=0;
  int an=0;
  getData(&gi,&me,&an);


  // Save data to queue
  if (millis() > inviaDebugTimer) { // every 30 minutes
    // send data if ruby isn't running (v=0)
    DateTime tcoda = rtc.now();

    // send data to sheet 2 (measures every 30 min)
    addToQueue(2,tcoda.unixtime() - 3600, c, dt, totS, vmax, vmed);
    if(v==0 || queuePos>MAXROWS) {
      sendQueuedData();
    }
    inviaDebugTimer = millis() + 30*60*1000; // 30 minutes
        
  }
  
  // save data if not already saved 
  if (hh==8 && mm==30 && lastDay!=gi) {
      // every day at 8.30 log data and get correct time from Internet
      writeline(0,F("Date time sync"),true);
      setOra();

      lastDay = gi;
      DateTime tcoda = rtc.now();
      //sendDataToCloud(1, tcoda.unixtime() - 3600, c, dt, totS, vmax, vmed);
      addToQueue(2,tcoda.unixtime() - 3600, c, dt, totS, vmax, vmed);
      addToQueue(1,tcoda.unixtime() - 3600, c, dt, totS, vmax, vmed);
      //azzero
      c=0;q=0;dt=0;d=0;tim=0;totS=0;vmed=0;vmax=0;
  }
  

  //
  // Read the reed sensor
  val = digitalRead(reedSensor); 

  
  //
  // detect when signal switch from HIGH to LOW
  //
  //     -----------+
  //                |
  //                +-----------------------
  //
  if (preval == HIGH && val == LOW) {
     
     long unsigned temptime = millis();
     if( temptime - debouncetimer > 200) {
       // prevent double reads for not proper movement of the wheel
       // probably it's better to move the magnet near the ceneter of the wheel
       c++;  // increae total counter 
       q++;  // counter for vmax calculus
       debouncetimer = temptime;
       digitalWrite(led, LOW); // ACCENDE
       ledtimer = millis() + 350;
     } else {
       val = preval;
       
     }
  }

  if ( millis() > ledtimer ) {
    digitalWrite(led, HIGH); // SPEGNE
  }


  if ( millis() > tim15 ) { 
      tim15 = millis()+5000;
      screen++;
      if(screen >= 4) screen=0;  
  }

  
  // save if read value is 0 or 1
  if (preval != val) preval = val;



   if( c0 != c || flag==0) {
      c0 = c;

      dt = p * c;
      if(totS == 0) vmed = 0;
        else vmed = 3.6 * dt / totS ;
      
   }



      display.clear();

      if(v>0) {
        printLabels(0);
        printInstantSpeed(1);


      } else {
        printLabels(3);
        printSpin();
        printInstantSpeed(0);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_10);
        if (screen == 0) display.drawString(0, 30, String(vmax)+" MAX");
        if (screen == 1 && totS > 0) display.drawString(0, 30, String(vmed)+" MED");
        if (screen == 2) display.drawString(0, 30, "Running time: " + String(totS)+"s");
        if (screen == 3) display.drawString(0, 30, "Distance: " + String(dt)+"m");
        drawPath();
      }
      display.display();


  
}

void drawPath() {
  int maxDist = 10;
  if(dt >   10 && dt<=50 ) maxDist = 50;
  if(dt >   50 && dt<=100 ) maxDist = 100;
  if(dt >  100 && dt<=500 ) maxDist = 500;
  if(dt >  500 && dt<=1000 ) maxDist = 1000;
  if(dt > 1000 && dt<=2000 ) maxDist = 2000;
  if(dt > 2000) maxDist = ( (int)dt / 1000 + 1) * 1000;
  

  display.setColor(BLACK);display.fillRect(0, 58, 128, 64);

  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 44, "0m");
  display.drawLine(0, 59, 0, 63);display.drawLine(127, 59, 127, 63);
  for (int16_t i=0; i<128; i+=3) display.drawLine(i, 61, i, 63);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(128, 44, (String)maxDist+"m");

  float dd = 128.0 / maxDist * dt;
  // Serial.println(dd);
  display.fillRect(0, 61, dd, 64);
  
  
}


void printLabels(int i){
    if(i==2 || i==1 || i==3) {
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.setFont(ArialMT_Plain_10);
      if(i==1 || i==3) display.drawString(0, 0, "km/h");
      display.setTextAlignment(TEXT_ALIGN_RIGHT);
      if(i==2 || i==3) display.drawString(128, 0, "Spins");
    }
    if(i==0){
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawString(64, 0, "km/h");
    }
}

void printSpin(){
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_16);
    display.drawString(128, 13, (String)c);  
}
void printInstantSpeed(int big){

    if(big==0) {
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.setFont(ArialMT_Plain_16);
      display.drawString(0, 13, (String)v);
    }
    if(big==1) {
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_24);
      display.drawString(64, 13, (String)v);
      display.drawString(63, 13, (String)v);
    }
    
}



void printy( String s) {
  writeline(0,s,true);
}

void writeline(int n, String s, boolean ref) {
    display.setColor(BLACK);display.fillRect(0, n*12, 128, (n+1)*12);
    display.setColor(WHITE);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, n*12 + 1, s);
    if(ref) display.display();
}



void sendDataToCloud(int sheet_name, unsigned long milli, int c, float dt, float totS, float vmax, float vmed){
  writeline(0, "Sending data...", true);

  const char *GScriptId = "AKfycbzmYtPBtgvngc-8c2odj_OI5F_I885Forz9yQNnKtdajp7hFrYWBR26U7qVlZ4A301A";

  
  // Enter command (insert_row or append_row) and your Google Sheets sheet name (default is Sheet1):
  String payload_base =  "{\"command\": \"insert_row\", \"sheet_name\": \"Sheet" + (String)sheet_name + "\", \"values\": ";
  String payload = "";
  
  // Google Sheets setup (do not edit)
  const char* host = "script.google.com";
  const int httpsPort = 443;
  const char* fingerprint = "";
  String url = String("/macros/s/") + GScriptId + "/exec";
  HTTPSRedirect* client = nullptr;


    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");

  if (client != nullptr){
    if (!client->connected()){
      client->connect(host, httpsPort);
    }
  }
  else{
    writeline(0, "Error (1)", true);
    Serial.println("Error creating client object!");
  }
  
  // Create json object string to send to Google Sheets
  payload = payload_base + "\"" + (String)c + "|" + (String)dt + "|" + (String)totS+ "|" + (String)vmax+ "|" + (String)vmed + "|" + (String)milli + "\"}";
  
  // Publish data to Google Sheets
  writeline(0, "Publishing data...)", true);
  Serial.println("Publishing data...");
  Serial.println(payload);
  if(client->POST(url, host, payload)){ 
    // do stuff here if publish was successful
  }
  else{
    // do stuff here if publish was not successful
    writeline(0, "Error (2)", true);
    Serial.println("Error while connecting");
  }

   delete client;    // delete HTTPSRedirect object
  client = nullptr; // delete HTTPSRedirect object

}






void readUserData(){
  // ---------------------
  // READ FROM EEPROM
  EEPROM.begin(sizeof(struct settings) );
  EEPROM.get( 0, userdata );
  

  // force reset ssid/pass
  //  strncpy(userdata.ssid,     "aaa",     sizeof("aaa") );
  //  strncpy(userdata.password, "b", sizeof("b") );
  //  userdata.ssid[3] = userdata.password[1] = '\0';
  //  EEPROM.put(0, userdata);
  //  EEPROM.commit();

}



boolean tryWifi(int sec) {
  // TRY WIFI FOR sec SECONDS
  WiFi.mode(WIFI_STA);
  WiFi.begin(userdata.ssid, userdata.password);

  byte tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    //Serial.print(".");
    delay(1000);
    if (tries++ > sec) {
      // fail to connect
      return false;
    }
  }
  return true; 
}




void setupPortal() {
      Serial.println("");
      Serial.println("Start access point");

      IPAddress apIP(172, 217, 28, 1);

      WiFi.mode(WIFI_AP);
      WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      WiFi.softAP("Setup portal");
    
      // if DNSServer is started with "*" for domain name, it will reply with
      // provided IP to all DNS request
      dnsServer.start(53, "*", apIP);
    
      // replay to all requests with same HTML
      webServer.onNotFound([]() {
        String responseHTML = F("<!DOCTYPE html><html lang='en'><head><meta name='viewport' content='width=device-width'><title>CaptivePortal</title></head><body><h1>Hello World!</h1><p><a href='/'>entra</a></p></body></html>");
        
        webServer.send(200, "text/html", responseHTML);
      });
      

      webServer.on("/",  handlePortal);
      webServer.begin();


      // never ending loop waiting access point configuration     
      while(true){
        delay(50);
        dnsServer.processNextRequest();
        webServer.handleClient();
      }
  
}

void handlePortal() {

  if (webServer.method() == HTTP_POST) {

    strncpy(userdata.ssid,     webServer.arg("ssid").c_str(),     sizeof(userdata.ssid) );
    strncpy(userdata.password, webServer.arg("password").c_str(), sizeof(userdata.password) );
    userdata.ssid[webServer.arg("ssid").length()] = userdata.password[webServer.arg("password").length()] = '\0';
    EEPROM.put(0, userdata);
    EEPROM.commit();
    Serial.println("saved");

    webServer.send(200,   "text/html",  F("<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Wifi Setup</h1> <br/> <p>Your settings have been saved successfully!<br />Please restart the device.</p></main></body></html>") );
    
    Serial.println("restart");
    delay(5000);
    ESP.restart();
      
  } else {

    Serial.println("form");

    webServer.send(200,   "text/html", F("<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title> <style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{cursor: pointer;border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1{text-align: center}</style> </head> <body><main class='form-signin'> <form action='/' method='post'> <h1 class=''>Wifi Setup</h1><br/><div class='form-floating'><label>SSID</label><input type='text' class='form-control' name='ssid'> </div><div class='form-floating'><br/><label>Password</label><input type='password' class='form-control' name='password'></div><br/><br/><button type='submit'>Save</button><p style='text-align: right'><a href='https://www.mrdiy.ca' style='color: #32C5FF'>mrdiy.ca</a></p></form></main> </body></html>") );
  }
  
}









String getTime(String host) {
  WiFiClient client;
  while (!!!client.connect(host.c_str(), 80)) {
    Serial.println("connection failed, retrying...");
  }

  client.print("HEAD / HTTP/1.1\r\n\r\n");
 
  while(!!!client.available()) {
     yield();
  }

  while(client.available()){
    if (client.read() == '\n') {    
      if (client.read() == 'D') {    
        if (client.read() == 'a') {    
          if (client.read() == 't') {    
            if (client.read() == 'e') {    
              if (client.read() == ':') {    
                client.read();
                String theDate = client.readStringUntil('\r');
                client.stop();
                return theDate;
              }
            }
          }
        }
      }
    }
  }
}


void printOra(){
    DateTime now = rtc.now();
      Serial.print(now.year(), DEC);
      Serial.print('/');
      Serial.print(now.month(), DEC);
      Serial.print('/');
      Serial.print(now.day(), DEC);
      Serial.print(' ');
      Serial.print(now.hour(), DEC);
      Serial.print(':');
      Serial.print(now.minute(), DEC);
      Serial.print(':');
      Serial.print(now.second(), DEC);
      Serial.print(" giorno ");
      Serial.print(now.dayOfTheWeek(), DEC);
      Serial.println();
}

void getOra(int *h,int *m,int *s) {
   DateTime now = rtc.now();
  *h = now.hour();
  *m = now.minute();
  *s = now.second();
}

void getData(int *d,int *m,int *a) {
   DateTime now = rtc.now();
  *d = now.day();
  *m = now.month();
  *a = now.year();
}


void setOra(){
  String s = getTime("www.google.it");
  

    char* pch;
    char *dup = strdup(s.c_str());
    pch = strtok(dup," :");
    int q=0;
    String ladata = "";String laora = "";
    
    while (pch != NULL)
    {
      String s1= (String)pch;
      if(q==1) { ladata=s1; }
      if(q==2) { ladata=s1 + " " + ladata; }
      if(q==3) { ladata=ladata + " " + s1; }
      if(q==4) { laora=s1; }
      if(q==5 || q==6) { laora= laora + ":" + s1; }
      pch = strtok(NULL, " :");
      q++;
    }

    DateTime n = DateTime(ladata.c_str(), laora.c_str());
  // scatta ora legale:
  // 26 marzo +1 ora (legalegreenwich+2)
  // torna ora solare:
  // 29 ottobre (solare greenwich+1)

    int dt = n.month()*100 + n.day();
    // tra il 27 marzo e il 29 ottobre +1 ora per ora legale)
    
    n = n + ( GMT + ((dt > 326 && dt<1030) ? 1 : 0))   *3600;
      
    //n = n + GMT  *3600;
    //if(debug) 
   // Serial.print("dt = ");
   // Serial.println(dt);
    rtc.adjust(n);
   
  

  
}
