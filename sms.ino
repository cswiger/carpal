char buf[20];    // for sms sender number, used in both functions

static void checkSMS(void)  // for text queries
{
    if(LSMS.available()) // Check if there is new SMS
    {
      
      char smsin[140];  // buffer for the text messages
      int v;
      int stri = 0;

      Serial.println(F("There is new message."));
      LSMS.remoteNumber(buf, 20); // display Number part
      Serial.print(F("Number:"));
      Serial.println(buf); 

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
    }
}


void parseCMDS(char (&commands)[140])
{
  char cmds[10][10];    // 10 commands up to 10 char
  char s[2] = " ";	// space seperated commands
  int ti = 0;      // number of commands and options in the array

  char *temp = strtok(commands,s);
  while(temp) {
    strcpy(cmds[ti++],temp);
    temp = strtok(NULL,s);
  }
  // now ti contains the number of tokens in the array to process

  if (strcmp(cmds[0],"get") == 0 | strcmp(cmds[0],"Get") == 0)
  {
    parseGET(cmds,ti);
  } else if (strcmp(cmds[0],"log") == 0 | strcmp(cmds[0],"Log") == 0 ) 
  { 
    parseLOG(cmds,ti);
  }     
}

void parseGET(char (&gcmds)[10][10],int ti) {
  long int pidquery = 0;
  char smsreply[140];
  
  if ( strcmp(gcmds[1],"mil") == 0 | strcmp(gcmds[1],"Mil") == 0 | strcmp(gcmds[1],"cel") == 0 | strcmp(gcmds[1],"Cel") == 0 )
  {
    Serial.println(F("got request for mil code"));
    if(obd2.isIgnitionOn)
    {
      obd2.get_pid(MIL_CODE,&pidquery);
      // here we can check and use and intelligently respond according to the MIL_CODE bit values in L1_obd2.h
      ltoa(pidquery,smsreply,10);    // write long base 10 to smsreply buffer
      if ( (pidquery>>MIL)&1 == 1 ) // Check Engine light is on
      {
        strcat(smsreply," Check Engine Light is on and there are ");
        long int numdtc = (pidquery>>DTC_CNT)&127;
        char numdtcs[2];
        ltoa(numdtc,numdtcs,10);
        strcat(smsreply,numdtcs);
        strcat(smsreply," diag test codes");
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
    if(LSMS.endSMS()) Serial.println(F("mil reply message sent"));
  } else if ( strcmp(gcmds[1],"coolant") == 0 | strcmp(gcmds[1],"Coolant") == 0 )
  {
    Serial.println(F("got request for coolant temp"));
    if(obd2.isIgnitionOn){
      obd2.get_pid(COOLANT_TEMP,&pidquery);
      ltoa(pidquery,smsreply,10);
      strcat(smsreply," deg c");
    } else {
      strcpy(smsreply,"OBD reports ignition off");
    }
    LSMS.beginSMS(buf);
    LSMS.print(smsreply);
    if(LSMS.endSMS()) Serial.println(F("coolant temp reply message sent"));
  } 
  // need section to get DTC, mode 3 - obd2 library has obd2.dtc_read(), populates a struct DTC[dtclen].codes with P12345, etc
  // https://en.wikipedia.org/wiki/OBD-II_PIDs#Mode_3_.28no_PID_required.29

}

void parseLOG(char (&lcmds)[10][10], int ti) {
  char smsreply[140];

  if (strcmp(lcmds[1],"stop") == 0)
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
    Serial.println(F("Stop all logging"));
    LSMS.beginSMS(buf);                // reply to sending number
    strcpy(smsreply,"Logging stopped");
    LSMS.print(smsreply);
    if(LSMS.endSMS()) Serial.println(F("log stop message sent"));
    return;
  } else if (strcmp(lcmds[1],"start") == 0)   // start logging 
  {
    if (strcmp(lcmds[2],"gprs") == 0 )
    {
      log_gprs = true;
      Serial.println(F("Logging to gprs"));
      strcpy(smsreply,"log gprs ");
    } else if (strcmp(lcmds[2],"sdcard") == 0 )
    {
      log_sdcard = true;
      // create filename at this point in time based on timestamp
      LDateTime.getTime(&t);
      snprintf(fn,80,"%.4d%.2d%.2d_%.2d%.2d%.2d.json",t.year,t.mon,t.day,t.hour,t.min,t.sec);
      Serial.print(F("Logging to sdcard file: "));
      Serial.println(fn);
      strcpy(smsreply,"log sdcard ");
      strcat(smsreply,fn);
      strcat(smsreply," ");
    } else 
    {
      // not "log start gprs" not "log start sdcard" - reply with error
      strcpy(smsreply,"error in command - use 'log start|stop gprs|sdcard <PIDs to log>'");
      LSMS.beginSMS(buf);                // reply to sending number
      LSMS.print(smsreply);
      if(LSMS.endSMS()) Serial.println(F("error reply message sent"));
      return;
    }
    // still in log start, check all options in rest of tokens
    for (int j=3; j<ti; j++)    // ti # of tokens in cmd array
    {
      if (strcmp(lcmds[j],"location") == 0) { log_location = true; strcat(smsreply,"location "); }
      if (strcmp(lcmds[j],"fuel") == 0) { log_fuel = true; strcat(smsreply,"fuel "); }
      if (strcmp(lcmds[j],"load") == 0) { log_load = true; strcat(smsreply,"load "); }
      if (strcmp(lcmds[j],"coolant") == 0) { log_coolant = true; strcat(smsreply,"coolant "); }
      if (strcmp(lcmds[j],"stft_b1") == 0) { log_stft_b1 = true; strcat(smsreply,"stft_b1 "); }
      if (strcmp(lcmds[j],"ltft_b1") == 0) { log_ltft_b1 = true; strcat(smsreply,"ltft_b1 "); }
      if (strcmp(lcmds[j],"stft_b2") == 0) { log_stft_b2 = true; strcat(smsreply,"stft_b2 "); }
      if (strcmp(lcmds[j],"ltft_b2") == 0) { log_ltft_b2 = true; strcat(smsreply,"ltft_b2 "); }
      if (strcmp(lcmds[j],"rpm") == 0) { log_rpm = true; strcat(smsreply,"rpm "); }
      if (strcmp(lcmds[j],"speed") == 0) { log_speed = true; strcat(smsreply,"speed "); }
      if (strcmp(lcmds[j],"timing") == 0) { log_timing_adv = true; strcat(smsreply,"timing "); }
      if (strcmp(lcmds[j],"air_temp") == 0) { log_int_air_temp = true; strcat(smsreply,"air_temp "); }
      if (strcmp(lcmds[j],"air_flow") == 0) { log_maf_air_flow = true; strcat(smsreply,"air_flow "); }
      if (strcmp(lcmds[j],"throttle") == 0) { log_throttle_pos = true; strcat(smsreply,"throttle "); }
      if (strcmp(lcmds[j],"b1s2_o2_v") == 0) { log_b1s2_o2_v = true; strcat(smsreply,"bls2_o2_v "); }
    }  // end of cmd scan for loop
  LSMS.beginSMS(buf);                // reply to sending number
  LSMS.print(smsreply);
  if(LSMS.endSMS()) Serial.println("log start message sent");  
  } else  // end of if(start) logging  
  {
    // not "log start" not "log stop" - reply with error
    strcpy(smsreply,"error in command - use 'log start|stop gprs|sdcard <PIDs to log>'");
    LSMS.beginSMS(buf);                // reply to sending number
    LSMS.print(smsreply);
    if(LSMS.endSMS()) Serial.println(F("log error message sent"));  
  }
}

