char buf[20];    // for sms sender number, used in both functions

static void checkSMS(void)  // for text queries
{
    if(LSMS.available()) // Check if there is new SMS
    {
      
      char smsin[140];  // buffer for the text messages
      int v;
      int stri = 0;

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
      
      parseCMDS(smsin);
    }  // END if sms
}   // END of checkSMS() 


void parseCMDS(char (&commands)[140])
{
  char smsreply[140];
  long int pidquery = 0;
  char cmd[10][10];
  char s[2] = " ";	// space seperated commands
  int ti = 0;

  char *temp = strtok(commands,s);
  while(temp) {
    strcpy(cmd[ti++],temp);
    temp = strtok(NULL,s);
  }
  // now ti contains the number of tokens in the array to process

  if (strcmp(cmd[0],"get") == 0 | strcmp(cmd[0],"Get") == 0)
  {  // start cmd[0] processing
    if ( strcmp(cmd[1],"mil") == 0 | strcmp(cmd[1],"Mil") == 0 | strcmp(cmd[1],"cel") == 0 | strcmp(cmd[1],"Cel") == 0 )
    {
      Serial.println("got request for mil code");
      if(obd2.isIgnitionOn)
      {
        obd2.get_pid(MIL_CODE,&pidquery);
        // here we can check and use and intelligently respond according to the MIL_CODE bit values in L1_obd2.h
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
    } else if ( strcmp(cmd[1],"coolant") == 0 | strcmp(cmd[1],"Coolant") == 0 )
    {
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
    } // END if cmd[1] eq "mil","coolant"
    // need setion to get DTC_CNT, count of diagnostic trouble codes
    // need section to get DTC, mode 3 
    // https://en.wikipedia.org/wiki/OBD-II_PIDs#Mode_3_.28no_PID_required.29
  } else if (strcmp(cmd[0],"log") == 0 | strcmp(cmd[0],"Log") == 0 ) 
  { // begin log processing
    if (strcmp(cmd[1],"stop") == 0)
    { // log stop
      logging = false;
      // turn everything off
      log_gprs = false;
      log_sdcard = false;
      log_location = false;
      log_fuel = false;
      log_load = false;
      log_coolant = false;
      log_stft_b1 = false;
      log_ltft_b1 = false;
      log_stft_b2 = false;
      log_ltft_b2 = false;
      log_rpm = false; 
      log_speed = false;
      log_timing_adv = false;
      log_int_air_temp = false;
      log_maf_air_flow = false;
      log_throttle_pos = false;
      log_b1s2_o2_v = false;
      // clean out the json
      root.remove("speed");
      root.remove("rpm");
      root.remove("fuel");
      root.remove("load");
      root.remove("coolant");
      root.remove("stftb1");
      root.remove("ltftb1");
      root.remove("stftb2");
      root.remove("ltftb2");
      root.remove("timing");
      root.remove("intairtemp");
      root.remove("mafairflow");
      root.remove("throttlepos");
      root.remove("b1s2o2v");
      latlng.set(0,0.0);   // guess these will always be in the json ?
      latlng.set(1,0.0);
      Serial.println("Stop all logging");
      return;
    } else if (strcmp(cmd[1],"start") == 0)   // start logging 
    {
      if (strcmp(cmd[2],"gprs") == 0 )
      {
        log_gprs = true;
        Serial.println("Logging to gprs");
      } else if (strcmp(cmd[2],"sdcard") == 0 )
      {
        log_sdcard = true;
        // create filename at this point in time based on timestamp
        LDateTime.getTime(&t);
        snprintf(fn,80,"%.4d%.2d%.2d_%.2d%.2d%.2d.json",t.year,t.mon,t.day,t.hour,t.min,t.sec);
        Serial.print("Logging to sdcard file: ");
        Serial.println(fn);
      } else 
      {
        // not "log start gprs" not "log start sdcard" - reply with error
        strcpy(smsreply,"error in command - use 'log start gprs location speed'");
        LSMS.beginSMS(buf);                // reply to sending number
        LSMS.print(smsreply);
        if(LSMS.endSMS()) Serial.println("error reply message sent");
        return;
      }
      // still in log start, check all options in rest of tokens
      for (int j=3; j<ti; j++)    // ti # of tokens in cmd array
      {
        if (strcmp(cmd[j],"location") == 0) log_location = true;
        if (strcmp(cmd[j],"fuel") == 0) log_fuel = true;
        if (strcmp(cmd[j],"load") == 0) log_load = true;
        if (strcmp(cmd[j],"coolant") == 0) log_coolant = true;
        if (strcmp(cmd[j],"stft_b1") == 0) log_stft_b1 = true;
        if (strcmp(cmd[j],"ltft_b1") == 0) log_ltft_b1 = true;
        if (strcmp(cmd[j],"stft_b2") == 0) log_stft_b2 = true;
        if (strcmp(cmd[j],"ltft_b2") == 0) log_ltft_b2 = true;
        if (strcmp(cmd[j],"rpm") == 0) log_rpm = true;
        if (strcmp(cmd[j],"speed") == 0) log_speed = true;
        if (strcmp(cmd[j],"timing") == 0) log_timing_adv = true;
        if (strcmp(cmd[j],"int_air_temp") == 0) log_int_air_temp = true;
        if (strcmp(cmd[j],"maf_air_flow") == 0) log_maf_air_flow = true;
        if (strcmp(cmd[j],"throttle") == 0) log_throttle_pos = true;
        if (strcmp(cmd[j],"b1s2_o2_v") == 0) log_b1s2_o2_v = true;
      }  // end of cmd scan for loop
    } // end of if(start) logging  
  } // end of if(log)     
}
