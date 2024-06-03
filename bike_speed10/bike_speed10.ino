#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

//#define PIN A1  // 
#define PIN D0  // LED D pin
#define LEDno 20  // LED D pin

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

//global variables
volatile int state = LOW; //*added
volatile int count = 0; //*adb
bool flag = false; //*added
unsigned long start, pulse_time;
float elapsed;
float circMetric=2.074; // wheel circumference (in meters) // pi*0.6604
float speedk, speedm;    // holds calculated speed vales in metric and imperial

//unsigned long start; 
volatile unsigned long tickCount;
unsigned long lastTickCount;
const unsigned long updateTime = 3000;  // msecs between updating speed and display
const unsigned long debounceTime = 100; // msecs of debounce for interrupts
int updateflag;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDno, PIN, NEO_GRB + NEO_KHZ800);

void isr_measure_old()
{
  //Function called by the interrupt
  if((millis()-start)>debounceTime) // 100 millisec debounce
    {
    //calculate elapsed
    elapsed=millis()-start;
    //reset start
    start=millis();
    //calculate speed in km/h
    //speedk=(3600*circMetric)/elapsed; 
    //calculate speed in m/s
    speedk=(circMetric*1000)/elapsed; 
    }
}
void speedCalc(unsigned long revolutions, unsigned long time)
{
  float speed = 1000.0 * revolutions / time;// 1000-m/s, 3600-m/hr //3600.0 * revolutions / time;  //revolutions is no of rev in updateTime
  //calculate speed in m/s
  speedk = speed * circMetric;

//calculate speed in mph
//  speedm = speed * circImperial;
}
void isr_measure()
{
  static unsigned long lastTickTime = 0;

  if ( millis() - lastTickTime >= debounceTime ) {
    // debounce time
    tickCount++;
    updateflag=1;
    lastTickTime += debounceTime;
  }
}
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< LEDno; i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / LEDno) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}
// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
void meteorRain(byte red, byte green, byte blue, byte meteorSize, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay) {  
  setAll(0,0,0);
 
  for(int i = 0; i < LEDno+LEDno; i++) {
    // fade brightness all LEDs one step
    for(int j=0; j<LEDno; j++) {
      if( (!meteorRandomDecay) || (random(10)>5) ) {
        fadeToBlack(j, meteorTrailDecay );        
      }
    }
    // draw meteor
    for(int j = 0; j < meteorSize; j++) {
      if( ( i-j <LEDno) && (i-j>=0) ) {
        strip.setPixelColor(i-j, red, green, blue);
      }
    }
    strip.show();
    delay(SpeedDelay);
  }
}
void setAll(byte red, byte green, byte blue) {
  for(int i = 0; i < LEDno; i++ ) {
    strip.setPixelColor(i, red, green, blue);
  }
  strip.show();
}
void fadeToBlack(int ledNo, byte fadeValue) {
    // NeoPixel
    uint32_t oldColor;
    uint8_t r, g, b;
    int value;
   
    oldColor = strip.getPixelColor(ledNo);
    r = (oldColor & 0x00ff0000UL) >> 16;
    g = (oldColor & 0x0000ff00UL) >> 8;
    b = (oldColor & 0x000000ffUL);

    r=(r<=10)? 0 : (int) r-(r*fadeValue/256);
    g=(g<=10)? 0 : (int) g-(g*fadeValue/256);
    b=(b<=10)? 0 : (int) b-(b*fadeValue/256);
   
    strip.setPixelColor(ledNo, r,g,b);
   // FastLED
   //leds[ledNo].fadeToBlackBy( fadeValue );
}
void ledAlloff()
{
  for(uint16_t i=0; i<LEDno; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0, 0));
    strip.show();
  }
}
void setup() {
// put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D8, INPUT);
  updateflag = 0;

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  for(;;);
  }
  delay(1000); // Pause for 2 seconds
  // Clear the buffer
  display.clearDisplay();
  display.setTextSize(2);
  //display.setRotation(3);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0,0);
  display.println(F("Speedometer"));

  display.display();
  delay(1000);
  start = millis();
  attachInterrupt(digitalPinToInterrupt(D8), isr_measure, RISING); //*added

  strip.begin();
  strip.setBrightness(50);
  strip.show(); // Initialize all pixels to 'off'
  for(int ii=0;ii<20;ii++)
  {
  //rainbowCycle(2);
//  meteorRain(0xff,0xff,0xff,7, 64, true, 30);
    strip.setPixelColor(ii, strip.Color(255, 150, 0));
    strip.show();
  delay(500);

  }
  ledAlloff();
}
void loop() {
// put your main code here, to run repeatedly:
  unsigned long tempTickCount, tmp;
  unsigned long now = millis();
  display.clearDisplay();

if (( now - start >= updateTime )||updateflag==1) {
    noInterrupts();  // turn off interrupts while copying count
    tempTickCount = tickCount;
    interrupts();
    updateflag = 0;
    elapsed = now - start;
    tmp = tempTickCount - lastTickCount;
    speedCalc( tempTickCount - lastTickCount, elapsed );
    start = now;
    lastTickCount = tempTickCount;

    display.setCursor(0,0);// Start at top-left corner
    display.println(F("Speed"));
    display.setCursor(0,20);// Start at top-left corner
    display.println(speedk);

    display.setCursor(0,40);// Start at top-left corner
//    display.print(F("Time:"));
//    display.setCursor(0,60);
    display.print(int(elapsed));
//    display.print(",");
//    display.println(int(tmp));
    display.display();
}
}