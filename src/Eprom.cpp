#include "Eprom.h"
#include <EEPROM.h>
#include <Arduino.h>

Eprom::Eprom(){

}
 void Eprom::begin()
 {
   EEPROM.begin(1024); 
 }

 boolean Eprom::dataIsSet(){
   byte b = EEPROM.read(0);
   return b == 'x';
 }
 boolean Eprom::shouldUpdateFirmware(){
   byte b = EEPROM.read(1);
   return b == 'x';
 }
 void Eprom::setShouldUpdateFirmware(boolean should){
   byte v = '0';
   if(should){
     v = 'x';
   }
   EEPROM.write(1, v);
   EEPROM.commit();
 }
 void Eprom::writeLong(int address, uint32_t value)
 {
   byte four = (value & 0xFF);
   byte three = ((value >> 8) & 0xFF);
   byte two = ((value >> 16) & 0xFF);
   byte one = ((value >> 24) & 0xFF);
   
   //Write the 4 bytes into the eeprom memory.
   EEPROM.write(address, four);
   EEPROM.write(address + 1, three);
   EEPROM.write(address + 2, two);
   EEPROM.write(address + 3, one);
 }
 uint32_t Eprom::readLong(int address)
 {
   //Read the 4 bytes from the eeprom memory.
   long four = EEPROM.read(address);
   long three = EEPROM.read(address + 1);
   long two = EEPROM.read(address + 2);
   long one = EEPROM.read(address + 3);
   
   //Return the recomposed long by using bitshift.
   return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
 }
 
 
 
