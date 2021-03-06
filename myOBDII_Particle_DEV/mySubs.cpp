#include "application.h"
#include "myQueue.h"
#include "mySubs.h"

extern char       rxIndex;
extern int        verbose;

// Simple OLED print
void  display(MicroOLED* oled, const uint8_t x, const uint8_t y, const String str,\
   const int hold, const ClearType clear, const FontType type, const uint8_t clearA)
{
  Serial.println(str);
  if (clearA) oled->clear(ALL);
  if (clear) oled->clear(PAGE);
  oled->setFontType(type);
  oled->setCursor(x, y*oled->getFontHeight());
  oled->print(str);
  oled->display();
  delay(hold);
}

// Simple OLED print
void  displayStr(MicroOLED* oled, const uint8_t x, const uint8_t y, const String str, \
  const int hold, const ClearType clear, const FontType type, const uint8_t clearA)
{
  Serial.print(str);
  if (clearA) oled->clear(ALL);
  if (clear) oled->clear(PAGE);
  oled->setFontType(type);
  oled->setCursor(x, y*oled->getFontHeight());
  oled->print(str);
  oled->display();
  delay(hold);
}

// Get and display engine codes
void  getCodes(MicroOLED* oled, const String cmd, unsigned long faultTime, char* rxData, uint8_t *ncodes, unsigned long codes[100], unsigned long activeCode[100], Queue *F)
{
  if ( faultTime<1454540170 || faultTime>1770159369 )  // Validation;  time on 03-Feb-2016 and 03-Feb-2026
  {
    Serial.printf("getCodes:  bad time = %u\n", faultTime);
    return;
  }
  if ( ping(oled, cmd, rxData) == 0 ) // success
  {
    int nActive = parseCodes(rxData, codes, ncodes);
    for ( int i=0; i<nActive; i++ )
    {
      F->newCode(faultTime, codes[i]);
      activeCode[i] = codes[i];
      //displayStr(oled, 0, 1, "getCodes:" + String(activeCode[i]));
      if ( i<nActive-1 ) displayStr(oled, 0, 2, ",");
      delay(1000);
    }
    display(oled, 0, 1, "");
    delay(1000);
  }
}

// Get and display jumper codes
void  getJumpFaultCodes(MicroOLED* oled, const String cmd, const String val, unsigned long faultTime, char* rxData, uint8_t *ncodes, unsigned long codes[100], unsigned long activeCode[100], const bool ignoring, Queue *F)
{
  pingJump(oled, cmd, val, rxData);
  int nActive = parseCodes(rxData, codes, ncodes);
  for ( int i=0; (i<nActive&&!ignoring); i++ )
  {
    F->newCode(faultTime, codes[i]);
    if ( verbose>2 )  F->Print();
    delay(1000);
  }
  display(oled, 0, 2, "");
}

//The getResponse function collects incoming data from the UART into the rxData buffer
// and only exits when a carriage return character is seen. Once the carriage return
// string is detected, the rxData buffer is null terminated (so we can treat it as a string)
// and the rxData index is reset to 0 so that the next string can be copied.
int   getResponse(MicroOLED* oled, char* rxData)
{
  char inChar=0;
  if ( verbose>4 )
  {
    Serial.printf("Rx:");
  }
  else delay(150);
  //Keep reading characters until we get a carriage return
  bool  notFound  = true;
  int   count     = 0;
  while (notFound && rxIndex<100 && ++count<100)
  {
    if(Serial1.available() > 0)
    {
      if(Serial1.peek() == '\r'){
        inChar  = Serial1.read();    // Clear buffer
        rxData[rxIndex]='\0';
        rxIndex = 0;                // Reset the buffer for next pass
        if ( verbose>4 ){
          Serial.printf(";\n");
        }
        else delay(150);
        notFound = false;
      }
      else    // New char
      {
        inChar = Serial1.read();
        if      (isspace(inChar)) continue;   // Strip line feeds left from previous \n-\r
        else if (inChar == '>')   continue;   // Strip prompts
        else if (inChar == '\0')  continue;   // Strip delimiters
        else rxData[rxIndex++] = inChar;
        if ( verbose>4 ){
          if ( verbose>5 ) Serial.printf("[");
          Serial.printf("%c", inChar);
          if ( verbose>5 ) Serial.printf("]");
        }
        else delay(150);
      }
    }
    else{   // !available
      if ( verbose>5 )
      {
        Serial.printf(".");
      }
      else delay(150);
    }
  }
  return (notFound);
}

