#include "application.h"
#include "myQueue.h"
#include "mySubs.h"

extern uint8_t    ncodes;
extern char       rxIndex;
extern int        verbose;

// Simple OLED print
void  display(MicroOLED* oled, const uint8_t x, const uint8_t y, const String str,\
   const uint8_t clear, const uint8_t type, const uint8_t clearA)
{
  Serial.println(str);
  if (clearA) oled->clear(ALL);
  if (clear) oled->clear(PAGE);
  oled->setFontType(type);
  oled->setCursor(x, y*oled->getFontHeight());
  oled->print(str);
  oled->display();
}

// Simple OLED print
void  displayStr(MicroOLED* oled, const uint8_t x, const uint8_t y, const String str, \
  const uint8_t clear, const uint8_t type, const uint8_t clearA)
{
  Serial.print(str);
  if (clearA) oled->clear(ALL);
  if (clear) oled->clear(PAGE);
  oled->setFontType(type);
  oled->setCursor(x, y*oled->getFontHeight());
  oled->print(str);
  oled->display();
}

// Get and display jumper codes
void  getJumpFaultCodes(MicroOLED* oled, const String cmd, const String val, unsigned long faultTime, char* rxData, long codes[100], long activeCode[100], const bool ignoring, Queue *F)
{
  pingJump(oled, cmd, val, rxData);
  int nActive = parseCodes(rxData, codes);
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
    oled->setFontType(0);
    oled->setCursor(0, oled->getFontHeight());
    oled->print("Rx:");
    oled->display();
  }
  else delay(100);
  //Keep reading characters until we get a carriage return
  bool  notFound  = true;
  int   count     = 0;
  while (notFound && rxIndex<20 && ++count<45)
  {
    if(Serial1.available() > 0)
    {
      if(Serial1.peek() == '\r'){
        inChar  = Serial1.read();    // Clear buffer
        rxData[rxIndex]='\0';
        rxIndex = 0;                // Reset the buffer for next pass
        if ( verbose>4 ){
          Serial.printf(";\n");
          oled->printf(";");
          oled->display();
        }
        else delay(100);
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
          oled->printf("%c", inChar);
          oled->display();
        }
        else delay(100);
      }
    }
    else{   // !available
      if ( verbose>5 )
      {
        Serial.printf(".");
        oled->printf(".");
        oled->display();
      }
      else delay(100);
    }
  }
  return (notFound);
}

// Parse the OBD-II long string of codes returned by UART.
int   parseCodes(const char *rxData, long *codes)
{
    int n = strlen(rxData);
    if ( verbose>4 ) Serial.printf("rxData[%d]=%s\n", n, rxData);
    if ( n < 8 )
    {
      ncodes = 0;
      return(0);
    }
    char numC[3];
    numC[0] = rxData[2];
    numC[1] = rxData[3];
    numC[2] = '\0';
    ncodes = strtol(numC, NULL, 10);
    if ( ncodes*4>(n-4) || ncodes>100 )
    {
      ncodes = 0;
      return(0);
    }
    int i = 0;
    int j = 4;
    char C[5];
    if ( verbose>4 ) Serial.printf("codes[%d]=", ncodes);
    while ( i < ncodes )
    {
      C[0] = rxData[j++];
      C[1] = rxData[j++];
      C[2] = rxData[j++];
      C[3] = rxData[j++];
      C[4] = '\0';
      codes[i++] = strtol(C, NULL, 10);
      if ( verbose>4 ) Serial.printf("%ld,", codes[i-1]);
    }
    if ( verbose>4 && ncodes>0 ) Serial.printf("\n");
    return(ncodes);
}


// Boilerplate driver
int   ping(MicroOLED* oled, const String cmd, char* rxData)
{
  int notConnected = rxFlushToChar(oled, '>');
  if (verbose>3) display(oled, 0, 0, "Tx:" + cmd, 1);
  else delay(100);
  Serial1.println(cmd + '\0');
  notConnected = rxFlushToChar(oled, '\r')  || notConnected;
  notConnected = getResponse(oled, rxData)  || notConnected;
  if (notConnected)
  {
    display(oled, 0, 0, "No conn>", 1, 1);
    int count = 0;
    while (!Serial.available() && count++<5) delay(1000);
    while (!Serial.read());
  }
  return (notConnected);
}

// boilerplate jumper driver
int   pingJump(MicroOLED* oled, const String cmd, const String val, char* rxData)
{
  if (verbose>3) display(oled, 0, 0, "Tx:" + cmd, 1);
  delay(1000);
  Serial1.println(cmd);
  delay(1000);
  //int notConnected = getResponse();
  int notConnected = rxFlushToChar(oled, '\r');
  delay(1000);
  if (verbose>3) display(oled, 0, 0, "Tx:" + val, 1);
  Serial1.println(val);
  delay(1000);
  notConnected = getResponse(oled, rxData) || notConnected;
  if (notConnected)
  {
    display(oled, 0, 0, "No conn>", 1, 1);
    int count = 0;
    while (!Serial.available() && count++<5) delay(1000);
    while (!Serial.read());
  }
  delay(1000);
  return(notConnected);
}

// Boilerplate driver
void  pingReset(MicroOLED* oled, const String cmd)
{
  int notConnected = rxFlushToChar(oled, '>');
  if (notConnected)
  {
    display(oled, 0, 0, "No conn>", 1, 1);
    int count = 0;
    while (!Serial.available() && count++<5) delay(1000);
    while (!Serial.read());  // Blocking read
  }
  if (verbose>3) display(oled, 0, 0, "Tx:" + cmd, 1);
  Serial1.println(cmd + '\0');
}


// Spin until pchar, 0 if found, 1 if fail
int   rxFlushToChar(MicroOLED* oled, const char pchar)
{
  char inChar=0;
  if ( verbose>4 )
  {
    Serial.printf("Rx:");
    oled->setFontType(0);
    oled->setCursor(0, oled->getFontHeight());
    oled->print("Rx:");
    oled->display();
  }
  else delay(100);
  //Keep reading characters until we get a carriage return
  bool notFound = true;
  int count = 0;
  while (notFound && rxIndex<20 && ++count<45)
  {
    if(Serial1.available() > 0)
    {
      if(Serial1.peek() == pchar){
        inChar  = Serial1.read();    // Clear buffer
        if ( verbose>4 )
        {
          Serial.printf("%c;\n", inChar);
          oled->printf("%c;", inChar);
          oled->display();
        }
        else delay(100);
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
          oled->printf("%c", inChar);
          oled->display();
        }
        else delay(100);
      }
    }
    else
    {   // !available
      if ( verbose>5 )
      {
        //delay(1000);
        Serial.printf(",");
        oled->printf(",");
        oled->display();
      }
      else delay(100);
    }
  }
  return (notFound);
}
