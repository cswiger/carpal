# carpal
Code for Linkit One to interface with an automobile OBD2 port to analyse and publish data. 

Clone into a directory "L1_obd2" and open with Arduino for Linkit One skd. 

Put 'commands.txt' in the root of your sdcard to have logging to sdcard start automatically on power up.
Can change it to something like "log start gprs location" to have EON maps start automatically, etc. 
All logging is by default OFF unless over-ridden with a startup command or turned on via SMS texts. 
