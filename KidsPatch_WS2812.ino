#include <Adafruit_NeoPixel.h>
#include "animations270.h"
//#include "animations180.h"
//#include "animations90.h"
//#include "animations.h"


#define MAX_BRIGHTNESS 50
#define MIN_BRIGHTNESS 10
#define LONG_PRESS_DURATION 2000
//#define AUTO_SLEEP_DURATION 3600000 // 60 minutes
#define AUTO_SLEEP_DURATION 1200000 // 20 minutes
#define PIN_BTN 4      // Arduino pin that connects to a push button
#define PIN_WS2812B 3  // Arduino pin that connects to WS2812B
#define NUM_PIXELS 25  // The number of LEDs (pixels) on WS2812B


Adafruit_NeoPixel WS2812B(NUM_PIXELS, PIN_WS2812B, NEO_GRB + NEO_KHZ800);

const uint8_t SINE_COUNT = 24;
const uint8_t SINE[] = { 4, 5, 6, 7, 7, 8, 8, 8, 7, 7, 6, 5, 4, 3, 2, 1, 1, 0, 0, 0, 1, 1, 2, 3 };

uint32_t buttonDown = 0;
bool goAsleep = false;
bool sleeping = false;
bool firstSleep = false;

uint8_t  currLine = 100;
uint16_t currAniSelect = 254;
uint8_t  currAniFrame = 0;
uint8_t  currAniCount = 0;
int      currAniAddr = 0;
int      currR = 40, currG = random(MAX_BRIGHTNESS), currB = random(MAX_BRIGHTNESS);
int      currDR = 2, currDG = 2, currDB = 3;
uint8_t  currRainbowMode = 0;
uint8_t  currRainbowTable[] { 0, 0, 0, 0, 0 };
uint8_t  currRainbowIndex = 0;
uint8_t  currRainboxShift = 4;
int      currRainbowBright = MIN_BRIGHTNESS;
int      currRainbowDiff = random(3) + 1;

uint8_t  currHeartIndex = 254;
int16_t  currHeartBlue = MIN_BRIGHTNESS;
int8_t   currHeartDiff = 1;

uint8_t  currDogIndex = 253;

volatile uint8_t  currMode = 0;
volatile uint32_t lastActionTime = 0;



void setup() {
  ADCSRA = 0; // Disable analog-to-digital conversion

  pinMode(0, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);

  WS2812B.begin();
  
  currRainbowTable[0] = SINE[0];
  currRainbowTable[1] = SINE_COUNT - 1;
  currRainbowTable[2] = SINE_COUNT - 2;
  currRainbowTable[3] = SINE_COUNT - 3;
  currRainbowTable[4] = SINE_COUNT - 4;
  
  // Disable interrupts
  cli();
  // Enable interrupt handler (ISR) for PIN_I (PCINT1)
  PCMSK |= (1 << PIN_BTN);
  // Enable PCINT interrupt in the general interrupt mask
  GIMSK |= (1 << PCIE);
  // Set pin as input pin - has dedecated pullup
  pinMode(PIN_BTN, INPUT);
  
  // Last line of setup - re-enable interrupts
  sei();
}

/**
 * Interrupt handler 
 */
ISR(PCINT0_vect) {
  if (sleeping) {
    // Interrupt while sleeping
    sleeping = false;
    MCUCR &= ~(1 << SE); // Disabling sleep mode
    buttonDown = 0;
    lastActionTime = millis();
    return;
  }
  
  if (digitalRead(PIN_BTN) == LOW) { // Button pressed
    // Remember when the button was pressed down
    buttonDown = millis();
  } else if (!sleeping && buttonDown != 0) { // button released
    uint32_t down = buttonDown;
    uint32_t time = millis();
    buttonDown = 0; // Set handled
    lastActionTime = millis();
    if (time < down) return; // Safe-guard
    uint32_t duration = time - down;
    if (duration < 25) { // De-bounce
      return;
    } else if (duration < 2000) { // Short press
      currMode++;
      currAniFrame = 253;
      currAniSelect = 253;
      currDogIndex = 253;
    } else { // Long press
      goAsleep = true; // Go aspleep at next loop
    }
  }
}

