#include <AccelStepper.h>             // Library for Stepper Controller.   Used to talk to the stepper motor
#include "SdFat.h"              
#include <EEPROM.h>                   // Library to write and read EEPROM on the arduino
#include "MiterSawGlobals.h"          // my library fro the globals used in this program.   shared with other automation tools for the shop
#include "MiterSawNextionGlobals.h"   // globals varialbe library for Nextion HMI objects and interacting wtih the Arduino


/*************************************************************************************************************************************
  Wiegert Miter Saw Stop

  v.1.0
  02_24_2023
  Cory D. Wiegert

    The Miter Saw Stop is an automation program to control a sliding stock stop as a miter saw fence.   The code requires
    a connection to a Human Machine Interface (HMI) to control initiate the settings and movement.   This program was written to support
    the Nextion HMI and the Nextion arduino libraries.   The nextion_listener and the objects defined in the MiterSawNextiongGlobals.h will 
    determine the CallBack priorities.   

    Using the AccelStepper libraries also simplifies the interface to the stepper motor.   There is no specific motor driver or motor necessary
    except to have a 4 wire connection.   

    The code is open source and free of any royalties or support.    If there are suggestions or bugs found, please report them in the gitRepo
    but, there is no ongoing commitment to maintain or modify the code for any other purpose than my personal use.
    ___________________________________________________________________________________________________________________________________________

    02_27_2023    CDW   v.1.0   -- starting the wiring of the stepper and driving it off the HMI
    03_18_2023    CDW   v.1.01  -- added the Calib button and function to the settings screen

*****************************************************************************************************************************************************************************/

/****************************************************************************************
   Decare the AccelStepper object and use the FULL2WIRE as a type of stepper motor.
   connecting to the TB6600, it is a 4 wire control.

   The pins must be defined and configured here, they can not be part of the Setup - because sMiter is set as a global variable
   it must have the buttons defined, prior to the setup -- NOTE TO SELF:  DO NOT TRY TO DYNAMICALLY SET THE STEP OR DIRECTION PINS
 *************************************************************************/

    AccelStepper  sMiter (AccelStepper::FULL2WIRE, stepPin, directionPin, HIGH);

/********************************
  Single function call to turn off power to motor
*********************************/

    int  turnMotorOff( int bON)
      {
        char  sCommand[44] = {'\0'};
        long  offColor = 63488;
        long  onColor = 34784;

        if (bON == DOWN)                  // Turn the motor controler off
          {
            digitalWrite ( enablePin, HIGH);
            nexSerial.write (sCommand);
            btPower.setValue(DOWN);
            digitalWrite (stepPin, LOW);
            bON =  UP;
          }
        else                           // Turn the motor controller on
          {
            digitalWrite ( enablePin, LOW);
            FlushBuffer();
            btPower.setValue(UP);
            digitalWrite (stepPin, HIGH);
            bON = DOWN;
          }
        return bON;
      }

/****************************************************
       long   calcSteps (float   inchValue)
           returns the number of steps to travel, given the value of inches passed to the function
 ***************************************************/
    long  calcSteps (float inchValue) 
      {
       // return inchValue / distPerStep * 4;  // 4 microsteps per step
         return inchValue / distPerStep;
      }

/****************************************************
       float   calcInches (long numOfSteps)
           returns the inch equivalent to the number of steps passed into the function
 ***************************************************/
    float  calcInches (long numOfSteps) 
      {
        return numOfSteps / distPerStep; // exact opposite logic of the calcSteps function
      }

/*************************************************
   void   setPositionField()
            sets the current position field after the stop has been moved
            will read from the current position of the Servo

  ************************************************/
    void setPositionField(bool bMotor = 1) 
      {
        char buffer[16];
        char  sCommand[40];
        curPosInch = sMiter.currentPosition() * distPerStep;
        if (curPosInch != 0)
          {
            memset (buffer, '\0', sizeof(buffer));
            dtostrf(curPosInch, 3, 4, buffer);
            dtostrf(curPosInch, 3, 4, posInch);
          }
        else
          strcpy(buffer, "0.000");
        if (bMotor)
          {
            sprintf(sCommand, "Home.tCurrent.txt=\"%s\"", buffer);
            nexSerial.write(sCommand);
            FlushBuffer();
          }
      }

