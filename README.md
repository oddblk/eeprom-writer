# eeprom-writer
Arduino firmware for EEPROM Writer project.

This code should be compiled by the Arduino IDE and run on an Arduino Mega 2560.

For project documentation, refer to http://danceswithferrets.org/geekblog/?page_id=903

Distributed under an acknowledgement licence, because I'm a shallow, attention-seeking tart. :)


## Updates
Two new commands added:

### D - Check and set write delay

D? - Print current delay time
Dn - Set delay to n micro seconds

### F - Try to find a working write delay

F - loop values from current write delay to 100us more with a preset pattern
Faddr:data - loop using the given pattern on given address
