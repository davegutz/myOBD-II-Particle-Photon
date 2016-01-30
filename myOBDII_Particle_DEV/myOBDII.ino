/*
OBD-II Interface Scantool
Display  CAN-based OBD-II output to OLED.   Tested on Mazdaspeed3 '07'.

See README.md
  Tasks TODO:
  1.  Function that waits for exact Tx response.
  2.  Rotable NVM storage function.
  3.  Setup task scheduler:
      a.  RPM, KPH on 1 sec
      b.  DTCs on 30 sec, record into NVM only once each startup.
      c.  NVM storage with DTCs.
  From ELM327 manual:  monitors for RTS or RS232 input.   Eiter interrupts it aborting any
  activity.   After interrupting, software should always wait for either the prompt character('>' or Hex '3E')
  before sending next command.   CR causes repeat of previous.
  Also may have NULL values = 00 ('\0') that should be continued on.

  Revision history:
  09-Jan-2016   Dave Gutz   Created

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
void  getCodes(MicroOLED* oled, const String cmd, unsigned long faultTime, char* rxData, long codes[100], long activeCode[100], Queue *F);

//
// Test features usually commented
bool              jumper            = false;    // not using jumper
extern int        verbose           = 5;        // Debugging Serial.print as much as you can tolerate.  0=none
bool              clearNVM          = false;    // Command to reset NVM on fresh load
bool              NVM_StoreAllowed  = false;    // Allow storing jumper faults
bool              ignoring          = true;    // Ignore jumper faults
//
// Disable flags if needed.  Usually commented
// #define DISABLE
//
// Usually defined
// #define USUALLY
//
// Constants always defined
// #define CONSTANT
#define MAX_SIZE 30  //maximum size of the array that will store Queue.
// Caution:::do not exceed about 30 for two code quees
#define DISPLAY_DELAY 		30000UL 		// Fault code display period
#define READ_DELAY 				10000UL 		// Fault code reading period
#define RESET_DELAY 			80000UL 		// Fault reset period
#define SAMPLING_DELAY		5000UL 		  // Data sampling period
//
// Dependent includes.   Easier to debug code if remove unused include files
#include "SparkFunMicroOLED.h"  // Include MicroOLED library
#include "math.h"fcff
//SYSTEM_MODE(MANUAL);

// Global variables
long              activeCode[100];
String            completeness;               // Internal readiness
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
long              codes[100];
Queue*            F;                          // Faults
Queue*            I;                          // Impending faults
const int         faultNVM 			= 1; 					// NVM location
const int         GMT 					= -5; 				// Greenwich mean time adjustment, hrs
int               impendNVM;  								// NVM locations, calculated
MicroOLED         oled;
long              pendingCode[100];
char              rxData[4*101];
int               timeSinceRes  = 0;          // min 65535
int               warmsSinceRes = 0;          // 255
int               kmSinceRes    = 0;          // km 65535
int               vehicleSpeed  = 0;          // kph 255
int               vehicleRPM    = 0;          // rpm 16383

extern uint8_t    ncodes        = 0;
extern char       rxIndex       = 0;



void setup()
{
  Serial.printf("\n\n\nSetup ...\n");
  WiFi.disconnect();
  Serial.begin(9600);
  Serial1.begin(9600);
  oled.begin();    // Initialize the OLED

  F = new Queue(MAX_SIZE, GMT, "FAULTS", (!jumper||NVM_StoreAllowed));
	I = new Queue(MAX_SIZE, GMT, "IMPENDING", (!jumper||NVM_StoreAllowed));
	if ( !clearNVM )
	{
		impendNVM = F->loadNVM(faultNVM);
		I->loadNVM(impendNVM);
	}

  delay(1500);  display(&oled, 0, 0, "NVM", 1, 0, 1);  delay(500);
  String dispStr;
  if ( F->printActive(&dispStr)>0 );
  else dispStr = "none";
  display(&oled, 0, 2, ("F:" + dispStr));
  if ( I->printActive(&dispStr)>0 );
  else dispStr = "none";
  display(&oled, 0, 3, ("I:" + dispStr));
  delay(10000);

//TODO enumerated type for last three display argumenets.   Add delay to arguments (before first optional)
  delay(1500);  display(&oled, 0, 0, "WAIT", 1, 0, 1);  delay(500);

  //Reset the OBD-II-UART
  Serial1.println("ATZ");
  delay(1000);
  getResponse(&oled, rxData);
  display(&oled, 0, 1, String(rxData), 0, 0);
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

  if ( jumper )
  {
    display(&oled, 0, 0, "JUMPER");
    delay(1000);
  }

  if ( reading )
  {
    unsigned long faultTime = Time.now();
    // Codes
    if ( jumper )
    {
      getJumpFaultCodes(&oled, "03", "43 01 20 02", faultTime, rxData, codes, \
        activeCode, ignoring, F);
      delay(1000);
      getJumpFaultCodes(&oled, "07", "47 02 20 12 20 13", faultTime, rxData, codes,\
        activeCode, ignoring, I);
    }
    else  // ENGINE
    {
      getCodes(&oled, "03", faultTime, rxData, codes, activeCode,  F);
      getCodes(&oled, "07", faultTime, rxData, codes, pendingCode, I);
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
      sprintf(tmp, "%3.0f mph", float(vehicleSpeed)*0.6);
      display(&oled, 0, 1, String(tmp));
      delay(1000);
    }
    else   // ENGINE
    {
      if (ping(&oled, "010D", rxData) == 0)
      {
        vehicleSpeed = strtol(&rxData[4], 0, 16);
        char tmp[100];
        sprintf(tmp, "%3.0f mph", float(vehicleSpeed)*0.6);
        display(&oled, 0, 0, String(tmp));
        delay(200);
      }
    }

    // RPM  2 bytes  ((A*256)+B)/4
    if ( jumper )
    {
      pingJump(&oled, "010C", "900", rxData);
      vehicleRPM = atol(rxData);
      display(&oled, 0, 1, String(vehicleRPM)+" rpm");
      delay(1000);
    }
    else // ENGINE
    {
      if (ping(&oled, "010C", rxData) == 0)
      {
        vehicleRPM = strtol(&rxData[4], 0, 16)/4;
        display(&oled, 0, 0, (String(vehicleRPM)+" rpm"));
        delay(200);
      }
    }


/*  Not available 2007 Mazdaspeed3
    // Time Since Reset 2 bytes
    if ( jumper )
    {
      pingJump(&oled, "4E", "65535", rxData);
      timeSinceRes = atol(rxData);
      display(&oled, 0, 1, String(timeSinceRes)+" min");
      delay(1000);
    }
    else // ENGINE
    {
      if (ping(&oled, "4E", rxData) == 0)
      {
        timeSinceRes = strtol(&rxData[2], 0, 16); // min
        display(&oled, 0, 0, (String(timeSinceRes)+" min"));
        delay(100);
      }
    }
*/

    // Warmups Since Reset 1 byte
    if ( jumper )
    {
      pingJump(&oled, "0130", "255", rxData);
      warmsSinceRes = atol(rxData);
      display(&oled, 0, 1, String(warmsSinceRes)+" wms  ");
      delay(1000);
    }
    else // ENGINE
    {
      if (ping(&oled, "0130", rxData) == 0)
      {
        warmsSinceRes = strtol(&rxData[4], 0, 16); // number
        display(&oled, 0, 0, (String(warmsSinceRes)+" wms  "));
        delay(500);
      }
    }

    // km Since Reset 2 byte
    if ( jumper )
    {
      pingJump(&oled, "0131", "65535", rxData);
      kmSinceRes = atol(rxData);
      char tmp[100];
      sprintf(tmp, "%5.0f mi ", float(kmSinceRes)*0.6);
      display(&oled, 0, 1, String(tmp));
      delay(1000);
    }
    else // ENGINE
    {
      if (ping(&oled, "0131", rxData) == 0)
      {
        kmSinceRes = strtol(&rxData[4], 0, 16); // km
        char tmp[100];
        sprintf(tmp, "%5.0f mi ", float(kmSinceRes)*0.6);
        display(&oled, 0, 0, String(tmp));
        delay(500);
      }
    }


    // Coolant temp 1 byte  0105
    if ( jumper )
    {
      pingJump(&oled, "0105", "215", rxData);
      coolantTemp = atoi(rxData);
      char tmp[100];
      sprintf(tmp, "%3.0f F    ", float(coolantTemp)*9/5+32);
      display(&oled, 0, 1, String(tmp));
      delay(1000);
    }
    else // ENGINE
    {
      if (ping(&oled, "0105", rxData) == 0)
      {
        coolantTemp = strtol(&rxData[4], 0, 16)-40;  // C
        char tmp[100];
        sprintf(tmp, "%3.0f F    ", float(coolantTemp)*9/5+32);
        display(&oled, 0, 0, String(tmp));
        delay(200);
      }
    }

    // Ready bytes  4 bytes
    if ( jumper )
    {
      pingJump(&oled, "0141", "10101010", rxData);
      completeness = String(rxData);
      display(&oled, 0, 1, completeness);
      delay(1000);
      pingJump(&oled, "0101", "10101010", rxData);
      completeness = String(rxData);
      display(&oled, 0, 1, completeness);
      delay(1000);
    }
    else // ENGINE
    {
      if (ping(&oled, "0141", rxData) == 0)
      {
        completeness = String(&rxData[5]);  // omit empty A
        display(&oled, 0, 0, completeness);
        delay(1000);
      }
      if (ping(&oled, "0101", rxData) == 0)
      {
        completeness = String(&rxData[5]);  // omit empty A
        display(&oled, 0, 0, completeness);
        delay(1000);
      }
    }



  }  // sampling


  if ( displaying )
	{
    uint8_t line;
    if ( jumper ) line = 2; else line = 1;
    String dispStr;
		if ( F->printActive(&dispStr)>0 );
		else dispStr = "none";
    display(&oled, 0, line, ("F:" + dispStr));
		if ( I->printActive(&dispStr)>0 );
    else dispStr = "none";
    display(&oled, 0, line+1, ("I:" + dispStr));
	} // displaying

  if ( resetting )
	{
    Serial.printf("RESETTING...\n");
    int finalNVM;
    if ( clearNVM )
    {
      impendNVM = F->clearNVM(faultNVM);
      if ( verbose>3 ) Serial.printf("clear faultNVM=%d\n", impendNVM);
      finalNVM  = I->clearNVM(impendNVM);
      if ( verbose>3 ) Serial.printf("clear impendNVM=%d\n", finalNVM);
    }
	  else
    {
      impendNVM = F->storeNVM(faultNVM);
      if ( verbose>3 ) Serial.printf("store faultNVM=%d\n", impendNVM);
      finalNVM  = I->storeNVM(impendNVM);
      if ( verbose>3 ) Serial.printf("store impendNVM=%d\n", finalNVM);
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
        if ( F->numActive()>0 )
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
  	  if ( verbose>3 ) Serial.printf("impendNVM=%d\n", impendNVM);
  	  if ( impendNVM<0 ) Serial.printf("Failed post-reset storeNVM\n");
      else Serial.printf("Success post-reset store NVM\n");
    }
	}

}

// Get and display engine codes
void  getCodes(MicroOLED* oled, const String cmd, unsigned long faultTime, char* rxData, long codes[100], long activeCode[100], Queue *F)
{
  if ( ping(oled, cmd, rxData) == 0 ) // success
  {
    int nActive = parseCodes(rxData, codes);
    for ( int i=0; i<nActive; i++ )
    {
      F->newCode(faultTime, codes[i]);
      activeCode[i] = codes[i];
      displayStr(oled, 0, 1, String(activeCode[i]));
      if ( i<nActive-1 ) displayStr(oled, 0, 2, ",");
      delay(1000);
    }
    display(oled, 0, 1, "");
    delay(1000);
  }
}