/***************************************************
     byte   checkStopButton ()
             checks to see if the Off button has been pressed during an automated move of the lift.
             usefull in chagnge bit, move router to limit or Auto button functions
 **********************************************************************/
    byte  checkStopButton ()
        {     
          int incomingByte;
          int   StopButtons[1][2] = {
            {0, 17}     //   these are the pages/control #'s for the stop buttons.  if the GUI changes, check here
          };
          int  screen;
          int  button;
        
          bGo = DOWN;
          incomingByte = nexSerial.read();

          while (incomingByte > -1 )
            {
              if (incomingByte == 101)
                {
                  screen = nexSerial.read();
                  button = nexSerial.read();
  
                }
              if ((StopButtons[0][0]== screen && StopButtons[0][1] == button)  )
                {
                  bGo = UP;
                  turnMotorOff(UP);
                  return bGo;  
                }
              incomingByte = nexSerial.read();
            }
          return bGo;
        }
 
/************************************************************
  void hMoveSpeedPopCallback(void *ptr)
        Slider hSlider component push callback function
        nSetSpeed is set through tht events in the Nextion IDE.  when the
        PopCallback is fired, we want to set the working motor speed, nothing else
**********************************************************/

    void hMoveSpeedPopCallback(void *ptr)
      {
        uint32_t scrVal;
        int slideVal;
        hMoveSpeed.getValue(&scrVal);
        slideVal = scrVal;
        workingMotorSpeed = slideVal;
        //setSpeedFromSlider(float(slideVal), DOWN);
      }

/***********************************************
   void btPowerPopCallback(void *ptr)

         Button btPower component pop callback function.
         if the motor controller enablePin is off, sets it on, and vice versa.
**************************/

    void btPowerPopCallback(void *ptr) 
      {
        uint32_t  butVal;
        int       temp;

        btPower.getValue(&butVal);
        temp = butVal;
        if (temp == 0 )
          bGo = turnMotorOff (UP);
        else
          bGo = turnMotorOff( DOWN);
      }

/*********************************************************
 *   void bParkPopCallback (void *ptr)
 *        
 *    function to move the stop to the left limit, moving it out of the way of the table.  
 *    will move at max speed, and bounce off the limit when we hit it
 * *********************************************************/
    void bParkPopCallback (void *ptr)
      {
          int cBuff = -1;

          LeftLimitSteps = calcSteps(LeftTravel);
          sMiter.moveTo(LeftLimitSteps );    // LeftLimitSteps needs to be set when the homing to the saw blad
          sMiter.setSpeed(maxMotorSpeed);

          while (!digitalRead(LEFT_SWITCH)  && sMiter.currentPosition() != sMiter.targetPosition())
            {
              sMiter.runSpeed();             // implements acceleration, and I have removed the call to setSpeed
              if (sMiter.currentPosition( ) > 0.95 * sMiter.targetPosition())
                {
                  sMiter.moveTo(sMiter.targetPosition());
                  sMiter.setSpeed(0.1* maxMotorSpeed);
                }
            }
                  
         if (digitalRead (LEFT_SWITCH))
           bounceMotorOffLimit (LEFT_SWITCH, RIGHT, &sMiter ); 
           
          setPositionField();
 
        }

