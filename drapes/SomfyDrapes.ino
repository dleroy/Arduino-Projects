/*
  Based in WiFi Web Server LED Blink
  created 25 Nov 2012
  by Tom Igoe
  This is taking http commands over wifi to control Somfy Drapes
*/
#include <SPI.h>
#include <WiFiNINA.h>

#include "arduino_secrets.h" 
IPAddress ip(192, 168, 1, 11);    // Static IP
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;        // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key Index number (needed only for WEP)

int openButton  = 9;             // Tied to Somfy controller open button
int closeButton = 8;             // Tied to Somfy controller close button
int closureDelay = 700;          // 700ms press by default

int status = WL_IDLE_STATUS;
WiFiServer server(80);

void setup() {
  Serial.begin(9600);      // initialize serial communication
  pinMode(openButton, OUTPUT);      // relay control
  pinMode(closeButton, OUTPUT);
  digitalWrite(openButton, HIGH);
  digitalWrite(closeButton, HIGH);     
  
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // config with static IP
  WiFi.config(ip);
  
  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid);                   // print the network name (SSID);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  server.begin();                           // start the web server on port 80
  printWifiStatus();                        // you're connected now, so print out the status
}


void loop() {
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("new client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print("Click <a href=\"/O\">here</a> to open the blinds<br>");
            client.print("Click <a href=\"/C\">here</a> to close the blinds<br>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check to see if the client request was "GET /O" or "GET /C":
        if (currentLine.endsWith("GET /O")) {
          digitalWrite(openButton, LOW);               // GET /O opens the blinds
          delay(closureDelay);
          digitalWrite(openButton, HIGH);

        }
        if (currentLine.endsWith("GET /C")) {
          digitalWrite(closeButton, LOW);               // GET /C closes the blinds
          delay(closureDelay);
          digitalWrite(closeButton, HIGH);
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disonnected");
  }
  delay(10000);
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print Wifi Firmware version and latest version
  Serial.print("Firmware Version: ");
  Serial.println(WiFi.firmwareVersion());
  Serial.print("Latest Firmware Version: ");
  Serial.println(WIFI_FIRMWARE_LATEST_VERSION);
  
  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}
