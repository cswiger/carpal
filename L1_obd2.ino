/*
  CarPal! Your automobile's OBD2 dashboard buddy. 

  v3c - changing all of adafruit url to snprintf
  v3b - got rid of 'String' objects in adafruit section and moved sms vars into it's function instead of globals
  v3a - added more detailed MIL code bit results testing
  v3 - added SMS to text for DTC and other requests
  v2b - added function prototypes and moved supporting functions below loop()
  v2a - added adafruit.io support, sorted out LGPRSClient *client usage for raw connections
  v2 - added proper json library support
  
  GSM/GPRS, GPS location published to pubnub with some auto stats obtained via obd2 
  the pubnub message is PubNub EON Mapbox map compatible
  This is mainly a technology test sketch

*/

#include "L1_obd2.h"
#include "PubNub.h"

// Adafruit io support
char server[] =  "io.adafruit.com";
char path[] = "/api/groups/AutoStats/send.json?x-aio-key=xxxxyourxadafruitxaioxkeyxherexxxxxxxxxx&Load=";
int port = 80; // HTTP

//import all the necessary files for GPRS connectivity
#include "LGPRS.h"
#include "LGPRSClient.h"

// and SMS
#include <LGSM.h>


// GPS and math for converting deg.min+decimal to deg.decimal
#include <LGPS.h>
#include <math.h>

// OBD2 library
#include "OBD2.h"

// For preparing the pubnub messages
#include <ArduinoJson.h>
StaticJsonBuffer<200> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();
JsonArray& latlng = root.createNestedArray("latlng");

// leds for gprs status
#define rled 12
#define gled 11

//define the required keys for using PubNub
char pubkey[] = "pub-c-12345678-1234-1234-1234-yourpubkeyxx";
char subkey[] = "sub-c-87654321-4321-4321-4321-yoursubkeyxx";
char channel[] = "map1";

OBD2 obd2;

// this was down in loop and the device kept resetting? 
// There appearently used to be an issue with that
LGPRSClient *client;

gpsSentenceInfoStruct info;
char buff[256];    // used in gps and general purpose string buffer
double latitude;
double longitude;
char NS;          // for gps quadrant, N(+)l S(-)latitude, E(+) W(-) longitude
char EW;

// for string json fix so EON map will work - it wants a LIST of json latlng cooridinates
char pub[82];
char rightbr[] = "]";
char leftbr[] = "[";

// some flags for error light signal - start off with all in error condition and clear as each subsystem comes up
boolean gprsError = true;
boolean gpsError = true;
boolean obdError = true;
boolean pubnubError = true;

void setup()
{
    // Turn GPS on first as it takes a while to lock
    LGPS.powerOn();   

    // start with red ON and green OFF - they are wired thru npn transistors so that a +v high value is ON
    pinMode(rled, OUTPUT);
    pinMode(gled, OUTPUT);
    redledon();

    Serial1.begin(9600);  // STN1110 obd2 board from SparkFun
    Serial.begin(115200);
    
    obd2.Init(&Serial1);  // this runs obd2.check_supported_pids() before returning
    
    //Connect to the GRPS network in order to send/receive PubNub messages    
    while (!LGPRS.attachGPRS("att.mvno",NULL,NULL))
    {
        delay(1000);
    }
    
    // if we get here clear the gprsError flag
    gprsError = false;

    while(!LSMS.ready()) delay(1000);    
    
    PubNub.begin(pubkey, subkey);
    
    // Create initial json entry
    latlng.add(double_with_n_digits(38.355577,6));
    latlng.add(double_with_n_digits(-81.685728,6));    // create initial entry

}

int interval = 15000;  // millis per sample
int last = 0;          // start
int myspeed = 0;
int myrpm = 0;
long coolantTemp = 0;
long myload = 0;