/********************************************************
    void bRightPushCallback (void *ptr)

         Button bRight component push callback function.
        While the button is pressed, the stop will move right until the limit is hit.
        speed is already set from the slider, current position will be set on the Pop

 *****************************************************/
    void bRightPushCallback (void *ptr)  
      {
        int cBuff = -1;
        char buffer[16];
        uint32_t   uMotor_;

        nMoveSpeed.getValue (&uMotor_);
        workingMotorSpeed = uMotor_;
        while (!digitalRead(RIGHT_SWITCH) && cBuff == -1 && bGo && digitalRead(REZERO))
          {
            curPos = sMiter.currentPosition() - stepSize;
            sMiter.moveTo(curPos);
            sMiter.setSpeed(-workingMotorSpeed);

            while (sMiter.currentPosition() != sMiter.targetPosition()  && !digitalRead (RIGHT_SWITCH) && digitalRead(REZERO))
              sMiter.runSpeed();
            cBuff = nexSerial.read();

          }

        if (digitalRead(RIGHT_SWITCH))
          bounceMotorOffLimit (RIGHT_SWITCH, LEFT, &sMiter);
          
      setPositionField(1);
      sMiter.stop();
    }   

    void bRightPopCallback (void *ptr)
      {
        setPositionField(1);
      }

/********************************************************
    void bLeftPushCallback (void *ptr)

         Button bLeft component push callback function.
        While the button is pressed, the stop will move Left until the limit is hit.
        speed is already set from the slider, current position will be set on the Pop

 *****************************************************/
    void bLeftPushCallback (void *ptr)  
      {
        int cBuff = -1;
        char buffer[16];
        uint32_t uMotor_;

       nMoveSpeed.getValue (&uMotor_);
        workingMotorSpeed = uMotor_;

        while (!digitalRead(LEFT_SWITCH) && cBuff == -1 && bGo)
          {
            curPos = sMiter.currentPosition() + stepSize;
            sMiter.moveTo(curPos);
            sMiter.setSpeed(workingMotorSpeed);

            while (sMiter.currentPosition() != sMiter.targetPosition()  && !digitalRead (LEFT_SWITCH))
                sMiter.runSpeed();
            cBuff = nexSerial.read();
          }

        if (digitalRead(LEFT_SWITCH))
          bounceMotorOffLimit (LEFT_SWITCH, RIGHT, &sMiter);
          
        setPositionField();
      }

    void bLeftPopCallback (void *ptr)
      {
        setPositionField(1);
      }

/***********************************************************
 *  void swLeftRightPopCallBack (void *ptr)
 *      switch to desinate which way to move the stop.   Useful in feeding stock into he saw
 *      or setting the stop left of the saw and feeding stock into the stop
*******************************************************************/
    void swLeftRightPopCallback (void *ptr)
      {
        uint32_t   switch_;
        
        swLeftRight.getValue(&switch_);
        DIRECTION = switch_;
      }

/***********************************************************
 *  void swTravelPopCallBack (void *ptr)
 *      switch to determine exact travel distance, or relative to current.   If set to "goto"
 *      the stop will move to the exact distance set in teh Move To box.   if set to Relative, the 
 *      stop will move the Move To distance from the current point, in the direction set by Direction switch
*******************************************************************/
    void swTravelPopCallback (void *ptr)
      {
        uint32_t   switch_;
        
        swTravel.getValue(&switch_);
        EXACT_RELATIVE = switch_;
      }