// Parse the OBD-II long string of codes returned by UART.
int   parseCodes(const char *rxData, unsigned long *codes, uint8_t *ncodes)
{
    int n = strlen(rxData);
    if ( verbose>4 ) Serial.printf("rxData[%d]=%s\n", n, rxData);
    if ( n < 8 )
    {
      *ncodes = 0;
      return(0);
    }
    char numC[3];
    numC[0] = rxData[2];
    numC[1] = rxData[3];
    numC[2] = '\0';
    *ncodes = strtol(numC, NULL, 10);
    if ( *ncodes*4>(n-4) || *ncodes>100 )
    {
      *ncodes = 0;
      return(0);
    }
    int i = 0;
    int j = 4;
    char C[5];
    if ( verbose>4 ) Serial.printf("parseCodes: codes[%d]=", *ncodes);
    while ( i < *ncodes )
    {
      C[0] = rxData[j++];
      C[1] = rxData[j++];
      C[2] = rxData[j++];
      C[3] = rxData[j++];
      C[4] = '\0';
      unsigned long newCode = strtol(C, NULL, 10);
      if ( newCode>0 && newCode<3500 )  // Validation
      {
        codes[i++] = newCode;
        if ( verbose>4 ) Serial.printf("%ld,", codes[i-1]);
      }
      else
      {
        *ncodes--;
        Serial.printf("[rejecting bad code %ld],", newCode);
      }
    }
    if ( verbose>4 && *ncodes>0 ) Serial.printf("\n");
    return(*ncodes);
}


// Boilerplate driver
int   ping(MicroOLED* oled, const String cmd, char* rxData)
{
  int notConnected = rxFlushToChar(oled, '>');
  if (verbose>3) Serial.println("Tx:" + cmd);
  else delay(150);
  Serial1.println(cmd + '\0');
  notConnected = rxFlushToChar(oled, '\r')  || notConnected;
  notConnected = getResponse(oled, rxData)  || notConnected;
  if (notConnected)
  {
    display(oled, 0, 0, "No conn>", 0, page, font8x16);
    int count = 0;
    while (!Serial.available() && count++<5) delay(1000);
    //while (!Serial.read()); // Blocking read
  }
  else if ( strstr(rxData, "NODATA") ) notConnected = 1;
  return (notConnected);
}

// boilerplate jumper driver
int   pingJump(MicroOLED* oled, const String cmd, const String val, char* rxData)
{
  if (verbose>3) Serial.println("Tx:" + cmd);
  delay(500);
  Serial1.println(cmd);
  delay(500);
  int notConnected = rxFlushToChar(oled, '\r');
  delay(500);
  if (verbose>3) Serial.println("Tx:" + val);
  Serial1.println(val);
  delay(500);
  notConnected = getResponse(oled, rxData) || notConnected;
  if (notConnected)
  {
    display(oled, 0, 0, "No conn>", 0, page, font8x16);
    int count = 0;
    while (!Serial.available() && count++<5) delay(1000);
    //while (!Serial.read());   // Blocking read
  }
  delay(500);
  return(notConnected);
}

// Boilerplate driver
void  pingReset(MicroOLED* oled, const String cmd)
{
  int notConnected = rxFlushToChar(oled, '>');
  if (notConnected)
  {
    display(oled, 0, 0, "No conn>", 0, page, font8x16);
    int count = 0;
    while (!Serial.available() && count++<5) delay(1000);
    //while (!Serial.read());  // Blocking read
  }
  if (verbose>3) Serial.println("Tx:" + cmd);
  Serial1.println(cmd + '\0');
}


// Spin until pchar, 0 if found, 1 if fail
int   rxFlushToChar(MicroOLED* oled, const char pchar)
{
  char inChar=0;
  if ( verbose>4 )
  {
    Serial.printf("Rx:");
  }
  else delay(150);
  //Keep reading characters until we get a carriage return
  bool notFound = true;
  int count = 0;
  while (notFound && rxIndex<100 && ++count<100)
  {
    if(Serial1.available() > 0)
    {
      if(Serial1.peek() == pchar){
        inChar  = Serial1.read();    // Clear buffer
        if ( verbose>4 )
        {
          Serial.printf("%c;\n", inChar);
        }
        else delay(150);
        notFound = false;
      }
      else    // New char
      {
        inChar = Serial1.read();
        if      (isspace(inChar)) continue;   // Strip line feeds left from previous \n-\r
        else if (inChar == '\0')  continue;   // Strip delimiters
        else if ( verbose>4 )
        {
          if ( verbose>5 ) Serial.printf("<");
          Serial.printf("%c", inChar);
          if ( verbose>5 ) Serial.printf(">");
        }
        else delay(150);
      }
    }
    else
    {   // !available
      if ( verbose>5 )
      {
        Serial.printf(",");
      }
      else delay(150);
    }
  }
  return (notFound);
}
