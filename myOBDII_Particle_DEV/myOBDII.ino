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
//
// Test features usually commented
//#define TEST_SERIAL
//
// Disable flags if needed.  Usually commented
// #define DISABLE
//
// Usually defined
// #define USUALLY
//
// Constants always defined
// #define CONSTANT
#define REFRESH_NVM   false						// Command to reset NVM on fresh load
#define MAX_SIZE 30  //maximum size of the array that will store Queue.
// Caution:::do not exceed about 30 for two code quees
#define DISPLAY_DELAY 		30000UL 		// Fault code display period
#define READ_DELAY 				10000UL 		// Fault code reading period
#define RESET_DELAY 			80000UL 		// Fault reset period
#define SAMPLING_DELAY		5000UL 		  // Data sampling period
//
// Dependent includes.   Easier to debug code if remove unused include files
#include "SparkFunMicroOLED.h"  // Include MicroOLED library
#include "math.h"
//SYSTEM_MODE(MANUAL);

// Global variables
long              activeCode[100];
long              codes[100];
Queue*            F;                          // Faults
Queue*            I;                          // Impending faults
const int         faultNVM 			= 1; 					// NVM location
const int         GMT 					= -5; 				// Greenwich mean time adjustment, hrs
int               impendNVM;  								// NVM locations, calculated
MicroOLED         oled;
long              pendingCode[100];
char              rxData[4*101];
int               vehicleSpeed  = 0;          // kph
int               vehicleRPM    = 0;          // rpm

extern uint8_t    ncodes        = 0;
extern char       rxIndex       = 0;
extern int        verbose       = 3;         // Debugging Serial.print as much as you can tolerate.  0=none



void setup()
{
  Serial.printf("\n\n\nSetup ...\n");
  WiFi.off();
  Serial.begin(9600);  Serial1.begin(9600);
  oled.begin();    // Initialize the OLED

  F = new Queue(MAX_SIZE, GMT, "FAULTS");
	I = new Queue(MAX_SIZE, GMT, "IMPENDING");
	if ( !REFRESH_NVM )
	{
		impendNVM = F->loadNVM(faultNVM);
		I->loadNVM(impendNVM);
	}
  else
  {
    impendNVM = F->loadNVM(faultNVM);
		I->loadNVM(impendNVM);
    F->resetAll();
    I->resetAll();
    impendNVM = F->storeNVM(faultNVM);
    if ( impendNVM<0 || I->storeNVM(impendNVM)<0 ) Serial.printf("Failed pre-resest storeNVM\n");
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

  #ifdef TEST_SERIAL   // Jumper Tx to Rx
    display(&oled, 0, 0, "JUMPER");
    delay(1000);
  #else
    display(&oled, 0, 0, "ENGINE");
    delay(100);
  #endif

  if ( reading )
  {
    unsigned long faultTime = Time.now();
    // Codes
    #ifdef TEST_SERIAL   // Jumper Tx to Rx
    char serCode[100];
    sprintf(serCode, "43 01 20 02");
    pingJump(&oled, "03", serCode, rxData);
    int nActive = parseCodes(rxData, codes);
    for ( int i=0; i<nActive; i++ )
    {
      F->newCode(faultTime, codes[i]); if ( verbose > 2 ) {F->Print();}
      delay(1000);
    }
    display(&oled, 0, 2, "");
    delay(1000);
    sprintf(serCode, "47 02 20 02 20 03");
    pingJump(&oled, "07", serCode, rxData);
    int nPending = parseCodes(rxData, codes);
    for ( int i=0; i<nPending; i++ )
    {
      I->newCode(faultTime, codes[i]); if ( verbose > 2 ) {I->Print();}
      delay(1000);
    }
    display(&oled, 0, 2, "");
    #else
    if ( ping(&oled, "03", rxData) == 0 )
    {
      int nActive = parseCodes(rxData, codes);
      for ( int i=0; i<nActive; i++ )
      {
        activeCode[i] = codes[i];
        displayStr(&oled, 0, 2, String(activeCode[i]));
        if ( i<nActive-1 ) displayStr(&oled, 0, 2, ",");
        delay(1000);
      }
      display(&oled, 0, 2, "");
      delay(1000);
    }
    if ( ping(&oled, "07", rxData) == 0 )
    {
      int nPending = parseCodes(rxData, codes);
      for ( int i=0; i<nPending; i++ )
      {
        pendingCode[i] = codes[i];
        displayStr(&oled, 0, 2, String(pendingCode[i]));
        if ( i<nPending-1 ) displayStr(&oled, 0, 2, ",");
        delay(1000);
      }
      display(&oled, 0, 2, "");
    }
    #endif
  }   // reading

  if ( sampling )
  {
    // Speed
    #ifdef TEST_SERIAL   // Jumper Tx to Rx
    pingJump(&oled, "010D", "60", rxData);
    vehicleSpeed = atol(rxData);
    display(&oled, 0, 1, String(vehicleSpeed)+" kph");
    delay(1000);
    #else
    if (ping(&oled, "010D", rxData) == 0)
    {
      //vehicleSpeed = strtol(&rxData[6],0,16);
      vehicleSpeed = strtol(&rxData[4], 0, 16);
      display(&oled, 0, 1, String(vehicleSpeed)+" kph");
      delay(100);
    }
    #endif

    // RPM
    #ifdef TEST_SERIAL   // Jumper Tx to Rx
    pingJump(&oled, "010C", "900", rxData);
    vehicleRPM = atol(rxData);
    display(&oled, 0, 1, String(vehicleRPM)+" rpm");
    delay(1000);
    #else
    if (ping(&oled, "010C", rxData) == 0)
    {
      //NOTE: RPM data is two bytes long, and delivered in 1/4 RPM from the OBD-II-UART
      //vehicleRPM = ((strtol(&rxData[6],0,16)*256)+strtol(&rxData[9],0,16))/4;
      vehicleRPM = strtol(&rxData[4], 0, 16)/4;
      display(&oled, 0, 1, (String(vehicleRPM)+" rpm"));
      delay(100);
    }
    #endif
  }  // sampling


  if ( displaying )
	{
    String dispStr;
		if ( F->printActive(&dispStr)>0 );
		else dispStr = "none";
    display(&oled, 0, 2, ("F:" + dispStr));
		if ( I->printActive(&dispStr)>0 );
    else dispStr = "none";
    display(&oled, 0, 3, ("I:" + dispStr));
	} // displaying

  if ( resetting )
	{
		impendNVM = F->storeNVM(faultNVM);
		if ( verbose > 3 ) Serial.printf("impendNVM=%d\n", impendNVM);
		if ( impendNVM<0 || I->storeNVM(impendNVM)<0 ) Serial.printf("Failed pre-resest storeNVM\n");
		else
		{
      #ifdef TEST_SERIAL   // Jumper Tx to Rx
      F->resetAll();
      #else
      if ( F->numActive()>0 )
      {
        pingReset(&oled, "04");
        F->resetAll();
        I->resetAll();
      }
      #endif
		}
		impendNVM = F->storeNVM(faultNVM);
		if ( verbose > 3 ) Serial.printf("impendNVM=%d\n", impendNVM);
		if ( impendNVM<0 ) Serial.printf("Failed post-reset storeNVM\n");
	}

}