/***********************************************************
 *  void bMoveStopPopCallBack (void *ptr)
 *      switch to desinate how to manage the MoveTo after the Go button is called.  
 *      If automating the moving of the stop when pushing the Next Cut button off the cut list
 *      have the Cut List option selected.   If manually entering the Move To values 
 *      have the Manual option selected.
*******************************************************************/
    void bMoveStopPopCallback (void *ptr)
      {
        char sMove_[10];
        uint32_t switch_;
        int   _steps = 0;
      
        sMiter.stop();
        swLeftRight.getValue(&switch_);
        DIRECTION = switch_;
        
        swTravel.getValue(&switch_);
        EXACT_RELATIVE = switch_;
        
        tMove.getText(sMove_, sizeof(sMove_));
        curPosInch = atof(sMove_);
        if (curPosInch <= LeftTravel)
          {  
            if (EXACT_RELATIVE == EXACT)              // if we are moving to an exact position or relative to current position
              {
                sMiter.moveTo (calcSteps(curPosInch));
    
                if (sMiter.currentPosition () < sMiter.targetPosition())
                  sMiter.setSpeed (workingMotorSpeed);
                else
                  sMiter.setSpeed(-workingMotorSpeed);
              }
            else        // switch is set to relative travel, have to check teh direction
              {
                if (DIRECTION == RIGHT)
                  {
                    cKerf.getValue(&switch_);
                    sMiter.moveTo (sMiter.currentPosition()-calcSteps(curPosInch));
                    if (switch_ == 1)
                      sMiter.moveTo (sMiter.currentPosition() -(calcSteps(curPosInch)+ calcSteps(kerf)));
                    sMiter.setSpeed(-workingMotorSpeed);
                  }
                else
                  {
                  sMiter.moveTo (sMiter.currentPosition() + calcSteps(curPosInch));
                  sMiter.setSpeed (workingMotorSpeed);
                  }
              }

            while ( sMiter.currentPosition() != sMiter.targetPosition() && !digitalRead(LEFT_SWITCH) && !digitalRead (RIGHT_SWITCH))
              {
                sMiter.runSpeed();
                _steps++;
              }   

            if (digitalRead(LEFT_SWITCH))
              bounceMotorOffLimit (LEFT_SWITCH, RIGHT, &sMiter);
            else if (digitalRead(RIGHT_SWITCH))
              bounceMotorOffLimit (RIGHT_SWITCH, LEFT, &sMiter);
            setPositionField();
          }
        else
          {
            strcpy(sMove_, "<0>");
            tMove.setText(sMove_);
          }
      }

/***********************************************************
 *  void bMoveStopPopCallBack (void *ptr)
 *      button called Next Cut which appears below the cut list table, when pressed, the UI 
 *      sets the move to value, and this call back will call the bMoveStopPopCallback
*******************************************************************/
    void bNextPopCallback (void *ptr)
      {
          bMoveStopPopCallback(&bMoveStop);
      }
 
/********************************************************
    void bSetPinsopCallback (void *ptr)

         Button bLeft component push callback function.
        While the button is pressed, the stop will move Left until the limit is hit.
        speed is already set from the slider, current position will be set on the Pop

 *****************************************************/
    void bSetPinsPopCallback (void *ptr)
      {
        uint32_t screenVal;
        int   eeAddress = 0;

        bSaveLocalPopCallback(&bSetPins);
        SaveSettingsEEPROM();

      }

/********************************************************
    void bSetZerolPopCallback (void *ptr)

         Setting the 0 position of the stop.   Used if there is a need to set a 0 point other than the
         blade.   Likely if stock is goign to be fed into the blade, instead of stock being measure from the 
         blade to the stop

 *****************************************************/
    void bSetZeroPopCallback (void *ptr)
      {
          long workSpeed_;

          workSpeed_ = workingMotorSpeed;
          sMiter.setCurrentPosition(0);
          sMiter.setSpeed (workingMotorSpeed);
          setPositionField();
  // MAKE SURE TO FIND LEFT STOP, AND RESET TRAVEL TO LEFT STOP FOR THE PARK BUTTON.   
      }

