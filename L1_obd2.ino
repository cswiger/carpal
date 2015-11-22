/*
  CarPal! Your automobile's OBD2 dashboard buddy. 

  v5a - cleanup and debug sms logging commands
  v5 - moved gps functions to it's own tab, added sd card support
  v4 - moved checkSMS to it's own tab for expanding sms command parsing, added the logging bool switches that can be turned
       on/off via sms - how to delete a json item when logging has been turned off, instead of just repeating the last value logged?
       also next: add sdcard support and how to get accurate timestamps from cell service
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
char path[] = "/api/groups/AutoStats/send.json?x-aio-key=xxxxyourxaioxkeyxherexxxxxxxxxxxxxxxxxxx&Load=";
int port = 80; // HTTP

//import all the necessary files for GPRS connectivity
#include "LGPRS.h"
#include "LGPRSClient.h"

// for clock and filename timestamps
#include <LDateTime.h>

// and SMS
#include <LGSM.h>


// GPS and math for converting deg.min+decimal to deg.decimal
#include <LGPS.h>
#include <math.h>

// SD storage card
#include <LSD.h>
// filename for storing data
char fn[80];
// filename of startup logging commands
char startup[] = "logging.cmd";

// OBD2 library
#include "OBD2.h"

// for setting and getting the date time
datetimeInfo t;
// Globals for parseGPRMC to set
int year, month, day, hour, minute, second;

// For preparing the pubnub messages
#include <ArduinoJson.h>
StaticJsonBuffer<200> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();
JsonArray& latlng = root.createNestedArray("latlng");

// leds for system status
#define rled 3
#define gled 2

//define the required keys for using PubNub
char pubkey[] = "pub-c-a7e5745b-0ad1-42ac-8cce-xxxyourxpubxkeyxhere";
char subkey[] = "sub-c-4df1fc96-825f-11e5-8495-xxxyourxsubxkeyxhere";
char channel[] = "cardata";

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
char pub[256];
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
    latlng.add(double_with_n_digits(0.000000,6));
    latlng.add(double_with_n_digits(0.000000,6));    // create initial entry

    // sync up clock with gps
    while ( year < 2015 )   // sanity check we actually sync'd up with gps, linkit one defaults to 2004
   {
     Serial.println("Getting date");
     delay(1000);
     LGPS.getData(&info);  
     parseGPRMC((const char*)info.GPRMC);
   }
   
   t.year = year;
   t.mon = month;
   t.day = day;
   t.hour = hour;
   t.min = minute;
   t.sec = second;

   Serial.println("Setting date");   
   LDateTime.setTime(&t);

   // prep sd card for logs - make sure the SPI/sdcard switch is in the 'sdcard' position
   Serial.print("Initializing SD card...");
   // make sure that the default chip select pin is set to
   // output, even if you don't use it:
   pinMode(10, OUTPUT);

   // see if the card is present and can be initialized:
   LSD.begin();
   Serial.println("card initialized.");

}

int interval = 15000;  // millis per sample
int last = 0;          // start

// these have their own function in obd2 library
int vehicle_speed = 0;
int engine_rpm = 0;
// everything else is return long
long fuel_status = 0;
long engine_load = 0;
long coolant_temp = 0;
long stft_b1 = 0;
long ltft_b1 = 0;
long stft_b2 = 0;
long ltft_b2 = 0;
long timing_adv = 0;
long int_air_temp = 0;
long maf_air_flow = 0;
long throttle_pos = 0;
long b1s2_o2_v = 0;


void loop()
{
  if ( millis() > last + interval )
  {  // BEGIN main tasks path
    //Refresh Vehicle status, should be called in every loop
    obd2.Refresh();
    if(obd2.isIgnitionOn){
      if (log_speed) 
      {
        vehicle_speed = obd2.Speed();
        root["speed"] = vehicle_speed;
      }
      if (log_rpm)
      {
        engine_rpm = obd2.RPM();
        root["rpm"] = engine_rpm;
      }
      if (log_fuel) 
      {
        obd2.get_pid(FUEL_STATUS,&fuel_status);
        root["fuel"] = fuel_status;
      }
      if (log_load) 
      {
        obd2.get_pid(LOAD_VALUE,&engine_load);
        root["load"] = engine_load;
      }
      if (log_coolant) 
      {
        obd2.get_pid(COOLANT_TEMP,&coolant_temp); 
        root["coolant"] = coolant_temp;
      }
      if (log_stft_b1) 
      {
        obd2.get_pid(STFT_BANK1,&stft_b1);
        root["stftb1"] = stft_b1;
      }
      if (log_ltft_b1) 
      {
        obd2.get_pid(LTFT_BANK1,&ltft_b1);
        root["ltftb1"] = ltft_b1;
      }
      if (log_stft_b2) 
      {
        obd2.get_pid(STFT_BANK2,&stft_b2);
        root["stftb2"] = stft_b2;
      }
      if (log_ltft_b2) 
      {
        obd2.get_pid(LTFT_BANK2,&ltft_b2);
        root["ltftb2"] = ltft_b2;
      }
      if (log_timing_adv) 
      {
        obd2.get_pid(TIMING_ADV,&timing_adv);
        root["timing"] = timing_adv;
      }
      if (log_int_air_temp) 
      {
        obd2.get_pid(INT_AIR_TEMP,&int_air_temp);
        root["intairtemp"] = int_air_temp;
      }
      if (log_maf_air_flow) 
      {
        obd2.get_pid(MAF_AIR_FLOW,&maf_air_flow);
        root["mafairflow"] = maf_air_flow;
      }
      if (log_throttle_pos) 
      {
        obd2.get_pid(THROTTLE_POS,&throttle_pos);
        root["throttlepos"] = throttle_pos;
      }
      if (log_b1s2_o2_v) 
      {
        obd2.get_pid(B1S2_O2_V,&b1s2_o2_v);
        root["b1s2o2v"] = b1s2_o2_v;
      }
      obdError = false;
    } else {
      obdError = true;
    }
    
    if(log_location)
    {
      // get gps location info
      Serial.println("Updating json");      // for Pubnub EON Mapbox map
      LGPS.getData(&info);
      parseGPGGA((const char*)info.GPGGA);
      latlng.set(0,double_with_n_digits(convertDegMinToDecDeg(latitude),6));      // get 6 digits precision!
      latlng.set(1,double_with_n_digits(convertDegMinToDecDeg(longitude),6));
    }
    
    root.printTo(buff,80);
   
    strcpy(pub,leftbr);  // reset target string, put square brackets around json {...} so we have [{...}], EON wants a LIST
    strcat(pub,buff);
    strcat(pub,rightbr);
    if (log_gprs) {
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
      
      // publish to Adafruit IO - this is a fixed format of what to log, if not logged just gets zeros
      if (client->connect(server, port))
      {
        Serial.println("connected to adafruit");
        // Make a HTTP request:
        snprintf(buff,255,"GET %s%li&CoolantTemp=%li&Location=%.6f,%.6f HTTP/1.1",path,engine_load,coolant_temp,convertDegMinToDecDeg(latitude),convertDegMinToDecDeg(longitude));
        client->println(buff);
        client->print("Host: ");
        client->println(server);
        client->println("Connection: close");
        client->println();
        gprsError = false;
      }
      else
      {
        // if you didn't get a connection to the server:
        Serial.println("Adafruit connection failed");
        gprsError = true;
      }
    }     // don't want else clause here, can log BOTH gprs and sdcard
    
    if (log_sdcard)
    {
      // log to sd card - filename fn was set when log sdcard started
      LFile dataFile = LSD.open(fn, FILE_WRITE);
      dataFile.println(pub);
      dataFile.close();
      Serial.println("Logged to sdcard");
      Serial.println(pub);
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


