/*
OBD-II Interface Scantool
Display  CAN-based OBD-II output to OLED.   Tested on Mazdaspeed3 '07'.

See README.md
  Tasks TODO:
  From ELM327 manual:  monitors for RTS or RS232 input.   Eiter interrupts it aborting any
  activity.   After interrupting, software should always wait for either the prompt character('>' or Hex '3E')
  before sending next command.   CR causes repeat of previous.
  Also may have NULL values = 00 ('\0') that should be continued on.

  Revision history:
  09-Jan-2016   Dave Gutz   Created
  27-Feb-2016   Dave Gutz   Tune up for this year's inspection

  Distributed as-is; no warranty is given.
*/


//Sometimes useful for debugging
//#pragma SPARK_NO_PREPROCESSOR
//
// Standard
#include "application.h"
SYSTEM_THREAD(ENABLED);      // Make sure heat system code always run regardless of network status
#include "myQueue.h"
#include "mySubs.h"

//
// Test features
bool              jumper            = false;    // not using jumper
extern int        verbose           = 5;        // Debugging Serial.print as much as you can tolerate.  0=none
bool              clearNVM          = false;    // Command to reset NVM on fresh load
bool              NVM_StoreAllowed  = false;    // Allow storing jumper faults
bool              ignoring          = true;    // Ignore jumper faults

// Disable flags if needed.  Usually commented
// #define DISABLE

// Usually defined
// #define USUALLY

// Constants always defined
#define MAX_SIZE 30  //maximum size of the array that will store Queue.
// Caution:::do not exceed about 30 for two code quees
#define DISPLAY_DELAY 		30000UL 		// Fault code display period
#define READ_DELAY 				30000UL 		// Fault code reading period
#define RESET_DELAY 			90000UL 		// Fault reset period
#define SAMPLING_DELAY		5000UL 		  // Data sampling period

// Dependent includes.   Easier to debug code if remove unused include files
#include "SparkFunMicroOLED.h"  // Include MicroOLED library
#include "math.h"fcff
//SYSTEM_MODE(MANUAL);

// Global variables
unsigned long      activeCode[MAX_SIZE];
/*                     Test enabled	Test incomplete
Empty                  A0-A7
Reserved	             B3	           B7
Components	           B2	           B6
Fuel System	           B1	           B5
Misfire	               B0	           B4
EGR System	           C7	           D7
Oxygen Sensor Heater	 C6	           D6
Oxygen Sensor	         C5	          D5
A/C Refrigerant	      C4	           D4
Secondary Air System	C3	          D3
Evaporative System	  C2	          D2
Heated Catalyst	      C1	         D1
Catalyst	           C0	           D0
*/
int               coolantTemp   = 0;          // Coolant temp -40 to 215 C
unsigned long     codes[MAX_SIZE];
Queue*            F;                          // Faults
Queue*            I;                          // Impending faults
const int         faultNVM 			= 1; 					// NVM location
const int         GMT 					= -5; 				// Greenwich mean time adjustment, hrs
int               impendNVM;  								// NVM locations, calculated
MicroOLED         oled;
uint8_t           ncodes        = 0;          // Number of fault codes
unsigned long     pendingCode[MAX_SIZE];
char              rxData[4*101];
int               timeSinceRes  = 0;          // min 65535
int               warmsSinceRes = 0;          // 255
int               kmSinceRes    = 0;          // km 65535
int               vehicleSpeed  = 0;          // kph 255
int               vehicleRPM    = 0;          // rpm 16383

extern char       rxIndex       = 0;



