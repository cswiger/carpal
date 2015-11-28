#!/usr/bin/env python
# easy python script to convert a CarPal log file from json to csv for uploading to MapBox
# looks for 'latlng' position and currently 'speed' in the json - MapBox allows 1 label per marker

import sys
import json

if len(sys.argv) != 3:
  print("Usage: ./mapit.py in.json out.csv")
  sys.exit(1)

fh = open(sys.argv[1],'r')

line = fh.readline()
jtrip = json.loads(line)
line = fh.readline()
while line != '':
  jtrip += json.loads(line)
  line = fh.readline()

fh.close()
fh = open(sys.argv[2],'w')
fh.write("latitude,longitude,speed\n")
for i in range(len(jtrip)):
   fh.write(str(jtrip[i]["latlng"][0]) + "," + str(jtrip[i]["latlng"][1]) + "," + str(jtrip[i]["speed"]) + "\n")

fh.close()