/**
 * Set the chip into sleep mode
 */
void fallAsleep() {
  sleeping = true;

  // Enabling sleep mode and powerdown sleep mode
  MCUCR |= (1 << SM1);
  // Enabling sleep enable bit
  MCUCR |= (1 << SE);
  // Sleep instruction to put controller to sleep
  __asm__ __volatile__ ( "sleep" "\n\t" :: );
}



/**
 * Read the frame count of a stored animation (first value)
 * 
 * @param addr   The pointer of the progmem animation
 * @return       The number of frames teh animation has
 */
uint8_t getAniFrameCount(int addr) {
  return (uint8_t)pgm_read_dword(addr) & 0xFF;
}

/**
 * Writes all pixels of an animation frame to the WS2812B
 * 
 * @param addr    The pointer of the progmem animation
 * @param frame   The index of the frame of the animation (one-based)
 * @param shift   The number of pixels to shift the frame to the right
 * @param color   The color for enabled bits of the animation
 * @param bg      The color for disabled bits of the animation (will not be set if the hightest bit 0x80000000 is 1, enabling transparency)
 */
void setAniImage(int addr, uint8_t frame, int8_t shift, uint32_t color, uint32_t bg = 0) {
  // Read frame from program memory
  uint32_t frameData = pgm_read_dword(addr + (frame * sizeof(uint32_t)));
  // Set all pixels of the frame in the given color or backcolor
  int8_t col;
  if ((bg & 0x80000000) == 0) {
    for (int8_t i = 0; i < 5 ; i++) {
      col = shift + i;
      if (col < 0 || col > 4) continue;
      WS2812B.setPixelColor(     col, ((frameData & ((uint32_t)1 << (24 - i))) == 0) ? bg : color);
      WS2812B.setPixelColor( 9 - col, ((frameData & ((uint32_t)1 << (19 - i))) == 0) ? bg : color);
      WS2812B.setPixelColor(10 + col, ((frameData & ((uint32_t)1 << (14 - i))) == 0) ? bg : color);
      WS2812B.setPixelColor(19 - col, ((frameData & ((uint32_t)1 << ( 9 - i))) == 0) ? bg : color);
      WS2812B.setPixelColor(20 + col, ((frameData & ((uint32_t)1 << ( 4 - i))) == 0) ? bg : color);
    }
  } else {
    for (int8_t i = 0; i < 5 ; i++) {
      col = shift + i;
      if (col < 0 || col > 4) continue;
      if ((frameData & ((uint32_t)1 << (24 - i))) != 0) WS2812B.setPixelColor(     col, color);
      if ((frameData & ((uint32_t)1 << (19 - i))) != 0) WS2812B.setPixelColor( 9 - col, color);
      if ((frameData & ((uint32_t)1 << (14 - i))) != 0) WS2812B.setPixelColor(10 + col, color);
      if ((frameData & ((uint32_t)1 << ( 9 - i))) != 0) WS2812B.setPixelColor(19 - col, color);
      if ((frameData & ((uint32_t)1 << ( 4 - i))) != 0) WS2812B.setPixelColor(20 + col, color);
    }
  }
}

/**
 * Create a new color by slightly shifting the RGB values
 */