void setup()
{
  Serial.printf("\n\n\nSetup ...\n");
  WiFi.disconnect();
  Serial.begin(9600);
  Serial1.begin(9600);
  oled.begin();    // Initialize the OLED

  F = new Queue(MAX_SIZE, GMT, "FAULTS",    (!jumper||NVM_StoreAllowed), verbose);
	I = new Queue(MAX_SIZE, GMT, "IMPENDING", (!jumper||NVM_StoreAllowed), verbose);
	if ( !clearNVM )
	{
		impendNVM   = F->loadNVM(faultNVM);
		int endNVM  = I->loadNVM(impendNVM);
    if ( endNVM>EEPROM.length() ) // Too much NVM
    {
      display(&oled, 0, 0, "NVM OVER", 300000, page, font8x16, ALL);
    }
    delay(1500);
	}
  if ( jumper ) display(&oled, 0, 0, "JUMPER", 3000, page, font5x7, ALL);
  display(&oled, 0, 0, "ACTIVE", 500, page, font5x7, ALL);

  String dispStr;
  if ( F->printActive(&dispStr)>0 );
  else dispStr = "----  ";
  display(&oled, 0, 1, ("F:" + dispStr));
  if ( I->printActive(&dispStr)>0 );
  else dispStr = "----  ";
  display(&oled, 0, 3, ("I:" + dispStr), 10000);

  display(&oled, 0, 0, "STORED IMPEND", 0, page, font5x7, ALL);
  if ( I->printInActive(&dispStr, 2)>0 );
  else dispStr = "----  ";
  display(&oled, 0, 1, ("I:" + dispStr), 5000);

  display(&oled, 0, 0, "STORED FAULTS", 0, page, font5x7, ALL);
  if ( F->printInActive(&dispStr, 2)>0 );
  else dispStr = "----  ";
  display(&oled, 0, 1, ("F:" + dispStr), 10000);


  //Reset the OBD-II-UART
  display(&oled, 0, 0, "WAIT", 500, page, font5x7, ALL);
  Serial1.println("ATZ");
  delay(1000);
  getResponse(&oled, rxData);
  display(&oled, 0, 1, String(rxData));
  Serial.printf("setup ending\n");
  delay(2000);
  WiFi.off();
}


