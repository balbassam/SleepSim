//================================================== file = prcToRes.c ======
//=  Program to determine sleep time for an output of vecToPrc data series  =
//=   - Dual time-out policy (two time-out values for each 24 hours day)    =
//===========================================================================
//=  Notes:                                                                 =
//=    1) Input from input file "in.prc" as parameter(see example below)    =
//=    2) Parameters for system wattage is in the name of "in.prc" file     =
//=        in the format "id, name, on, off"where on is wattage consumption =
//=        while CPU is on, and off is wattage consuption while in sleep    =
//=    3) Output is stored in file "name.res"                               =
//=    4) Output is in the format "savings,percent,dollars,wakeups"         =
//=         Where savings is the KWh savings of the policy                  =
//=               percent is the percentage of watts saved                  = 
//=               dollars is the monetary savings                           = 
//=               wakeups is the total number of forced wakeups             = 
//=                                                                         = 
//=    5) Input is of format AAAUUUSSSIIIZOOO...                            =
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
//=    9) Ignore warnings on build                                          =
//=-------------------------------------------------------------------------=
//=  Build: bcc32 prcToRes.c                                                =
//=-------------------------------------------------------------------------=
//=  Execute: prcToRes.exe in.prc                                           =
//=-------------------------------------------------------------------------=
//=  Author: Bader AlBassam                                                 =
//=          University of South Florida                                    =
//=          WWW: http://www.csee.usf.edu/~bader                            =
//=          Email: bader@mail.usf.edu                                      =
//=-------------------------------------------------------------------------=
//=  History: BTB (08/17/12) - Genesis (from sleepSim3.c)                   =
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
#define PRICEPERKWH 0.09           // Dollar Price of each KWh consumed

//----- Globals -------------------------------------------------------------
char X[MAX_SIZE];                  // Time series read from "in.prc"
int  N;                            // Number of values in "in.prc"
FILE *InFile;                      // "in.prc" file
int  AoffTime;                     // Total minutes computers already off
int  AsleepTime;                   // Total mintues computers already sleep

//----- Prototypes ----------------------------------------------------------
// Function to load X[] and determine N
void loadX(void);
// Compute sleep time
void computeSleep(int *sleepTime, int *wakeUpCount); 
// Computes wattage Savings in percent
double computeSavingsPercent(int sleepTime, int sleepWatts, int activeWatts); 
// Computes wattage Savings in watts
double computeSavingsWatts(int sleepTime, int sleepWatts, int activeWatts);
//Sets parameters
void getParameters(char* line, float **parameters, char *outFileName);

//===========================================================================
//=  Main program                                                           =
//===========================================================================
int main(int argc, char *argv[])
{
  float    *parameters[NUMPARAMETERS]; // Array of parameters
  int      timeOut1, timeOut2;         // Inactivity timeout values
  int      timeOutCurrent;             // Current timeout value
  int      time1, time2;               // Inactivity timeout change times
  int      dailyTime;                  // Time from last midnight
  int      idleState;                  // Flag for idle state
  int      idleCount;                  // Counter for idle state
  int      wakeUpCount;                // Counter for wake-up events
  int      sleepTime;                  // Total sleep time
  float    activeWatts;                // Consumption while on 
  float    sleepWatts;                 // Consumption while sleep 

  char     dataFile[255];              // Name of in.prc file
  char     outFileName[255];           // Name of .res file
  char     computerName[250];          // Name of computer used for outputFile
  char     params[128];                // Parameters from first line of file
  FILE     *procFile;                  // .prc file

  int      i;                          // Loop counter

  // Initialize default values
  activeWatts = 100;    // 100 Watts active consumption
  sleepWatts = 0;       // 0 Watts idle consumption
  timeOut1 = 45;        // 45 minutes midnight to 8am and 5pm to midnight
  timeOut2 = 120;       // 2 hours 8am to 5pm
  time1 = 480;          // 8 am
  time2 = 1020;         // 5 pm

  // check for command line arguments
  if(argc != 2)
  {
    fprintf(stdout, "usage %s inputfile\n", argv[0]);
    return -1;
  }
  else
    strncpy (dataFile,argv[1], 255); // read from in.prc file

  InFile = fopen(dataFile,"r");  // Open files for data
  if(InFile == NULL)
  {
    fprintf(stdout, "*** ERROR - \tCannot read file %s\n", argv[1]);
    return -1;
  }
 
  //Fill Parameter array
  parameters[0] = &activeWatts;
  parameters[1] = &sleepWatts;

  //Read first line of in.prc file for parameters and set them accordingly
  fgets(params, 128, InFile);
  getParameters(params, parameters, outFileName);

  //Get the name of the computer and open a new file (name.res) for writing
  strncpy(computerName,outFileName,250);

  //Add file extension
  strncat(outFileName,".res",5);

  //Open file for write
  procFile = fopen(outFileName,"w");
  if(procFile == NULL)
  {
    fprintf(stdout, "*** ERROR - \tCannot write to file %s\n",outFileName );
    return -1;
  }

  // Load X and determine N
  loadX();

  // Determine total sleep time and number of forced wake-ups
  computeSleep(&sleepTime, &wakeUpCount);

  //-----------Output to .res file-------------------------------------------
  //Savings in KWh
  fprintf(procFile,"%.2f,",
    computeSavingsWatts(sleepTime, sleepWatts, activeWatts) ); 

  //Savings % 
  fprintf(procFile,"%.2f,",
    computeSavingsPercent(sleepTime, sleepWatts,activeWatts) ); 

  //Saving in dollars
  fprintf(procFile,"%.2f,",
    PRICEPERKWH * computeSavingsWatts(sleepTime, sleepWatts, activeWatts) );

  // Number of forced wakeups recorded
  fprintf(procFile,"%d\n",wakeUpCount); 

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
    X[i] = value;
    i++;
  }
  N = i;

  return;
}

