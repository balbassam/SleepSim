//================================================== file = vecToprc.c ======
//=  Program to determine sleep time for an vecToPrc data series            =
//=   - Dual time-out policy (two time-out values for each 24 hours day)    =
//===========================================================================
//=  Notes:                                                                 =
//=    1) Input from input file "in.vec" as parameter(see example below)    =
//=    2) Parameters for system wattage is in the name of "in.prc" file     =
//=        in the format "id, name, on, off"where on is wattage consumption =
//=        while CPU is on, and off is wattage consuption while in sleep    =
//=    3) Output is stored in file "name.res"                               =
//=    4) Input is of format AAAUUUSSSIIIOOO...                             =
//=       where an "A" signifies that the computer was active               =
//=       "O" signifies that the computer was off                           =
//=       "S" signifies that the computer was sleeping                      =
//=       "I" signifies that the computer was idle                          =
//=       "U" signifies states which are unknown                            =
//=    5) Output is of format AAAUUUSSSIIIZOOO...                           =
//=       where an "A" signifies that the computer was active               =
//=       "O" signifies that the computer was off                           =
//=       "S" signifies that the computer was sleeping                      =
//=       "I" signifies that the computer was idle                          =
//=       "U" signifies states which are unknown                            =
//=       "Z" signifies states which are enforced sleep                     =
//=    6) It assumed that the data starts at midnight (time = 0 minutes)    =
//=    7) Must initialize timeOut1 and timeOut2 to desired values           =
//=    8) Must initialize time1 and time2 to desired values where from time =
//=       0 to time1 timeOut1 is used, from time1+1 to time2 timeout2 is    =
//=       is used, and from timeout2+1 to 1440 timeout1 is used for each    =
//=       day.                                                              =
//=    9) Must initialize wakeUpTime to time that the computer should be    =
//=       woken up by magic packet. Set to -1 to prevent wake up            =
//=   10) Ignore warnings on build                                          =
//=-------------------------------------------------------------------------=
//=  Build: bcc32 sleepSim3.c                                               =
//=-------------------------------------------------------------------------=
//=  Execute: sleepSim3 in.vec                                              =
//=-------------------------------------------------------------------------=
//=  Author: Bader AlBassam                                                 =
//=          University of South Florida                                    =
//=          WWW: http://www.csee.usf.edu/~bader                            =
//=          Email: bader@mail.usf.edu                                      =
//=-------------------------------------------------------------------------=
//=  History: BTB (08/17/12) - Genesis (from sleepSim3.c)                   =
//=         : BTB (09/24/12) - Added wake up from sleep capability          =
//=         : BTB (10/02/12) - Added many power policy capability           =
//===========================================================================
//----- Include files -------------------------------------------------------
#include <stdio.h>                 // Needed for printf() and feof()
#include <string.h>                // Needed for strtok()
#include <stdlib.h>                // Needed for exit()

//----- Defines -------------------------------------------------------------
#define    FALSE       0           // Boolean false
#define     TRUE       1           // Boolean true
#define   ONEDAY    1440           // Number of minutes in one day
#define MAX_SIZE 1000000           // Maximum size of input data in minutes
#define NUMPARAMETERS  2           // Numer of parameters used

typedef struct PowerPolicy {
    int timeOut1;                  // First timeout value
    int timeOut2;                  // Second timeout value
    
    int time1;                     // Timeout change time
    int time2;                     // Second timeout change time

    int wakeUpTime;                // When the PC should wake if.

} Policy;

//----- Globals -------------------------------------------------------------
char X[MAX_SIZE];                  // Time series read from "in.vec"
int  N;                            // Number of values in "in.vec"
FILE *InFile;                      // "in.vec" file
int  AoffTime;                     // Total minutes computers already off
int  AsleepTime;                   // Total mintues computers already sleep

//----- Prototypes ----------------------------------------------------------
// Function to load X[] and determine N
void loadX(void);
// Output X vector
void outputX(FILE *outPutFile);
// Sets parameters
void getParameters(char* line, float **parameters, char *outFileName);
// Wakes up from poweroff by use of Magic Packet
void wakeUpDevice(int position, int timeOut);

