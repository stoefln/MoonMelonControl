// OTA Updates
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <QueueArray.h>

void patchFirmware(const String& url) {
  Serial.print("trying to download patch from "); Serial.print(url); Serial.println("...");
  t_httpUpdate_return ret = ESPhttpUpdate.update(url);
  Serial.print("result: ");Serial.println(ret);

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

int getMaxQVal(QueueArray<int> & sensorQ){
  int val = 0;
  int maxVal = 0;
  while(!sensorQ.isEmpty()){
    val = sensorQ.pop();
    if(val > maxVal){
      maxVal = val;
    }
  }
  return maxVal;
}