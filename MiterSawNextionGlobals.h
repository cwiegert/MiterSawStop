#include <Nextion.h>
// Declare your Nextion objects - Example (page id = 0, component id = 1, component name = "b0"    Fully qualifying with the screen name
//  creates ability to use in Serial3.write commands, without having to go through the libraries)

NexPage     Home =           NexPage(0, 0, "Home");                 // Main Page
NexPage     Settings =       NexPage (1,0,"Settings");            // Setting page
NexButton   bMoveStop =      NexButton(0, 2, "bMoveStop");          // Button to move the stop, this drives the motor
NexButton   bZeroToBlade =   NexButton(0, 4, "bZeroToBlade");       // Button to move the stop to 0 out on the blace
NexButton   bMovetoZero =    NexButton(0, 5, "bMovetoZero");        // Button to move the stop to the 0 point
NexSlider   hMoveSpeed =     NexSlider (0, 23, "hMoveSpeed");       // the slider for the speed of the motor.
NexNumber   nMoveSpeed =     NexNumber (0, 24, "nMoveSpeed");       // display of the currently set motor speed
NexDSButton swLeftRight =    NexDSButton (0, 31, "swLeftRight");    // switch to define which direction to move the stop
NexText     tMove =          NexText (0, 20, "tMove");              // text box holding the measurement for the stop
NexButton   bRight =         NexButton(0, 7, "bRight");             // buton for nudging and moving the fence to the right
NexButton   bLeft =          NexButton (0, 6, "bLeft");             // button for nudging and moving the stop to the Left
NexButton   bNext =          NexButton (0, 18, "bNext");            // button to hit the next line in the cut list data sheet, calls bMoveStop
NexDSButton btPower =        NexDSButton (0, 17, "bPower");        // power button for the stepper motor
NexButton   bPark =          NexButton (0, 3, "bPark");             // button to move the stop to the Left limit and move it out of the way
NexButton   bSetZero =       NexButton (0, 32, "bSetZero");         // Button to set the current position as the 0 position, used if a 0 point is necessary prior to moving right
NexText     tCurrent =       NexText(0, 6, "tCurrent");             // textbox to display current position of teh stop
NexDSButton swTravel =       NexDSButton (0, 40, "swTravel");    // switch to define which direction to move the stop
NexCheckbox cKerf    =       NexCheckbox (0, 42, "cKerf");          //  check box to set whether or not we shoudl add hte kerf to the travel distance
NexButton   bSetPins =       NexButton(1, 17, "bSetPins");           // button to write settings to EEPROM on Arduino and NEXTION
NexText     tUpVal   =       NexText (1, 8, "tUpVal");              //  Right limit switch on Settings screen
NexText     tDownVal =       NexText (1, 9, "tDownVal");            //  Left Limit switch from the Settings screen
NexText     tZeroPin =       NexText (1, 10, "tZeroPin");           //  field value where zeroing pin is stored on settings screen
NexText     tEnablePin =     NexText (1, 11, "tEnablePin");         //  Enable Pin field on teh Settings Screen
NexText     tMicroSteps =    NexText (1, 24, "tMicroSteps");        //  mocrosteps data entry field on the settings screen
NexText     tAcceleration =  NexText (1, 26, "tAcceleration");      //  data entry field for setting maxAccleration
NexText     tStepsPerRev =   NexText (1, 28, "tStepsPerRev");      //  data enry field on Seetings screen for the stepsPerRevoluiton variable
NexText     tMaxSpeed =      NexText (1, 2, "tMaxSpeed");           //  data entry field for setting maximum motor speed
NexText     tWorkSpeed =     NexText (1, 5, "tWorkSpeed");          //  data entry field for setting the work speed on the Settings screen
NexText     tStepSize =      NexText (1, 7, "tStepSize");           //  data entry field on settings screen for setting arduino step size
NexButton   bSaveLocal =     NexButton (1, 19, "bSaveLocal");       //  Button to write the Settings UI to the arduino global variables
NexText     tKerf      =     NexText (1, 32, "tKerf");              //  textbox for holding the kerf of the blade
NexText     tLeftInch   =    NexText (1, 34, "tLeftInch");           //  inch value for the left most travel.   used in teh Park button5
NexButton   bCalib      =    NexButton (1,35,"bCalib");             //   button to move 1000 steps, will be used to figure out the travel per step
NexText     tdistPerStep =   NexText(1,36,"tdistPerStep");
//NexButton   b0  =            NexButton (0,40,"b0");                 // test button


// Any object for which we will have an event handler will need to be listed in the nex_Listen_list array.   The send component ID must be set in the UI on the event
//  which will be handled

//  01/03/2021 --- add the buttons for moving the fence..   not going to add it today

NexTouch *nex_listen_list[] = {
  &bZeroToBlade,
  &bMoveStop,
  &bMovetoZero,
  &bRight,
  &bLeft,
  &btPower,
  &bNext,
  &bPark,
  &bSetZero,
  &bSetPins,
  &bSaveLocal,
  &hMoveSpeed,
  &swTravel,
  &swLeftRight,
  &bCalib,
  NULL
};


/***************************************
    void Flushbuffer()
        when serial write to the Nextion, need to send final command
 ***********************************/
    void FlushBuffer() 
      {
          nexSerial.write(0xff);
          nexSerial.write(0xff);
          nexSerial.write(0xff);
      }
/***********************************************************
  *         int   bounceMotorOffLimit (int whichLimit, bool bDirection)
  * 
  *         used to bounce the motor off the limit switch if it has hit the limit
  *              whichLimit ==> which limit switch are we bouncing from
  *              bDirection ==> which direction should the motor bounce
  *         returns True or false
  ************************************************************************/
     void bounceMotorOffLimit (int whichLimit, int bDirection, AccelStepper *whichMotor)
       {
        
        nexSerial.write("vis pStop,1");              // bring up the Stop sign to help remind we have hit a limit switch
        FlushBuffer();  
         while ( digitalRead(whichLimit) )   //  bounce the screw off the limiter switch
           {
              if (bDirection == RIGHT )   //  02_25_2023 -- need to check which direction the motor is running.  
                {
                  curPos = whichMotor->currentPosition() - 50;      //just the opposit as above
                  whichMotor->move(curPos);
                  whichMotor->setSpeed(-workingMotorSpeed);
                }
              else
                {
                  curPos = whichMotor->currentPosition() + 50;      //just the opposite as above
                  whichMotor->move(curPos);
                  whichMotor->setSpeed(workingMotorSpeed);      
                }
              while (whichMotor->currentPosition() != curPos && digitalRead(whichLimit))
                whichMotor->runSpeed();
          }
        nexSerial.write("vis pStop,0");              // bring up the Stop sign to help remind we have hit a limit switch
        FlushBuffer();
      }