uint32_t getNextAniColor() {
  int c;
  c = (int)currR + currDR;
  if (c > MAX_BRIGHTNESS) { currR = MAX_BRIGHTNESS; currDR = random(5) - 5; } else if (c < MIN_BRIGHTNESS) { currR = MIN_BRIGHTNESS; currDR = random(5) + 1; } else { currR = (uint8_t)c; }
  c = (int)currG + currDG;
  if (c > MAX_BRIGHTNESS) { currG = MAX_BRIGHTNESS; currDG = random(5) - 5; } else if (c < MIN_BRIGHTNESS) { currG = MIN_BRIGHTNESS; currDG = random(5) + 1; } else { currG = (uint8_t)c; }
  c = (int)currB + currDB;
  if (c > MAX_BRIGHTNESS) { currB = MAX_BRIGHTNESS; currDB = random(5) - 5; } else if (c < MIN_BRIGHTNESS) { currB = MIN_BRIGHTNESS; currDB = random(5) + 1; } else { currB = (uint8_t)c; }
  return WS2812B.Color(currR, currG, currB);
}

/**
 * Main loop for animation mode
 */
void loopAnis(uint8_t aniSet) {
  currAniFrame++;
  uint8_t frame = currAniFrame;
  uint8_t factor = aniSet == 4 ? 32 : 0;
  if (frame > currAniCount) {
    currAniFrame = frame = 1;
    currAniSelect++;
    switch (aniSet) {
      case 1: // Mario
        currAniSelect = 0;
        currAniAddr = &ANI_MARIO;
        break;
      case 2: // Pong
        currAniSelect = 0;
        currAniAddr = &ANI_PONG;
        break;
      case 3: // Car
        currAniSelect = 0;
        currAniAddr = &ANI_CAR;
        break;
      case 4: // Jumping dots
        currAniSelect = 0;
        currAniAddr = &JUMPING_DOTS;
        break;
      default: // Man running
        if (currAniSelect < 5) {
          currAniAddr = &ANI_MAN_RUN;
        } else if (currAniSelect < 6) {
          currAniAddr = &ANI_MAN_ROLL;
        } else if (currAniSelect < 11) {
          currAniAddr = &ANI_MAN_RUN;
        } else if (currAniSelect < 12) {
          currAniAddr = &ANI_MAN_FALL;
        } else if (currAniSelect < 13) {
          delay(500);
          currAniAddr = &ANI_MAN_STANDUP;
        } else if (currAniSelect < 14) {
          currAniAddr = &ANI_KAMI;
        } else if (currAniSelect < 15) {
          delay(200);
          currAniAddr = &ANI_BOOM;
        } else {
          currAniSelect = 0;
          currAniAddr = &ANI_MAN_RUN;
        }
        break;
    }
    currAniCount = getAniFrameCount(currAniAddr);
  }

  //WS2812B.clear();
  //setAniImage(currAniAddr, frame, 0, getNextAniColor());
  setFadeImage(currAniAddr, frame, 0, getNextAniColor(), factor);
  WS2812B.show();
  delay(100);
}


/**
 * Returns the rainbow color set by a specific shift and brightness
 *
 * @param shift        The color index in the rainbow colors (overflow allowed)
 * @param brightness   The max value of R, G, and B
 */
uint32_t getRainbowColor(uint8_t shift, uint8_t brightness) {
  switch (shift % 12) {
    case 0:  return WS2812B.Color(brightness,     0,              0);
    case 1:  return WS2812B.Color(brightness,     brightness / 2, 0);
    case 2:  return WS2812B.Color(brightness,     brightness,     0);
    case 3:  return WS2812B.Color(brightness / 2, brightness,     0);
    case 4:  return WS2812B.Color(0,              brightness,     0);
    case 5:  return WS2812B.Color(0,              brightness,     brightness / 2);
    case 6:  return WS2812B.Color(0,              brightness,     brightness);
    case 7:  return WS2812B.Color(0,              brightness / 2, brightness);
    case 8:  return WS2812B.Color(0,              0,              brightness);
    case 9:  return WS2812B.Color(brightness / 2, 0,              brightness);
    case 10: return WS2812B.Color(brightness,     0,              brightness);
    case 11: return WS2812B.Color(brightness,     0,              brightness / 2);
    
    default: return WS2812B.Color(brightness,     brightness,     brightness);
  }
}//reed

/**
 * Set a column of the WS2812B with a specific rainbow shift
 */
