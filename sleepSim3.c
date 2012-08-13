//================================================== file = sleepSim3.c =====
//=  Program to determine sleep time for an idleCollect data series         =
//=   - Dual time-out policy (two time-out values for each 24 hours day)    =
//===========================================================================
//=  Notes:                                                                 =
//=    1) Input from input file "in.dat" as parameter(see example below)    =
//=    2) Parameters for system wattage is in the name of the "in.dat" file =
//=        in the format "in_on_off.dat" where on is wattage consumption    =
//=        while CPU is on, and off is wattage consuption while in sleep    =
//=    3) Output is stored in files "usage.txt" and "wattage.txt"           =
//=    4) usage holds output of sleep time                                  =
//=    5) wattage holds wattage savings                                     =
//=    6) Input is of format AAAUUUSSSIIIOOO...                             =
//=       where an "A" signifies that the computer was active               =
//=       "O" signifies that the computer was off                           =
//=       "S" signifies that the computer was sleeping                      =
//=       "I" signifies that the computer was idle                          =
//=       "U" signifies states which are unknown                            =
//=    7) It assumed that the data starts at midnight (time = 0 minutes)    =
//=    8) Must initialize timeOut1 and timeOut2 to desired values           =
//=    9) Must initialize time1 and time2 to desired values where from time =
//=       0 to time1 timeOut1 is used, from time1+1 to time2 timeout2 is    =
//=       is used, and from timeout2+1 to 1440 timeout1 is used for each    =
//=       day.                                                              =
//=   10) Must initialize VERBOSE to TRUE or FALSE                          =
//=   11) Ignore warnings on build                                          =
//=-------------------------------------------------------------------------=
//= Example "in.dat" file:                                                  =
//=  OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO    =
//=  OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOAAAAAAAAAAAAAAAAAAAAIIIIIIIIIIIIII    =
//=  IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII    =
//=  IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII    =
//=  IIIIIIIIIIIIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    =
//=-------------------------------------------------------------------------=
//= Example output and files                                                =
//= (for above "in.dat" and timeOut1 = 45, timeOut2 = 120,                  =
//= time1 = 480, time2 = 1020, on wattage = 50, off wattage = 10):          =
//=------------------------------------------- sleepSim3.c -----            =
//=-  timeOut value #1             = 45 minutes                             =
//=-  timeOut value #2             = 120 minutes                            =
//=-  time to switch #1            = 480 minutes                            =
//=-  time to switch #2            = 1020 minutes                           =
//=---------------------------------------------------------------          =
//=-  Total time                   = 360 minutes                            =
//=-  Resulting sleep time         = 115 minutes                            =
//=-  Resulting number of wake-ups = 2 events                               =
//=---------------------------------------------------------------          =
//=360 115 2   sleep   = 31.944444 % of total time                          =
//=            savings = 25.555556 % of total watts                         =
//=                                                                         =
//= "wattage" file                                                          =
//=     25.56                                                               =
//= "usage" file                                                            =
//=     31.944444                                                           =
//=-------------------------------------------------------------------------=
//=  Build: bcc32 sleepSim3.c                                               =
//=-------------------------------------------------------------------------=
//=  Execute: sleepSim3 in.dat                                              =
//=-------------------------------------------------------------------------=
//=  Author: Bader AlBassam                                                 =
//=          University of South Florida                                    =
//=          WWW: http://www.csee.usf.edu/~bader                            =
//=          Email: bader@mail.usf.edu                                      =
//=-------------------------------------------------------------------------=
//=  History: BTB (08/08/12) - Genesis (from sleepSim1.c)                   =
//===========================================================================
//----- Include files -------------------------------------------------------
#include <stdio.h>                 // Needed for printf() and feof()
#include <string.h>                // Needed for strtok()
#include <stdlib.h>                // Needed for exit()

//----- Defines -------------------------------------------------------------
#define    FALSE       0           // Boolean false
#define     TRUE       1           // Boolean true
#define   ONEDAY    1440           // Number of minutes in one day
#define  VERBOSE   FALSE           // Verbose mode flag
#define MAX_SIZE 1000000           // Maximum size of input data in minutes

//----- Globals -------------------------------------------------------------
char X[MAX_SIZE];                  // Time series read from "in.dat"
int  N;                            // Number of values in "in.dat"
FILE *InFile;                      // "in.dat" file
FILE *WattageFile;                 // holds wattage Savings    
FILE *UsageFile;                   // holds usage Savings
int  ActiveWatts;                  // Consumption while on 
int  SleepWatts;                   // Consumption while sleep 
int  AoffTime;                     // Total minutes computers already off
int  AsleepTime;                   // Total mintues computers already sleep

//----- Prototypes ----------------------------------------------------------
void loadX(void);                 // Function to load X[] and determine N
void outputX(void);               // Output X vector
void computeSleep(int *sleepTime, int *wakeUpCount); // Compute sleep time
// Computes wattage Savings
double  computeSavings(int sleepTime, int sleepWatts, int activeWatts); 

