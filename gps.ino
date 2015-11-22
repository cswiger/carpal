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
 
  int tmp, num ;     // hour, minute, second are now global
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

// use this to get year, month day 
void parseGPRMC(const char* GPRMCstr)
{
  /* Refer to http://www.gpsinformation.org/dale/nmea.htm#RMC
   * Sample data: $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
   *
   * Where:
   *   RMC          Recommended Minimum sentence C
   *   123519       Fix taken at 12:35:19 UTC
   *   A            Status A=active or V=Void.
   *   4807.038,N   Latitude 48 deg 07.038' N
   *   01131.000,E  Longitude 11 deg 31.000' E
   *   022.4        Speed over the ground in knots
   *   084.4        Track angle in degrees True
   *   230394       Date - 23rd of March 1994
   *   003.1,W      Magnetic Variation
   *  *6A          The checksum data, always begins with *
   */
  
  
  double latitude;
  double longitude;
  char NS;          // for gps quadrant, N(+)l S(-)latitude, E(+) W(-) longitude
  char EW;
  int tmp, num ;
  if(GPRMCstr[0] == '$')
  {
    tmp = getComma(1, GPRMCstr);
    hour     = (GPRMCstr[tmp + 0] - '0') * 10 + (GPRMCstr[tmp + 1] - '0');
    minute   = (GPRMCstr[tmp + 2] - '0') * 10 + (GPRMCstr[tmp + 3] - '0');
    second    = (GPRMCstr[tmp + 4] - '0') * 10 + (GPRMCstr[tmp + 5] - '0');
    
    //sprintf(buff, "UTC timer %2d-%2d-%2d", hour, minute, second);
    //Serial.println(buff);
    
    tmp = getComma(9, GPRMCstr);
    day     = (GPRMCstr[tmp + 0] - '0') * 10 + (GPRMCstr[tmp + 1] - '0');
    month   = (GPRMCstr[tmp + 2] - '0') * 10 + (GPRMCstr[tmp + 3] - '0');
    year    = (GPRMCstr[tmp + 4] - '0') * 10 + (GPRMCstr[tmp + 5] - '0') + 2000;

    //sprintf(buff, "Year: %d, Month: %d, Day: %d\n", year, month, day);
    //Serial.println(buff);
   
  }
  else
  {
    Serial.println("Not get data"); 
  }
}


