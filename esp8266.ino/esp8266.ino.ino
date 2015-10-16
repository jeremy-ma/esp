///////////////////////////////////////////////////////////////////////////////////////
// This program uses the ESP8266 Wifi module to access a webpage. It's an adaptation of
// the program discussed at http://hackaday.io/project/3072/instructions
// and shown here http://dunarbin.com/esp8266/retroBrowser.ino .
//
// This program was ported to the ZPUino 2.0, a softcore processor that runs on an FPGA
// and emulates an Arduino.
//
// This program works with version 00160901 of the ESP8266 firmware.
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
// This program uses the ESP8266 Wifi module to access a webpage. It's an adaptation of
// the program discussed at http://hackaday.io/project/3072/instructions
// and shown here http://dunarbin.com/esp8266/retroBrowser.ino .
//
// This program was ported to the ZPUino 2.0, a softcore processor that runs on an FPGA
// and emulates an Arduino.
//
// This program works with version 00160901 of the ESP8266 firmware.
///////////////////////////////////////////////////////////////////////////////////////

#include <SoftwareSerial.h>


#define SSID                "Jeremy"
#define PASS                "potatoes" // ESP8266 works with WPA2-PSK (AES) passwords.

//#define SSID              "BigPond1F5A"
//#define PASS               "4614345322"
/*
#define DEST_HOST         "retro.hackaday.com"
#define DEST_PAGE         "/permission"
#define DEST_IP           "192.254.235.21"
*/
#define LOCK_CONTROL        8
/*
#define DEST_HOST           "www.xess.com"
#define DEST_PAGE           "/static/media/pages/xess_esp8266.html"
#define DEST_IP             "192.241.200.6"
#define DEST_PORT           80
*/
#define DEST_HOST           "parcelapp.herokuapp.com"
#define DEST_PAGE           "/permission"
#define DEST_IP             "parcelapp.herokuapp.com"
//#define DEST_IP             "50.16.215.101"
#define DEST_PORT           80

#define TIMEOUT             5000 // mS
#define CONTINUE            false
#define HALT                true
#define DO_ECHO             true
#define NO_ECHO             false
#define CONNECTION_TRIES    5
#define ECHO_COMMANDS // Un-comment to echo AT+ commands to serial monitor.



#define BUFFER_SIZE 50
#define CR          13
#define BARCODE_BAUD_RATE 9600
#define SERIAL_BAUD_RATE 9600
#define RX_BAR 4
#define TX_BAR 5
#define RS232_INVERT 1

void clearBuffer(void);

SoftwareSerial barcodeSerial(RX_BAR, TX_BAR, RS232_INVERT);

bool barcodeAvailable = false;
char* buffer;
char currIndex = 0;
byte temp;




SoftwareSerial esp8266(2, 3); // RX,TX


// Print error message and loop stop.
void errorHalt(String msg)
{
  Serial.println(msg);
  Serial.println("HALT");
  while(true);
}

// Wait until a char is received from the ESP8266. Then return it. 
char getChar()
{
  while(esp8266.available() == false)
    ;
  return esp8266.read();
}

// Buffer used to compare strings against the output from the ESP8266.
#define CB_SIZE  20
char comp_buffer[CB_SIZE] = {0,}; // Fill buffer with string terminator.

// Get char from ESP8266 and also shift it into the comparison buffer.
char getCharAndBuffer()
{
  char b = getChar();
  char *cb_src, *cb_dst;
  cb_src = comp_buffer+1;
  cb_dst = comp_buffer;
  for(int i=1; i<CB_SIZE-1; i++)
    *cb_dst++ = *cb_src++;
  *cb_dst = b;
  return b;
}

// Compare the string against the newest contents of the buffer.
boolean cb_match(String &s)
{
  
  char *buf = (char *) malloc(sizeof(char) * (s.length()+1));
  s.toCharArray(buf, s.length() + 1);
  boolean ret = strcmp(buf, comp_buffer + CB_SIZE - 1 - s.length()) == 0; // Return true on match.
  free(buf);
  return ret;
}

// Read characters from ESP8266 module and echo to serial until keyword occurs or timeout.
boolean echoFind(String keyword, boolean do_echo)
{
  // Fail if the target string has not been sent by deadline.
  unsigned long deadline = millis() + TIMEOUT;
  
  while(millis() < deadline)
  {
    char ch = getCharAndBuffer();
    if(do_echo)
      Serial.write(ch);
    if(cb_match(keyword))
      return true;
  }
  return false;  // Timed out
}