//===========================================================================
//=  Main program                                                           =
//===========================================================================
int main(int argc, char *argv[])
{
  float    *parameters[NUMPARAMETERS]; // Array of parameters

  Policy   weekDayPolicy;              // Power policy for weekdays
  Policy   weekEndPolicy;              // Power policy for weekends
  Policy*  activePolicy;               // Active policy for main loop

  int      timeOutCurrent;             // Current timeout value
  int      dailyTime;                  // Time from last midnight
  int      dayCounter;                 // Days simulation has run for
  int      idleState;                  // Flag for idle state
  int      idleCount;                  // Counter for idle state
  int      wakeUpCount;                // Counter for wake-up events
  int      sleepTime;                  // Total sleep time
  float    activeWatts;                // Consumption while on 
  float    sleepWatts;                 // Consumption while sleep 

  char     dataFile[255];              // Name of in.vec file
  char     outFileName[255];           // Name of .prcfile
  char     computerName[250];          // Name of computer used for outputFile
  char     params[128];                // Parameters from first line of file
  FILE     *procFile;                  // .prc file

  int      i;                          // Loop counter

  // Initialize default values
  activeWatts = 100;    // 100 Watts active consumption
  sleepWatts = 0;       // 0 Watts idle consumption

  // Setup policy for weekdays
  weekDayPolicy.timeOut1    = 45;      // 45 minutes midnight to 8am and 6pm to midnight
  weekDayPolicy.timeOut2    = 480;     // 8 hours 8am to 6pm
  weekDayPolicy.time1       = 480;     // 8 am
  weekDayPolicy.time2       = 1080;    // 6 pm
  weekDayPolicy.wakeUpTime  = 480;     // 8 am

  // Setup policy for weekends
  weekEndPolicy.timeOut1    = 45;      // 45 minutes midnight to 8am and 6pm to midnight
  weekEndPolicy.timeOut2    = 45;      // 45 minutes 8am to 6pm
  weekEndPolicy.time1       = 480;     // 8 am
  weekEndPolicy.time2       = 480;     // 6 pm, same as time1 so doesn't matter
  weekEndPolicy.wakeUpTime  = -1;      // don't wake up


  // check for command line arguments
  if(argc != 2)
  {
    fprintf(stdout, "usage %s inputfile\n", argv[0]);
    return -1;
  }
  else
    strncpy (dataFile,argv[1], 255); // read from in.vec file

  InFile = fopen(dataFile,"r");  // Open files for data
  if(InFile == NULL)
  {
    fprintf(stdout, "*** ERROR - \tCannot read file %s\n", argv[1]);
    return -1;
  }
 
  //Fill Parameter array
  parameters[0] = &activeWatts;
  parameters[1] = &sleepWatts;

  //Read first line of file for parameters and set accordingly
  fgets(params, 128, InFile);
  getParameters(params, parameters, outFileName);

  //Get the name of the computer and open a new file (name.res) for writing
  strncpy(computerName,outFileName,250);

  //Add file extension
  strncat(outFileName,".prc",5);
  
  //Open file for write
  procFile = fopen(outFileName,"w");
  if(procFile == NULL)
  {
    fprintf(stdout, "*** ERROR - \tCannot write to file %s\n" ,outFileName );
    return -1;
  }

  // Load X and determine N
  loadX();

  // Start at a weekday
  activePolicy = &weekDayPolicy;

  // Will be incremented to 0 in beginning of simulation loop
  dayCounter = -1;

  // ****************** Main simulation loop ****************
  idleState = FALSE;
  idleCount = 0;
  for (i=0; i<N; i++)
  {
    // Set dailyTime to zero when cross midnight
    if ((i % ONEDAY) == 0) 
    {
      dailyTime = 0;
      dayCounter ++;
      dayCounter = dayCounter %7;
      printf("%d\n",dayCounter);
    }

    // Set plan for day of week
    if (dayCounter == 1 || dayCounter == 2)
    {
      // Saturday and Sunday are weekends
      activePolicy = &weekEndPolicy;
    }
    else
    {
      // Every other day of the week
      activePolicy = &weekDayPolicy;
    }

    // Determine if start of next idle period
    if ((X[i] == 'I')  && (idleState == FALSE))
      idleState = TRUE;

    // Determine if start of next busy period
    if ((X[i] == 'A') || (X[i]  == 'U') || (X[i] == 'O') || (X[i] == 'S'))
    {
      idleState = FALSE;
      idleCount = 0;
    }

    // Execute the timeout while in an idle period
    if (idleState == TRUE)
    {
      if ((dailyTime <= activePolicy->time1) || (dailyTime > activePolicy->time2))
       timeOutCurrent = activePolicy->timeOut1;
      else
       timeOutCurrent = activePolicy->timeOut2;

      
      //set timeout for the next policy
      if ((dailyTime == activePolicy->time2 + 1) || (dailyTime == activePolicy->time1 + 1))
      {
        // Stay asleep if already been asleep
        if (X[i-1] == 'Z')
          idleCount = timeOutCurrent;
        else //If computer wasn't in a forced sleep keep it awake
            idleCount = 0;
      }


      // Put computer to sleep if timout has been triggered
      if (idleCount >= timeOutCurrent)
        X[i] = 'Z';
      else
        idleCount++;
    }

    //Wake up at beginning of the day
    if (dailyTime == activePolicy->wakeUpTime)
    {
      idleState  = FALSE;
      idleCount  = 0;
      wakeUpDevice(i,timeOutCurrent);
    }

    // Increment dailyTime
    dailyTime++;
  }

  //Include parameter line in procFile
  fprintf(procFile,"%s,",computerName);
  fprintf(procFile,"%f,",*parameters[0]);
  fprintf(procFile,"%f\n",*parameters[1]);

  // Output input vector
  outputX(procFile);

  //close file pointers
  fclose(procFile);
  return 0;
}

