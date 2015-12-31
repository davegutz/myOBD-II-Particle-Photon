/*
OBD-II Interface
Display OBD-II output to OLED.
  OLED Connections:
    MicroOLED ------------- Photon
      GND ------------------- GND
      VDD ------------------- 3.3V (VCC)
    D1/MOSI ----------------- A5 (don't change)
    D0/SCK ------------------ A3 (don't change)
      D2
      D/C ------------------- D6 (can be any digital pin)
      RST ------------------- D7 (can be any digital pin)
      CS  ------------------- A2 (can be any analog pin)
  UART Connections:
    UART/FTID --------------- Photon
      GND ------------------- GND
      Tx  ------------------- Rx (Serial1)
      Rx  ------------------- Tx (Serial1)
    Hardware Platform: Particle Photon
                       SparkFun Photon Micro OLED
                      SparkFun UART/FTID OBD-II Shield
  Distributed as-is; no warranty is given.
*/
//#include "application.h"
SYSTEM_THREAD(ENABLED);      // Make sure heat system code always run regardless of network status
//#define TEST_SERIAL
#include "SparkFunMicroOLED.h"  // Include MicroOLED library
#include "math.h"
void display(const uint8_t x, const uint8_t y, const String str, const uint8_t clear=0, const uint8_t type=0, const uint8_t clearA=0);
MicroOLED oled;
//SYSTEM_MODE(MANUAL);

//This is a character buffer that will store the data from the serial port
char rxData[20];
char rxIndex      = 0;
//Variables to hold the speed and RPM data.
int vehicleSpeed  = 0;  // kph
int vehicleRPM    = 0;  // rpm
int verbose       = 5;  // Debugging Serial.print as much as you can tolerate.  0=none


void setup()
{
  Serial.begin(9600);
  Serial1.begin(9600);
  delay(2000);

  oled.begin();    // Initialize the OLED
  display(0, 0, "WAIT", 1, 1, 1);

  #ifndef TEST_SERIAL   // Jumper Tx to Rx
    WiFi.off();
  #endif

  //Reset the OBD-II-UART
  if (Serial1.available()) Serial1.println("ATZ");
  delay(3000);
}


void loop(){
  if (verbose>4)
  {
    display(0, 0, "begin", 1, 1);
  }
  delay(1000);

  // Get speed
  if (verbose>3)
  {
    display(0, 0, "Tx:010D", 1);
  }
  //Query the OBD-II-UART for the Vehicle Speed
  Serial1.flush();
  Serial1.println("010D");
  getResponse();
  #ifdef TEST_SERIAL   // Jumper Tx to Rx
    delay(1000);
    if (verbose>3)
    {
      display(0, 0, "Tx:60", 1);
      delay(1000);
    }
    //Serial1.flush();
    Serial1.println("60");
  #endif
  getResponse();
  #ifndef TEST_SERIAL
    vehicleSpeed = strtol(&rxData[6],0,16);
  #else
    delay(1000);
    vehicleSpeed = atol(rxData);
  #endif
  if (strlen(rxData)>4)
    display(0, 2, String(vehicleSpeed)+" kph");
  else
    display(0, 2, "--- kph");

  //Delete any data that may be left over in the serial port.
  delay(2000);

  // Get rpm
  if (verbose>3)
  {
    display(0, 0, "Tx:010C", 1);
    #ifdef TEST_SERIAL   // Jumper Tx to Rx
      delay(1000);
    #endif
  }
  Serial1.flush();
  Serial1.println("010C");
  getResponse();
  #ifdef TEST_SERIAL   //   Jumper Tx to Rx
    delay(1000);
    if (verbose>3)
    {
      display(0, 0, "Tx:900", 1);
      delay(1000);
    }
    //Serial1.flush();
    Serial1.println("900");
  #endif
  getResponse();
  //NOTE: RPM data is two bytes long, and delivered in 1/4 RPM from the OBD-II-UART
  #ifndef TEST_SERIAL
    vehicleRPM = ((strtol(&rxData[6],0,16)*256)+strtol(&rxData[9],0,16))/4;
  #else
    delay(1000);
    vehicleRPM = atol(rxData);
  #endif
  if (strlen(rxData)>4)
    display(0, 2, String(vehicleRPM)+" rpm");
  else
    display(0, 2, "--- rpm");
  delay(2000);
}


// Simple OLED print
void display(const uint8_t x, const uint8_t y, const String str, const uint8_t clear, const uint8_t type, const uint8_t clearA)
{
  Serial.println(str); Serial.flush();
  if (clearA) oled.clear(ALL);
  if (clear) oled.clear(PAGE);
  oled.setFontType(type);
  oled.setCursor(x, y*oled.getFontHeight());
  oled.print(str);
  oled.display();
}


//The getResponse function collects incoming data from the UART into the rxData buffer
// and only exits when a carriage return character is seen. Once the carriage return
// string is detected, the rxData buffer is null terminated (so we can treat it as a string)
// and the rxData index is reset to 0 so that the next string can be copied.
void getResponse(void){
  char inChar=0;
  if (verbose>4){
    Serial.printf("Rx:"); Serial.flush();
    oled.setFontType(0);
    oled.setCursor(0, oled.getFontHeight());
    oled.print("Rx:");
    oled.display();
  }
  //Keep reading characters until we get a carriage return
  bool go = true;
  unsigned long count = 0UL;
  #ifndef TEST_SERIAL
    const unsigned long countMax = ULONG_MAX;
  #else
    const unsigned long countMax = 10UL;
  #endif
  while(go && count++<countMax){
    //If a character comes in on the serial port, we need to act on it.
    if(Serial1.available() > 0){
      //Start by checking if we've received the end of message character ('\r').
      if(Serial1.peek() == '\r'){
        //Clear the Serial1 buffer
        inChar=Serial1.read();
        //Serial1.read();   // empty the buffer of lf
        rxData[rxIndex++]='\0';
        //Reset the buffer index so that the next character goes back at the beginning of the string.
        rxIndex=0;
        if (verbose>4){
          Serial.printf(";\n"); Serial.flush();
          oled.printf(";");
          oled.display();  // Display what's in the buffer (splashscreen)
        }
        go = false;
      }
      //If we didn't yet get the end of message character, add the new character to the string.
      else{
        //Get the new character from the Serial1 port.
        inChar = Serial1.read();
        if (isspace(inChar)) continue;  // Strip line feeds left from previous \n-\r
        if (verbose>4){
          Serial.printf("%c", inChar); Serial.flush();
          oled.printf("%c", inChar);
          oled.display();  // Display what's in the buffer (splashscreen)
          #ifdef TEST_SERIAL   //   Jumper Tx to Rx
            delay(100);
          #endif
        }
        //Add the new character to the string, and increment the index variable.
        rxData[rxIndex++] = inChar;
      }
    }
    else{   // not available
      if (verbose>4){
        #ifdef TEST_SERIAL   //   Jumper Tx to Rx
          delay(1000);
          Serial.printf("."); Serial.flush();
          oled.printf(".");   oled.display();
        #endif
      }
    }
  }
}