void loop()
{
  if ( millis() > last + interval )
  {  // BEGIN main tasks path
    //Refresh Vehicle status, should be called in every loop
    obd2.Refresh();
    if(obd2.isIgnitionOn){
      myspeed = obd2.Speed();
      myrpm = obd2.RPM();
      obd2.get_pid(COOLANT_TEMP,&coolantTemp);
      obd2.get_pid(LOAD_VALUE,&myload);
      obdError = false;
    } else {
      obdError = true;
    }
    
    // get gps location info
    LGPS.getData(&info);
    parseGPGGA((const char*)info.GPGGA);
    
    Serial.println("Updating json");      // for Pubnub EON Mapbox map
    latlng.set(0,double_with_n_digits(convertDegMinToDecDeg(latitude),6));      // get 6 digits precision!
    latlng.set(1,double_with_n_digits(convertDegMinToDecDeg(longitude),6));
    root["speed"] = myspeed;
    root["rpm"] = myrpm;
    root["coolantTemp"] = coolantTemp;
    root.printTo(buff,80);
   
    strcpy(pub,leftbr);  // reset target string, put square brackets around json {...} so we have [{...}], EON wants a LIST
    strcat(pub,buff);
    strcat(pub,rightbr);
    Serial.println("publishing a message");
    Serial.println( pub );
    client = PubNub.publish(channel, pub, 60);  // channel,message,timeout
    if (!client) {
        Serial.println("publishing error");
        pubnubError = true;
    } else {
        pubnubError = false;
        while (client->connected()) {
          while (client->connected() && !client->available()); // wait
          char c = client->read();
          Serial.print(c);
        }
        client->stop();
        Serial.println();
    }
   
    // publish to Adafruit IO
    if (client->connect(server, port))
    {
      Serial.println("connected");
      // Make a HTTP request:
      snprintf(buff,255,"GET %s%li&CoolantTemp=%li&Location=%.6f,%.6f HTTP/1.1",path,myload,coolantTemp,convertDegMinToDecDeg(latitude),convertDegMinToDecDeg(longitude));
      client->println(buff);
      client->print("Host: ");
      client->println(server);
      client->println("Connection: close");
      client->println();
    }
    else
    {
      // if you didn't get a connection to the server:
      Serial.println("Adafruit connection failed");
    }
   
    if ( gpsError | gprsError | obdError | pubnubError ) 
    {
      redledon();
    } else {
      greenledon();
    }
    
    last = millis();  // update cycle time
   
  }  // END main tasks path
  
  // check for sms
  checkSMS();
    
}  // END loop()

// some handy led functions
static void redledon(void) {
  digitalWrite(rled, HIGH);
  digitalWrite(gled, LOW);
}

static void greenledon(void) {
  digitalWrite(rled, LOW);
  digitalWrite(gled, HIGH);
}

// GPS Functions

// converts lat/long from degree-minute format to decimal-degrees
static double convertDegMinToDecDeg (double degMin) {
  double min = 0.0;
  double decDeg = 0.0;
 
  //get the minutes, fmod() requires double
  min = fmod((double)degMin, 100.0);
 
  //rebuild coordinates in decimal degrees
  degMin = (int) ( degMin / 100 );
  decDeg = degMin + ( min / 60 );
 
  return decDeg;
}

static unsigned char getComma(unsigned char num,const char *str)
{
  unsigned char i,j = 0;
  int len=strlen(str);
  for(i = 0;i < len;i ++)
  {
     if(str[i] == ',')
      j++;
     if(j == num)
      return i + 1; 
  }
  return 0; 
}

static double getDoubleNumber(const char *s)
{
  char buf[10];
  unsigned char i;
  double rev;
  
  i=getComma(1, s);
  i = i - 1;
  strncpy(buf, s, i);
  buf[i] = 0;
  rev=atof(buf);
  return rev; 
}

static double getIntNumber(const char *s)
{
  char buf[10];
  unsigned char i;
  double rev;
  
  i=getComma(1, s);
  i = i - 1;
  strncpy(buf, s, i);
  buf[i] = 0;
  rev=atoi(buf);
  return rev; 
}

void parseGPGGA(const char* GPGGAstr)
{
  /* Refer to http://www.gpsinformation.org/dale/nmea.htm#GGA
   * Sample data: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
   * Where:
   *  GGA          Global Positioning System Fix Data
   *  123519       Fix taken at 12:35:19 UTC
   *  4807.038,N   Latitude 48 deg 07.038' N
   *  01131.000,E  Longitude 11 deg 31.000' E
   *  1            Fix quality: 0 = invalid
   *                            1 = GPS fix (SPS)
   *                            2 = DGPS fix
   *                            3 = PPS fix
   *                            4 = Real Time Kinematic
   *                            5 = Float RTK
   *                            6 = estimated (dead reckoning) (2.3 feature)
   *                            7 = Manual input mode
   *                            8 = Simulation mode
   *  08           Number of satellites being tracked
   *  0.9          Horizontal dilution of position
   *  545.4,M      Altitude, Meters, above mean sea level
   *  46.9,M       Height of geoid (mean sea level) above WGS84
   *                   ellipsoid
   *  (empty field) time in seconds since last DGPS update
   *  (empty field) DGPS station ID number
   *  *47          the checksum data, always begins with *
   */
 
  int tmp, hour, minute, second, num ;
  if(GPGGAstr[0] == '$')
  {
    tmp = getComma(1, GPGGAstr);
    hour     = (GPGGAstr[tmp + 0] - '0') * 10 + (GPGGAstr[tmp + 1] - '0');
    minute   = (GPGGAstr[tmp + 2] - '0') * 10 + (GPGGAstr[tmp + 3] - '0');
    second    = (GPGGAstr[tmp + 4] - '0') * 10 + (GPGGAstr[tmp + 5] - '0');

//    sprintf(buff, "UTC timer %2d-%2d-%2d", hour, minute, second);
//    Serial.println(buff);

    tmp = getComma(2, GPGGAstr);
    latitude = getDoubleNumber(&GPGGAstr[tmp]);
    tmp = getComma(3, GPGGAstr);
    NS = GPGGAstr[tmp];
    tmp = getComma(4, GPGGAstr);
    longitude = getDoubleNumber(&GPGGAstr[tmp]);
    tmp = getComma(5, GPGGAstr);
    EW = GPGGAstr[tmp];
    
    if (NS == 'S') latitude = -latitude;
    if (EW == 'W') longitude = -longitude;

//    sprintf(buff, "latitude = %10.4f, longitude = %10.4f", latitude, longitude);
//    Serial.println(buff);    
    
    tmp = getComma(7, GPGGAstr);
    num = getIntNumber(&GPGGAstr[tmp]);    

//    sprintf(buff, "satellites number = %d", num);
//    Serial.println(buff); 
    gpsError = false;
  }
  else
  {
    Serial.println("Did not get GPS data"); 
    gpsError = true;
  }
}

