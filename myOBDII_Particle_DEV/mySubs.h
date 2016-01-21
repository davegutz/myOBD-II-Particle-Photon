#ifndef _MYSUBS_H
#define _MYSUBS_H

#include "SparkFunMicroOLED.h"  // Include MicroOLED library

void  display(MicroOLED* oled, const uint8_t x, const uint8_t y, const String str, \
  const uint8_t clear=0, const uint8_t type=0, const uint8_t clearA=0);
void  displayStr(MicroOLED* oled, const uint8_t x, const uint8_t y, const String str,\
   const uint8_t clear=0, const uint8_t type=0, const uint8_t clearA=0);
int   getResponse(MicroOLED* oled, char* rxData);
int   parseCodes(const char *rxData, long *codes);
int   ping(MicroOLED* oled, const String cmd, char* rxData);
int   pingJump(MicroOLED* oled, const String cmd, const String val, char* rxData);
void  pingReset(MicroOLED* oled, const String cmd);
int   rxFlushToChar(MicroOLED* oled, const char pchar);

#endif
