#include <SoftwareSerial.h>
#include "WiFly.h"

#include <SdFat.h>
#include <MD_MIDIFile.h>
SdFat  SD;
MD_MIDIFile SMF;

#define USE_MIDI  1   // set to 1 to enable MIDI output, otherwise debug output

#if USE_MIDI // set up for direct MIDI serial output

#define DEBUG(x)
#define DEBUGln(x)
#define DEBUGX(x)
#define DEBUGS(s)
#define SERIAL_RATE 31250

#else // don't use MIDI to allow printing debug statements

#define DEBUG(x)  Serial.print(x)
#define DEBUGln(x)  Serial.println(x)
#define DEBUGX(x) Serial.print(x, HEX)
#define DEBUGS(s) Serial.print(F(s))
#define SERIAL_RATE 9600

#endif // USE_MIDI


// SD chip select pin for SPI comms.
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Other hardware will be different as documented for that hardware.
const uint8_t SD_SELECT = 10;

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
#if USE_MIDI
  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
  {
    Serial.write(pev->data[0] | pev->channel);
    Serial.write(&pev->data[1], pev->size-1);
  }
  else
    Serial.write(pev->data, pev->size);
#endif
  DEBUG("\n");
  DEBUG(millis());
  DEBUG("\tM T");
  DEBUG(pev->track);
  DEBUG(":  Ch ");
  DEBUG(pev->channel+1);
  DEBUG(" Data ");
  for (uint8_t i=0; i<pev->size; i++)
  {
  DEBUGX(pev->data[i]);
    DEBUG(' ');
  }
}

void sysexCallback(sysex_event *pev)
// Called by the MIDIFile library when a system Exclusive (sysex) file event needs 
// to be processed through the midi communications interface. Most sysex events cannot 
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
{
  DEBUG("\nS T");
  DEBUG(pev->track);
  DEBUG(": Data ");
  for (uint8_t i=0; i<pev->size; i++)
  {
    DEBUGX(pev->data[i]);
    DEBUG(' ');
  }
}

#define SSID      "d3m"
#define KEY       "buxtehude"
// check your access point's security mode, mine was WPA20-PSK
// if yours is different you'll need to change the AUTH constant, see the file WiFly.h for avalable security codes
#define AUTH      WIFLY_AUTH_WPA2_PSK
 
int flag = 0;
 
// Pins' connection
// Arduino       WiFly
//  2    <---->    TX
//  3    <---->    RX
 
SoftwareSerial wiflyUart(2, 3); // create a WiFi shield serial object
WiFly wifly(&wiflyUart); // pass the wifi siheld serial object to the WiFly class
 

// Send the string s to wifly if emit is true; return the string length
uint16_t getLengthAndSend(const char* s, bool emit){
  if(emit)wiflyUart.print(s);
  return strlen(s);
}

class title {
  public:
    title(const char* aFile, const char* aDescription);
    void start();
    void cancel();
    uint16_t getHtmlEntry(bool emit);
    const char* file;
    const char* description;
};

title* currentTitle=NULL;
/*
const title liszt("liszt.mid", "Liszt : B.A.C.H.");
const title franck("franck.mid", "Franck : Choral n°3");
const title boellman("toccata.mid", "Boellman : Toccata");
const title grigny("grigny.mid", "Grigny : Noël suisse");
const title bach("bach.mid", "JS Bach : Ich ruf zu dir");
const title *playList[] = {&liszt, &franck, &boellman, &grigny, &bach};
*/
const title rien("rien.mid", "Rien");
const title *playList[] = {&rien};

title::title(const char* aFile, const char* aDescription){
  file = aFile;
  description = aDescription;
}
void title::start(){
  if(this == currentTitle)return;
  int err = SMF.load(file);
  if (err != MD_MIDIFile::E_OK)
  {
    DEBUG(" - SMF load Error ");
    DEBUG(err);
  }
  else
  { 
    DEBUG("Now playing: ");
    DEBUGln(description);
    currentTitle=this;
  }
}
void title::cancel(){
  DEBUG("Now stopping: ");
  DEBUGln(description);
  currentTitle=NULL;
}
uint16_t title::getHtmlEntry(bool emit){
  return 
    getLengthAndSend("<li><a href=\"", emit)+
    getLengthAndSend(file, emit)+
    getLengthAndSend("\">", emit)+
    getLengthAndSend(description, emit)+
    getLengthAndSend("</a></li>", emit);
}

