// Compiler optimisation mightycore: -O3

#include <Adafruit_SSD1306.h>
#include <TinyGPS++.h>
#include <SD.h>
#include <I2C.h>
#include <avr/wdt.h>
#include <EEPROM.h>

#define OLED_DC 3
#define OLED_CS 2
#define OLED_RESET 1
#define btn1 A5
#define btn2 A4
#define sdCs 10
#define cardDetectPin 11
#define latchOut 4
#define eepromIntervalAddress 2
#define LTC2942Address 0x64
#define eeprDispSleepTimeAddr 3
#define MMA8452 0x1c
#define fixAgeMax 2000

TinyGPSPlus gps;
Adafruit_SSD1306 display(OLED_DC, OLED_RESET, OLED_CS);
// Uses short 8.3 names for files, at most 8 characters
File dataFile;

boolean cardPresent = false;
boolean logFlag = false;
boolean fix = false;
boolean ignoreFunction = false;
boolean cardDetectPinStatus;
boolean doNotDim = false;
boolean newFileFlag = true;
byte menuItem = 0;
byte numberOfIcons = 0;
byte iconWidth;
byte dataItem = 0;
byte rawAccelData[6]; // x/y/z accel register data stored here
byte orientation;
byte totalIconWidth = 0;
byte xPosition = 0;
char fileName[13];
int batPercentageSegment;
int twosComplementConversion[6];
unsigned long previousMillis = 0;
unsigned long previousAcrMillis;
unsigned long numberOfLogs;
unsigned long logInterval;
unsigned long lastConfirmedFix;
unsigned long fixAge;
unsigned long shutDownCountStart;
unsigned long shutDownCounter;
unsigned long displaySleepCounter;
unsigned long displaySleepTime;
long lastAcrValues [2] = {0, 0};
long batteryCapacity;
long eepromBatteryCapacity;
float temp;
String fileNameString;

const uint8_t gpsIcon[] PROGMEM = {0x80, 0x82, 0xC4, 0xC8, 0xE0, 0x70, 0x7C, 0x1F};
const uint8_t fixIcon[] PROGMEM = {0x99, 0x42, 0x24, 0x99, 0x99, 0x24, 0x42, 0x99};
const uint8_t sdWarnIcon[] PROGMEM = {0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x00, 0x3C, 0x3C};

boolean easterMode = false;
int xApple;
int yApple;
boolean caught = true;
int score = 0;
int hitboxSize;
boolean runGame = false;
unsigned long currentGameTimerMillis;
unsigned long startGameTimerMillis;
boolean gamePlayed = false;