// Echo module output until 3 newlines encountered.
// (Used when we're indifferent to "OK" vs. "no change" responses.)
void echoSkip()
{
  echoFind("\n", DO_ECHO);        // Search for nl at end of command echo
  echoFind("\n", DO_ECHO);        // Search for 2nd nl at end of response.
  echoFind("\n", DO_ECHO);        // Search for 3rd nl at end of blank line.
}

// Send a command to the module and wait for acknowledgement string
// (or flush module output if no ack specified).
// Echoes all data received to the serial monitor.
boolean echoCommand(String cmd, String ack_str, boolean halt_on_fail)
{
  // Send the command to the ESP8266.
  esp8266.print(cmd);
  esp8266.write("\r"); // Append return to commands.
  esp8266.write("\n");
  
  #ifdef ECHO_COMMANDS
  Serial.println("\n\n\r--------------------------------------");
  Serial.println("COMMAND: \n");
  #endif

  char * ack = (char *) malloc(sizeof(char) * (ack_str.length() + 1));
  ack_str.toCharArray(ack,ack_str.length()+1);
  
  // If no ack response specified, skip all available module output.
  if (strlen(ack) == 0)
    echoSkip();
  else
  {
    // Otherwise wait for ack.
    if (!echoFind(ack, DO_ECHO))    // timed out waiting for ack string 
    {
      
      if (halt_on_fail){
        errorHalt(cmd + " failed");// Critical failure halt.
      } else {
        free(ack);
        return false;            // Let the caller handle it.
      }
    }
  }
  free(ack);
  return true;                   // ack blank or ack found
}

// Data packets have the format "+IPD,0,1024:lwkjfwsnv....". This routine gets
// the second number preceding the ":" that indicates the number of characters 
// in the packet.
int getPacketLength()
{
  while(getChar() != ',')
    ;
  char len[10];
  for(int i=0; i<10; i++)
  {
    char c = getChar();
    if(c == ':')
    {
      len[i] = 0; // Terminate string.
      break;
    }
    len[i] = c;
  }
  return atoi(len);
}

// Echo a received Wifi packet to the PC.
void echoPacket()
{

  char pop;
  if(echoFind("+IPD,", NO_ECHO))
  {
    
    for(int l = getPacketLength(); l>0; l--){
      pop = getChar();
      if (pop == '\r'){
        Serial.write("CR");
      } else if (pop == '\n'){
        Serial.write("NL");
      } else {
        Serial.write(pop);
      }
    }
      
  }
}

boolean getServerResp(String keyword){
  if (echoFind("+IPD,",NO_ECHO)){
    if (echoFind(keyword,NO_ECHO)){
      return true;
    }
  }
  return false;
}


// Connect to the specified wireless network.
boolean connectWiFi(String ssid, String pwd)
{
  String cmd = "AT+CWJAP=\"Jeremy\",\"potatoes\"";
  if (echoCommand(cmd, "OK", CONTINUE)) // Join Access Point
  {
    Serial.println("\nConnected to WiFi.");
    return true;
  }
  else
  {
    Serial.println("\nConnection to WiFi failed.");
    return false;
  }
}

// Establish a TCP link to a given IP address and port.
boolean establishTcpLink(String ip, int port)
{
  String cmd = "AT+CIPSTART=0,\"TCP\",\"parcelapp.herokuapp.com\",80";
  return echoCommand(cmd, "OK", CONTINUE);
}

// Request a page from an HTTP server.
boolean requestPage(String host, String page, int port)
{
  // Create raw HTTP request for web page.
  //String http_req = "GET " + page + " HTTP/1.1\r\nHost: " + host + ":" + port + "\r\n\r\n";
  String http_req = "GET /permission?tracking= HTTP/1.1\r\nHost: parcelapp.herokuapp.com:80\r\n\r\n";

  //Serial.println("Printing http request");
  Serial.println(http_req);
  //Serial.println("Http request length");
  Serial.println(http_req.length());
  // Ready the module to receive raw data.
  String cmd = "AT+CIPSEND=0,";
  cmd = cmd + (http_req.length()); // Tell the ESP8266 how long the coming HTTP request is.
  Serial.println("About to CIPSEND");
  if (!echoCommand(cmd, ">", CONTINUE))
  {
    echoCommand("AT+CIPCLOSE", "", CONTINUE);
  }
  //Serial.println("About to send get request");
  // Send the raw HTTP request.
  return echoCommand(http_req, "OK", CONTINUE);
}