/********************************************************
    void bZeroToBladePopCallback (void *ptr)

         moves to the right until a INIT_PULLUP is hit.  Typically, an aligator clip on the blade, shorted
         against a metal end on the stop will cause the "stop to trigger" when the blade is touched.   Set the position
         to 0, and calculate the LEft limit off the max distance of travel.

 *****************************************************/
    void bZeroToBladePopCallback (void *ptr)
      {
        while (digitalRead (REZERO) && !digitalRead(RIGHT_SWITCH) && bGo )
          {
            sMiter.moveTo (sMiter.currentPosition() - 1);
            sMiter.setSpeed(-500);
            while (sMiter.currentPosition() != sMiter.targetPosition() && !digitalRead(RIGHT_SWITCH) && digitalRead (REZERO))
              sMiter.runSpeed();
            bGo = checkStopButton();
          }
        if (digitalRead(RIGHT_SWITCH))
            bounceMotorOffLimit (RIGHT_SWITCH, LEFT, &sMiter);
        
        sMiter.setCurrentPosition(0);
        setPositionField();
        nexSerial.write("vis bPark,1");
        FlushBuffer();
        nexSerial.write("vis t6,1");
        FlushBuffer();
        LeftLimitSteps = calcSteps (LeftTravel);
      }

  
  /*****************************************************************
  *   void bMovetoZeroPopCallback(void *ptr)
          used to move the stop to the previously set 0 mark
  ****************************************************************/
     
    void bMovetoZeroPopCallback (void *ptr)
      {

          if(sMiter.isRunning())
            sMiter.stop();
          sMiter.moveTo(0);
          sMiter.setSpeed(-13000);

          while (!digitalRead(LEFT_SWITCH) && !digitalRead (RIGHT_SWITCH) && sMiter.currentPosition () !=0 )
              {
                sMiter.runSpeed();  
                if (sMiter.currentPosition() <1000)
                  {
                    sMiter.moveTo(0);
                    sMiter.setSpeed (-200);
                  }   
              }
            
          if (digitalRead(LEFT_SWITCH) || digitalRead (RIGHT_SWITCH)) 
            {
              if (digitalRead(LEFT_SWITCH))
                bounceMotorOffLimit (LEFT_SWITCH, RIGHT, &sMiter);
              else if (digitalRead(RIGHT_SWITCH))
                bounceMotorOffLimit (RIGHT_SWITCH, LEFT, &sMiter);
            }
          setPositionField();
          sMiter.stop();
      }

/********************************************************
    void bSaveLocalPopCallback (void *ptr)

         Popcallback for save local button on settings screen.  Will translate the UI variables
         to global variables in the arduio called.    Is called from the push on the screen
         and from the SaveEEPROM button

 *****************************************************/
    void bSaveLocalPopCallback (void *ptr)
      {
        saveLocal();
      }

/************************************************************************
 *  void bCalibPopCallback( voice *ptr)
 *      
 *      button used to move 1000 steps to tighten the distance per step value
 *      on the push of the button, the stop will move 1000 steps left of current position
 *      each time.   allows for marked calculation of distancePerStep
********************************************************************************/
    void bCalibPopCallback( void *ptr)
      {
        
        int _steps = 5000;

        if(sMiter.isRunning())
          sMiter.stop();
        sMiter.moveTo (sMiter.currentPosition() + _steps);
        sMiter.setSpeed (500);

        while (!digitalRead(LEFT_SWITCH) && !digitalRead (RIGHT_SWITCH) && sMiter.currentPosition () != sMiter.targetPosition() )
            sMiter.run();     
          
        if (digitalRead(LEFT_SWITCH) || digitalRead (RIGHT_SWITCH)) 
          {
            if (digitalRead(LEFT_SWITCH))
              bounceMotorOffLimit (LEFT_SWITCH, RIGHT, &sMiter);
            else if (digitalRead(RIGHT_SWITCH))
              bounceMotorOffLimit (RIGHT_SWITCH, LEFT, &sMiter);
          }
        setPositionField();
        sMiter.stop();
        
      }

/**************************************************************************
 *    saveLocal()
 *      function to save the Settings screen to local variables.   called by SaveLocal button
 *      and SaveSettingEEPROM button
*********************************************************************************/
    void saveLocal ()
      {
        char  screenVal[24] = {"\0"};
        char *ender_;

        
        tUpVal.getText (screenVal, sizeof(screenVal));
        RIGHT_SWITCH = atoi(screenVal);
        tDownVal.getText(screenVal, sizeof(screenVal));
        LEFT_SWITCH = atoi(screenVal);
        tZeroPin.getText (screenVal, sizeof(screenVal));
        REZERO = atoi(screenVal);
        tEnablePin.getText (screenVal, sizeof(screenVal));
        enablePin = atoi(screenVal);
        tMicroSteps.getText (screenVal, sizeof(screenVal));
        microPerStep = atoi (screenVal);
        tAcceleration.getText (screenVal, sizeof(screenVal));
        maxAcceleration = atol(screenVal);
        tStepsPerRev.getText (screenVal, sizeof (screenVal));
        stepsPerRevolution = atoi (screenVal);
        tMaxSpeed.getText (screenVal, sizeof(screenVal));
        maxMotorSpeed = atol(screenVal);
        tWorkSpeed.getText(screenVal, sizeof(screenVal));
        workingMotorSpeed = atol(screenVal);
        tStepSize.getText ( screenVal, sizeof(screenVal));
        stepSize = atoi (screenVal);
        tKerf.getText( screenVal, sizeof(screenVal));
        //kerf=strtod(screenVal, &ender_);
        kerf = atof(screenVal);
        tLeftInch.getText(screenVal, sizeof(screenVal));
        LeftTravel = atof(screenVal);
        tdistPerStep.getText(screenVal, sizeof(screenVal));
        distPerStep = atof(screenVal);

      }
