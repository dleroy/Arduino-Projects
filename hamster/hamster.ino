/*
 *  Hamster Tracker
 *  This program counts the rotations of a hamster wheel using a hall effect sensor. It
 *  then keeps a running tabulation of the following statistics:
 *  Total Distance: in miles
 *  Total Revolutions:
 *  Average Speed:  mph
 *  Fastest Speed:  mph
 *
 * Author: Dave LeRoy
 * Source: http://github/???
 */
 
// Configuration constants
#define TRUE                 1
#define FALSE                0
#define PROCESSING_INTERVAL  6000        // 1 minute by default
#define MAX_BANK_SAMPLES      1024       // sizing of the banks
#define WHEEL_CIRC           18.75       // hamster wheel circumference in inches
#define INIT_FASTEST_LAP     0xffffffff  // initial fastest lap in milliseconds for comparison

// Constants used in stats calculations
#define INCHES_PER_MILE         63360
#define MSEC_PER_REV_CONV       1065.3409       // conversion factor to go from millis/rev to miles per hour

// Variables used in stats collection
int volatile currBank;                                  // which of the 2 banks is currently accumlating sensor input.
int lastBank;                                           // which of the 2 banks is currently being looked at for stats. 
unsigned int volatile bank[2][MAX_BANK_SAMPLES];        // the 2 banks to hold sample data
unsigned int volatile currBankIndex[MAX_BANK_SAMPLES];  // current location in each bank
unsigned int gLastTimestamp;                            // store last timestamp reading from prior bank to do speed calculations
int gFastestLap;                                        // store fastest lap for max speed calulations
unsigned int volatile gHigh;

// Hamster wheel statistics calculated
float gStatTotalMiles;                          // total hamster miles
int   gStatTotalRevolutions;                    // total hamster revolutions
float gStatAvgSpeed;                            // average hamster speed mph
float gStatMaxSpeed;                            // max hamster speed mph

// the setup routine runs once when you press reset:
void setup() {           
  initDataCollection();                          // Initialize collection banks and statistics
  initSerial(); 
  initInterrupts();                              // Interrupts disabled by this routine
}

// the loop routine runs over and over again forever:
void loop() {
  switchBanks();                   // Start collecting data in other bank
  calcStats();                     // Calculate stats on data collected in last interval
  printStats();                    // Print update stats
  delay(PROCESSING_INTERVAL);      // Wait while new data is collected
}

// Initialize serial interface
void initSerial() {
  Serial.begin(9600);                            // Start serial port
} 

// Initialize interrupts
void initInterrupts() {
  noInterrupts();                                // Disable interrupts until we have set time
  attachInterrupt(0, hamsterInterrupt, RISING);  // Interrupt 0 on Mega2560 is pin 2
}

// Reset hamster statistics
void resetStatistics() {
 gStatTotalMiles = 0;
 gStatTotalRevolutions = 0;
 gStatAvgSpeed = 0;
 gStatMaxSpeed = 0;
 gLastTimestamp = 0;
 gFastestLap = INIT_FASTEST_LAP;
}

// Initialize banks and statistics
void initDataCollection() {
  currBank = 0; lastBank = 1;
  currBankIndex[0] = currBankIndex[1] = 0;   // XXX switch to bank ptrs later
  resetStatistics();
}

// switch active banks. 1 always collecting data, the other being analyzed for stats update
void switchBanks() {
  noInterrupts();                // protect bank switching with lock
  gLastTimestamp = bank[currBank][currBankIndex[currBank]];    // Store last timestamp before switch banks
  currBank = (currBank + 1) % 2; // switch banks
  lastBank = (lastBank + 1) % 2; // easy reference to last bank for stats calculations
  currBankIndex[currBank] = 0;   // reset ptr to current bank to start collecting from beginning of bank
  if (currBankIndex[lastBank]) {
    currBankIndex[lastBank]--;     // account for auto incrementing past last collected index by 1
  }
  interrupts();
}

// Interrupt handler for updating wheel statistics
void hamsterInterrupt() {
  // simply stamp time in appropriate bank/location and increment ptr
  bank[currBank][currBankIndex[currBank]++] = millis();          // XXX switch to ptrs later
  if (gHigh) {
    digitalWrite(13, HIGH);
    gHigh = 0;
  } else {
    digitalWrite(13, LOW);
    gHigh = 1;
  }
}

// update the total revolutions calculation
void calcTotalRevolutions() {
  gStatTotalRevolutions += currBankIndex[lastBank];
}

// update the total miles calculation
void calcTotalMiles() {
  int revolutions = currBankIndex[lastBank];
  gStatTotalMiles +=  (revolutions * WHEEL_CIRC) / INCHES_PER_MILE;
}

// update the avg speed calculation. More complicated. Ignore time when hamsters arent active.
void calcAvgSpeed() {
}

// update the max speed calculation. Current just taking fastest revolution
void calcMaxSpeed() {
  int updated = FALSE;
  unsigned int newLap;
  int tmpVal = 0;
  int x;

  // Boundary case, compare last timestamp in prior bank to first in this bank
  if (currBankIndex[lastBank] && gLastTimestamp) {
    newLap = bank[lastBank][0] - gLastTimestamp;
    if (newLap && newLap < gFastestLap) {
      gFastestLap = bank[lastBank][0] - gLastTimestamp;
    }    
  }

  // Loop through lastBank and see if any of the revolution speeds are faster than current max speed
  for (x=1; x <= currBankIndex[lastBank]; x++) {
    newLap = bank[lastBank][x] - bank[lastBank][x-1];
    if (newLap && (newLap < gFastestLap)) {
      gFastestLap = bank[lastBank][x] - bank[lastBank][x-1];
      updated = TRUE;
    }
  }

  // store last timestamp for next calculation boundary case    
  gLastTimestamp = bank[lastBank][currBankIndex[lastBank]];

  // Calculate max speed based on fastest lap and wheel circumference
  if (updated) {
    gStatMaxSpeed = MSEC_PER_REV_CONV/gFastestLap;
  }
}

// Calculate Statistics
void calcStats() {
   calcTotalRevolutions();
   calcTotalMiles();                  // depends on total revolution count being calculated first
   calcAvgSpeed();
   calcMaxSpeed();
}

// Print Statistics
void printStats() {
  Serial.print("Total Revolutions:     ");
  Serial.println(gStatTotalRevolutions);
  Serial.print("Total Distance (miles): ");
  Serial.println(gStatTotalMiles);
  Serial.print("Avg Speed (mph):       ");
  Serial.println(gStatAvgSpeed);
  Serial.print("Max Speed (mph):       ");
  Serial.println(gStatMaxSpeed);
  Serial.println("");
}

// set time from serial input  (until we get NTP available)
void setTime() {
}
