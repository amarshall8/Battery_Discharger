#include <SPI.h>
#include <stdint.h>
#include <TFTv2.h>
#include <SeeedTouchScreen.h>

//define relay pins
#define chan0Rel 2
#define chan1Rel 3

//define fan pin

#define fanPin 10

//define pins for touchscreen
#define YP A2   // must be an analog pin, use "An" notation!
#define XM A1   // must be an analog pin, use "An" notation!
#define YM 14   // can be a digital pin, this is A0
#define XP 17   // can be a digital pin, this is A3

/* Measured ADC values for (0,0) and (210-1,320-1)
TS_MINX corresponds to ADC value when X = 0
TS_MINY corresponds to ADC value when Y = 0
TS_MAXX corresponds to ADC value when X = 240 -1
TS_MAXY corresponds to ADC value when Y = 320 -1 */

#define TS_MINX 116*2
#define TS_MAXX 890*2
#define TS_MINY 83*2
#define TS_MAXY 913*2

TouchScreen ts = TouchScreen(XP, YP, XM, YM);

//define status variables
uint8_t selchannel = 0;
uint8_t startStopFinish_ch0 = 0;
uint8_t startStopFinish_ch1 = 0;

//define actual 5v rail voltage on arduino
float arduino5V = 5.00;

//define voltage divider variables and characteristics for channel 0
#define analogInputch0 A4

float channel_0_voltage = 0;

//define voltage divider variables and characteristics for channel 1
#define analogInputch1 A5

float channel_1_voltage = 0;

uint16_t startColor;
uint16_t stopColor;
uint16_t runColor;
uint16_t textColor;
uint16_t selectColor;

int oldTime = 0;

Point p;

//------------------------------------------------------------------------------------------------------------------------

void setup() {

    pinMode(2,OUTPUT);       //init Relay 1 trigger pin
    pinMode(3,OUTPUT);       //init Relay 2 trigger pin
    pinMode(analogInputch0, INPUT); //init ch0 analog input for voltage meter
    pinMode(analogInputch1, INPUT); //init ch1 analog input for voltage meter
    pinMode(fanPin, OUTPUT);
    analogWrite(fanPin, 0);
    
    //make sure relays are disabled
    digitalWrite(chan0Rel, HIGH);
    digitalWrite(chan1Rel, HIGH);

    startColor = rgb(26,230,60);
    runColor = rgb(7,122,209);
    stopColor = rgb(255,20,20);
    textColor = rgb(170,223,238);
    selectColor = rgb(38,41,118);

    setupTemplate();
    
    Serial.begin(9600);
    Serial.println("Online");
    
}

//------------------------------------------------------------------------------------------------------------------------

void loop() {
  int currentTime = millis() / 1000;
  int deltaTime = currentTime - oldTime;
  oldTime = currentTime;
  //setup touchscreen input
  //a point object holds x y and z coordinates
  p = ts.getPoint();

  //map resistance to touchscreen
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, 240);
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, 320);

  //we have some minimum pressure we consider 'valid'
  //pressure of 0 means no pressing!
  if (p.z > __PRESSURE) {
    checkSel();
      if (p.y >= 235 && p.y < 315) { //set Y bounds for start/stop buttons
      checkStartStop();
    }
  }

  channel_0_voltage = readCH0();
  channel_1_voltage = readCH1();
  
  
  if (startStopFinish_ch0 == 1 && channel_0_voltage < 3.8){
    startStopFinish_ch0 = 2;
  }

  if (startStopFinish_ch1 == 1 && channel_1_voltage < 3.8){
    startStopFinish_ch1 = 2;
  }

  channel0State();
  channel1State();

  fanState();

  if (deltaTime >= 1){
    Tft.fillRectangle(135, 80, 70, 30, selectColor); //refresh LCD section
    Tft.drawFloat(channel_0_voltage, 140, 80, 2, textColor); //draw new voltage for L Battery Bank
    Tft.fillRectangle(135, 175, 70, 30, selectColor); //refresh LCD section
    Tft.drawFloat(channel_1_voltage, 140, 175, 2, textColor); //draw new voltage for R Battery Bank
    deltaTime = 0;
  }

}

void checkSel(){
  //detect which channel to select
  if (p.x >= 10 && p.y < 215) //set X bounds for channel selection boxes
  {
    if (p.y >= 40 && p.y < 125) //set Y bounds for channel 0 selection box
    {
      //modify channel selection status and update graphics
      selchannel=0; //set channel 0 variable
      Tft.drawRectangle(9, 134, 217, 87, BLACK); // blank out lower border
      Tft.drawRectangle(9, 39, 217, 87, WHITE); //create upper border
    }
    else if (p.y >= 135 && p.y < 220) //set Y bounds for channel 1 selection box
    {
      //modify channel selection status and update graphics
      selchannel=1; //set channel 1 variable
      Tft.drawRectangle(9, 39, 217, 87, BLACK); //blank out upper border
      Tft.drawRectangle(9, 134, 217, 87, WHITE); // create lower border
    }
  }
}

