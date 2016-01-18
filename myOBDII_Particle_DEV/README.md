myOBDII.ino
OBD-II Interface Scantool
Display  CAN-based OBD-II output to OLED.   Tested on Mazdaspeed3 '07'.

    OLED Connections:
      MicroOLED ------------- Photon
        GND ------------------- GND
        VDD ------------------- 3.3V (VCC)
      D1/MOSI ----------------- A5 (don't change)
      D0/SCK ------------------ A3 (don't change)
        D2
        D/C ------------------- D6 (can be any digital pin)
        RST ------------------- D7 (can be any digital pin)
        CS  ------------------- A2 (can be any digital pin)
    UART Connections (with FT231X interface mounted):
      UART/FT231X --------------- Photon
        GND ------------------- GND
        Tx  ------------------- Rx (Serial1)
        Rx  ------------------- Tx (Serial1)
      Hardware Platform: Particle Photon
                         SparkFun Photon Micro OLED
                         SparkFun UART/FTID OBD-II Shield (FT213X)

  Reaquirements:
  Prime:
  1.  Scantool shall reset diagnostic trouble code DTC nuisance P2002 faults on Mazda '07' Speed3 CAN before they light the MIL.
  2.  Scantool shall etermine emission readiness for inspection.
  3.  Scantool shall run entirely off standard diagnostic connector DLC.

  Secondary:
  1.  Scantool should record date and operating conditions of any DTCs as soon as they happen that would light MIL.
  2.  Scantool should have a Cloud Function interface for manual query interruption.
  3.  Scantool should allow non P2002 DTCs to light MIL.   It may not be possible but at least momentarily while the non P2002 exists, it should lVight MIL.
  4.  Non P2002 DTCs should be reset by a momentary reset button.  Can always get out the $20 scantool to do this infrequent task.
  5.  OTA dump of NVM as soon as WiFi available.   Alternatively, USB driven request to CoolTerm (or other simple serial monitor).

   Dependencies:  SparkFunMicroOLED