void loop(){
  FaultCode newOne;
  bool 									displaying;
  bool 									reading;
  bool 									resetting;
  bool                  sampling;
  unsigned long 				now = millis();     // Keep track of time
  static unsigned long 	lastDisplay = 0UL;  // Last display time, ms
  static unsigned long 	lastRead  	= -READ_DELAY;  // Last read time, ms
  static unsigned long 	lastReset 	= 0UL;  // Last reset time, ms
  static unsigned long 	lastSample 	= 0UL;  // Last reset time, ms

  reading 		= ((now-lastRead   ) >= READ_DELAY);
	if ( reading   ) lastRead = now;

	resetting 	= ((now-lastReset  ) >= RESET_DELAY);
  if ( resetting  ) lastReset  = now;

	displaying	= ((now-lastDisplay) >= DISPLAY_DELAY);
	if ( displaying ) lastDisplay = now;

  sampling	= ((now-lastSample) >= SAMPLING_DELAY);
	if ( sampling ) lastSample = now;

  if ( jumper ) display(&oled, 0, 0, "JUMPER", 1000);

  if ( reading )
  {
    unsigned long faultTime = Time.now();
    // Codes
    if ( jumper )
    {
      getJumpFaultCodes(&oled, "03", "43 01 20 02", faultTime, rxData, &ncodes, codes, \
        activeCode, ignoring, F);
      delay(1000);
      getJumpFaultCodes(&oled, "07", "47 02 20 12 20 13", faultTime, rxData, &ncodes, codes,\
        activeCode, ignoring, I);
    }
    else  // ENGINE
    {
      getCodes(&oled, "03", faultTime, rxData, &ncodes, codes, activeCode,  F);
      getCodes(&oled, "07", faultTime, rxData, &ncodes, codes, pendingCode, I);
    }
  }   // reading

  if ( sampling )
  {
    // Speed
    if ( jumper )
    {
      pingJump(&oled, "010D", "60", rxData);
      vehicleSpeed = atol(rxData);
      char tmp[100];
      sprintf(tmp, "%5.0f  mph", float(vehicleSpeed)*0.6);
      display(&oled, 0, 1, String(tmp), 1000);
    }
    else   // ENGINE
    {
      if (ping(&oled, "010D", rxData) == 0)
      {
        vehicleSpeed = strtol(&rxData[4], 0, 16);
        char tmp[100];
        sprintf(tmp, "%5.0f  mph", float(vehicleSpeed)*0.6);
        display(&oled, 0, 0, String(tmp), 200);
      }
      else
      {
        display(&oled, 0, 0, "----  mph", 200);
      }
    }

    // RPM  2 bytes  ((A*256)+B)/4
    if ( jumper )
    {
      pingJump(&oled, "010C", "900", rxData);
      vehicleRPM = atol(rxData);
      display(&oled, 0, 1, String(vehicleRPM)+"  rpm  ", 1000);
    }
    else // ENGINE
    {
      if (ping(&oled, "010C", rxData) == 0)
      {
        vehicleRPM = strtol(&rxData[4], 0, 16)/4;
        display(&oled, 0, 0, (String(vehicleRPM)+"  rpm"), 200);
      }
      else
      {
        display(&oled, 0, 0, "----  rpm", 200);
      }
    }


    // Warmups Since Reset 1 byte
    if ( jumper )
    {
      pingJump(&oled, "0130", "255", rxData);
      warmsSinceRes = atol(rxData);
      display(&oled, 0, 1, String(warmsSinceRes)+"   wms   ", 1000);
    }
    else // ENGINE
    {
      if (ping(&oled, "0130", rxData) == 0)
      {
        warmsSinceRes = strtol(&rxData[4], 0, 16); // number
        display(&oled, 0, 0, (String(warmsSinceRes)+"   wms   "), 500);
      }
      else
      {
        display(&oled, 0, 0, "---- wms   ", 200);
      }
    }

    // km Since Reset 2 byte
    if ( jumper )
    {
      pingJump(&oled, "0131", "65535", rxData);
      kmSinceRes = atol(rxData);
      char tmp[100];
      sprintf(tmp, "%6.0f  mi", float(kmSinceRes)*0.6);
      display(&oled, 0, 1, String(tmp), 1000);
    }
    else // ENGINE
    {
      if (ping(&oled, "0131", rxData) == 0)
      {
        kmSinceRes = strtol(&rxData[4], 0, 16); // km
        char tmp[100];
        sprintf(tmp, "%6.0f  mi", float(kmSinceRes)*0.6);
        display(&oled, 0, 0, String(tmp), 500);
      }
      else
      {
        display(&oled, 0, 0, "----    mi", 200);
      }
    }

    // Coolant temp 1 byte  0105
    if ( jumper )
    {
      pingJump(&oled, "0105", "215", rxData);
      coolantTemp = atoi(rxData);
      char tmp[100];
      sprintf(tmp, "%7.0f  F", float(coolantTemp)*9/5+32);
      display(&oled, 0, 1, String(tmp), 1000);
    }
    else // ENGINE
    {
      if (ping(&oled, "0105", rxData) == 0)
      {
        coolantTemp = strtol(&rxData[4], 0, 16)-40;  // C
        char tmp[100];
        sprintf(tmp, "%7.0f  F", float(coolantTemp)*9/5+32);
        display(&oled, 0, 0, String(tmp), 200);
      }
      else
      {
        display(&oled, 0, 0, "------- F", 200);
      }
    }

    // Ready bytes  4 bytes
    if ( jumper )
    {
      pingJump(&oled, "0101", "101010101010", rxData);
      display(&oled, 0, 1, String(rxData), 1000);
    }
    else // ENGINE
    {
      if (ping(&oled, "0101", rxData) == 0)
      {
        String str = "1-" + String(&rxData[4]);
        char tmp[100];
        sprintf(tmp, "%10s", str.c_str());
        display(&oled, 0, 0, tmp, 1500);
      }
      else
      {
        display(&oled, 0, 0, "-------------", 1500);
      }
    }



  }  // sampling


  if ( displaying )
	{
    uint8_t line;
    if ( jumper ) line = 2; else line = 1;
    String dispStr;
		F->printActive(&dispStr);
    display(&oled, 0, line, ("F:" + dispStr));
		I->printActive(&dispStr);
    display(&oled, 0, line+1, ("I:" + dispStr));
	} // displaying

  if ( resetting )
	{
    Serial.printf("RESETTING...\n");
    int finalNVM;
    if ( clearNVM )
    {
      impendNVM = F->clearNVM(faultNVM);
      finalNVM  = I->clearNVM(impendNVM);
      if ( finalNVM>EEPROM.length() ) // Too much NVM
      {
        display(&oled, 0, 0, "NVM OVER", 300000, page, font8x16, ALL);
      }
    }
	  else
    {
      impendNVM = F->storeNVM(faultNVM);
      finalNVM  = I->storeNVM(impendNVM);
      if ( finalNVM>EEPROM.length() ) // Too much NVM
      {
        display(&oled, 0, 0, "NVM OVER", 300000, page, font8x16, ALL);
      }
    }
    if ( impendNVM<0 || finalNVM<0 )
      if ( clearNVM )
        Serial.printf("Failed pre-reset clear NVM\n");
      else
        Serial.printf("Failed pre-reset storeNVM\n");
    else
	  {
      Serial.printf("Success clear/store NVM\n");
      if ( jumper )
      {
        F->resetAll();
      }
      else // ENGINE
      {
        if ( F->numActive()>0 || (I->numActive()>0 && warmsSinceRes>2) )
        {
          pingReset(&oled, "04");
          F->resetAll();
          I->resetAll();
        }
      }
	  }
    if ( !clearNVM )
    {
      impendNVM = F->storeNVM(faultNVM);
      finalNVM  = I->storeNVM(impendNVM);
      Serial.printf("Post-reset store NVM\n");
    }
	}

}