void clearBuffer(void) {
  for (int i = 0; i < BUFFER_SIZE; i++) {
    buffer[i] = 0;
  }
  currIndex = 0;
}



void setup()  
{
  delay(5000);
  Serial.begin(9600);  // Initialize serial port to the PC.

  // Rename the ESP8266 serial port to something more standard.
  // Initialize the serial port to the ESP8266, but also assign the RX & TX lines
  // to specific pins of the ZPUino FPGA. Obviously, this part is not needed when
  // using a standard Arduino.
  esp8266.begin(9600);
  Serial.begin(SERIAL_BAUD_RATE);     // communication with the host computer
  barcodeSerial.begin(BARCODE_BAUD_RATE);  
  buffer = (char*)malloc(sizeof(char)*BUFFER_SIZE);
  esp8266.listen();
  pinMode(LOCK_CONTROL, OUTPUT);
  digitalWrite(LOCK_CONTROL,LOW);
  
  // Wait for a keypress from the PC before running the demo.
  /*
  while(!Serial.available())
    ;
  
  Serial.read();
` */

  delay(1000);

  Serial.println("\n\n\n\r----------- ESP8266 Demo -----------\n\n");

  echoCommand("AT+RST",      "ready", HALT);      // Reset the module.  
  
  //echoCommand("AT+GMR",      "OK",    CONTINUE);  // Show module's firmware ID. 
  echoCommand("AT+CWMODE=1", "",      CONTINUE);  // Set module into station mode.
  echoCommand("AT+CIPMUX=1", "OK",    CONTINUE);  // Allow multiple connections. Necessary for TCP link.

  delay(1000);  // Let things settle down for a bit...

  //echoCommand("AT+CWLAP", "OK", HALT);       // Scan for available access points.

  // Connect to the Wifi.
  for(int i=1; i<=CONNECTION_TRIES; i++)
  {
    if(connectWiFi(SSID, PASS))
      break;
    if(i == CONNECTION_TRIES)
      return;
  }

  delay(1000);  // Let the link stabilize.

  //echoCommand("AT+CIFSR", "", HALT);      // Show the IP address assigned by the access point.
}

String tracking = "";
void loop() 
{

  barcodeSerial.listen();
  //wait
  Serial.println("waiting");
  while(!barcodeSerial.available());
  
  while (!barcodeAvailable)   {
     if (barcodeSerial.available()){
       temp = barcodeSerial.read();
       Serial.print(char(temp));
       if (temp == CR) {
          barcodeSerial.read();
          barcodeAvailable = true;
       } else {
          buffer[currIndex] = temp;
          currIndex++;
       }
     }

  }

  if (barcodeAvailable) {
    Serial.print(buffer);
    clearBuffer();
    tracking = String(buffer);
    clearBuffer();
    barcodeAvailable = false;
  }
  
  esp8266.listen();

  if (establishTcpLink(DEST_IP, DEST_PORT) == false){
    Serial.println("restarting");
    return;
  }
    
  delay(1000);  // Once again, let the link stabilize.
  // Show the connection status.
  
  if (echoCommand("AT+CIPSTATUS", "OK",CONTINUE) == false) {
    Serial.println("restarting");
    return;
  }
  delay(1000);

  //echoCommand("AT+CIPSTO=10", "OK", CONTINUE);
  
  //  a page from an HTTP server.
  //String page = String(DEST_PAGE) + "?tracking=" + tracking;
  String page = "/permission?tracking=";
  Serial.println(page);
  if (requestPage(DEST_HOST, page, DEST_PORT) == false){
    Serial.println("Rpage, restarting");
    return;
  }

  //Serial.println("Waiting for server response");
  // Loop forever echoing data received over the Wifi from the HTTP server.
  if (getServerResp("YES")){
    Serial.println("confirm");
    digitalWrite(LOCK_CONTROL, HIGH); 
    delay(10000);
    digitalWrite(LOCK_CONTROL, LOW); 
    //echoPacket();
  } else {
    Serial.println("reject");
  }

  echoCommand("AT+CIPCLOSE=0", "OK", CONTINUE);
  /*
  while (true){
    echoPacket();
  }
  *?
  errorHalt("ONCE ONLY");
  */
}