void setNextRainbowColumn(uint8_t shift, uint8_t brightness, uint8_t col) {
  WS2812B.setPixelColor(     col, getRainbowColor(shift    , brightness));
  WS2812B.setPixelColor( 9 - col, getRainbowColor(shift + 1, brightness));
  WS2812B.setPixelColor(10 + col, getRainbowColor(shift + 2, brightness));
  WS2812B.setPixelColor(19 - col, getRainbowColor(shift + 3, brightness));
  WS2812B.setPixelColor(20 + col, getRainbowColor(shift + 4, brightness));
  currRainbowTable[col] = shift;
}

/**
 * Adds a new shift to the start of the WS2812B and moves all current pixels
 * one to the right (last pixels stored in currRainbowTable)
 */
void setNextRainbow(uint8_t shift, uint8_t brightness) {
  for (uint8_t i = 4; i > 0; i--) {
    setNextRainbowColumn(currRainbowTable[i - 1], brightness, i);
  }
  setNextRainbowColumn(shift, brightness, 0);
}

/**
 * Main loop for rainbow mode
 */
void loopRainbow() {
  // Adjust brightness
  currRainbowBright += currRainbowDiff;
  if (currRainbowBright > MAX_BRIGHTNESS) {
    currRainbowBright = MAX_BRIGHTNESS;
    currRainbowDiff = random(4) - 4;
  } else if (currRainbowBright < MIN_BRIGHTNESS) {
    currRainbowBright = MIN_BRIGHTNESS;
    currRainbowDiff = random(4) + 1;
  }
  
  switch (currRainbowMode) {
    case 0: case 1: // Sine wave movement
      setNextRainbow(SINE[currRainbowIndex % SINE_COUNT], (uint8_t)(currRainbowBright & 0xFF));
      WS2812B.show();
      delay(100);
      
      currRainbowIndex++;
      if (currRainbowIndex >= (SINE_COUNT * 4)) {
        currRainbowIndex = 0;
        currRainbowMode++;
      }
      break;
    case 2: case 3:  // Linear movement
      currRainbowIndex++;
      if (currRainbowIndex > 100) {
        currRainbowIndex = 0;
        currRainbowMode++;
      } else {
        setNextRainbow(currRainbowIndex + SINE[0], (uint8_t)(currRainbowBright & 0xFF));
        WS2812B.show();
        delay(100);
      }
      break;
    default:
      currRainbowMode = 0;
  }
}


uint32_t decreaseColor(uint32_t color, uint8_t factor) {
  if (color == 0) return 0;
  
  return (((((color & 0x00FF0000) >> 16) * factor / 255) & 0xFF) << 16)
       | (((((color & 0x0000FF00) >>  8) * factor / 255) & 0xFF) <<  8)
     |  (( (color & 0x000000FF)        * factor / 255) & 0xFF)       ;
}

/**
 * Fades out the existing image and puts the new one on top
 * @param addr    The pointer of the progmem animation
 * @param frame   The index of the frame of the animation (one-based)
 * @param shift   The number of pixels to shift the frame to the right
 * @param color   The color for enabled bits of the animation
 * @param factor  The factor out of 255 for all current pixels
 */
void setFadeImage(int addr, uint8_t frame, uint8_t shift, uint32_t color, uint8_t factor) {
  // Read frame from program memory
  uint32_t frameData = pgm_read_dword(addr + (frame * sizeof(uint32_t)));
  // Fade all current pixels
  for (int8_t i = 0; i < 25; i++) {
    WS2812B.setPixelColor(i, decreaseColor(WS2812B.getPixelColor(i), factor));
  }
  // Set all active pixels of the frame in the given color
  int8_t col;
  for (int8_t i = 0; i < 5 ; i++) {
    col = shift + i;
    if (col < 0 || col > 4) continue;
    if ((frameData & ((uint32_t)1 << (24 - i))) != 0) WS2812B.setPixelColor(     col, color);
    if ((frameData & ((uint32_t)1 << (19 - i))) != 0) WS2812B.setPixelColor( 9 - col, color);
    if ((frameData & ((uint32_t)1 << (14 - i))) != 0) WS2812B.setPixelColor(10 + col, color);
    if ((frameData & ((uint32_t)1 << ( 9 - i))) != 0) WS2812B.setPixelColor(19 - col, color);
    if ((frameData & ((uint32_t)1 << ( 4 - i))) != 0) WS2812B.setPixelColor(20 + col, color);
  }
}