void setup() {

  // Clear the reset bit
  MCUSR &= ~_BV(WDRF);
  // Disable the WDT
  WDTCSR |= _BV(WDCE) | _BV(WDE);
  WDTCSR = 0;

  delay(1000);

  display.clearDisplay();

  pinMode(btn1, INPUT_PULLUP);
  pinMode(btn2, INPUT_PULLUP);
  pinMode(cardDetectPin, INPUT_PULLUP);
  pinMode(sdCs, OUTPUT);
  pinMode(latchOut, OUTPUT);
  digitalWrite(latchOut, HIGH);

  logFlag = 0;

  I2c.begin();
  I2c.setSpeed(1); // Fast I2c 400Khz
  display.begin(SSD1306_SWITCHCAPVCC);
  display.setRotation(2);

  cardDetectPinStatus = digitalRead(cardDetectPin);

  Serial.begin(9600);

  // Change baud rate to 4800
  Serial.println("$PMTK251,4800*14");
  delay(500);
  Serial.end();

  // Restart Serial @ new baud
  Serial.begin(4800);

  // Set GPS to standby
  Serial.println("$PMTK161,0*28");

  if (cardDetectPinStatus == false) {
    SD.begin(sdCs);
    cardPresent = true;
  }
  else {
    cardPresent = false;
  }

  // Set initial display orientation
  readPlOrientation ();

  // Test program initial batteryCapacity, highbyte @ eeprom 0, lowbyte @ eeprom 1; Test program logInterval
  // EEPROM.write( 0, highByte(18823) );
  // EEPROM.write( 1, lowByte(18823) );

  // Test program ACR value
  // writeReg16( LTC2942Address, 0x02, 49000 );

  // Read stored EEPROM battery capacity
  eepromBatteryCapacity = word( EEPROM.read(0), EEPROM.read(1) );
  // Read stored log interval / display sleeptime
  logInterval = EEPROM.read(eepromIntervalAddress) * 1000L ;
  displaySleepTime = EEPROM.read(eeprDispSleepTimeAddr) * 1000L;
  if (displaySleepTime == 65000) {
    doNotDim = true;
  }

  // Initialise MMA8452
  MMA8452init();

  // Setup LTC2942 control register, ADC to automatic, ALCC to CC
  I2c.write( LTC2942Address, 0x01, B11111010 );

  if (digitalRead(btn2) == LOW) {
    easterMode = true;
  }

  // Easter egg minigame here
  if (easterMode == true) {
    
    while(1) {

      while (runGame == true) {

        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.setTextSize(1);
        display.print("Pts:");
        display.print(score);
          
        readAccelRegisters();
        
        int x = map(twosComplementConversion[1], -128, 128, 127, 0);
        int y = map(twosComplementConversion[0], -128, 128, 63, 0);
  
        hitboxSize = 10;
        
        display.fillCircle(x, y, hitboxSize, WHITE);
  
        if (caught == true) {
          xApple = random(0, 127);
          yApple = random(8, 63);
          caught = false;
        }
  
        display.fillCircle(xApple, yApple, 3, WHITE);
  
        if (x >= (xApple - hitboxSize) && x <= (xApple + hitboxSize) && y >= (yApple - hitboxSize) && y <= (yApple + hitboxSize)) {
          caught = true;
          score++;
        }

        currentGameTimerMillis = millis();
        unsigned long gameTimeMillis = currentGameTimerMillis - startGameTimerMillis;
        unsigned long gameTimeLimit = 10000;
        if ( gameTimeMillis > gameTimeLimit ) {
          runGame = false;
          gamePlayed = true;
        }
        
        int timeBarWidth = map(gameTimeMillis, 0, gameTimeLimit, 0, 84);

        display.fillRect(42, 0, 84 - timeBarWidth, 7, WHITE);

        display.display();
        display.clearDisplay();
        
      }

      display.setTextColor(WHITE);
      display.setCursor(0, 22);
      display.setTextSize(3);

      if (gamePlayed == true) {
        display.print(score);
        display.print(" Pts!");
      }
      else {
        display.print("Start!");        
      }
      
      display.display();
      display.clearDisplay();
      
      // Shutdown
      if (digitalRead(btn1) == LOW || tapDetect() == 64 ) {
        startGameTimerMillis = millis();
        score = 0;
        runGame = true;      
        shutDownCountStart = millis();
        while (digitalRead(btn1) == LOW) {
          shutDownCounter = millis();
          if (shutDownCounter > (shutDownCountStart + 1000)) {
            display.clearDisplay();
            display.display();
            digitalWrite(latchOut, LOW);
          }
        }
      }    
    }
 
  }


}

