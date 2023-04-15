// MStack GSM Wardriver - by Jilles Groenendijk (2023-04-16)

#include <M5Stack.h>
#include <TinyGPS++.h> // http://arduiniana.org/libraries/tinygpsplus/

#define DISPLAY_INTERVAL 5000
#define SCAN_INTERVAL 5000

TinyGPSPlus gps;
char gsmterm[256];
uint32_t GSMTermOffset = 0;
uint32_t TowersFound = 0;
unsigned long LastScan = millis();
char gps_date[12];
char gps_time[12];
char gps_lat[12];
char gps_lng[12];
char gps_alt[12];
char gps_hdp[12];
char gps_age[12];
char gps_sat[12];
String Network[3];
int NetworkIndex=0;
bool validfix=false;

String OldFilename="";
String Filename="";
File FileHandle;

void setup() {

  M5.begin();
  M5.Power.begin();
  
  M5.Lcd.clear();
  M5.Lcd.setBrightness(1);
  M5.Lcd.setTextColor(GREEN, BLACK);

  if ( !SD.begin() ) { 
    M5.Lcd.println("Card failed, or not present");
    while (1);
  } 

  // Console
  Serial.begin(115200);
  
  // Reset COM.GSM Module
  pinMode(2, OUTPUT);
  digitalWrite(2, 0);
  delay(5000);
  digitalWrite(2, 1);  

  // Port         Speed    Bitconfig  TX  RX 
  //Serial1.begin(115200, SERIAL_8N1, 16, 17);  // SIM800Module
  Serial1.begin(9600, SERIAL_8N1,  5,  0);      // COM.GSM
 
  Serial1.print("AT+CNETSCAN\r\n");                       

  // GPS
  // Port       Speed  Bitconfig  TX  RX 
  Serial2.begin(9600, SERIAL_8N1, 15, 13);      // COM.GPS

  for(int i=0;i<4;i++) Network[i]="n/a";
  gsmterm[0] = '\0';

}

void loop() {
  unsigned long start = millis();

  displayInfo();

  do {
    while (Serial1.available() > 0) gsmencode(SD,Serial1.read());
    while (Serial2.available() > 0) gps.encode(Serial2.read());
  } while (millis() - start < DISPLAY_INTERVAL);

  if (millis() - SCAN_INTERVAL < LastScan) {
    Serial1.print("AT+CNETSCAN=1\r\n");
    delay(1000);
    Serial1.print("AT+CNETSCAN\r\n");
    LastScan = millis();
  }
}

bool gsmencode(fs::FS &fs, char c) {
  String Line;
  String Filename;
  int pos=0;
  switch(c) {
    case '\r': {
      Line=String(gsmterm);  
      if ( Line.indexOf("Operator:")==1 ) {
 
        Line.replace("Operator","");
        Line.replace("MNC","");
        Line.replace("MCC","");
        Line.replace("Rxlev","");
        Line.replace("Cellid","");
        Line.replace("Arfcn","");
        Line.replace("Lac","");
        Line.replace("Bsic","");
        Line.replace(":","");
        Line.trim();

        pos=Line.indexOf(',');
        int items=3;
        for(int i=0;i<Line.length();i++) {
          if(Line[i]==',' && items-->0) {
            pos=i;
          }
        }        
        if(pos>0){
          for(int i=0;i<=3;i++) {
            if(Network[i]==Line.substring(0,pos)) {
              pos=0;
              i=4;              
            }
          }
          if(validfix) {
            Filename="/"+String(gps_date)+".csv";
            Filename.replace("-","");

            if(OldFilename!=Filename) {
              if(OldFilename.length()>0) {
                FileHandle.close();
              }
              OldFilename=Filename;
              Serial.println("SD.open("+Filename+")");
              FileHandle = fs.open(Filename,FILE_APPEND);
            }
            TowersFound++;
            FileHandle.println(String(gps_date)+" "+String(gps_time)+" "+String(gps_lat)+","+String(gps_lng)+","+String(gps_alt)+","+String(gps_hdp)+","+String(gps_age)+","+String(gps_sat)+","+Line);
            Serial.println("Flush");
            FileHandle.flush();
            Serial.println(String(gps_date)+" "+String(gps_time)+" "+String(gps_lat)+","+String(gps_lng)+","+String(gps_alt)+","+String(gps_hdp)+","+String(gps_age)+","+String(gps_sat)+","+Line);
          }
        }
        if(pos>0){          
          Network[NetworkIndex]=Line.substring(0,pos);
          NetworkIndex++;
          if(NetworkIndex>=3) NetworkIndex=0;          
        }
        
        Serial1.print("AT+CNETSCAN\r\n");
        LastScan = millis();

      }    
      GSMTermOffset=0;
      gsmterm[GSMTermOffset] = '\0';     
      return true;
    }
    break;

    case '\n':
      
    default: {
      if (GSMTermOffset < sizeof(gsmterm) - 2)
        gsmterm[GSMTermOffset++] = c;
        gsmterm[GSMTermOffset] = '\0';
      return true;
      }
    }

  return false;
}

void displayInfo() {
  String Line= "";

  M5.Lcd.clear();
  if (gps.date.isValid() && gps.time.isValid()) {
    if(gps.date.year()>=2023) {

      sprintf(gps_date,"%04d-%02d-%02d",gps.date.year(),gps.date.month(),gps.date.day());  
      sprintf(gps_time,"%02d:%02d:%02d",gps.time.hour(),gps.time.minute(),gps.time.second()); 

      Line=String(gps_date) + "   " + String(gps_time);
      M5.Lcd.drawCentreString(Line,160,0,4);
    }
  } else {
    validfix=false;
  }

  Line="Towers: "+String(TowersFound);
  M5.Lcd.drawRightString(Line,320,30,4);

  if (gps.satellites.isValid()) {    
    sprintf(gps_sat,"%d",gps.satellites.value());
    Line=String(gps_sat)+" Satellites";
    M5.Lcd.drawString(Line,0,30,4);
  }
  if (gps.location.isValid() && gps.altitude.isValid() && gps.hdop.isValid() ) {
    sprintf(gps_lat,"%.7f",gps.location.lat());
    sprintf(gps_lng,"%.7f",gps.location.lng());  
    sprintf(gps_age,"%.1f",gps.location.age());
    sprintf(gps_alt,"%.2f",gps.altitude.meters()); 
    sprintf(gps_hdp,"%.1f",gps.hdop.hdop());
    Line=String(gps_lat)+" x "+String(gps_lng);
    M5.Lcd.drawCentreString(Line,160,60,4);

    Line="Age:"+String(gps_age)+" Alt:"+String(gps_alt)+" Hdop:"+String(gps_hdp);
    M5.Lcd.drawCentreString(Line,160,90,4);
    validfix=true;    
  }
  Line="Filesize: "+String(FileHandle.position());
  M5.Lcd.drawCentreString(Line,160,120,4);
  for(int i=0;i<3;i++) {
    M5.Lcd.drawCentreString(Network[i],160,150+(i*30),4);
  }
}