//---------------------------------------------------------------------------
//-  Load X and determine N                                                 -
//---------------------------------------------------------------------------
void loadX()
{
  char     value;                  // Value read-in
  int      i;                      // Loop counter

  // Load the series X and determine N
  i = 0;
  while(1)
  {
    value = fgetc(InFile);
    if (feof(InFile) || (value == '\n')) break;
    if (value == 'O')        // Off 
      X[i] = 'O';
    else if (value == 'S')   // Sleep
      X[i] = 'S';
    else if (value == 'I')   // Idle
      X[i] = 'I';
    else if (value == 'A')   // Active
      X[i] = 'A';
    else if (value == 'U')   // Unknown
      X[i] = 'U';
    else
    {
      printf("*** ERROR - illegal entry in input = %d (decimal)", value);
      exit(-1);
    }
    i++;
  }
  N = i;

  return;
}

//---------------------------------------------------------------------------
//-  Output X vector                                                        -
//---------------------------------------------------------------------------
void outputX(FILE *outPutFile)
{
  int      i;                      // Loop counter

  for (i=0; i<N; i++)
    fprintf(outPutFile,"%c", X[i]);
}

//---------------------------------------------------------------------------
//-  Sets data based on parameters obtained from first line of in.vec       -
//---------------------------------------------------------------------------
void getParameters(char* line, float **parameters, char *outFileName)
{
  char     *tokens;                     //used for splitting strings
  char     *tokenHolder;                //Pointer to last token found
  int      i;                           //loop counter

  //Fill as many parameters as possible
  //Skip the first parameter which only has device i.d.
  tokens = ", "; 
  tokenHolder = strtok(line,tokens);

  //Grab the device name parameter
  tokenHolder = strtok(NULL,tokens);
  
  //Leave room for extension in outFileName
  strncpy(outFileName,tokenHolder,250);

  //Start obtaining the rest of the parameters
  tokenHolder = strtok(NULL,tokens);

  for(i = 0; i < NUMPARAMETERS; ++i)
  {
     if( tokenHolder == NULL )
     {
       //Warn user that defaults will be set
       printf("*** WARNING - No parameters are set after %d\n", i);
       break;
     }

     //Grab the next parameter
     *parameters[i] = atof(tokenHolder);
     tokenHolder = strtok(NULL,tokens);
  }
}

//---------------------------------------------------------------------------
//-  Force wakes up a pc from an off state.                                 -
//---------------------------------------------------------------------------
void wakeUpDevice(int position, int timeOut)
{
  int      i;                      // Loop counter

  //loop to turn pc on if it was already off for the duration of timeOut - 1
  for(i = 0; i < timeOut; ++i)
  {
    //Check if the PC is asleep
    if( X[position + i] == 'Z' ||
        X[position + i] == 'S' ||
        )
    {
      X[position + i] = 'I'; //When awoken, the PC is assumed idle
    }
    else
      break;                 //If was not in sleep, then stop overwriting 
  }
}

