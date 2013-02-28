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
#include <SPI.h>
#include <WiFi.h>
#include<stdlib.h>


// Configuration constants
#define TRUE                 1
#define FALSE                0
#define PROCESSING_INTERVAL  30000        // 30 secs by default
#define MAX_BANK_SAMPLES      1024       // sizing of the banks
#define WHEEL_CIRC           18.75       // hamster wheel circumference in inches
#define INIT_FASTEST_LAP     0xffffffff  // initial fastest lap in milliseconds for comparison

// Constants used in stats calculations
#define INCHES_PER_MILE         63360
#define INCHES_PER_METER        39.3700787
#define MSEC_PER_REV_CONV       1065.3409       // conversion factor to go from millis/rev to miles per hour

//////////////////////////////////////////////////////////////////////////////////////////

// Cosm related constants
#define API_KEY "wZ2Oh89Pc6J9yhKZH3gmGZlvU5mSAKxkRVJvSlhVY3BBZz0g" // your Cosm API key
#define FEED_ID 107080                                             // your Cosm feed ID
#define USER_AGENT      "Hamster"                                  // user agent is the project name

// Wifi related variable declarations
char ssid[] = "smackdown";      //  your network SSID (name) 
char pass[] = "carnielchesterford";   // your network password

int status = WL_IDLE_STATUS;

// initialize the library instance:
WiFiClient client;

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
IPAddress server(216,52,233,121);      // numeric IP for api.pachube.com
//char server[] = "api.cosm.com";   // name address for pachube API

unsigned long lastConnectionTime = 0;          // last time you connected to the server, in milliseconds
boolean lastConnected = false;                 // state of the connection last time through the main loop
const unsigned long postingInterval = 10*1000; //delay between updates to pachube.com

//////////////////////////////////////////////////////////////////////////////////////////

// Variables used in stats collection
int volatile currBank;                                  // which of the 2 banks is currently accumlating sensor input.
int lastBank;                                           // which of the 2 banks is currently being looked at for stats. 
unsigned int volatile bank[2][MAX_BANK_SAMPLES];        // the 2 banks to hold sample data
unsigned int volatile currBankIndex[2];                 // current location in each bank
unsigned int gLastTimestamp;                            // store last timestamp reading from prior bank to do speed calculations
int gFastestLap;                                        // store fastest lap for max speed calulations
unsigned int volatile gHigh;

// Hamster wheel statistics calculated
unsigned int   gStatTotalRevolutions;           // total hamster revolutions
unsigned int   gStatTotalMeters;               // total hamster meters
float gStatTotalMiles;                          // total hamster miles
float gStatAvgSpeed;                            // average hamster speed mph
float gStatMaxSpeed;                            // max hamster speed mph

//////////////////////////////////////////////////////////////////////////////////////////


// the setup routine runs once when you press reset:
void setup() {           
  initDataCollection();                          // Initialize collection banks and statistics
  initSerial(); 
  initWifi();                                    // Get Wifi connection established before we start collecting data.
  initInterrupts();                              // Interrupts disabled by this routine
  pinMode(13, OUTPUT);                           // Blink LED on each lap
}

// the loop routine runs over and over again forever:
void loop() {
  switchBanks();                   // Start collecting data in other bank
  calcStats();                     // Calculate stats on data collected in last interval
  printStats();                    // Print update stats
  sendFeedUpdate();                // Send data to Cosm
  delay(PROCESSING_INTERVAL);      // Wait while new data is collected
}

// Initialize serial interface
void initSerial() {
  Serial.begin(9600);                            // Start serial port
} 

// Initialize interrupts
void initInterrupts() {
  noInterrupts();                                // Disable interrupts until we have set time
  attachInterrupt(0, hamsterInterrupt, FALLING);  // Interrupt 0 on Mega2560 is pin 2
}

// Reset hamster statistics
void resetStatistics() {
 gStatTotalMiles = 0;
 gStatTotalRevolutions = 0;
 gStatTotalMeters = 0;
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

// update the total meters calculation
void calcTotalMeters() {
  int revolutions = currBankIndex[lastBank];
  gStatTotalMeters +=  (revolutions * WHEEL_CIRC) / INCHES_PER_METER;
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
   calcTotalMeters();                  // depends on total revolution count being calculated first
   calcAvgSpeed();
   calcMaxSpeed();
}

// Print Statistics
void printStats() {
  Serial.print("Total Revolutions:     ");
  Serial.println(gStatTotalRevolutions);
  Serial.print("Total Distance (meters): ");
  Serial.println(gStatTotalMeters);
  Serial.print("Total Distance (miles): ");
  Serial.println(gStatTotalMiles);
  Serial.print("Avg Speed (mph):       ");
  Serial.println(gStatAvgSpeed);
  Serial.print("Max Speed (mph):       ");
  Serial.println(gStatMaxSpeed);
  Serial.println("");
}

// Initialize Wifi connection
void initWifi() {
  
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present"); 
    // don't continue:
    while(true);
  } 
  
  // attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) { 
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:    
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  } 
  // you're connected now, so print out the status:
  printWifiStatus();
}

void sendFeedUpdate() {

  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:
  while (client.available()) {
    char c = client.read();
    Serial.print(c);
  }

  // if there's no net connection, but there was one last time
  // through the loop, then stop the client:
  if (!client.connected() && lastConnected) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }

  // if you're not connected, and ten seconds have passed since
  // your last connection, then connect again and send data:
  if(!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
    sendDataStream("laps", currBankIndex[lastBank]);
    sendDataStream("meters", gStatTotalMeters);
  }
  // store the state of the connection for next time through
  // the loop:
  lastConnected = client.connected();
}

// this method makes a HTTP connection to the server:
void sendDataStream(String name, unsigned int value) {
  // if there's a successful connection:
  if (client.connect(server, 80)) {
    Serial.println("connecting...");
    // send the HTTP PUT request:
    client.print("PUT /v2/feeds/");
    client.print(FEED_ID);
    client.println(".csv HTTP/1.1");
    client.println("Host: api.cosm.com");
    client.print("X-ApiKey: ");
    client.println(API_KEY);
    client.print("User-Agent: ");
    client.println(USER_AGENT);
    client.print("Content-Length: ");

    // calculate the length of the sensor reading in bytes + 1 for comma
    int thisLength = 1 +  name.length() + getIntLength(value);
    client.println(thisLength);

    // last pieces of the HTTP PUT request:
    client.println("Content-Type: text/csv");
    client.println("Connection: close");
    client.println();

    // here's the actual content of the PUT request:
    client.print(name);
    client.print(',');
    client.println(value);
  } 
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }
   // note the time that the connection was made or attempted:
  lastConnectionTime = millis();
}


// This method calculates the number of digits in an
// integer.  Since each digit of the ASCII decimal
// representation is a byte, the number of digits equals
// the number of bytes:

int getIntLength(int someValue) {
  // there's at least one byte:
  int digits = 1;
  // continually divide the value by ten, 
  // adding one to the digit count for each
  // time you divide, until you're at 0:
  int dividend = someValue /10;
  while (dividend > 0) {
    dividend = dividend /10;
    digits++;
  }
  // return the number of digits:
  return digits;
}

// This method calculates the number of digits in a float
// Since each digit of the ASCII decimal
// representation is a byte, the number of digits equals
// the number of bytes:

int getFloatLength(float someValue) {
  char resultStr[20];
  int i, len;
  
  dtostrf(someValue,4,2,resultStr);
  len = 0;
  for(i=0;i<20;i++) {
    if (resultStr[i] == '\0')
      break;
    len++;
  }
  return len;
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}





