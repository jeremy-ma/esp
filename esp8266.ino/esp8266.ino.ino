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

#define TCPCONNECT "AT+CIPSTART=0,\"TCP\",\"parcelapp.herokuapp.com\",80"

#define BUFFER_SIZE 50
#define CR          13
#define BARCODE_BAUD_RATE 9600
#define SERIAL_BAUD_RATE 9600
#define RX_BAR 4
#define TX_BAR 5
#define RS232_INVERT 1

#define PATIENT true
#define IMPATIENT false

void clearBuffer(void);

SoftwareSerial barcodeSerial(RX_BAR, TX_BAR, RS232_INVERT);

bool barcodeAvailable = false;
char buffer[BUFFER_SIZE];
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
char getChar(boolean patient)
{
  int count=0;
  int numTimes = 0;
  int limit;

  if (patient){
    limit = 200;
  } else {
    limit = 50;
  }
  
  while(esp8266.available() == false){
    if (count > 30000){
      count = 0;
      numTimes += 1;
      //Serial.println(numTimes);
    } else {
      count += 1;
    }
    if (numTimes > limit){
      break;
    }
  }
    ;
  return esp8266.read();
}

// Buffer used to compare strings against the output from the ESP8266.
#define CB_SIZE  50
char comp_buffer[CB_SIZE] = {0,}; // Fill buffer with string terminator.

// Get char from ESP8266 and also shift it into the comparison buffer.
char getCharAndBuffer(boolean patient)
{
  char b = getChar(patient);
  char *cb_src, *cb_dst;
  cb_src = comp_buffer+1;
  cb_dst = comp_buffer;
  for(int i=1; i<CB_SIZE-1; i++)
    *cb_dst++ = *cb_src++;
  *cb_dst = b;
  return b;
}

// Compare the string against the newest contents of the buffer.
/*
boolean cb_match(String &s)
{
  char *buf = (char *) malloc(sizeof(char) * (s.length()+1));
  s.toCharArray(buf, s.length() + 1);
  boolean ret = strcmp(buf, comp_buffer + CB_SIZE - 1 - s.length()) == 0; // Return true on match.
  free(buf);
  return ret;
}
*/

//TODO: FIX THIS s
boolean cb_matchE(char *s){
  boolean ret = strcmp(s,comp_buffer + CB_SIZE - 1 - strlen(s)) == 0;
  return ret;
}
/*
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
*/
boolean echoFindE(char *keyword, boolean do_echo, boolean patient){
  unsigned long deadline = millis() + TIMEOUT;
  while (millis() < deadline){
    char ch = getCharAndBuffer(patient);
    if (do_echo){
      Serial.write(ch);
    } 
    if (cb_matchE(keyword)) {
      //Serial.println("findFound");
      return true;
    }
    //Serial.println("findloop");
  }
  return false;
}

boolean echoFindTwoE(char * pos, char * neg, boolean do_echo){
  unsigned long deadline = millis() + TIMEOUT;
  while (millis() < deadline){
    char ch = getCharAndBuffer(IMPATIENT);
    if (do_echo){
      Serial.write(ch);
    } 
    if (cb_matchE(pos)) {
      return true;
    } else if (cb_matchE(neg)){
      return false;
    }
  }
  return false;
}

// Echo module output until 3 newlines encountered.
// (Used when we're indifferent to "OK" vs. "no change" responses.)
void echoSkip(boolean patient)
{
  echoFindE("\n", DO_ECHO, patient);        // Search for nl at end of command echo
  echoFindE("\n", DO_ECHO, patient);        // Search for 2nd nl at end of response.
  echoFindE("\n", DO_ECHO, patient);        // Search for 3rd nl at end of blank line.
}
/*
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
  Serial.println("\n\n\r----");
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
*/

boolean echoCommandE(char *cmd, char *ack, boolean patient){
  esp8266.print(cmd);
  esp8266.write("\r");
  esp8266.write("\n");
  Serial.println("\n\n\r----");
  Serial.print("COMMAND:");
  Serial.print(cmd);
  Serial.print("\n");
  if (strlen(ack) == 0){
    echoSkip(patient);
  } else {
    if (!echoFindE(ack,DO_ECHO,patient)){
      //Serial.println("Didn't Find");
      return false;
    }
  }
  //Serial.println("found");
  return true;
}

