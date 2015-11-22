static void redledon(void);
static void greenledon(void);
static double convertDegMinToDecDeg (double degMin);
static unsigned char getComma(unsigned char num,const char *str);
static double getDoubleNumber(const char *s);
static double getIntNumber(const char *s);
void parseGPGGA(const char* GPGGAstr);
void parseGPRMC(const char* GPRMCstr);
static void checkSMS(void);
void parseCMDS(char (&commands)[140]);

/* MIL_CODE bit value meanings, from https://en.wikipedia.org/wiki/OBD-II_PIDs#Mode_1_PID_01
   My Avalon returns 484608 or 00000000 00000111 01100101 00000000
   Test with something like:

   if ((val>>CAT_AVAIL)&1 == 1 ) { strcpy(smsreply,"Catalyst test available"); }

 */
#define MIL 31            // Malfunction indicator light, check engine light A7
#define DTC_CNT 24        // diagnostic test codes count, A0-A6
#define RESERVED 23       // reserved, should be 0, B7
#define COMPS_INCOMPL 22  // components test incomplete, B6
#define FUELS_INCOMPL 21  // fuel system test incomplete, B5
#define MISF_INCOMPL 20   // misfire test incomplete, B4
#define COMP_SPK 19       // compression (1) or spark (0) ignition, B3
#define COMPS_AVAIL 18    // components test available, B2
#define FUELS_AVAIL 17    // fuel system test available, B1
#define MISF_AVAIL 16     // misfire test available, B0
#define EGR_AVAIL 15      // EGR system test available, C7
#define O2SENH_AVAIL 14   // Oxygen sensor heator test available, C6
#define O2SEN_AVAIL 13    // Oxygen sensor test available, C5
#define ACREF_AVAIL 12    // A/C refrigerant test available, C4
#define SECAS_AVAIL 11    // Secondary air system test available, C3
#define EVAP_AVAIL 10     // Evaporative system test available, C2
#define HCAT_AVAIL 9      // Heated catalyst test available, C1
#define CAT_AVAIL 8       // Catalyst test available, C0
#define EGR_INCOMPL 7     // EGR system test incomplete, D7
#define OS2SENH_INCOMPL 6 // Oxygen sensor heator test incomplete, D6
#define O2SEN_INCOMPL 5   // Oxygen sensor test incomplete, D5
#define ACREF_INCOMPL 4   // A/C refrigerant test incomplete, D4
#define SECAS_INCOMPL 3   // Secondary air system test incomplete, D3
#define EVAP_INCOMPL 2    // Evaporative system test incomplete, D2
#define HCAT_INCOMPL 1    // Heated catalyst test incomplete, D1
#define CAT_INCOMPL 0     // Catalyst test incomplete, D0

// various items to log
bool logging = false;    // to log or not to log, that is the flag - is this needed? if either log_gprs or log_sdcard is set should suffice
bool log_gprs = false;   // where to log, over the radio
bool log_sdcard = false;  // or to local storage
bool log_location = false;  // gps coordinates
bool log_fuel = false;    // fuel system status pid 0x03
bool log_load = false;    // engine load pid 0x04
bool log_coolant = false;  // coolant temp pid 0x05
bool log_stft_b1 = false;   // short term fuel trim bank 1 pid 0x06  -- these are as yet unprocessed in obd2 library, just raw A value
bool log_ltft_b1 = false;   // long term fuel trim bank 1 pid 0x07   -- should be (A-128) * 100/128,  -100 to 99.22
bool log_stft_b2 = false;   // short term fuel trim bank 2 pid 0x08
bool log_ltft_b2 = false;   // long term fuel trim bank 2 pid 0x09
bool log_rpm = false;      // engine rpm 0x0C
bool log_speed = false;    // vehicle speed pid 0x0D
bool log_timing_adv = false; // timing advance pid 0x0E
bool log_int_air_temp = false; // intake air temp pid 0x0F
bool log_maf_air_flow = false;  // MAF air flow rate pid 0x10
bool log_throttle_pos = false;  // throttle position
bool log_b1s2_o2_v = false;     // bank 1, sensor 2: oxygen sensor voltage, short term fuel trim
