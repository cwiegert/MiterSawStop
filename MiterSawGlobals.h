#define LEFT 0    // 02_25_2023 -- once the motor is set, need to assess which is right and left, for bounce motor off limit
#define RIGHT 1     // 02_25_2023 -- once the motor is set, need to assess which is right and left, for bounce motor off limit
#define DOWN 1
#define UP 0
#define EXACT 0
#define RELATIVE 1

#define SD_WRITE 53
#define DEBUG 1       // 0 if no debug, 1 if debug.   change the value of this line to turn on the writeDebug feature
        
#if DEBUG == 1
#define debug(x) Serial.print (x)
#define debugLn(x) Serial.println(x)
#else
#define debug(x)
#define debugLn(x)
#endif
//#define FILE_WRITE (O_WRITE | O_READ | O_CREAT)   // redefine the file write function based on this thread   https://forum.arduino.cc/index.php?topic=616594.0


 
      
        /****************************************************************************
                 HIGH -- arduino constant for 1.   Used for turning the digital pins on and off.  Motor state, HIGH = ON
                 LOW  -- arduino constant for 0    Used for turning the digital pins on and off   Motor state, LOW = OFF
                          no need to define them here, they are already defined in libraries
        *********************************************************************************/
              
        int         LEFT_SWITCH;
        int         RIGHT_SWITCH;
        int         REZERO;
                
        /*
                   Code for the stepper motor controller.   Need to set the appropriate pins for the digital write and signal
                    These wires go to the stepper controller, not the stepper directely 
                    directionPin and stepPin must be set prior to the AccelStepper object create.   Can not set them through a config file
        */
        
        const  PROGMEM byte   directionPin = 11;             // pin that will go to the D+ on the stepper controller  have to declare here, as the stepper is a global class
        const  PROGMEM byte   stepPin = 13;                  // pin that will wire to the P+ (pulse) on the stepper controlloer   have to declare here as the stepper is a global class
        byte   enablePin;                                    //  pin that will wire to the E+ on the stepper controller.   Turns the controller on and off  LOW turns motor on, HIGH turns motor off
      //  int    initSpeed = 2000;                            //  sets the initial speed of the motor.   Don't think we need acceleration in the router, but will find out
        long   maxMotorSpeed;                               //  as defined - maximum speed motor will run.   Sets the 100% on the speed slider
        long   maxAcceleration;                      //  maximum number of steps for acceleration
        long   workingMotorSpeed;                           //  active working speed, set by the slider, and will be somewhere between 0 and 100%
        int    stepsPerRevolution = 1600;     //  number of steps required for 1 revolution of leadscrew
        int    microPerStep =   8;            //  number of microSteps.   Set on the TB600 controller by dip switch   Off | On | Off
        //float  distPerStep = 0.00024606;      //  inches per step with calculated off <microPerStep> = 8
        //float  distPerStep = 0.00049212;      //  inches per step with calculated off <microPerStep> = 8
        float   distPerStep = 0.00073920;
        float   LeftTravel;     // distance from teh blade to the left limit switch
        
        const  PROGMEM int    pulseWidthMicros = 20;         //  miliseconds  -- delay for the steps.   the higher the number the slower the lead screw will turn
        const  PROGMEM int    millisBetweenSteps = 20;       //  milliseconds - or try 1000 for slower steps   delay between pulses sent to the controller
        long   curPos;                        //  return value from CurrentPosition() call on the stepper
        byte   DIRECTION;                     //  tests the direction the stop should be moving
        byte   EXACT_RELATIVE;                //  used to set the wether to move exact distance or relative off current
        byte   HOME_MOTOR = 1;                //  Used with the swFence switch to tell which motor to use when zeroing and resetting
        byte   automatedMove;                 //  value set by the cut list switch, determines if the input field is active for the Move To option
        int    stepSize = 3;                 //  used in the moving of the router to set precision on how large the steps should be for the runToPosition() function
        //const  PROGMEM char   CR = '\n';                     //  Carraige return constant, used for the writeDebug function to add a new line at the end of the function
        char   preSetTxt[8];                 //  text set in the pre-set combo box.   
        int    preSetNum;                    //  index of the pre-set values.   used when reading the Nextion variable which gets set when a value is selected
        long   LeftLimitSteps;                      //  step value when the lower limit switch is hit
        long   highLimit;                     //  step value when the upper limit switch is hit
        float   curPosInch;
        float   kerf;                         //  kerf of the blade
        char    posInch[16] = {'\0'};         //  string to hold inch position for moving slider
         SdFat   sdCard;                       //  pointer to the SD card reader.   Used in storing config and memory files
        uint32_t   bGo = DOWN;                    //  used as a flag to test if the stop button has been pressed.
       // char    storeFile[25] = {'\0'};       //  name of file where the memory will be stored
        int     eeAddress;
      //  char    ver[21] = {'\0'};
        int     boardMemory;

/***********************************************************************************
    Setting up an array of structures to hold the config to do a lookup for # of steps required to move a specific distance
    
    <lookup value>, <inch value>, <decimal equivalent>, <baseline steps>

    Baseline steps are based on a 200 steps per turn, on a 1.8 degree pitch, with a lead of 10mm Leed (10mm traveled per turn)
    and no microsteps
                      steps per turn * microsteps*4 (phases)                          inches
                      ---------------------------  = # of steps per mm to travel     --------  =  # of steps per inch travel
                          distance traveled                                         25.4 mm/inch

                          multiply the basics steps by 8, to get the 8 microsteps
************************************************************************************************/
    struct inchCacl {
          int    index;
          char   inches[6];
          float  decimal;
          float  steps;
          char   label[30];
        }  preSetLookup; 
        
        
/**********************
    My easy way to write to the Serial debugger screen
 **********************/
        
        void writeDebug (String toWrite, byte LF)
        {
          if (LF)
          {
            debugLn(toWrite);
          }
          else
          {
            debug(toWrite);
          }
        
        }
 
 