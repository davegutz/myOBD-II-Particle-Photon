/*
* OBD-II-UART Quickstart Sketch
* Written by Ryan Owens for SparkFun Electronics 7/5/2011
* Updates for Arduino 1.0+ by Toni Klopfenstein
*
* Released under the 'beerware' license
* (Do what you want with the code, but if we ever meet then you buy me a beer)
*
* This sketch will grab RPM and Vehicle Speed data from a vehicle with an OBD port
* using the OBD-II-UART board from SparkFun electronics. The data will be displayed
* on a Serial 16x2 OLED. See the tutorial at https://www.sparkfun.com/tutorials/294
* to learn how to hook up the hardware:
*
*/

#include "SparkFunMicroOLED.h"  // Include MicroOLED library
#include "math.h"
void display(const uint8_t x, const uint8_t y, const String str, const uint8_t clear=0, const uint8_t type=0, const uint8_t clearA=0);
MicroOLED oled;


//This is a character buffer that will store the data from the Serial1 port
char rxData[20];
char rxIndex=0;

//Variables to hold the speed and RPM data.
int vehicleSpeed=0;
int vehicleRPM=0;

void setup(){
  oled.begin();    // Initialize the OLED
  Serial1.begin(9600);

  //Clear the old data from the OLED.
  display(0, 0, "WAIT", 1, 0, 1);

  //Wait for a little while before sending the reset command to the OBD-II-UART
  delay(1500);
  //Reset the OBD-II-UART
  Serial1.println("ATZ");
  //Wait for a bit before starting to send commands after the reset.
  delay(2000);

  //Delete any data that may be in the Serial1 port before we begin.
  Serial1.flush();
}

void loop(){
  //Delete any data that may be in the Serial1 port before we begin.
  Serial1.flush();
  //Query the OBD-II-UART for the Vehicle Speed
  Serial1.println("010D");
  //Get the response from the OBD-II-UART board. We get two responses
  //because the OBD-II-UART echoes the command that is sent.
  //We want the data in the second response.
  getResponse();
  getResponse();
  //Convert the string data to an integer
  vehicleSpeed = strtol(&rxData[6],0,16);
  //Print the speed data to the OLED
  display(0, 0, String(vehicleSpeed)+" kph");
  delay(100);

  //Delete any data that may be left over in the Serial1 port.
  Serial1.flush();
  //Query the OBD-II-UART for the Vehicle rpm
  Serial1.println("010C");
  getResponse();
  getResponse();
  //Convert the string data to an integer
  //NOTE: RPM data is two bytes long, and delivered in 1/4 RPM from the OBD-II-UART
  vehicleRPM = ((strtol(&rxData[6],0,16)*256)+strtol(&rxData[9],0,16))/4;
  //Print the rpm data to the OLED
  display(0, 1, String(vehicleRPM)+" rpm");

  //Give the OBD bus a rest
  delay(100);

}

//The getResponse function collects incoming data from the UART into the rxData buffer
// and only exits when a carriage return character is seen. Once the carriage return
// string is detected, the rxData buffer is null terminated (so we can treat it as a string)
// and the rxData index is reset to 0 so that the next string can be copied.
void getResponse(void){
  char inChar=0;
  //Keep reading characters until we get a carriage return
  while(inChar != '\r'){
    //If a character comes in on the Serial1 port, we need to act on it.
    if(Serial1.available() > 0){
      //Start by checking if we've received the end of message character ('\r').
      if(Serial1.peek() == '\r'){
        //Clear the Serial1 buffer
        inChar=Serial1.read();
        //Put the end of string character on our data string
        rxData[rxIndex]='\0';
        //Reset the buffer index so that the next character goes back at the beginning of the string.
        rxIndex=0;
      }
      //If we didn't get the end of message character, just add the new character to the string.
      else{
        //Get the new character from the Serial1 port.
        inChar = Serial1.read();
//        if (isspace(inChar)) continue;  // Strip line feeds left from previous \n-\r
        //Add the new character to the string, and increment the index variable.
        rxData[rxIndex++]=inChar;
      }
    }
  }
}

// Simple OLED print
void display(const uint8_t x, const uint8_t y, const String str, const uint8_t clear, const uint8_t type, const uint8_t clearA)
{
  if (clearA) oled.clear(ALL);
  if (clear) oled.clear(PAGE);
  oled.setFontType(type);
  oled.setCursor(x, y*oled.getFontHeight());
  oled.print(str);
  oled.display();
}