static void checkSMS()  // for text queries
{
    if(LSMS.available()) // Check if there is new SMS
    {
      
      char smsin[140];  // buffer for the text messages
      char cmd1[10];    // first token of sms command parsed
      char cmd2[10];    // second token of sms command parse
      char s[2] = " ";	// space seperated commands
      char buf[20];    // for sms sender number

      int v;
      int stri = 0;
      char smsreply[140];
      long int pidquery = 0;

      Serial.println("There is new message.");
      LSMS.remoteNumber(buf, 20); // display Number part
      Serial.print("Number:");
      Serial.println(buf);
      Serial.print("Content:"); // display Content part

      while(true)
      {
        v = LSMS.read();
        if(v < 0) {            // -1 end of message
          smsin[stri] = '\0';  // end of message, null terminate incoming message at index
          break;
        } else
          smsin[stri++] = (char)v;  // save the char in buffer and then increment index
      }
      LSMS.flush(); // delete message
      
      // tokenize the first two space seperated words in the message, there can be more than two, rest ignored
      strcpy(cmd1,strtok(smsin,s));
      strcpy(cmd2,strtok(NULL,s));    // FIXME if only one token, this is garbage!
      
      if (strcmp(cmd1,"get") == 0 | strcmp(cmd1,"Get") == 0) {
        if ( strcmp(cmd2,"mil") == 0 | strcmp(cmd2,"Mil") == 0 | strcmp(cmd2,"cel") == 0 | strcmp(cmd2,"Cel") == 0 ) {
          Serial.println("got request for mil code");
          if(obd2.isIgnitionOn){
            obd2.get_pid(MIL_CODE,&pidquery);
            // here we can check and use and intelligently respond according to the bit codes in L1_obd2.h
            ltoa(pidquery,smsreply,10);    // write long base 10 to smsreply buffer
            if ( (pidquery>>MIL)&1 == 1 ) // Check Engine light is on
            {
              strcat(smsreply," Check Engine Light is on");
            }
            if ( (pidquery>>CAT_AVAIL)&1 == 1 ) // catalyst test available, for example
            {
              strcat(smsreply," Catalyst test available");
            }
          } else {
            strcpy(smsreply,"OBD reports ignition off");
          }
          LSMS.beginSMS(buf);                // reply to sending number
          LSMS.print(smsreply);
          if(LSMS.endSMS()) Serial.println("mil reply message sent");
        } else if ( strcmp(cmd2,"coolant") == 0 | strcmp(cmd2,"Coolant") == 0 ) {
          Serial.println("got request for coolant temp");
          if(obd2.isIgnitionOn){
            obd2.get_pid(COOLANT_TEMP,&pidquery);
            ltoa(pidquery,smsreply,10);
            strcat(smsreply," deg c");
          } else {
            strcpy(smsreply,"OBD reports ignition off");
          }
          LSMS.beginSMS(buf);
          LSMS.print(smsreply);
          if(LSMS.endSMS()) Serial.println("coolant temp reply message sent");
        }
        // need setion to get DTC_CNT, count of diagnostic trouble codes
        // need section to get DTC, mode 3 
        // https://en.wikipedia.org/wiki/OBD-II_PIDs#Mode_3_.28no_PID_required.29
      }
            
    }  // END if sms
}    
    