void textExamples()
{
  printTitle("Text!", 1);

  // Demonstrate font 0. 5x8 font
  oled.clear(PAGE);     // Clear the screen
  oled.setFontType(0);  // Set font to type 0
  oled.setCursor(0, 0); // Set cursor to top-left
  // There are 255 possible characters in the font 0 type.
  // Lets run through all of them and print them out!
  for (int i=0; i<=255; i++)
  {
    // You can write byte values and they'll be mapped to
    // their ASCII equivalent character.
    oled.write(i);  // Write a byte out as a character
    oled.display(); // Draw on the screen
    delay(10);      // Wait 10ms
    // We can only display 60 font 0 characters at a time.
    // Every 60 characters, pause for a moment. Then clear
    // the page and start over.
    if ((i%60 == 0) && (i != 0))
    {
      delay(500);           // Delay 500 ms
      oled.clear(PAGE);     // Clear the page
      oled.setCursor(0, 0); // Set cursor to top-left
    }
  }
  delay(500);  // Wait 500ms before next example

  // Demonstrate font 1. 8x16. Let's use the print function
  // to display every character defined in this font.
  oled.setFontType(1);  // Set font to type 1
  oled.clear(PAGE);     // Clear the page
  oled.setCursor(0, 0); // Set cursor to top-left
  // Print can be used to print a string to the screen:
  oled.print(" !\"#$%&'()*+,-./01234");
  oled.display();       // Refresh the display
  delay(1000);          // Delay a second and repeat
  oled.clear(PAGE);
  oled.setCursor(0, 0);
  oled.print("56789:;<=>?@ABCDEFGHI");
  oled.display();
  delay(1000);
  oled.clear(PAGE);
  oled.setCursor(0, 0);
  oled.print("JKLMNOPQRSTUVWXYZ[\\]^");
  oled.display();
  delay(1000);
  oled.clear(PAGE);
  oled.setCursor(0, 0);
  oled.print("_`abcdefghijklmnopqrs");
  oled.display();
  delay(1000);
  oled.clear(PAGE);
  oled.setCursor(0, 0);
  oled.print("tuvwxyz{|}~");
  oled.display();
  delay(1000);

  // Demonstrate font 2. 10x16. Only numbers and '.' are defined.
  // This font looks like 7-segment displays.
  // Lets use this big-ish font to display readings from the
  // analog pins.
  for (int i=0; i<25; i++)
  {
    oled.clear(PAGE);            // Clear the display
    oled.setCursor(0, 0);        // Set cursor to top-left
    oled.setFontType(0);         // Smallest font
    oled.print("A0:");          // Print "A0"
    oled.setFontType(2);         // 7-segment font
    oled.print(analogRead(A0));  // Print a0 reading
    oled.setCursor(0, 16);       // Set cursor to top-middle-left
    oled.setFontType(0);         // Repeat
    oled.print("A1:");
    oled.setFontType(2);
    oled.print(analogRead(A1));
    oled.setCursor(0, 32);
    oled.setFontType(0);
    oled.print("A7:");
    oled.setFontType(2);
    oled.print(analogRead(A7));
    oled.display();
    delay(100);
  }

  // Demonstrate font 3. 12x48. Stopwatch demo.
  oled.setFontType(3);  // Use the biggest font
  int ms = 0;
  int s = 0;
  while (s <= 50)
  {
    oled.clear(PAGE);     // Clear the display
    oled.setCursor(0, 0); // Set cursor to top-left
    if (s < 10)
      oled.print("00");   // Print "00" if s is 1 digit
    else if (s < 100)
      oled.print("0");    // Print "0" if s is 2 digits
    oled.print(s);        // Print s's value
    oled.print(":");      // Print ":"
    oled.print(ms);       // Print ms value
    oled.display();       // Draw on the screen
    ms++;         // Increment ms
    if (ms >= 10) // If ms is >= 10
    {
      ms = 0;     // Set ms back to 0
      s++;        // and increment s
    }
    delay(1);
  }
}

// Center and print a small title
// This function is quick and dirty. Only works for titles one
// line long.
void printTitle(String title, int font)
{
  int middleX = oled.getLCDWidth() / 2;
  int middleY = oled.getLCDHeight() / 2;

  oled.clear(PAGE);
  oled.setFontType(font);
  // Try to set the cursor in the middle of the screen
  oled.setCursor(middleX - (oled.getFontWidth() * (title.length()/2)),
                 middleY - (oled.getFontWidth() / 2));
  // Print the title:
  oled.print(title);
  oled.display();
  delay(1500);
  oled.clear(PAGE);
}