/****************************************************************************
        void  saveSettingsEEPROM()
            writes settings variables to EEPROM, starting at address 0
 * **************************************************************************/
    void SaveSettingsEEPROM()
      {
        int eeAddress = 0;

        EEPROM.put (eeAddress, RIGHT_SWITCH);
        eeAddress += sizeof(RIGHT_SWITCH);
        EEPROM.put(eeAddress, LEFT_SWITCH);
        eeAddress += sizeof(LEFT_SWITCH);
        EEPROM.put(eeAddress, REZERO);
        eeAddress += sizeof(REZERO);
        EEPROM.put(eeAddress, enablePin);
        eeAddress += sizeof(enablePin);
        EEPROM.put(eeAddress, microPerStep);
        eeAddress += sizeof(microPerStep);
        EEPROM.put (eeAddress, maxAcceleration);
        eeAddress += sizeof(maxAcceleration);
        EEPROM.put (eeAddress, stepsPerRevolution);
        eeAddress += sizeof(stepsPerRevolution);
        EEPROM.put (eeAddress, maxMotorSpeed);
        eeAddress += sizeof(maxMotorSpeed);
        EEPROM.put (eeAddress, workingMotorSpeed);
        eeAddress += sizeof(workingMotorSpeed);
        EEPROM.put (eeAddress, stepSize);
        eeAddress += sizeof( stepSize);
        EEPROM.put(eeAddress, kerf);
        eeAddress += sizeof(kerf);
        EEPROM.put(eeAddress, LeftTravel);
        eeAddress += sizeof(LeftTravel);
        EEPROM.put (eeAddress, distPerStep);
        eeAddress += sizeof(distPerStep);
      }

/****************************************************************************
        void  readSettingsEEPROM()
            reads all the pins and motor settings from EEPROM
            use EEPROM_WRITER_ROUTER.ino to put all the settings into EEPROM
            this will pull them back, and store into the global variables
 * **************************************************************************/
    void readSettingsEEPROM()
      {
        int  eeAddress = 0;

        EEPROM.get (eeAddress, RIGHT_SWITCH);
        eeAddress += sizeof(RIGHT_SWITCH);
        EEPROM.get(eeAddress, LEFT_SWITCH);
        eeAddress += sizeof(LEFT_SWITCH);
        EEPROM.get(eeAddress, REZERO);
        eeAddress += sizeof(REZERO);
        EEPROM.get(eeAddress, enablePin);
        eeAddress += sizeof(enablePin);
        EEPROM.get(eeAddress, microPerStep);
        eeAddress += sizeof(microPerStep);
        EEPROM.get (eeAddress, maxAcceleration);
        eeAddress += sizeof(maxAcceleration);
        EEPROM.get (eeAddress, stepsPerRevolution);
        eeAddress += sizeof(stepsPerRevolution);
        EEPROM.get (eeAddress, maxMotorSpeed);
        eeAddress += sizeof(maxMotorSpeed);
        EEPROM.get (eeAddress, workingMotorSpeed);
        eeAddress += sizeof(workingMotorSpeed);
        EEPROM.get (eeAddress, stepSize);
        eeAddress += sizeof( stepSize);
        EEPROM.get(eeAddress, kerf);
        eeAddress += sizeof(kerf);
        EEPROM.get (eeAddress, LeftTravel);
        eeAddress+= sizeof(LeftTravel);
        EEPROM.get (eeAddress, distPerStep);
        eeAddress += sizeof(distPerStep);

      }