const char * pageHeader = ""; /*R"(
<html><head><title>Pipe organ as a juke box</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
body {  background-color: black;  font-family: verdana;  color: white;}
h1,h2 {  text-align: center;}
h3 { color: grey }
a {  color: yellow;  font-size: 20px;}
table {width: 100%;}
</style>
</head>
<link rel="icon" href="data:;base64,iVBORw0KGgo=">
<body><h1>Les grandes orgues</h1><h2>de la Basilique St Joseph de Grenoble</h2>
)";*/
const char * pageFooter = ""; //"</body></html>";
const char * pageTitle = ""; //"<h3>Oeuvres disponibles :</h3><ul>";
const char * pageUpdate = ""; //"</ul><p><a href=\"/\">Mettre à jour</a></p>";
const char * pageAction = ""; //"</p><table><tr><td align=left><a href=\"cancel\">Arrêter</a></td><td align=right><a href=\"/\">Mettre à jour</a></td></tr></table>"
const char * pageTitleBody = ""; //"<h3>Oeuvre en cours d'audition :</h3><p>"

uint16_t getPageBody(char* request, bool emit){
  uint16_t pageLength=0;
  // number of elements in array
  int const n = sizeof( playList ) / sizeof( playList[ 0 ] );
  int i;
  // Check for a cancel request
  if(currentTitle != NULL && strstr(request, "cancel") != NULL){
    currentTitle->cancel();
    i=-1;
  }else for (i = n; i--;){ // search for a known file in the request
    title *e = (title*)playList[i];
    if(strstr(request, e->file) != NULL)break;
  }
  
  // if a play is in progress, display its page
  if(currentTitle != NULL){
     for (i = n; currentTitle != playList[--i];);
  }

  if(i<0){
    // not found: push the full list of titles
    pageLength+=getLengthAndSend(pageTitle, emit);
    for (int i = 0; i != n; ++i){
      title *e = (title*)playList[i];
      pageLength+=e->getHtmlEntry(emit);
    }
    pageLength+=getLengthAndSend(pageUpdate, emit);    
  }
  
  if(i >= 0){
    // found: push the page for this title
    pageLength+=getLengthAndSend(pageTitleBody, emit);
    pageLength+=getLengthAndSend(playList[i]->description, emit);
    pageLength+=getLengthAndSend(pageAction, emit);
    // If this title is not playing yet, start it now
    if(emit && currentTitle != playList[i]){
      title* c=(title*)playList[i];
      c->start();
    }
  }
  return pageLength;
}

void setup()
{
    // No current title
    currentTitle=NULL;
    
    wiflyUart.begin(9600); // start wifi shield uart port
    Serial.begin(SERIAL_RATE); // start the arduino serial port
    DEBUGln("--------- WIFLY Webserver --------");
 
    // wait for initilization of wifly
    delay(1000);
 
    wifly.reset(); // reset the shield
    delay(1000);
    //set WiFly params
 
    wifly.sendCommand("set ip local 80\r"); // set the local comm port to 80
    delay(100);
 
    wifly.sendCommand("set comm remote 0\r"); // do not send a default string when a connection opens
    delay(100);
 
    wifly.sendCommand("set comm open *OPEN*\r"); // set the string that the wifi shield will output when a connection is opened
    delay(100);
 
    DEBUGln("Join " SSID );
    if (wifly.join(SSID, KEY, AUTH)) {
        DEBUGln("OK");
    } else {
        DEBUGln("Failed");
    }
 
    delay(5000);
 
    wifly.sendCommand("get ip\r");
    char c;
 
    while (wifly.receive((uint8_t *)&c, 1, 300) > 0) { // print the response from the get ip command
        DEBUG((char)c);
    }
 
    DEBUGln("Web server ready");

    // Initialize SD
    if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
    {
      DEBUGln("SD init fail!");
    }
  
    // Initialize MIDIFile
    SMF.begin(&SD);
    SMF.setMidiHandler(midiCallback);
    SMF.setSysexHandler(sysexCallback);

}

void loop()
{
  if(currentTitle != NULL)
  {
    if (!SMF.isEOF())
      SMF.getNextEvent();
    else
      currentTitle->cancel();
  }
  
  if(wifly.available())
  { // the wifi shield has data available
    if(wiflyUart.find((char *)"*OPEN*")) // see if the data available is from an open connection by looking for the *OPEN* string
    {
      DEBUG("New Browser Request: ");
      delay(1000); // delay enough time for the browser to complete sending its HTTP request string
      char request[20];
      memset(request, 0, sizeof(request));  
      wifly.receive((uint8_t *)&request, sizeof(request), 300);
      DEBUGln(request);
        
      // Build the returned page
      uint16_t htmlLength=strlen(pageHeader)+getPageBody(request, false)+strlen(pageFooter)-strlen("<html></html>")-1;
      
      // send HTTP header
      wiflyUart.println("HTTP/1.1 200 OK");
      wiflyUart.println("Content-Type: text/html; charset=UTF-8");
      wiflyUart.print("Content-Length: "); 
      wiflyUart.println(htmlLength); // length of HTML code between <html> and </html>
      wiflyUart.println("Connection: close");
      wiflyUart.println();
      
      // send webpage's HTML code
      wiflyUart.print(pageHeader);
      getPageBody(request, true);
      wiflyUart.print(pageFooter);
    }
  }
}