void loop() {

  /////////////////////// BTN STUFF HERE
  
  if (menuItem == 0) {
    // Btn1 Stuff
    if (digitalRead(btn1) == LOW) {
      wakeDisplay();      
      shutDownCountStart = millis();
      while (digitalRead(btn1) == LOW) {
        shutDownCounter = millis();
        if (shutDownCounter > (shutDownCountStart + 1000)) {
          display.clearDisplay();
          display.display();
          finaliseDataFile();
          // Set ADC to sleep
          I2c.write( LTC2942Address, 0x01, B00111010 );
          // Power down supply fet
          digitalWrite(latchOut, LOW);
        }
      }
      if (fix == true) {
        dataItem++;
        if (dataItem > 2) {
          dataItem = 0;
        }
      }
      delay(250);
    }
  
    // btn2 stuff
    if ( digitalRead(btn2) == LOW) {
      wakeDisplay();       
      shutDownCountStart = millis();
      while (digitalRead(btn2) == LOW) {
        shutDownCounter = millis();
        if (shutDownCounter > (shutDownCountStart + 1000)) {
          menuItem++;
          ignoreFunction = true;
          break;
        }
      }

      if (ignoreFunction == false) {
        logFlag = !logFlag;
        if (logFlag == true) {
          // Wake GPS, when GPS is reset, baud rate is set to default 9600, will need to reset it to 4800 (for some reason GPS only accepts serial commands @ 4800 baud ...)
          // NB. Increased capacitance on VCC from 1uF + 0.1uF to 10uF + 0.1uF, GPS serial stable
          Serial.println("$PMTK161,1*29");
          Serial.end();
          Serial.begin(9600);
          // Set update rate to 5Hz
          // Serial.println("$PMTK220,200*2C");
          // Set baud rate to 4800
          Serial.println("$PMTK251,4800*14");
          Serial.end();
          Serial.begin(4800);
          // Set Nav speed threshold (Command list: https://cdn-shop.adafruit.com/datasheets/PMTK_A11.pdf; Checksum calculator: http://www.hhhh.org/wiml/proj/nmeaxor.html); PA6H Datasheet: https://cdn-shop.adafruit.com/datasheets/GlobalTop-FGPMMOPA6H-Datasheet-V0A.pdf
          Serial.println("$PMTK386,1.0*3C");
          Serial.flush();
          // Set newfileflag to true
          newFileFlag = true;
        }
        else if (logFlag == false) {
          finaliseDataFile();
          // Set GPS to standby
          Serial.println("$PMTK161,0*28");
        } 
      }
      ignoreFunction = false;
      delay(500);
    }

  }

  if (menuItem == 1) {
    // Btn 1 stuff
    if (digitalRead(btn1) == LOW) {
      wakeDisplay(); 
      logInterval += 5000;
      if (logInterval >= 65000) {
        logInterval = 5000;
      }
      EEPROM.write(eepromIntervalAddress, logInterval / 1000);
      delay(250);
    }
    // Btn 2 stuff
    if (digitalRead(btn2) == LOW) {
      wakeDisplay(); 
      menuItem++;
      delay(250);
    }
  }

  if (menuItem == 2) {
    // Btn 1 stuff
    if (digitalRead(btn1) == LOW) {
      wakeDisplay(); 
      displaySleepTime += 5000;
      if (displaySleepTime == 65000) {
        doNotDim = true;
      }
      if (displaySleepTime > 65000) {
        doNotDim = false;
        displaySleepTime = 5000;
      }
      EEPROM.write(eeprDispSleepTimeAddr, (displaySleepTime / 1000L)); // Byte cast?
      delay(250);
    }
    // Btn 2 stuff
    if ( digitalRead(btn2) == LOW) {
      wakeDisplay(); 
      menuItem++;
      if (menuItem == 3) {
        menuItem = 0;
      }
      delay(250);
    }
  }

  // Sleep display when displaySleepCounter expires
  if ( ( millis() - displaySleepCounter ) > displaySleepTime ) {
    if (doNotDim == false && menuItem == 0) {
      display.ssd1306_command(SSD1306_DISPLAYOFF);
    }    
  }

  // Reset on card insertion/removal
  if ( digitalRead(cardDetectPin) != cardDetectPinStatus ) {
    Serial.flush();
    softwareReset(WDTO_15MS);
  }

  /////////////////////// GPS + LOGGING STUFF HERE
  
  // While Serial available, encode gps
  while (Serial.available() > 0) {
    gps.encode(Serial.read());
  }

  // Have fix?
  if (gps.location.age() < fixAgeMax) {
    fix = true;
  }
  else {
    fix = false;
  }

  // Store logs
  unsigned long currentMillis = millis();
  if (logFlag == true && fix == true && cardPresent == true) {
    
    // sprintf(fileName, "%02d%02d%1s.gpx", gps.date.month(), gps.date.day(), "_");
      
    if (newFileFlag == true) {
      for(int i = 1; ; i++) {
         fileNameString = gps.date.month();
         fileNameString = fileNameString + gps.date.day();
         fileNameString.concat("_");
         fileNameString.concat(i);
         fileNameString = fileNameString + ".gpx";
         fileName[fileNameString.length()+1];
         fileNameString.toCharArray(fileName, sizeof(fileName));
         if(!SD.exists(fileName))
         {
            dataFile = SD.open(fileName,FILE_WRITE);            
            dataFile.print("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
            dataFile.print("<gpx version=\"1.0\">");
            dataFile.print("<trk><name>Track gadget</name><number>1</number><trkseg>");
            newFileFlag = false;
            break;
         }
      }
    }
    else {
      dataFile = SD.open(fileName,FILE_WRITE);
    }

    if (dataFile) {
      
      if (currentMillis - previousMillis > (logInterval)) {
        if (gps.location.isUpdated()) {

          dataFile.print("<trkpt lat=\"");
          dataFile.print(gps.location.lat(), 6);
          dataFile.print("\" lon=\"");
          dataFile.print(gps.location.lng(), 6);
          dataFile.print("\"><ele>");
          dataFile.print(gps.altitude.meters(), 0);
          dataFile.print("</ele><time>");
          dataFile.print(gps.date.year()); 
          dataFile.print("-"); 
          dataFile.print(gps.date.month()); 
          dataFile.print("-");
          dataFile.print(gps.date.day());
          dataFile.print("T");
          dataFile.print(gps.time.hour());
          dataFile.print(":");
          dataFile.print(gps.time.minute());
          dataFile.print(":");
          dataFile.print(gps.time.second());
          dataFile.println("Z</time></trkpt>");

          numberOfLogs++;
          previousMillis = currentMillis;

        }
      }
    }
    dataFile.close();
  }

  /////////////////////// BATTERY GAUGE STUFF HERE
  
  /*
    Sense resistor = 100mOhms
    POR:             32767 acr units
    800mAh:          18823 acr units
    Charge Complete: 65535 acr units

    Bat fuel gauge method:

    When ACR is 65535: Store last known good ACR value before Charge Complete. Size battery capacity based on accumulated charge, not discharge.

      -> read ACR periodically
        -> store last 2 values in array
      -> if second value is 65535 (Charge Complete, CC pin will be HIGH)
        -> 1st value = max ACR reached whilst charging
        -> 1st value - 32767 = battery capacity in acr units
        -> write battery capacity in acr units to ACR register
        -> Store acr battery capacity to EEPROM
        -> clear array or shutdown
      -> ACR now equals 1st value
  */

  // Periodically store last two read values ACR register in array + read temp
  unsigned long currentAcrMillis = millis();
  if ( (currentAcrMillis - previousAcrMillis) > 1000 ) {
    
    lastAcrValues[0] = lastAcrValues[1];
    lastAcrValues[1] = readReg16(LTC2942Address, 0x02);
    temp = (600.00 * (readReg16(LTC2942Address, 0x0c) / 65535.00)) - 273.15;

    // If battery is fully charged, lastAcrValues array last value is 65535 (MCP73831 STAT pin HIGH -> LTC2942 CC pin HIGH) -> lastAcrValues array first value is written to ACR
    // Only write updated battery size to ACR when first value is up-to-date and battery data has not been skewed due to internal discharge
  
    // Store calibrated battery size when unit is on and charged
    if ( (lastAcrValues[1] == 65535) && (lastAcrValues[0] > 0) && (lastAcrValues[0] < 65000) ) {
      // Disable ALCC, if configured as CC, writing to ACR register will have no effect, ACR will be overwritten because CC is HIGH...
      I2c.write( LTC2942Address, 0x01, B11111000 );
      // Write last known good ACR value, now known battery size
      writeReg16( LTC2942Address, 0x02, lastAcrValues[0] );
      // Store updated battery capacity in EEPROM, will be read on startup, display accurate battery level
      batteryCapacity = lastAcrValues[0] - 32767;
      EEPROM.write(0, highByte(batteryCapacity));
      EEPROM.write(1, lowByte(batteryCapacity));
      // Clear display for bootscreen
      display.clearDisplay();
      display.display();
      // Shutdown when charge complete
      digitalWrite(latchOut, LOW);
    }
    // Else store old calibrated battery size to last known value when unit was off and charged.
    else if ( (lastAcrValues[1] > 65000) && (lastAcrValues[0] == 0) ) {
      // Disable ALCC, if configured as CC, writing to ACR register will have no effect, ACR will be overwritten because CC is HIGH...
      I2c.write( LTC2942Address, 0x01, B11111000 );
      // Write last known good ACR value, now known battery size
      writeReg16(LTC2942Address, 0x02, 32767 + eepromBatteryCapacity);
      // Setup LTC2942 control register, ADC to automatic, ALCC to CC
      I2c.write( LTC2942Address, 0x01, B11111010 );
    }
    // Map battery ACR units to segments display 
    batPercentageSegment = map(lastAcrValues[1], 32767, 32767 + eepromBatteryCapacity, 0, 8);    
    previousAcrMillis = currentAcrMillis;
    
  }
  
  /////////////////////// ACCELEROMETER STUFF HERE

  readAccelRegisters();
  readPlOrientation(); // Pin 1 Bottom left top facing; 3 start-towards, 2 start-away, 1 start-left, 0 start-Right

  /////////////////////// OLED STUFF HERE
  
  // BG bat 1
  display.fillRect(0, 0, 45, 13, WHITE);
  // BG bat 2
  display.fillRect(2, 2, 41, 9, BLACK);
  // BG tip
  display.fillRect(45, 3, 2, 7, WHITE);

  // Draw battery segments
  for (int i = 0; i <= batPercentageSegment; i++) {
    display.fillRect(i * 5 + 3, 3, 4, 7, WHITE);
  }

  // Bitmaps and icons top right
  display.setTextSize(1);
  display.setTextColor(BLACK);
  totalIconWidth = 0;
  xPosition = 0;
  numberOfIcons = 0;
  if (cardPresent == false) {
    iconWidth = 12;
    calcXPosIcon();
    display.fillRect(xPosition, 0, iconWidth, 13, WHITE);
    display.drawBitmap(xPosition + 2, 2, sdWarnIcon, 8, 8, 0);
  }
  
  if (logInterval < 10000) {
    iconWidth = 18;
  }
  else {
    iconWidth = 25;
  }
  calcXPosIcon();
  display.fillRect(xPosition, 0, iconWidth, 13, WHITE);
  display.setCursor(xPosition + 3, 3);
  display.print(logInterval / 1000);
  display.print("s");
  
  if (logFlag == 1) {
    if (gps.satellites.value() < 10) {
      iconWidth = 24;
    }
    else{
      iconWidth = 31;
    }
    calcXPosIcon();
    display.fillRect(xPosition, 0, iconWidth, 13, WHITE);
    display.setCursor(xPosition + 3, 3);
    display.print(gps.satellites.value());
    display.drawBitmap(xPosition + 12 + (iconWidth - 24), 2, gpsIcon, 8, 8, 0);
  }

  // Main info text
  display.setTextColor(WHITE);
  display.setCursor(0, 22);
  
  switch (menuItem) {
    case 0:
      display.setTextSize(3);
      if (logFlag == 0) {
        display.print("PAUSE");        
      }
      else if (logFlag == 1) {
        if (fix == true) {
          switch (dataItem) {
            case 0:
              display.print(gps.speed.kmph(), 2);
              display.setTextSize(1);
              display.print("KMH");
              break;
            case 1: 
              display.print(gps.altitude.meters(), 2);
              display.setTextSize(1);
              display.print("MTR");
              break;
            case 2:
              addZeroToTime(gps.time.hour());
              display.setTextSize(1);
              display.print("\"");
              display.setTextSize(3);
              addZeroToTime(gps.time.minute());
              display.setTextSize(1);
              display.print("\"");
              display.setTextSize(3);
              addZeroToTime(gps.time.second());
              break;
          }
          display.setTextSize(3);
        }
        else {
          if ( (millis() % 500) < 250 ) {
            display.print("GET FIX");
          }
        }
      }
      break;
    case 1:
      display.setTextSize(2);
      display.setCursor(0, 16);
      display.print("LOG: ");
      if ( (millis() % 500) < 250 ) {
        display.print(logInterval / 1000);
        display.print("s");  
      } 
      display.setCursor(0, 33);
      display.print("DIM: ");
      if (doNotDim == true) {
        display.print("None");
      }
      else  {
        display.print(displaySleepTime / 1000);
        display.print("s");    
      }  
      break;
    case 2:
      display.setTextSize(2);
      display.setCursor(0, 16);
      display.print("LOG: ");
      display.print(logInterval / 1000);
      display.print("s");
      
      display.setCursor(0, 33);
      display.print("DIM: ");
      if ( (millis() % 500) < 250 ) {
        if (doNotDim == true) {
          display.print("None");
        }
        else {
          display.print(displaySleepTime / 1000);
          display.print("s");    
        }
      }      
      break;
  }

  // Bottom info
  display.fillRect(0, 51, 128, 13, WHITE);
  display.setTextSize(1);
  display.setCursor(4, 54);
  display.setTextColor(BLACK);
  if (cardPresent == false) {
    display.print("NO CARD!");
  }
  else {
    display.print(numberOfLogs);
    display.print(" LOGS ");
    display.print(temp, 1);
    display.print("C");
  }

  display.display();
  display.clearDisplay();

}

void finaliseDataFile() {
  if (newFileFlag == false) {
    dataFile = SD.open(fileName,FILE_WRITE);
    if(dataFile) {
      dataFile.print("</trkseg></trk></gpx>");
    }
    dataFile.close();    
  }
}

void calcXPosIcon() {
  totalIconWidth += iconWidth;
  numberOfIcons++;
  xPosition = 130 - totalIconWidth - (numberOfIcons * 2);
}

void addZeroToData(int dataValueToZero) {
  if (dataValueToZero < 10) {
    dataFile.print("0");
  }
  dataFile.print(dataValueToZero);
}

void addZeroToTime(unsigned int timeValueToZero) {
  if (timeValueToZero < 10) {
    display.print("0");
  }
  display.print(timeValueToZero);

}

void wakeDisplay() {
  displaySleepCounter = millis();
  display.ssd1306_command(SSD1306_DISPLAYON);
}

void softwareReset(uint8_t prescaler) {
  // Start watchdog with the provided prescaler
  // wdt_enable(prescaler);
  // Wait for the prescaller time to expire
  // while(1) {}
}

int readReg8 (byte deviceAddress, int regAddress) {
  int data = 0;
  I2c.read(deviceAddress, regAddress, 1);
  data = I2c.receive();
  return(data);
}

word readReg16(byte deviceAddress, int regAddress) {
  byte dataH, dataL;
  word data16 = 0;
  // Point to status register regAddress and receive two bytes of data
  I2c.read(deviceAddress, regAddress, 2);
  dataH = I2c.receive();
  dataL = I2c.receive();
  // Combine MSB and LSB to word
  data16 = word(dataH, dataL);
  return (data16);
}

void writeReg16(byte deviceAddress, int regAddress, word wordToWrite) {
  I2c.write( deviceAddress, regAddress, highByte(wordToWrite) );
  I2c.write( deviceAddress, regAddress + 1, lowByte(wordToWrite) );
}

// Datarate or scale not set by variable yet...
void MMA8452init() {
  I2c.write(MMA8452, 0x2a, B00000000); // Control registers can only be changed when in sleep mode
  I2c.write(MMA8452, 0x2a, B00001100); // ODR to 400hz, LNOISE enabled, sleep still enabled
  I2c.write(MMA8452, 0x2b, B00000001); // Low power low noise SMODS oversampling
  I2c.write(MMA8452, 0x0e, B00000001); // Set g-force scale to +-4g ( If the low-noise bit is set in register 0x2A then the maximum threshold will be limited to 4 g regardless of the full-scale range.)
  I2c.write(MMA8452, 0x11, B01000000); // Enable Portrait / Landscape
  I2c.write(MMA8452, 0x12, B10100000); // Debounce counter at 100ms (at 800 hz)
  I2c.write(MMA8452, 0x21, B01010000); // Single tap detect on z-axis
  I2c.write(MMA8452, 0x25, B00101010); // Z-axis tap detect threshold set to 0x2a (// half of example 0x2a, example uses +-2g)
  I2c.write(MMA8452, 0x26, B00101000); // Set Time Limit for Tap Detection to 25 ms (0.625ms per step, 40 steps)
  I2c.write(MMA8452, 0x27, B01010000); // Set Latency Time to 100 ms (1.25ms per step, 80 steps)
  I2c.write(MMA8452, 0x2a, B00001101); // Activate MMA8452, ODR 400hz
}

void readAccelRegisters() {
  I2c.read(MMA8452, 0x01, 6);
  for(int i = 0; i < 6; i++) {
    rawAccelData[i] = I2c.receive();
  }
  // loop to calculate 12-bit ADC and g value for each axis
  for (int l = 0; l < 6; l += 2) {
    twosComplementConversion[l/2] = ((rawAccelData[l] << 8) | rawAccelData[l+1]) >> 4;  // Turn the MSB and LSB into a 12-bit value
    if (rawAccelData[l] > 0x7F) {  // If the number is negative, we have to make it so manually (no 12-bit data type)
      twosComplementConversion[l/2] = ~twosComplementConversion[l/2] + 1;
      twosComplementConversion[l/2] *= -1;  // Transform into negative 2's complement
    }
  }  
}

void readPlOrientation () {
  I2c.read(MMA8452, 0x10, 1);
  orientation = (I2c.receive() & B00000110) >> 1;
  if (orientation == 2) {
    display.setRotation(0);
  }
  if (orientation == 3) {
    display.setRotation(2);
  }
}

byte tapDetect () {
  I2c.read(MMA8452, 0x22, 1); // Read PULSE_SRC register
  byte readByte = I2c.receive() & B01000000;
  return(readByte);
}
