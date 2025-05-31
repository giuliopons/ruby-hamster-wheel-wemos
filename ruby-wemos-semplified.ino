/*
 *  Ruby hamster wheel / WEMOS (simplified)
 */
 
#include "SSD1306.h" 
SSD1306  display(0x3c, D2, D1);

int reedSensor = D7;  // MINI REED SENSOR CONNECTED ON A2

float r;          // wheel radius in cm
float p;          // wheel circumference in m
float vmax = 0;   // max speed reached
float vmed = 0;   // avarage speed

int val = LOW;
int preval = 0;
int c = 0;
int q = 0;
int c0 = 0;
int deltat = 2;  // sampling time in seconds

unsigned long tim=0;
unsigned long tim15=0;

float v,d,dt;

byte flag = 0;

float totS = 0;
unsigned long lastSec=0;
unsigned long debouncetimer = 0;

void setup() {

  // SERIAL MONITOR
  Serial.begin(115200);
  delay(1000);
  Serial.println("start... ");

  // DISPLAY
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
 
  // REED
  pinMode(reedSensor, INPUT);

  // wheel radius
  r = 6.0;  // cm

  // calculate circunference
  p = 2.0 * (r / 100) * 3.1415;   // meters
  
}

int screen = 0;


void loop() {


  //
  // every 2 (deltat) seconds determine the speed of the hamster
  flag=1;
  if ( millis() > tim ) {
    tim = millis() + deltat * 1000  ;    // milliseconds
    d = p * q;                 // distance run in meters
    v = 3.6 * d  / deltat ;    // speed in km/h calculated every deltat seconds
    q = 0;                     // start back to count rounds every deltat seconds
    if(v>vmax) vmax=v;         // save max speed
    flag=0;                    // data updated
  
  }

  if (v > 0) {    // count the seconds running
    if( millis() > lastSec) {
     lastSec = millis() + 500;
     totS += .5;     // total time running
    }
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
       q++;  // counter for instant speed and vmax
       debouncetimer = temptime;
     } else {
       val = preval;
     }
  }

  // every 5 seconds switch the screen with different info
  if ( millis() > tim15 ) { 
    tim15 = millis()+5000;
    screen++;
    if(screen >= 4) screen=0;  
  }
  
  // save if read value is 0 or 1
  if (preval != val) preval = val;

  if( c0 != c || flag==0) {
    c0 = c;
    dt = p * c;   // total distance
    if(totS == 0) vmed = 0;
      else vmed = 3.6 * dt / totS ;  // avarage speed (considering only running time)    
  }

  display.clear();

  if(v>0) {
    // it's running!
    
    printLabels(0);
    printInstantSpeed(1);

  } else {
    // it's resting.
    
    printLabels(3);
    printSpin();
    printInstantSpeed(0);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);

    // show different info on the "resting screens":
    if (screen == 0) display.drawString(0, 30, String(vmax)+" MAX");  // max speed
    if (screen == 1 && totS > 0) display.drawString(0, 30, String(vmed)+" AVG");   // avarage
    if (screen == 2) display.drawString(0, 30, "Running time: " + String(totS)+"s"); // running time
    if (screen == 3) display.drawString(0, 30, "Distance: " + String(dt)+"m");   // total distance
  }
  display.display(); 
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
    // use small font
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 13, (String)v);
  }
  if(big==1) {
    // use big font
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 13, (String)v);
    display.drawString(63, 13, (String)v);
  }  
}