// Data packets have the format "+IPD,0,1024:lwkjfwsnv....". This routine gets
// the second number preceding the ":" that indicates the number of characters 
// in the packet.
int getPacketLength()
{
  while(getChar(IMPATIENT) != ',')
    ;
  char len[10];
  for(int i=0; i<10; i++)
  {
    char c = getChar(IMPATIENT);
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
  if(echoFindE("+IPD,", NO_ECHO, IMPATIENT))
  {
    for(int l = getPacketLength(); l>0; l--){
      pop = getChar(IMPATIENT);
      Serial.write(pop);
    }
      
  }
}

boolean getServerResp(char * pos, char * neg){
  if (echoFindE("+IPD,",NO_ECHO,IMPATIENT)){
    if (echoFindTwoE(pos,neg,NO_ECHO)){
      return true;
    }
  }
  return false;
}


// Connect to the specified wireless network.
boolean connectWiFi()
{
  char *cmd = "AT+CWJAP=\"Jeremy\",\"potatoes\"";
  if (echoCommandE(cmd, "OK",PATIENT)) // Join Access Point
  {
    Serial.println("\nwifisuccess");
    return true;
  }
  else
  {
    Serial.println("\nwififail");
    return false;
  }
}


// Establish a TCP link to a given IP address and port.
boolean establishTcpLink()
{
  
  return echoCommandE(TCPCONNECT, "OK",IMPATIENT);
}
/*
// Request a page from an HTTP server.
boolean requestPage(String host, char *tracking, int port)
{
  // Create raw HTTP request for web page.
  char req_buf[128];
  sprintf(req_buf, "GET /permission?tracking=%s HTTP/1.1\r\nHost: parcelapp.herokuapp.com:80\r\n\r\n",tracking);
  String http_req = String(req_buf);
  //Serial.println("Printing http request");
  Serial.println(http_req);
  //Serial.println("Http request length");
  Serial.println(http_req.length());
  // Ready the module to receive raw data.
  char cmdbuf[50];
  sprintf(cmdbuf,"AT+CIPSEND=0,%d",http_req.length());
  String cmd = String(cmdbuf);
  //cmd = cmd + (http_req.length()); // Tell the ESP8266 how long the coming HTTP request is.
  Serial.println("About to CIPSEND");
  if (!echoCommand(cmd, ">", CONTINUE))
  {
    echoCommand("AT+CIPCLOSE", "", CONTINUE);
  }
  //Serial.println("About to send get request");
  // Send the raw HTTP request.
  return echoCommand(http_req, "OK", CONTINUE);
}
*/
boolean requestPageE(char *tracking){
  char req_buf[128];
  sprintf(req_buf, "GET /permission?tracking=%s HTTP/1.1\r\nHost: parcelapp.herokuapp.com:80\r\n\r\n",tracking);
  char cmdbuf[25];
  sprintf(cmdbuf,"AT+CIPSEND=0,%d",strlen(req_buf));
  if(!echoCommandE(cmdbuf,">",IMPATIENT)){
    echoCommandE("AT+CIPCLOSE","",IMPATIENT);
    return false;
  }
  boolean ret = echoCommandE(req_buf,"OK", IMPATIENT);
  return ret;
}

void clearBuffer(void) {
  for (int i = 0; i < BUFFER_SIZE; i++) {
    buffer[i] = 0;
  }
  currIndex = 0;
}

void setup()  
{
  delay(1000);
  Serial.begin(9600);  // Initialize serial port to the PC.

  // Rename the ESP8266 serial port to something more standard.
  // Initialize the serial port to the ESP8266, but also assign the RX & TX lines
  // to specific pins of the ZPUino FPGA. Obviously, this part is not needed when
  // using a standard Arduino.
  esp8266.begin(9600);
  Serial.begin(SERIAL_BAUD_RATE);     // communication with the host computer
  barcodeSerial.begin(BARCODE_BAUD_RATE);  
  //buffer = (char*)malloc(sizeof(char)*BUFFER_SIZE);
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

  //Serial.println("\n\n\n\r----------- ESP8266 Demo -----------\n\n");

  echoCommandE("AT+RST",      "ready", PATIENT);      // Reset the module.  
  
  //echoCommand("AT+GMR",      "OK",    CONTINUE);  // Show module's firmware ID. 
  echoCommandE("AT+CWMODE=1", "",IMPATIENT);  // Set module into station mode.
  echoCommandE("AT+CIPMUX=1", "OK",IMPATIENT);  // Allow multiple connections. Necessary for TCP link.

  delay(1000);  // Let things settle down for a bit...

  //echoCommand("AT+CWLAP", "OK", HALT);       // Scan for available access points.

  // Connect to the Wifi.
  for(int i=1; i<=CONNECTION_TRIES; i++)
  {
    if(connectWiFi())
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
  int i;
  /*
  barcodeSerial.listen();
  //wait
  Serial.println("waiting");
  while(!barcodeSerial.available());
  clearBuffer();
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
          buffer[currIndex] = '\0';
       }
     }

  }
  
  if (barcodeAvailable) {
    Serial.print(buffer);
    //clearBuffer();
    //tracking = String(buffer);
    //clearBuffer();
    barcodeAvailable = false;
  }
  */
  delay(2000);
  sprintf(buffer,"123456789");
  esp8266.listen();

  for (i=0; i<=CONNECTION_TRIES; i++){
    if (establishTcpLink()){
      break;
    } else if (i==CONNECTION_TRIES) {
      Serial.println("restarting");
      clearBuffer();
      return;
    }
    delay(1000);
  }
    
  delay(1000);  // Once again, let the link stabilize.
  // Show the connection status.
  
  if (echoCommandE("AT+CIPSTATUS", "OK", IMPATIENT) == false) {
    Serial.println("restarting");
    clearBuffer();
    return;
  }
  delay(1000);

  //echoCommand("AT+CIPSTO=10", "OK", CONTINUE);
  //char pagebuf[128];
  //sprintf(pagebuf,"/permission?tracking=%s",buffer);
  //String page = String(pagebuf);

  for (i=0;i<=1;i++){
    if (requestPageE(buffer)){
      break;
    } else if (i==1){
      Serial.println("Rpage, restarting");
      clearBuffer();
      return;
    }
  }
  clearBuffer();

  Serial.println("wait for resp");
  // Loop forever echoing data received over the Wifi from the HTTP server.
  if (getServerResp("YES","NO")){
    Serial.println("confirm");
    digitalWrite(LOCK_CONTROL, HIGH); 
    delay(5000);
    digitalWrite(LOCK_CONTROL, LOW); 
    //echoPacket();
  } else {
    Serial.println("reject");
  }

  echoCommandE("AT+CIPCLOSE=0", "OK",IMPATIENT);
  Serial.println("end");
  /*
  while (true){
    echoPacket();
  }
  *?
  errorHalt("ONCE ONLY");
  */
}