/*
#include <math.h>

#define _Digole_Serial_SPI_
#define SC_W 160  //screen width in pixels
#define SC_H 128  //screen Hight in pixels
#define _TEXT_ 0
#define _GRAPH_ 1

//
// Digole.h
//

class DigoleSerialDisp : public Print {
public:

//
// UART/I2C/SPI Functions
//

#if defined(_Digole_Serial_SPI_)
    DigoleSerialDisp(uint8_t pinSS) {

        _Comdelay = 10;
        _SS = pinSS;

    }

    void begin(void) {

        pinMode(_SS, OUTPUT);
        digitalWrite(_SS, HIGH);
        SPI.setBitOrder(MSBFIRST);
        SPI.setClockDivider(SPI_CLOCK_DIV64);
        SPI.setDataMode(0);
        SPI.begin();

    }

    void end(void) {
        pinMode(_SS, INPUT);
        SPI.end();
    }

    size_t write(uint8_t value) {

        PIN_MAP[_SS].gpio_peripheral->BRR = PIN_MAP[_SS].gpio_pin; //Low
        SPI.setDataMode(3);
        SPI.transfer(value);
        SPI.setDataMode(0);
        PIN_MAP[_SS].gpio_peripheral->BSRR = PIN_MAP[_SS].gpio_pin; //High
        return 1;
    }
#endif

#if defined(_Digole_Serial_SoftSPI_)
    DigoleSerialDisp(uint8_t pinData, uint8_t pinClock, uint8_t pinSS) {

        _Comdelay = 1;

        _Clock = pinClock;
        _Data = pinData;
        _SS = pinSS;

    }

    void begin(void) {

        pinMode(_Clock, OUTPUT);
        pinMode(_Data, OUTPUT);
        pinMode(_SS, OUTPUT);
        digitalWrite(_Clock, LOW);
        digitalWrite(_Data, LOW);
        digitalWrite(_SS, HIGH);

    }

    void end(void) {
        pinMode(_Clock, INPUT);
        pinMode(_Data, INPUT);
        pinMode(_SS, INPUT);
    }

    size_t write(uint8_t value) {

        PIN_MAP[_SS].gpio_peripheral->BRR = PIN_MAP[_SS].gpio_pin; //Low
        delayMicroseconds(1);
        shiftOut(_Data, _Clock, MSBFIRST, value);
        delayMicroseconds(1);
        PIN_MAP[_SS].gpio_peripheral->BSRR = PIN_MAP[_SS].gpio_pin; //High
        return 1;
    }
#endif

    //
    // Print Functions
    //

    size_t println(const String &v) {
        preprint();
        Print::println(v);
        Print::println("\x0dTRT");
    }

    size_t println(const char v[]) {
        preprint();
        Print::println(v);
        Print::println("\x0dTRT");
    }

    size_t println(char v) {
        preprint();
        Print::println(v);
        Print::println("\x0dTRT");
    }

    size_t println(unsigned char v, int base = DEC) {
        preprint();
        Print::println(v, base);
        Print::println("\x0dTRT");
    }

    size_t println(int v, int base = DEC) {
        preprint();
        Print::println(v, base);
        Print::println("\x0dTRT");
    }

    size_t println(unsigned int v, int base = DEC) {
        preprint();
        Print::println(v, base);
        Print::println("\x0dTRT");
    }

    size_t println(long v, int base = DEC) {
        preprint();
        Print::println(v, base);
        Print::println("\x0dTRT");
    }

    size_t println(unsigned long v, int base = DEC) {
        preprint();
        Print::println(v, base);
        Print::println("\x0dTRT");
    }

    size_t println(double v, int base = 2) {
        preprint();
        Print::println(v, base);
        Print::println("\x0dTRT");
    }

    size_t println(const Printable& v) {
        preprint();
        Print::println(v);
        Print::println("\x0dTRT");
    }

    size_t println(void) {
        Print::println("\x0dTRT");
    }


    size_t print(const String &v) {
        preprint();
        Print::println(v);
    }

    size_t print(const char v[]) {
        preprint();
        Print::println(v);
    }

    size_t print(char v) {
        preprint();
        Print::println(v);
    }

    size_t print(unsigned char v, int base = DEC) {
        preprint();
        Print::println(v, base);
    }

    size_t print(int v, int base = DEC) {
        preprint();
        Print::println(v, base);
    }

    size_t print(unsigned int v, int base = DEC) {
        preprint();
        Print::println(v, base);
    }

    size_t print(long v, int base = DEC) {
        preprint();
        Print::println(v, base);
    }

    size_t print(unsigned long v, int base = DEC) {
        preprint();
        Print::println(v, base);
    }

    size_t print(double v, int base = 2) {
        preprint();
        Print::println(v, base);
    }

    size_t print(const Printable& v) {
        preprint();
        Print::println(v);
    }

    //
    // Header Functions
    //

    void disableCursor(void) {
        Print::print("CS0");
    }

    void enableCursor(void) {
        Print::print("CS1");
    }

    void drawStr(uint8_t x, uint8_t y, const char *s) {
        Print::print("TP");
        write(x);
        write(y);
        Print::print("TT");
        Print::println(s);
    }

    void setPrintPos(uint8_t x, uint8_t y, bool graph = false) {
        if (graph == false) {
            Print::print("TP");
            write(x);
            write(y);
        } else {
            Print::print("GP");
            write(x);
            write(y);
        }
    }

    void clearScreen(void) {
        Print::print("CL");
    }

    void setLCDColRow(uint8_t col, uint8_t row) {
        Print::print("STCR");
        write(col);
        write(row);
        Print::print("\x80\xC0\x94\xD4");
    }

    void setI2CAddress(uint8_t add) {
        Print::print("SI2CA");
        write(add);
        _I2Caddress = add;
    }

    void displayConfig(uint8_t v) {
        Print::print("DC");
        write(v);
    }

    void displayStartScreen(uint8_t m) {
        Print::print("DSS");
        write(m);
    } //display start screen

    void setMode(uint8_t m) {
        Print::print("DM");
        write(m);
    } //set display mode

    void setTextPosBack(void) {
        Print::print("ETB");
    } //set text position back to previous, only one back allowed

    void setTextPosOffset(char xoffset, char yoffset) {
        Print::print("ETO");
        write(xoffset);
        write(yoffset);
    }

    void setTextPosAbs(uint8_t x, uint8_t y) {
        Print::print("ETP");
        write(x);
        write(y);
    }
    void setLinePattern(uint8_t pattern) {
        Print::print("SLP");
        write(pattern);
    }
    void setLCDChip(uint8_t chip) {      //only for universal LCD adapter
        Print::print("SLCD");
        write(chip);
    }

     void digitalOutput(uint8_t x) {
         Print::print("DOUT");
         write(x);
    }

    void preprint(void);
    //----------Functions for Graphic LCD/OLED adapters only---------
    //the functions in this section compatible with u8glib
    void drawBitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap);
    void drawBitmap256(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap);
    void drawBitmap262K(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap);
    void setTrueColor(uint8_t r, uint8_t g, uint8_t b);
    void setRot90(void);
    void setRot180(void);
    void setRot270(void);
    void undoRotation(void);
    void setRotation(uint8_t);
    void setContrast(uint8_t);
    void drawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
    void drawCircle(uint8_t x, uint8_t y, uint8_t r, uint8_t = 0);
    void drawDisc(uint8_t x, uint8_t y, uint8_t r);
    void drawFrame(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
    void drawPixel(uint8_t x, uint8_t y, uint8_t = 1);
    void drawLine(uint8_t x, uint8_t y, uint8_t x1, uint8_t y1);
    void drawLineTo(uint8_t x, uint8_t y);
    void drawHLine(uint8_t x, uint8_t y, uint8_t w);
    void drawVLine(uint8_t x, uint8_t y, uint8_t h);
    //-------------------------------
    //special functions for our adapters
    void setFont(uint8_t font); //set font, available: 6,10,18,51,120,123, user font: 200-203
    void nextTextLine(void); //got to next text line, depending on the font size
    void setColor(uint8_t); //set color for graphic function
    void backLightOn(void); //Turn on back light
    void backLightOff(void); //Turn off back light
    void directCommand(uint8_t d); //send command to LCD drectly
    void directData(uint8_t d); //send data to LCD drectly
    void moveArea(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, char xoffset, char yoffset); //move a area of screen to another place
    void uploadStartScreen(int lon, const unsigned char *data); //upload start screen
    void uploadUserFont(int lon, const unsigned char *data, uint8_t sect); //upload user font

private:
    uint8_t _I2Caddress;
    uint8_t _Clock;
    uint8_t _Data;
    uint8_t _SS;
    uint8_t _Comdelay;

};

//
// Digole.cpp
//

void DigoleSerialDisp::preprint(void) {
    //write((uint8_t)0);
    Print::print("TT");
}

//----------Functions for Graphic LCD/OLED adapters only---------
void DigoleSerialDisp::drawBitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap) {
    uint8_t i = 0;
    if ((w & 7) != 0)
        i = 1;
    Print::print("DIM");
    write(x);
    write(y);
    write(w);
    write(h);
    for (int j = 0; j < h * ((w >> 3) + i); j++) {
        write(bitmap[j]);
    }
}

void DigoleSerialDisp::setRot90(void) {
    Print::print("SD1");
}

void DigoleSerialDisp::setRot180(void) {
    Print::print("SD2");
}

void DigoleSerialDisp::setRot270(void) {
    Print::print("SD3");
}

void DigoleSerialDisp::undoRotation(void) {
    Print::print("SD0");
}

void DigoleSerialDisp::setRotation(uint8_t d) {
    Print::print("SD");
    write(d);
}

void DigoleSerialDisp::setContrast(uint8_t c) {
    Print::print("CT");
    write(c);
}

void DigoleSerialDisp::drawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    Print::print("FR");
    write(x);
    write(y);
    write(x + w);
    write(y + h);
}

void DigoleSerialDisp::drawCircle(uint8_t x, uint8_t y, uint8_t r, uint8_t f) {
    Print::print("CC");
    write(x);
    write(y);
    write(r);
    write(f);
}

void DigoleSerialDisp::drawDisc(uint8_t x, uint8_t y, uint8_t r) {
    drawCircle(x, y, r, 1);
}

void DigoleSerialDisp::drawFrame(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    Print::print("DR");
    write(x);
    write(y);
    write(x + w);
    write(y + h);
}

void DigoleSerialDisp::drawPixel(uint8_t x, uint8_t y, uint8_t color) {
    Print::print("DP");
    write(x);
    write(y);
    write(color);
}

void DigoleSerialDisp::drawLine(uint8_t x, uint8_t y, uint8_t x1, uint8_t y1) {
    Print::print("LN");
    write(x);
    write(y);
    write(x1);
    write(y1);
}

void DigoleSerialDisp::drawLineTo(uint8_t x, uint8_t y) {
    Print::print("LT");
    write(x);
    write(y);
}

void DigoleSerialDisp::drawHLine(uint8_t x, uint8_t y, uint8_t w) {
    drawLine(x, y, x + w, y);
}

void DigoleSerialDisp::drawVLine(uint8_t x, uint8_t y, uint8_t h) {
    drawLine(x, y, x, y + h);
}

void DigoleSerialDisp::nextTextLine(void) {
    write((uint8_t) 0);
    Print::print("TRT");
}

void DigoleSerialDisp::setFont(uint8_t font) {
    Print::print("SF");
    write(font);
}

void DigoleSerialDisp::setColor(uint8_t color) {
    Print::print("SC");
    write(color);
}

void DigoleSerialDisp::backLightOn(void) {
    Print::print("BL");
    write((uint8_t) 1);
}

void DigoleSerialDisp::backLightOff(void) {
    Print::print("BL");
    write((uint8_t) 0);
}

void DigoleSerialDisp::directCommand(uint8_t d) {
    Print::print("MCD");
    write(d);
}

void DigoleSerialDisp::directData(uint8_t d) {
    Print::print("MDT");
    write(d);
}

void DigoleSerialDisp::moveArea(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, char xoffset, char yoffset) {
    Print::print("MA");
    write(x0);
    write(y0);
    write(w);
    write(h);
    write(xoffset);
    write(yoffset);
}

void DigoleSerialDisp::uploadStartScreen(int lon, const unsigned char *data) {
    Print::print("SSS");
    write((uint8_t) (lon % 256));
    write((uint8_t) (lon / 256));
    delay(300);
    for (int j = 0; j < lon; j++) {
        if((j%32)==0) {
            delay(50);
            delay(_Comdelay);
        }

        write(data[j]);
    }
}

void DigoleSerialDisp::uploadUserFont(int lon, const unsigned char *data, uint8_t sect) {
    Print::print("SUF");
    write(sect);
    write((uint8_t) (lon % 256));
    write((uint8_t) (lon / 256));
    for (int j = 0; j < lon; j++) {
        if((j%32)==0) {
            delay(50);
            delay(_Comdelay);
        }

        write(data[j]);
    }
}

void DigoleSerialDisp::drawBitmap256(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap) {
    Print::print("EDIM1");
    write(x);
    write(y);
    write(w);
    write(h);
    for (int j = 0; j < h * w; j++) {
        if((j%1024)==0) {
            delay(_Comdelay);
        }
        write(bitmap[j]);
    }
}

void DigoleSerialDisp::drawBitmap262K(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap) {
    Print::print("EDIM3");
    write(x);
    write(y);
    write(w);
    write(h);
    for (int j = 0; (j < h * w * 3); j++) {
        if((j%1024)==0) {
            delay(_Comdelay);
        }
        write(bitmap[j]);
    }
}

void DigoleSerialDisp::setTrueColor(uint8_t r, uint8_t g, uint8_t b) {	//Set true color
    Print::print("ESC");
    write(r);
    write(g);
    write(b);
}

// **************************
// *** End Digole Library ***
// **************************

const unsigned char psyduck256[] = {

    0,0,0,0,0,0,0,0,144,0,0,0,0,0,0,0,0,0,0,0,0,132,136,136,136,136,136,136,136,136,136,136,136,136,136,0,0,0,0,144,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,0,0,0,0,0,0,136,136,136,136,136,136,136,136,136,136,132,96,140,172,176,172,172,172,172,172,172,172,172,172,172,136,100,136,136,136,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,0,0,0,144,0,0,96,132,132,132,132,100,136,136,136,136,100,96,172,212,244,244,244,244,244,244,244,244,244,240,204,140,104,136,136,104,0,0,144,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,0,0,0,0,136,96,172,172,208,176,176,176,172,140,140,172,176,172,208,244,248,248,248,248,248,248,248,248,248,244,240,204,172,172,172,136,68,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,0,0,0,0,136,132,172,212,244,244,244,248,212,140,136,212,244,244,244,248,248,248,248,248,248,248,248,248,244,244,240,208,208,240,208,140,100,68,0,0,144,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,0,0,0,0,136,132,208,244,248,248,248,248,212,176,176,244,248,248,248,248,248,248,248,248,248,248,248,248,244,244,240,240,240,240,240,208,140,136,68,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,0,0,0,0,136,132,176,244,248,248,244,176,172,212,244,248,248,248,248,248,248,248,248,248,248,248,248,248,248,248,244,244,240,240,240,240,208,140,100,68,0,0,144,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,0,0,0,0,136,100,176,244,248,244,212,172,176,244,248,248,248,248,248,248,248,248,248,248,248,248,248,248,248,248,248,244,240,240,240,240,240,208,140,136,104,136,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,0,144,0,0,96,96,172,244,244,176,176,212,248,248,248,248,248,248,248,248,248,248,248,248,248,248,248,248,248,248,248,244,240,240,240,240,240,240,208,172,136,136,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,0,0,136,100,140,172,208,248,244,140,136,212,244,212,244,248,248,248,248,248,248,248,248,248,248,248,248,244,212,212,212,208,208,208,208,240,240,240,240,172,136,136,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,0,0,136,132,172,212,248,248,244,172,96,136,172,136,176,244,248,248,248,248,248,248,248,248,248,248,244,176,136,140,136,136,136,132,172,240,244,240,208,172,104,100,0,0,144,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,0,0,136,100,208,248,248,244,212,177,178,178,178,178,177,212,244,248,248,248,248,248,248,248,248,244,212,177,178,178,178,178,178,178,177,208,208,240,240,204,140,136,104,136,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,144,0,0,96,96,176,248,248,172,177,219,255,255,255,255,219,177,172,244,248,248,248,248,248,248,244,172,177,219,255,255,255,255,255,255,219,173,168,240,240,240,208,172,136,136,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,0,0,0,32,64,140,172,208,212,212,177,182,255,255,255,255,255,255,182,177,212,244,248,248,248,248,244,212,177,182,255,255,255,255,255,255,255,255,182,173,208,240,240,240,204,136,136,0,0,0,0,0,0,0,0,0,0,0,0,0
    ,144,0,0,64,96,172,212,212,172,177,219,255,255,255,146,146,255,255,255,219,177,176,244,248,248,244,176,177,219,255,255,255,146,146,255,255,255,255,255,219,177,172,240,240,172,136,104,0,0,144,0,0,0,0,0,0,0,0,0,0
    ,0,136,96,140,140,212,248,244,136,178,255,255,255,255,146,146,255,255,255,255,178,132,208,244,244,208,132,178,255,255,255,255,146,146,255,255,255,255,255,255,178,132,204,208,172,136,136,100,128,0,0,0,0,0,0,0,0,0,0,0
    ,0,136,132,172,212,248,248,244,136,178,255,255,255,255,255,255,255,255,255,255,178,96,136,172,172,136,96,178,255,255,255,255,255,255,255,255,255,255,255,255,178,96,136,136,136,136,136,100,96,0,0,144,0,0,0,0,0,0,0,0
    ,0,136,132,208,244,248,248,212,172,141,219,255,255,255,255,255,255,255,255,219,177,172,172,172,172,172,172,177,219,255,255,255,255,255,255,255,255,255,255,219,177,172,172,140,136,136,176,172,172,96,136,0,0,0,0,0,0,0,0,0
    ,0,136,132,176,244,248,248,212,172,136,141,214,255,255,255,255,255,255,182,177,213,249,249,249,249,249,249,213,177,182,255,255,255,255,255,255,255,255,182,177,212,244,244,212,136,140,212,212,172,132,136,0,0,0,0,0,0,0,0,0
    ,0,136,100,176,244,248,248,244,172,136,132,177,219,255,255,255,255,219,177,176,249,253,253,253,253,253,253,249,176,177,219,255,255,255,255,255,255,219,177,172,212,248,248,244,176,176,248,248,208,132,136,0,0,0,0,0,0,0,0,0
    ,0,96,96,172,244,248,248,212,172,140,212,212,177,178,178,177,146,177,213,249,249,253,249,249,249,249,253,249,249,213,177,146,177,177,177,178,178,177,212,212,172,176,244,248,244,244,248,244,176,132,136,0,0,0,0,0,0,0,0,0
    ,132,140,172,208,244,248,248,212,140,172,248,244,172,96,136,172,136,176,249,253,249,249,249,249,249,249,249,249,253,249,176,136,172,172,172,136,96,172,244,248,176,172,244,248,248,248,248,244,176,100,132,0,0,0,0,0,0,0,0,0
    ,136,172,212,244,248,248,248,212,140,140,248,244,140,136,213,249,249,249,249,249,176,176,249,253,253,249,176,176,249,249,249,249,249,249,248,212,136,140,244,248,244,244,248,248,248,248,248,244,172,96,96,0,0,144,0,0,0,0,0,0
    ,136,176,244,248,248,248,248,244,172,172,212,212,176,177,249,253,253,253,253,249,140,140,249,249,249,213,172,176,249,253,253,253,253,253,249,248,176,176,212,244,248,248,248,248,248,248,248,244,208,172,140,100,136,0,0,0,0,0,0,0
    ,136,172,244,248,248,248,248,244,240,208,172,176,213,249,249,249,249,249,253,249,140,140,249,249,176,172,213,249,249,249,249,249,249,249,249,248,248,212,176,176,244,248,248,248,248,248,248,248,244,212,172,132,136,0,0,0,0,0,0,0
    ,136,172,244,248,248,248,248,244,244,208,136,140,249,253,249,249,249,249,249,249,176,176,249,249,176,176,249,253,249,249,249,249,249,249,249,248,249,248,140,136,212,248,248,248,248,248,248,248,248,244,208,100,136,0,0,0,0,0,0,0
    ,136,172,244,248,248,248,248,244,244,208,136,140,249,253,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,248,248,249,248,140,140,244,248,248,248,248,248,248,248,248,244,176,100,96,0,0,144,0,0,0,0
    ,136,172,244,248,248,248,248,244,244,208,172,176,213,249,249,249,249,249,249,249,253,253,249,249,253,253,249,249,249,249,249,249,249,248,248,248,248,212,176,172,212,244,248,248,248,248,248,248,248,244,208,136,136,104,136,0,0,0,0,0
    ,136,172,244,248,248,248,248,244,240,240,208,208,172,177,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,248,249,248,176,172,212,212,172,176,244,248,248,248,248,244,244,240,240,208,172,136,136,0,0,0,0,0
    ,136,172,244,248,248,248,248,244,244,240,244,208,136,140,249,253,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,248,248,212,172,140,212,208,136,140,212,248,248,248,248,244,240,240,240,240,172,136,136,0,0,0,0,0
    ,136,172,244,248,248,248,248,248,248,244,244,208,136,172,249,253,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,248,176,176,212,208,176,140,136,172,212,248,248,248,248,244,244,240,240,208,172,104,100,0,0,144,0,0
    ,136,172,244,248,248,248,248,248,248,244,244,208,136,172,249,253,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,244,140,136,208,212,172,136,136,172,244,248,248,248,248,244,244,240,240,208,204,140,136,104,136,0,0,0
    ,136,176,244,248,248,248,248,248,248,244,244,208,132,140,249,253,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,244,172,100,140,172,208,208,136,172,212,248,248,248,248,244,244,240,240,240,240,208,172,136,136,0,0,0
    ,136,172,212,248,248,248,248,248,248,244,240,204,172,177,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,248,208,140,136,172,240,240,168,172,212,248,248,248,248,244,244,240,240,240,240,240,204,136,136,0,0,0
    ,100,140,140,212,248,248,248,248,248,244,172,172,213,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,212,140,168,208,208,168,172,212,248,248,248,248,244,244,240,240,240,240,208,172,136,136,0,0,0
    ,0,64,96,140,212,244,248,248,244,212,176,177,249,253,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,248,172,136,208,208,168,172,212,248,248,248,248,244,244,240,240,240,240,208,172,136,136,0,0,0
    ,0,32,64,140,140,208,244,244,176,176,213,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,244,172,136,208,208,168,172,212,248,248,248,244,240,240,240,240,240,240,208,172,136,136,0,0,0
    ,144,0,0,64,96,172,244,244,136,140,249,253,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,244,172,136,208,208,136,172,244,248,248,244,240,240,240,240,240,240,240,208,172,136,136,0,0,0
    ,144,0,0,64,96,172,248,244,136,140,249,253,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,248,140,136,208,208,136,140,244,248,244,240,240,240,240,240,240,240,240,208,172,136,136,0,0,0
    ,0,136,100,140,172,208,212,212,176,177,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,212,172,172,240,240,172,172,212,244,240,240,240,240,240,240,240,240,240,208,172,136,136,0,0,0
    ,0,136,132,172,212,212,176,176,213,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,248,176,172,208,240,240,240,240,208,172,172,208,240,240,240,240,240,240,240,240,208,172,136,136,0,0,0
    ,0,136,132,208,248,244,140,140,249,253,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,244,140,136,208,240,240,240,240,208,136,136,208,240,240,240,240,240,240,240,240,208,172,136,136,0,0,0
    ,0,136,132,176,244,244,136,140,249,253,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,248,140,136,208,240,240,240,240,208,136,136,208,240,240,240,240,240,240,240,240,240,204,136,136,0,0,0
    ,0,136,100,176,244,244,176,176,213,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,212,172,172,208,240,240,240,240,208,172,172,208,208,240,240,240,240,240,240,240,208,172,136,136,0,0,0
    ,0,96,96,172,244,248,244,212,172,176,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,249,176,172,208,240,240,240,240,240,240,240,240,208,172,172,208,240,240,240,240,240,208,140,136,0,0,0,0,0
    ,132,140,172,208,244,248,248,244,172,172,212,248,244,248,249,249,249,249,249,249,249,249,249,249,249,249,249,249,248,244,248,212,172,168,208,240,240,240,240,240,240,240,240,208,172,172,208,208,208,208,240,208,140,100,104,104,136,136,136,136
    ,136,172,212,244,248,248,248,248,244,212,176,140,140,176,249,249,249,249,249,249,249,249,249,249,249,249,249,249,176,140,140,172,208,208,240,240,240,240,240,240,240,240,240,240,208,208,172,140,172,172,172,140,136,100,136,136,136,136,136,136
    ,136,176,244,248,248,248,248,248,248,244,176,136,136,176,212,248,244,244,244,244,244,244,244,244,244,244,248,212,176,140,140,172,240,244,240,240,240,240,240,240,240,240,240,240,240,240,172,136,136,136,172,172,136,172,172,204,172,172,172,136
    ,136,172,244,248,248,248,248,248,248,248,244,212,244,212,176,172,172,172,172,172,172,172,172,172,172,172,172,176,212,244,212,244,244,244,244,240,240,240,240,240,240,240,240,240,240,240,240,208,136,140,208,208,172,172,208,240,240,208,172,136
    ,136,172,244,248,248,248,248,248,248,248,248,248,248,244,172,96,100,136,136,136,136,136,136,136,136,100,96,172,244,248,248,248,248,244,244,240,240,240,240,240,240,240,240,240,240,240,240,208,172,172,240,240,172,172,208,244,244,208,172,136

};

DigoleSerialDisp mydisp(SS);

int random(int maxRand) {
    return rand() % maxRand;
}

const unsigned char welcomeimage[] = {
  0, 0, 0, 0, 0, 127
    , 0, 8, 1, 2, 0, 127
    , 0, 8, 0, 148, 0, 127
    , 0, 16, 0, 89, 112, 127
    , 3, 144, 0, 16, 144, 127
    , 28, 145, 192, 16, 144, 127
    , 1, 30, 128, 80, 144, 127
    , 9, 49, 3, 208, 144, 127
    , 5, 72, 0, 80, 144, 127
    , 2, 72, 0, 150, 144, 127
    , 3, 8, 0, 152, 208, 127
    , 5, 24, 0, 64, 160, 127
    , 4, 148, 0, 64, 128, 127
    , 8, 36, 0, 128, 128, 127
    , 16, 34, 3, 240, 128, 127
    , 32, 65, 0, 14, 0, 127
    , 0, 129, 128, 1, 252, 127
    , 3, 0, 64, 0, 0, 127
    , 0, 0, 0, 0, 0, 127
    , 0, 0, 0, 0, 0, 127
    , 0, 0, 0, 0, 0, 127
};
int ptr;
const char a[] = "disp char array";
const char b = 'Q';
int c = 3456;
String d = "I'm a string";
float pi = 3.1415926535;
double lg10;
const unsigned char fonts[] = {
  6, 10, 18, 51, 120, 123};
const char *fontdir[] = {
  "0\xb0", "90\xb0", "180\xb0", "270\xb0"};

void resetpos1(void) //for demo use, reset display position and clean the demo line
{
  mydisp.setPrintPos(0, 0, _TEXT_);
  delay(1500); //delay 2 seconds
  mydisp.println("                "); //display space, use to clear the demo line
  mydisp.setPrintPos(0, 0, _TEXT_);
}
void resetpos(void) //for demo use, reset display position and clean the demo line
{
  mydisp.setPrintPos(0, 1, _TEXT_);
  delay(1500); //delay 2 seconds
  mydisp.println("                "); //display space, use to clear the demo line
  mydisp.setPrintPos(0, 1, _TEXT_);
}

void setup() {
//  delay(10);  //waiting for display model ready
  mydisp.begin();
  mydisp.clearScreen(); //CLear screen
  //----------Special function for color OLED ------------
  mydisp.print("Spark Core Digole Graphic Color OLED Demo");
  delay(2000);
  resetpos1();
  mydisp.clearScreen();
  mydisp.setMode('C'); //set graphic Drawing Mode to COPY
  mydisp.print("Draw a 256 Color Psyduck!");
  mydisp.drawBitmap256(60, 39, 60, 50, psyduck256);  //use our image convert tool to convert, www.digole.com/tools
  //----------for text LCD adapter and graphic LCD adapter ------------
  resetpos1();
  mydisp.clearScreen();
  mydisp.setMode('C'); //set graphic Drawing Mode to COPY
//  for(unsigned int i=0;i<65535;i++)
//  {
//    mydisp.Print::print(random(255));
//  }
  //mydisp.displayConfig(1);    //set config display ON, 0=off
  //mydisp.setI2CAddress(0x29);  //this function only working when you connect using I2C, from 1 to 127
  //delay(1000);
  //mydisp.setLCDColRow(16,2);  //set LCD Col and Row, only time set up is OK
mydisp.displayConfig(0);
mydisp.disableCursor(); //disable cursor, enable cursore use: enableCursor();
  mydisp.drawStr(2, 0, "Demo TEXT now"); //display string at: x=4, y=0
  //Test print function
  mydisp.setColor(random(254)+1);
  mydisp.setPrintPos(0, 1, _TEXT_);
  mydisp.print(a); // display a char array
  resetpos();
  mydisp.setColor(random(254)+1);
  mydisp.print("display a char:");
  mydisp.setColor(random(254)+1);
  mydisp.print(b); //display a char
  resetpos();
  mydisp.setColor(random(254)+1);
  mydisp.print("int as DEC:");
  mydisp.setColor(random(254)+1);
  mydisp.print(c); //display 3456 in Dec
  resetpos();
  mydisp.setColor(random(254)+1);
  mydisp.print("as HEX:");
  mydisp.setColor(random(254)+1);
  mydisp.print(c, HEX); //display 3456 in Hex
  resetpos();
  mydisp.setColor(random(254)+1);
  mydisp.print("as OCT:");
  mydisp.setColor(random(254)+1);
  mydisp.print(c, OCT); //display 3456 in Oct
  resetpos();
  mydisp.setColor(random(254)+1);
  mydisp.print("BIN:");
  mydisp.setColor(random(254)+1);
  mydisp.print(c, BIN); //display 3456 in Bin
  resetpos();
  mydisp.setColor(random(254)+1);
  mydisp.print("float pi=");
  mydisp.setColor(random(254)+1);
  mydisp.print(pi); //display float of PI
  resetpos();
  mydisp.setColor(random(254)+1);
  mydisp.print("Pi6=");
  mydisp.setColor(random(254)+1);
  mydisp.print(pi, 6); //display PI in 6 Accuracy
  resetpos();
  mydisp.setColor(random(254)+1);
  mydisp.print("Pi*3=");
  mydisp.setColor(random(254)+1);
  mydisp.print(pi * 3, 6); //display PI time 3 in 6 Accuracy
  resetpos();
  mydisp.setColor(random(254)+1);
  mydisp.print("lg5=");
  mydisp.setColor(random(254)+1);
  mydisp.print(log(5), 8); //display log(5) in 8 Accuracy
  resetpos();
  mydisp.setColor(random(254)+1);
  mydisp.print(d); //display String object
  resetpos();
  for (uint8_t j = 0; j < 2; j++) //making "Hello" string moving
  {
    for (uint8_t i = 0; i < 10; i++) //move string to right
    {
      mydisp.setPrintPos(i, 1, _TEXT_);
      mydisp.setColor(random(254)+1);
      mydisp.print(" Hello ");
      delay(100); //delay 100ms
    }
    for (uint8_t i = 0; i < 10; i++) //move string to left
    {
      mydisp.setPrintPos(9 - i, 1, _TEXT_);
      mydisp.setColor(random(254)+1);
      mydisp.print(" Hello ");
      delay(100);
    }
  }
  mydisp.setColor(random(254)+1);
  mydisp.print("Enjoy it!");
  mydisp.enableCursor(); //enable cursor
  /*---------for Graphic LCD adapter only-------/
  mydisp.disableCursor(); //enable cursor
  resetpos();
  mydisp.setColor(random(254)+1);
  mydisp.drawStr(0, 1, "Negative/flash");
  mydisp.setMode('~');
  for (uint8_t i = 0; i < 10; i++) {
    mydisp.drawBox(0, 13, 110, 13);
    delay(500);
  }
  resetpos();
  mydisp.clearScreen();
  mydisp.setColor(random(254)+1);
  mydisp.drawStr(0, 0, "Draw image in 4 dir");
  //display image in 4 different directions, use setRot90();setRot180();setRot270();undoRotation();setRotation(dir);
  for (uint8_t i = 0; i < 4; i++) {
    mydisp.setRotation(i);
    mydisp.setColor(random(254)+1);
    mydisp.drawBitmap(12, 12, 41, 21, welcomeimage);
  }
  mydisp.undoRotation();
  resetpos1();
  mydisp.clearScreen();
  mydisp.setColor(random(254)+1);
  mydisp.drawStr(0, 0, "draw a box");
  mydisp.setColor(random(254)+1);
  mydisp.drawBox(20, 15, 30, 40);
  //test drawing a circle
  resetpos1();
  mydisp.clearScreen();
  mydisp.setColor(random(254)+1);
  mydisp.drawStr(0, 0, "draw a circle and a pie");
  mydisp.setColor(random(254)+1);
  mydisp.drawCircle(31, 31, 30);
  mydisp.setColor(random(254)+1);
  mydisp.drawCircle(31, 31, 14, 1);
  //test drawing a Frame
  resetpos1();
  mydisp.clearScreen();
  mydisp.setColor(random(254)+1);
  mydisp.drawStr(0, 0, "draw a Frame");
  mydisp.setColor(random(254)+1);
  mydisp.drawFrame(20, 15, 30, 40);
  //Clear an area use set color =0
  resetpos1();
  mydisp.clearScreen();
  mydisp.drawStr(0, 0, "Clear an area by using set color=0");
  mydisp.setColor(random(254)+1);
  mydisp.drawCircle(35, 70, 35, 1);
  mydisp.setColor(random(254)+1);
  mydisp.drawBox(80, 40, 80, 60);
  mydisp.setMode('C'); //set graphic Drawing Mode to COPY
  mydisp.setColor(0);
  mydisp.drawCircle(110, 70, 25, 1);
  mydisp.drawBox(20, 50, 40, 30);
  //test drawing Pixels
  resetpos1();
  mydisp.clearScreen();
  mydisp.setColor(random(254)+1);
  mydisp.drawStr(0, 0, "draw Pixels");
  for (uint8_t i = 0; i < 20; i++) {
    mydisp.setColor(random(254)+1);
    mydisp.drawPixel(random(SC_W-1), 12 + random(SC_H-13));
  }
  //test drawing Lines
  resetpos1();
  mydisp.clearScreen();
  mydisp.setColor(random(254)+1);
  mydisp.drawStr(0, 0, "draw Lines");
  for (uint8_t i = 0; i < 10; i++) {
    mydisp.setColor(random(254)+1);
    mydisp.drawLine(random(SC_W-1), 12 + random(SC_H-13), random(SC_W-1), 12 + random(SC_H-13));
  }
  //test drawing Lines to
  resetpos1();
  mydisp.clearScreen();
  mydisp.setColor(random(254)+1);
  mydisp.drawStr(0, 0, "draw Lines to");
  mydisp.setColor(random(254)+1);
  mydisp.drawLine(random(SC_W-1), 12 + random(SC_H-13), random(SC_W-1), 12 + random(SC_H-13));
  for (uint8_t i = 0; i < 10; i++) {
    mydisp.setColor(random(254)+1);
    mydisp.drawLineTo(random(SC_W-1), 12 + random(SC_H-13));
  }
  //test differnt font
  resetpos1();
  mydisp.clearScreen();
  mydisp.setColor(random(254)+1);
  mydisp.drawStr(0, 0, "default font");
  mydisp.setFont(fonts[0]);
  mydisp.nextTextLine();
  mydisp.setColor(random(254)+1);
  mydisp.print("Font 6");
  mydisp.setFont(fonts[1]);
  mydisp.nextTextLine();
  mydisp.setColor(random(254)+1);
  mydisp.print("Font 10");
  mydisp.setFont(fonts[2]);
  mydisp.nextTextLine();
  mydisp.setColor(random(254)+1);
  mydisp.print("Font 18");
  mydisp.setFont(fonts[3]);
  mydisp.nextTextLine();
  mydisp.setColor(random(254)+1);
  mydisp.print("Font 51");
  resetpos1();
  mydisp.clearScreen();
  mydisp.setFont(fonts[4]);
  mydisp.setPrintPos(0, 0, _TEXT_);
  mydisp.setColor(random(254)+1);
  mydisp.print("Font 120");
  mydisp.setFont(fonts[5]); //font 123 only have chars of space and 1 to 9
  mydisp.setColor(random(254)+1);
  mydisp.nextTextLine();
  mydisp.print("123");
  resetpos1();
  mydisp.clearScreen();
  //User font using number: 200,201,202,203,only available after upload to adapter
  mydisp.setFont(10);
  mydisp.setColor(random(254)+1);
  mydisp.drawStr(0, 0, "User font using:200,201,202,203,available after upload to adapter,16Kb space.");
  //move area on screen
  resetpos1();
  mydisp.clearScreen();
  mydisp.setColor(random(254)+1);
  mydisp.print("move area on screen");
  mydisp.setColor(random(254)+1);
  mydisp.drawCircle(31, 31, 14, 1);
  delay(2000);
  mydisp.moveArea(31, 31, 15, 15, 5, 8);
  //set display mode, affect all graphic functions, the available mode: "|"-or,"&"-and,"^"-xor,"!"-not,"C"-copy
  resetpos1();
  mydisp.clearScreen();
  mydisp.setColor(random(254)+1);
  mydisp.print("use xor or not mode to show a Circle Animation");
  mydisp.setMode('^');
  for (uint8_t i = 1; i < 31; i++) {
    mydisp.setColor(random(254)+1);
    mydisp.drawCircle(31, 31, i); //draw a circle
    delay(50);
    mydisp.drawCircle(31, 31, i); //disappear the previos draw
  }
  //this section show how to use enhanced TEXT position functions:
  //setTextPosBack(),setTextPosOffset() and setTextPosAbs() to make some thing fun
  resetpos1();
  mydisp.clearScreen();
  mydisp.setMode('|');
  mydisp.setColor(random(254)+1);
  mydisp.print("Bold D");
  mydisp.setFont(120);
  mydisp.nextTextLine();
  mydisp.setColor(random(254)+1);
  mydisp.print('D');
  mydisp.setTextPosBack(); //set text position back
  delay(2000);
  mydisp.setTextPosOffset(2, 0); //move text position to x+2
  mydisp.setColor(random(254)+1);
  mydisp.print('D'); //display D again, under or mode, it will bold 'D'
  //use setTextPosAbs() to make a Animation letter
  resetpos1();
  mydisp.clearScreen();
  mydisp.setColor(random(254)+1);
  mydisp.print("make > animation");
  mydisp.setFont(51);
  mydisp.setMode('C');
  for (uint8_t i = 0; i < SC_W-20; i++) {
    mydisp.setTextPosAbs(i, 45);
    mydisp.setColor(random(254)+1);
    mydisp.print('>');
    delay(5);
  }
  //show font in different direction
  resetpos1();
  mydisp.clearScreen();
  mydisp.setFont(fonts[1]);
  mydisp.setColor(random(254)+1);
  mydisp.print("show font in different direction");
  resetpos1();
  mydisp.clearScreen();
  for (uint8_t i = 0; i < 4; i++) {
    mydisp.setRotation(i);
    mydisp.setFont(fonts[2]);
    mydisp.setPrintPos(0, 0);
    for (uint8_t j = 0; j < 3; j++) {
      mydisp.setFont(fonts[2 - j]);
      if (j != 0)
        mydisp.nextTextLine();
      mydisp.setColor(random(254)+1);
      mydisp.print(fontdir[i]);
    }
  }
  resetpos1();
  mydisp.clearScreen();
  mydisp.setMode('C'); //set graphic Drawing Mode to COPY
  mydisp.setColor(random(254)+1);
  mydisp.drawHLine(0, SC_H/2-1, SC_W-1); //draw horizontal LiNe
  mydisp.setColor(random(254)+1);
  mydisp.setPrintPos(0, SC_H/2-1, 1); //Set Graphic position
  for (uint8_t i = 1; i <= SC_W-1; i = i + 6) //this loop will draw sin graph
  {
    mydisp.setColor(random(254)+1);
    mydisp.drawLineTo(i, (uint8_t) (SC_H/2 - (float) (sin(i * 3.14 / (SC_W/2-1))*(SC_H/2))));
  }
}

void loop() {
}

*/
