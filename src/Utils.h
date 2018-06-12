// OTA Updates
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

void patchFirmware() {
  Serial.println("patchFirmware2...");
  t_httpUpdate_return ret = ESPhttpUpdate.update("http://192.168.0.164:8000/Firmware/moon_melon.bin");
  Serial.println("patchFirmware done.");

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES\n");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK\n");
      break;
  }
}