//---------------------------------------------------------------------------
//-  Determine total sleep time and number of forced wake-ups               -
//---------------------------------------------------------------------------
void computeSleep(int *sleepTime, int *wakeUpCount)
{
  int      idleState;              // Flag for idle state
  int      i;                      // Loop counter

  //NOTE!!!
  //Forced wakeups are {Z,S}->{I,A,U} 
 // Loop to determine total sleep time and number of forced wake-ups
  *sleepTime = *wakeUpCount = 0;
  idleState = TRUE;
  for (i=0; i<N; i++)
  {
    // Determine total time Computer was already asleep or off
    if (X[i] == 'S')
      ++AsleepTime;

    if (X[i] == 'O')
      ++AoffTime;

    // Determine if start of next busy period
    if (((X[i] == 'A') || (X[i] == 'U') || (X[i] == 'I')) && (idleState == TRUE))
    {
      idleState = FALSE;
      *wakeUpCount = *wakeUpCount + 1;
    }

    // Determine if in an idle period
    if ((X[i] == 'S') || (X[i] == 'Z'))
      idleState = TRUE;

    // Tally the sleep
    if (X[i] == 'Z')
      *sleepTime = *sleepTime + 1;
  }
  return;
}

//---------------------------------------------------------------------------
//-  Determine total percent wattage savings based on sleepTime             -
//---------------------------------------------------------------------------
double  computeSavingsPercent(int sleepTime, int sleepWatts, int activeWatts)
{
  double S;              // Holds percentage of sleepTime
  double eq1, eq2;		 //two equations to calculate wattae savings
  
  //Calculate total savings
  //eq1 is prior to policy consumption
  eq1 = ( N - AoffTime - AsleepTime ) * activeWatts + AsleepTime * sleepWatts;
  //eq1 is post policy consumption
  eq2 = ( N - AoffTime - AsleepTime  - sleepTime) * activeWatts +
	  (AsleepTime + sleepTime) * sleepWatts;

  S = eq1 - eq2;

  return 100.0 * (S / eq1);
}

//---------------------------------------------------------------------------
//-  Determine total wattage savings based on sleepTime                     -
//---------------------------------------------------------------------------
double  computeSavingsWatts(int sleepTime, int sleepWatts, int activeWatts)
{
  double S;              // Holds percentage of sleepTime
  double eq1, eq2;		 //two equations to calculate wattae savings
  
  //Calculate total savings
  //eq1 is prior to policy consumption
  eq1 = ( N - AoffTime - AsleepTime ) * activeWatts + AsleepTime * sleepWatts;
  //eq1 is post policy consumption
  eq2 = ( N - AoffTime - AsleepTime  - sleepTime) * activeWatts +
	  (AsleepTime + sleepTime) * sleepWatts;

  S = eq1 - eq2;

  S = S / (60 * 1000); // Convert to KWh

  return S;
}


//---------------------------------------------------------------------------
//-  Sets data based on parameters obtained from first line of in.prc       -
//---------------------------------------------------------------------------
void getParameters(char* line, float **parameters, char *outFileName)
{
  char     *tokens;                     //used for splitting strings
  char     *tokenHolder;                //Pointer to last token found
  int      i;                           //loop counter

  //Fill as many parameters as possible
  tokens = ","; 
  tokenHolder = strtok(line,tokens);

  //Grab the device name parameter
  //Leave room for extension in outFileName
  strncpy(outFileName,tokenHolder,250);

  //Start obtaining the rest of the parameters
  tokenHolder = strtok(NULL,tokens);

  for(i = 0; i < NUMPARAMETERS ; ++i)
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
