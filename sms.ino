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