void loopHeart() {
  // Adjust blu in current color
  currHeartBlue += currHeartDiff;
  if (currHeartBlue < 0) {
    currHeartBlue = 0;
    currHeartDiff = random(2) + 1;
  } else if (currHeartBlue > MAX_BRIGHTNESS) {
    currHeartBlue = MAX_BRIGHTNESS;
    currHeartDiff = random(2) - 2;
  }
  
  currHeartIndex++;
  switch(currHeartIndex) {
    case 0: case 1: case 5:
      setFadeImage(&ANI_SIMPLEHEART, 1, 0, WS2812B.Color(MAX_BRIGHTNESS, 0, (uint8_t)currHeartBlue), 180);
      WS2812B.show();
      break;
    case 2: case 4: case 6:
      setFadeImage(&ANI_SIMPLEHEART, 2, 0, WS2812B.Color(MAX_BRIGHTNESS, 0, (uint8_t)currHeartBlue), 180);
      WS2812B.show();
      break;
    case 3: case 7: case 8: 
      setFadeImage(&ANI_SIMPLEHEART, 3, 0, WS2812B.Color(MAX_BRIGHTNESS, 0, (uint8_t)currHeartBlue), 180);
      WS2812B.show();
      break;
    
    default: 
      if (currHeartIndex < 20) {
        setFadeImage(&ANI_SIMPLEHEART, 4, 0, WS2812B.Color(MAX_BRIGHTNESS, 0, (uint8_t)currHeartBlue), 180);
        WS2812B.show();
      } else {
        currHeartIndex = 0;
      }
      break;
  }
  
  delay(100);
}


void loopDog() {
  WS2812B.clear();
  
  currDogIndex++;
  if (currDogIndex < 16) { // Stick, run in, run
    setAniImage(&ANI_DOG, currDogIndex, 0, getNextAniColor());
  } else if (currDogIndex < 66) { // 10x run
    setAniImage(&ANI_DOG, 11 + ((currDogIndex - 16) % 5), 0, getNextAniColor());
  } else if (currDogIndex < 71) { // run out
    setAniImage(&ANI_DOG, currDogIndex - 50, 0, getNextAniColor());
  } else if (currDogIndex < 75) { // empty
    setAniImage(&ANI_DOG, 20, 0, getNextAniColor());
  } else { // Restart
    currDogIndex = 0;
    setAniImage(&ANI_DOG, 0, 0, getNextAniColor());
  }

  WS2812B.show();
  delay(100);
}



void loop() {
  uint32_t time = millis();
  
  if (buttonDown != 0 && time - buttonDown > 2000) {
    // Button is long pressed -> turn display off
    WS2812B.clear();
    WS2812B.show();
    return;
  }  else if (time - lastActionTime > AUTO_SLEEP_DURATION) {
    // Running without action for too long
    WS2812B.clear();
    WS2812B.show();
    goAsleep = true;
  }
  
  if (goAsleep) {
    goAsleep = false; // Set handled
    fallAsleep();
    return;
  }
  
  uint8_t mode = currMode;

  switch (mode) {
    case 0: loopHeart(); break;
    case 1: loopDog(); break;
    case 2: loopRainbow(); break;
    case 3: loopAnis(0); break; // running
    case 4: loopAnis(1); break; // mario
    case 5: loopAnis(2); break; // pong
    case 6: loopAnis(3); break; // car
    case 7: loopAnis(4); break; // dots
    default:
      currMode = 0;
      break;
  }
}