//===========================================================================
//=  Main program                                                           =
//===========================================================================
int main(int argc, char *argv[])
{
  //
  int      timeOut1, timeOut2;     // Inactivity timeout values
  int      timeOutCurrent;         // Current timeout value
  int      time1, time2;           // Inactivity timeout change times
  int      dailyTime;              // Time from last midnight
  int      idleState;              // Flag for idle state
  int      idleCount;              // Counter for idle state
  int      wakeUpCount;            // Counter for wake-up events
  int      sleepTime;              // Total sleep time
  
  char     *dataFile;              //Name of in.dat file

  char     *tokens;                //used for splitting strings
  char     *tokenHolder;
  int      i;                      // Loop counter

  // Initialize timeout and time values
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
    dataFile = argv[1]; // read from in.dat file

  InFile = fopen(dataFile,"r");  // Open files for data
  if(InFile == NULL)
  {
    fprintf(stdout, "ERROR!\tCannot read file %s\n", argv[1]);
    return -1;
  }

  //splitFile with tokens to obtain ActiveWatts and SleepWatts from name of file
  tokens = "\\_."; //tokens used to split argv[1]
  tokenHolder = (char*) strtok(dataFile,tokens);
  SleepWatts = atoi(tokenHolder);
  while ((tokenHolder != NULL) && (strcmp(tokenHolder,"vec")))
  {
    ActiveWatts = SleepWatts;
    SleepWatts = atoi(tokenHolder);
    tokenHolder = (char*) strtok(NULL,tokens);
  }

  //output files to hold savings data
  WattageFile = fopen("wattage.txt","a");
  if(WattageFile == NULL)
  {
    fprintf(stdout, "ERROR!\tCannot read file %s\n","wattage.txt" );
    return -1;
  }
  UsageFile = fopen("usage.txt", "a");
  if(UsageFile == NULL)
  {
    fprintf(stdout, "ERROR!\tCannot read file %s\n", "usage.txt");
    return -1;
  }

  // Output banner
  printf("------------------------------------------- sleepSim3.c -----\n");

  // Load X and determine N
  loadX();

  // Output input vector if verbose mode
  if (VERBOSE == TRUE) outputX();

  // ****************** Main simulation loop ****************
  idleState = FALSE;
  for (i=0; i<N; i++)
  {
    // Set dailyTime to zero when cross midnight
    if ((i % ONEDAY) == 0) dailyTime = 0;

    // Determine if start of next idle period
    if (((X[i] == 'I') ) && (idleState == FALSE))
      idleState = TRUE;

    // Determine if start of next busy period
    if ((X[i] == 'A') || (X[i]  == 'U'))
    {
      idleState = FALSE;
      idleCount = 0;
    }

    // Execute the timeout while in an idle period
    if (idleState == TRUE)
    {
      if ((dailyTime <= time1) || (dailyTime > time2))
       timeOutCurrent = timeOut1;
      else
       timeOutCurrent = timeOut2;

      if ((dailyTime == time1) || (dailyTime == time2))    // Should this be +1 ????????????
        idleCount = 0;

      if (idleCount < timeOutCurrent)
      {
        X[i] = 'A';
        idleCount++;
      }
    }

    // Increment dailyTime
    dailyTime++;
  }
  // ********************************************************

  // Determine total sleep time and number of forced wake-ups
  computeSleep(&sleepTime, &wakeUpCount);

  // Output input vector if verbose mode
  if (VERBOSE == TRUE) outputX();

  // Output results
  printf("-  timeOut value #1             = %d minutes \n", timeOut1);
  printf("-  timeOut value #2             = %d minutes \n", timeOut2);
  printf("-  time to switch #1            = %d minutes \n", time1);
  printf("-  time to switch #2            = %d minutes \n", time2);
  printf("---------------------------------------------------------------\n");
  printf("-  Total time                   = %d minutes \n", N);
  printf("-  Resulting sleep time         = %d minutes \n", sleepTime);
  printf("-  Resulting number of wake-ups = %d events  \n", wakeUpCount);
  printf("---------------------------------------------------------------\n");
  printf("%d %d %d   sleep   = %f %% of total time \n",
    N, sleepTime, wakeUpCount, (100.0 * sleepTime / N));
  printf("            savings = %f %% of total wattage draw \n",
    (computeSavings(sleepTime, SleepWatts, ActiveWatts )));

  //Output to file
  fprintf(UsageFile,  "%f \n", (100.0 * sleepTime / N));
  fprintf(WattageFile,"%f \n", (computeSavings(sleepTime, SleepWatts,
    ActiveWatts)));

  //close file pointers
  fclose(UsageFile);
  fclose(WattageFile);
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
void outputX()
{
  int      i;                      // Loop counter

  for (i=0; i<N; i++)
    printf("%d", X[i]);
  printf("\n-------------------------------------------------------------\n");
}

//---------------------------------------------------------------------------
//-  Determine total sleep time and number of forced wake-ups               -
//---------------------------------------------------------------------------
void computeSleep(int *sleepTime, int *wakeUpCount)
{
  int      idleState;              // Flag for idle state
  int      i;                      // Loop counter

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
    if (((X[i] == 'A') || (X[i] == 'U')) && (idleState == TRUE))
    {
      idleState = FALSE;
      *wakeUpCount = *wakeUpCount + 1;
    }

    // Determine if in an idle period
    if ((X[i] == 'I') || (X[i] == 'S'))
      idleState = TRUE;

    // Tally the sleep
    if (X[i] == 'I')
      *sleepTime = *sleepTime + 1;
  }

  return;
}

//---------------------------------------------------------------------------
//-  Determine total wattage savings based on sleepTime                     -
//---------------------------------------------------------------------------
double  computeSavings(int sleepTime, int sleepWatts, int activeWatts)
{
  double S;              // Holds percentage of sleepTime
  double eq1, eq2;		 //two equations to calculate wattae savings
  
  //Calculate total savings
  //eq1 is prior to policy consumtion
  eq1 = ( N - AoffTime - AsleepTime ) * activeWatts + AsleepTime * sleepWatts;
  //eq1 is post policy consumtion
  eq2 = ( N - AoffTime - AsleepTime  - sleepTime) * activeWatts +
	  (AsleepTime + sleepTime) * sleepWatts;

  S = eq1 - eq2;

  return 100.0 * (S / eq1);
}
