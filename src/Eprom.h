#include <Arduino.h>

class Eprom {
private:
 
public:
 Eprom();
 void begin();
 boolean dataIsSet();
 void setShouldUpdateFirmware(boolean should);
 boolean shouldUpdateFirmware();
 uint32_t readLong(int );
 void writeLong(int address, uint32_t value);
};
