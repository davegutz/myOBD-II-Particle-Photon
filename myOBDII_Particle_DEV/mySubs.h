#ifndef _MYSUBS_H
#define _MYSUBS_H

#include "SparkFunMicroOLED.h"  // Include MicroOLED library
enum ClearType  : uint8_t {notPage, page};
enum FontType   : uint8_t {font5x7, font8x16, sevensegment, fontlargenumber, space01, space02, space03};

void  display(MicroOLED* oled, const uint8_t x, const uint8_t y, const String str, \
  const int hold=0, const ClearType clear=notPage, const FontType type=font5x7, const uint8_t clearA=0);
void  displayStr(MicroOLED* oled, const uint8_t x, const uint8_t y, const String str,\
  const int hold=0, const ClearType clear=notPage, const FontType type=font5x7, const uint8_t clearA=0);
void  getCodes(MicroOLED* oled, const String cmd, unsigned long faultTime, char* rxData, uint8_t *ncodes, unsigned long codes[100], unsigned long activeCode[100], Queue *F);
void  getJumpFaultCodes(MicroOLED* oled, const String cmd, const String val, unsigned long faultTime, char* rxData, uint8_t *ncodes, unsigned long codes[100], unsigned long activeCode[100], const bool ignoring, Queue *F);
int   getResponse(MicroOLED* oled, char* rxData);
int   parseCodes(const char *rxData, unsigned long *codes, uint8_t *ncodes);
int   ping(MicroOLED* oled, const String cmd, char* rxData);
int   pingJump(MicroOLED* oled, const String cmd, const String val, char* rxData);
void  pingReset(MicroOLED* oled, const String cmd);
int   rxFlushToChar(MicroOLED* oled, const char pchar);

#endif