void checkStartStop(){
  if (p.x >= 140 && p.x < 220) { //set X bounds for start button
    Serial.println("start");
    if(selchannel == 0 && channel_0_voltage > 3.8) {
      startStopFinish_ch0 = 1; //if Start area is pressed and channel 0 is selected, set startstop0 variable to true
    }

    else if(selchannel == 1 && channel_1_voltage > 3.8) {
      startStopFinish_ch1 = 1; //if Start area is pressed and channel 1 is selected, set startstop1 variable to true
    }
  }
  else if (p.x >= 20 && p.x < 100) //set X bounds for stop button
  {
    Serial.println("stop");
    if(selchannel == 0){
      startStopFinish_ch0 = 0; //if STOP area is pressed and channel 0 is selected, set startstop0 variable to false
    }

    else {
      startStopFinish_ch1 = 0; //if STOP area is pressed and channel 1 is selected, set startstop1 variable to false
    }
  }
  //Serial.println(startStopFinish_ch0);
  //Serial.println(startStopFinish_ch1);
}

float readCH0(){
  float voutch0 = 0.0;
  float bvoltch0 = 0.0;
  float R1ch0 = 99800.0; // resistance of R1ch0 (~100K) -measure resistance and update in code!
  float R2ch0 = 100000.0; // resistance of R2ch0 (~10K) -measure resistance and update in code!
  int valuech0 = 0;
  // read the analog input from channel 0 and output battery voltage
  valuech0 = analogRead(analogInputch0);
  bvoltch0 = float(valuech0) * (arduino5V/1023) * 1.0998;
  if (bvoltch0 < 0.09) //remove inaccurate readings below 0.9v
  {
  bvoltch0=0.0;
  }
  //display voltage of ch0 (left side) on the LCD
  return bvoltch0;
}

float readCH1(){
  float voutch1 = 0.0;
  float raw_voltage = 0.0;
  float bvoltch1= 0.0;
  int valuech1 = 0;

  // read the analog input from channel 1 and output battery voltage
  valuech1= analogRead(analogInputch1);
  bvoltch1 = float(valuech1) * (arduino5V/1023) * 1.0998;
  if (bvoltch1 < 0.09) //remove inaccurate readings below 0.9v
  {
  bvoltch1=0.0;
  }
  //display voltage of ch1 (right side) on the LCD
  return bvoltch1; 
}

void channel0State(){
  switch (startStopFinish_ch0){
    case 0:
      digitalWrite(chan0Rel, HIGH); //turn off channel 0 relay
      Tft.fillRectangle(13, 43, 15, 79, stopColor); //color activity box red for stopped status on channel 0
      break;

    case 1:
      digitalWrite(chan0Rel, LOW); //turn on channel 0 relay
      Tft.fillRectangle(13, 43, 15, 79, runColor); //color activity box blue for started status
      break;
    
    case 2:
      digitalWrite(chan0Rel, HIGH); //turn on channel 0 relay
      Tft.fillRectangle(13, 43, 15, 79, startColor); //color activity box blue for started status
      break;
  }
}

void channel1State(){
  switch (startStopFinish_ch1){
    case 0:
      digitalWrite(chan1Rel, HIGH); //turn off channel 1 relay)
      Tft.fillRectangle(13, 138, 15, 79, stopColor); //color activity box red for stopped status on channel 1
      break;

    case 1:
      digitalWrite(chan1Rel, LOW); //turn on channel 1 relay
      Tft.fillRectangle(13, 138, 15, 79, runColor); //color activity box blue for started status
      break;

    case 2:
      digitalWrite(chan1Rel, HIGH); //turn off channel 1 relay)
      Tft.fillRectangle(13, 138, 15, 79, startColor); //color activity box red for stopped status on channel 1
      break;
    }
}

void fanState(){
  if(startStopFinish_ch0 == 1 && startStopFinish_ch1 == 1){
    analogWrite(fanPin, 100);
  }
  else if (startStopFinish_ch0 == 1 || startStopFinish_ch1 == 1){
    analogWrite(fanPin, 60);
  }
  else{
    analogWrite(fanPin, 0);
  }
}

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void setupTemplate(){
  //draw out static graphics
    Tft.TFTinit();  // init TFT library
    Tft.fillScreen(0, 240, 0, 320, BLACK);
    Tft.drawString("Battery Discharger", 10, 4, 2, WHITE, PORTRAIT);
    Tft.fillCircle(180, 275, 40, startColor); //Start button
    Tft.fillCircle(60, 275, 40, stopColor); //STOP button
    Tft.drawString("Start", 150, 267, 2, BLACK, PORTRAIT);
    Tft.drawString("STOP", 35, 267, 2, BLACK, PORTRAIT);
    Tft.fillRectangle(10, 40, 215, 85, selectColor); //Left battery bank area
    Tft.fillRectangle(10, 135, 215, 85, selectColor); //Right battery bank area
    Tft.drawString("L Battery Bank", 35, 43, 2, textColor, PORTRAIT);
    Tft.drawString("R Battery Bank", 35, 138, 2, textColor, PORTRAIT);
    Tft.drawString("Voltage: 0.00", 35, 80, 2, textColor, PORTRAIT);
    Tft.drawString("Voltage: 0.00", 35, 175, 2, textColor, PORTRAIT);
    Tft.fillRectangle(13, 43, 15, 79, stopColor);
    Tft.fillRectangle(13, 138, 15, 79, stopColor);
}