/****************************************************
    void writeScreenAfterEEPROM ()

      set the arduio variables into the fields on the settings screen
      used on boot up to make sure the Arduio settings are stored in the
      UI and the settings screen is used to update the Arduino EEPROM
 * ******************************************************/
    void writeScreenAfterEEROM()
      {
        char sBuffer[120];
        char _temp[10];

        memset (sBuffer, '\0', sizeof (sBuffer));
        sprintf (sBuffer, "Settings.tUpVal.txt=\"%d\"", RIGHT_SWITCH );
        nexSerial.write (sBuffer);
        FlushBuffer();

        memset (sBuffer, '\0', sizeof (sBuffer));
        sprintf (sBuffer, "Settings.tDownVal.txt=\"%d\"", LEFT_SWITCH);
        nexSerial.write (sBuffer);
        FlushBuffer();

        memset (sBuffer, '\0', sizeof (sBuffer));
        sprintf(sBuffer, "Settings.tZeroPin.txt=\"%d\"", REZERO);
        nexSerial.write(sBuffer);
        FlushBuffer();

        memset (sBuffer, '\0', sizeof (sBuffer));
        sprintf(sBuffer, "Settings.tEnablePin.txt=\"%d\"", enablePin);
        nexSerial.write(sBuffer);
        FlushBuffer();

        memset (sBuffer, '\0', sizeof (sBuffer));
        sprintf(sBuffer, "Settings.tMicroSteps.txt=\"%d\"", microPerStep);
        nexSerial.write(sBuffer);
        FlushBuffer();

        memset (sBuffer, '\0', sizeof (sBuffer));
        sprintf(sBuffer, "Settings.tAcceleration.txt=\"%ld\"", maxAcceleration);
        nexSerial.write(sBuffer);
        FlushBuffer();

        memset (sBuffer, '\0', sizeof (sBuffer));
        sprintf(sBuffer, "Settings.tStepsPerRev.txt=\"%d\"", stepsPerRevolution);
        nexSerial.write(sBuffer);
        FlushBuffer();

        memset (sBuffer, '\0', sizeof (sBuffer));
        sprintf(sBuffer, "Settings.tMaxSpeed.txt=\"%ld\"", maxMotorSpeed);
        nexSerial.write(sBuffer);
        FlushBuffer();

        memset (sBuffer, '\0', sizeof (sBuffer));
        sprintf(sBuffer, "Settings.tWorkSpeed.txt=\"%ld\"", workingMotorSpeed);
        nexSerial.write(sBuffer);
        FlushBuffer();

        memset (sBuffer, '\0', sizeof (sBuffer));
        sprintf(sBuffer, "Settings.tStepSize.txt=\"%d\"", stepSize);
        nexSerial.write(sBuffer);
        FlushBuffer();

        memset (sBuffer, '\0', sizeof (sBuffer));
        dtostrf(kerf, 3, 3, _temp);
        sprintf(sBuffer, "Settings.tKerf.txt=\"%s\"", _temp);
        nexSerial.write(sBuffer);
        FlushBuffer();

        memset (sBuffer, '\0', sizeof (sBuffer));
        dtostrf(LeftTravel, 6, 3, _temp);
        sprintf(sBuffer, "Settings.tLeftInch.txt=\"%s\"", _temp);
        nexSerial.write(sBuffer);
        FlushBuffer();
        
        memset (sBuffer, '\0', sizeof (sBuffer));
        dtostrf(distPerStep, 12, 10, _temp);
        sprintf(sBuffer, "Settings.tdistPerStep.txt=\"%s\"", _temp);
        nexSerial.write(sBuffer);
        FlushBuffer();


      }

   /**************************************************************************************************/

    void setup()
      {
        uint32_t  scrVal;
        char sCommand[64];

                  /**
                 * Define nexSerial for communicate with Nextion touch panel. 
                 */
                    //#define nexSerial Serial3    // for the Arunino MEGA

        Serial.begin(1000000);
        delay (100);
   
        nexInit(115200);

        // Register the pop event callback function of the components
        bZeroToBlade.attachPop (bZeroToBladePopCallback, &bZeroToBlade);  //  homes stop to the blade, will want a grounding clip to connect to the blade
        btPower.attachPop(btPowerPopCallback, &btPower);                  //  turn the motor controller on and off by setting the enablePin
        bMoveStop.attachPop (bMoveStopPopCallback, &bMoveStop);           //  the Go button on the screen, moves the stop to the setpoint
        bNext.attachPop (bNextPopCallback, &bNext);                       //  calls the  button on the screen, moves the stop to the setpoint
        
        bMovetoZero.attachPop (bMovetoZeroPopCallback, &bMovetoZero);     //  moving the stop to the 0 zetpoint
        bRight.attachPush(bRightPushCallback, &bRight);                   //  moves the stop to the right
        bRight.attachPop (bRightPopCallback, &bRight);
        bLeft.attachPush (bLeftPushCallback, &bLeft);                     //  Moves the stop to the Left
        bLeft.attachPop (bLeftPopCallback, &bLeft);                       //  Moves the stop to the Left
        bPark.attachPop (bParkPopCallback, &bPark);                       //  Parks the stop leftmost positoin against the stop
        bSetZero.attachPop(bSetZeroPopCallback, &bSetZero);               //  sets the current position to the new 0 point
        bSetPins.attachPop (bSetPinsPopCallback, &bSetPins);              //  button to write the pin values from setting screens to EEPROM
        bSaveLocal.attachPop (bSaveLocalPopCallback, &bSaveLocal);        //  Button to save settings UI variables to arduino global working varaiables
        btPower.attachPop (btPowerPopCallback, &btPower);                 //  button for power to the stepper driver
        hMoveSpeed.attachPop (hMoveSpeedPopCallback,&hMoveSpeed);
        swTravel.attachPop (swTravelPopCallback, &swTravel);
        swLeftRight.attachPop(swLeftRightPopCallback, &swLeftRight);
        bCalib.attachPop(bCalibPopCallback, &bCalib);
 
        readSettingsEEPROM();         // read the config from EEPROM
        writeScreenAfterEEROM ();     // update the Settings screen with EEPROM values

        pinMode(LEFT_SWITCH, INPUT_PULLUP);          // Pin where the stop switch is wired PULLUP will define https://www.arduino.cc/en/Tutorial/InputPullupSerial
        pinMode(RIGHT_SWITCH, INPUT_PULLUP);
        pinMode(REZERO, INPUT_PULLUP);
        pinMode(enablePin, OUTPUT);
        pinMode(stepPin, OUTPUT);
        pinMode(directionPin, OUTPUT);
        //   set max speed and accelleration of the stepper motor

        sMiter.setMaxSpeed(maxMotorSpeed);
        sMiter.setAcceleration(maxAcceleration);
        sMiter.setMinPulseWidth(pulseWidthMicros);

        bGo = turnMotorOff( UP);                         // turns Router motor controller on, gives the torque necessary to hold the router in place
        sMiter.setSpeed (workingMotorSpeed);                     // set the initial router lift speed to the speed shown on the display
         if (digitalRead(LEFT_SWITCH) || digitalRead (RIGHT_SWITCH))    // check to see if the stepper motor is pegged aganist an end stop
            {
              if (digitalRead(LEFT_SWITCH))                             // if it is pegged, move it off the end stop
                bounceMotorOffLimit (LEFT_SWITCH, RIGHT, &sMiter);
              else if (digitalRead(RIGHT_SWITCH))
                bounceMotorOffLimit (RIGHT_SWITCH, LEFT, &sMiter);
            }
        sMiter.stop();
        setPositionField();
        memset (sCommand, '\0', sizeof(sCommand));
        
        nexSerial.write("vis bPark,0");
        FlushBuffer();
        nexSerial.write("vis t6,0");
        FlushBuffer();

      }

    void loop() 
      {
        // put your main code here, to run repeatedly:
        nexLoop(nex_listen_list);

      }