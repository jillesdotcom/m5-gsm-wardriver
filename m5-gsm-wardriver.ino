/**********************************************************
 * Project: M5 Stack GSM Wardriver Rev 1.00               *
 * Purpose: Collect Celltower locations for wigle.net     *
 * Author : Jilles Groenendijk                            *
 * Date   : 2023-04-22                                    *
 * Note   : Special thanks to Joseph Hewitt for helping   *
 *          with the code. Get a https://www.wardriver.uk *
 *                                                        *
 * Revisions:                                             *
 * 0.99 (2023-04-16) Initial Release                      *
 * 1.00 (2023-04-22) Complete rewrite                     *
 **********************************************************/ 

#include <M5Stack.h>
#include <TinyGPS++.h> // http://arduiniana.org/libraries/tinygpsplus/

const unsigned long RESPONSE_TIMEOUT = 20000;
TinyGPSPlus gps;
char current_filename[32] = "";
int lastdate = 0;
String celltowers = "";
unsigned long unique_celltowers = 0;
unsigned long filesize = 0;
File fh;  

String sendATCommand(String command) {
  unsigned long startTime = millis();
  String response = "";

  Serial1.println(command);

  while (millis() - startTime < RESPONSE_TIMEOUT) {
    while (Serial2.available() > 0) gps.encode(Serial2.read());    
    if (Serial1.available()) {
      char c = Serial1.read();
      response += c;
      if (response.endsWith("\r\nOK\r\n")) {
        break;
      } else if (response.endsWith("\r\nERROR\r\n")) {
        break;
      }
    }
  }
  return response;
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    String name = "";
    String fullname = "";
    char directory[256];
    int lastdate = 0;

    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            if (levels) {
              if(dirname=="/") {
                sprintf(directory,"%s%s",dirname,file.name());
              } else {
                sprintf(directory,"%s/%s",dirname,file.name());
              }
              // SD.rmdir(directory);
              listDir(fs, directory, levels - 1);
            }
        } else {
            fullname=String(dirname)+"/"+String(file.name());
            // SD.remove(fullname);
            Serial.print("  FILE: ");Serial.print(fullname);
            Serial.print("  SIZE: ");Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void openfile() {
  String filename = "";
  if(String(current_filename).length()!=0) {
    SD.mkdir(String(current_filename).substring(0,5));
    SD.mkdir(String(current_filename).substring(0,12));
    filename=String(current_filename);
    fh = SD.open(filename,FILE_APPEND);      
    Serial.println(filename);
  }
}

void setup() {
  String response = "";
  M5.begin();
  M5.Power.begin();  
  M5.Lcd.setBrightness(5);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  //M5.Lcd.setTextSize(1); 
  Serial.begin(115200); 
  Serial1.begin(115200, SERIAL_8N1, 5, 0);  // COM.GSM
  Serial2.begin(9600,   SERIAL_8N1,15,13);  // COM.GPS
  pinMode(2, OUTPUT);
  digitalWrite(2, 0);
  delay(2000);
  digitalWrite(2, 1);
  delay(2000);
  
  // Factory reset
  while(!response.endsWith("\r\nOK\r\n")) {
    response = sendATCommand("AT&F\r\n");
    // Serial.println(response); 
  }
  response="";

  // Set response
  while(!response.endsWith("\r\nOK\r\n")) {
    response = sendATCommand("ATE1V1I1\r\n");
    // Serial.println(response);
  }
  response="";

  // Configure extended CNETSCAN
  while(!response.endsWith("\r\nOK\r\n")) {
    response = sendATCommand("AT+CNETSCAN=1\r\n");
    // Serial.println(response); 
  }  
  pinMode(10, OUTPUT);
  if ( !SD.begin(4) ) { 
    Serial.println("Card failed, or not present");
  } else {
    listDir(SD, "/", 5);
  }
}

void loop() {
  // Parse data
  String response         = "";
  String networks         = "";
  String network          = "";
  String searchfor        = "";
  int    startline        = 0;
  int    startpos         = 0;
  int    endpos           = 0;

  // GPS data
  char   gps_timedate[20] = "";
  char   gps_loc[50]      = "";
  char   gps_sat[25]      = "";
  float  accuracy         = 0;
  long   alt              = 0;
  int    gsm_year         = 0;
  int    gsm_month        = 0;
  int    gsm_day          = 0;
  int    curdate          = 0;
  bool   validfix         = false;  

  // netscan data
  String network_operator = "";
  String mcc              = "";
  String mnc              = "";
  String rxlev            = "";
  String cellid           = "";
  String arfcn            = "";
  String lac              = "";
  String bsic             = "";

  // Wigle.net data
  int int_rxlev           = 0;
  int int_cellid          = 0;
  int int_arfcn           = 0;
  int int_lac             = 0;
  String mccmnc           = "";
  String wigle_cell_key   = "";
  String wigle            = "";

  // timestamp in UTC  
  if (gps.date.isValid() && gps.time.isValid()) {
    if(gps.date.year()>=2023) {
      gsm_year  = (int)gps.date.year();
      gsm_month = (int)gps.date.month();
      gsm_day   = (int)gps.date.day();
      curdate   = (gsm_year*10000)+(gsm_month*100)+(gsm_day);

      sprintf(gps_timedate,"%04d-%02d-%02d %02d:%02d:%02d",gsm_year,gsm_month,gsm_day,gps.time.hour(),gps.time.minute(),gps.time.second());
      sprintf(current_filename,"/%04d/%04d%02d/%04d%02d%02d.csv",gsm_year,gsm_year,gsm_month,gsm_year,gsm_month,gsm_day);

      if(lastdate != curdate) {
        if( lastdate != 0 ) {
          fh.close();
        }
        openfile();
        fh.println("WigleWifi-1.4,appRelease=1.0.0,model=M5Stack GSM Wardriver Rev1 ESP32,release=1.0.0,device=M5Stack GSM Wardriver Rev1 ESP32,display=i2c LCD,board=M5Stack ESP32,brand=JHewitt");
        fh.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
        Serial.println("WigleWifi-1.4,appRelease=1.0.0,model=M5Stack GSM Wardriver Rev1 ESP32,release=1.0.0,device=M5Stack GSM Wardriver Rev1 ESP32,display=i2c LCD,board=M5Stack ESP32,brand=JHewitt");
        Serial.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");        
        lastdate=curdate;
      }
    }
  }

  if (gps.location.isValid() && gps.altitude.isValid() && gps.hdop.isValid() && (gps.hdop.hdop() <= 250) ) {
    alt = gps.altitude.meters();
    accuracy = (float)gps.hdop.hdop() * 2.5;
    
    sprintf(gps_loc,"%.7f,%.7f,%.2f,%.2f",gps.location.lat(),gps.location.lng(),alt,accuracy); 
    validfix=true;
  }

  // Retrieve GPS info
  if (gps.satellites.isValid()) {
    if(validfix) {
      sprintf(gps_sat,"%d Satellites (FIX)",gps.satellites.value());
    }else {
      sprintf(gps_sat,"%d Satellites (NO FIX)",gps.satellites.value());
    }
  }  else {
    sprintf(gps_sat,"0 Satellites");    
  }

  // Run NETSCAN
  response = sendATCommand("AT+CNETSCAN\r\n");

  // Does the line contain operator
  if (response.indexOf("Operator:") != -1) {
    if (response.indexOf(",Lac:")==-1) {
      // Not extended, run configuration again
      response = sendATCommand("AT+CNETSCAN=1\r\n");
      // Serial.println(response); 
    } else {
      // Serial.println("response:"+response); 
      if (response.indexOf("AT+CNETSCAN") != -1) {
        // found the response line
        int start = response.indexOf("\r\n") + 2;
        int end = response.lastIndexOf("\r\n");
        networks = response.substring(start, end);
        // Serial.println("networks:"+networks); 

        start=0;
        while(networks.indexOf("Operator",start)!= -1 ) {

          end = networks.indexOf("\r\n",start)+1;
          network=networks.substring(start,end);
          // Serial.println("network:"+network); 

          // parse the network information
          searchfor="Operator:\"";
          startpos=network.indexOf(searchfor)+searchfor.length();
          if (startpos == -1) break;
          endpos=network.indexOf("\"",startpos);
          if (endpos == -1) break;
          network_operator = network.substring(startpos,endpos);

          searchfor="MCC:";
          startpos=network.indexOf(searchfor,endpos)+searchfor.length();
          if (startpos == -1) break;
          endpos=network.indexOf(",",startpos);
          if (endpos == -1) break;
          mcc = network.substring(startpos,endpos);;

          searchfor="MNC:";
          startpos=network.indexOf(searchfor,endpos)+searchfor.length();
          if (startpos == -1) break;
          endpos=network.indexOf(",",startpos);
          if (endpos == -1) break;
          mnc = network.substring(startpos,endpos);

          searchfor="Rxlev:";
          startpos=network.indexOf(searchfor,endpos)+searchfor.length();
          if (startpos == -1) break;
          endpos=network.indexOf(",",startpos);
          if (endpos == -1) break;
          String rxlev = network.substring(startpos,endpos);

          searchfor="Cellid:";
          startpos=network.indexOf(searchfor,endpos)+searchfor.length();
          if (startpos == -1) break;
          endpos=network.indexOf(",",startpos);
          if (endpos == -1) break;
          cellid = network.substring(startpos,endpos);

          searchfor="Arfcn:";
          startpos=network.indexOf(searchfor,endpos)+searchfor.length();
          if (startpos == -1) break;
          endpos=network.indexOf(",",startpos);
          if (endpos == -1) break;
          arfcn = network.substring(startpos,endpos);

          searchfor="Lac:";
          startpos=network.indexOf(searchfor,endpos)+searchfor.length();
          if (startpos == -1) break;
          endpos=network.indexOf(",",startpos);
          if (endpos == -1) break;
          lac = network.substring(startpos,endpos);

          searchfor="Bsic:";
          startpos=network.indexOf(searchfor,endpos)+searchfor.length();
          if (startpos == -1) break;
          endpos=network.length();
          if (endpos == -1) break;
          bsic = network.substring(startpos,endpos);

          // Wiggle customalisation
          int_rxlev      = rxlev.toInt() - 80;
          int_cellid     = strtol(cellid.c_str(), 0, 16);
          int_arfcn      = arfcn.toInt();
          int_lac        = strtol(lac.c_str(), 0, 16);
          mccmnc         = mcc + mnc ;
          wigle_cell_key = mccmnc.substring(0,7) + "_" + String(int_lac) + "_"+ String(int_cellid);
          
          /*
          // print the network information
          Serial.print("Network Operator Name..........: "); Serial.println(network_operator);
          Serial.print("Mobile Country Code............: "); Serial.println(mcc);
          Serial.print("Mobile Network Code............: "); Serial.println(mnc);
          Serial.print("Rx Level.......................: "); Serial.println(rxlev);
          Serial.print("Cell ID........................: "); Serial.println(cellid);
          Serial.print("Abs Radio Frequency Channel No.: "); Serial.println(arfcn);
          Serial.print("Location Area Code.............: "); Serial.println(lac);
          Serial.print("Base Station Identity Code.....: "); Serial.println(bsic);
          Serial.println();
          */
          if(validfix) {
            if(celltowers.indexOf(wigle_cell_key+";") == -1) {
              wigle = wigle_cell_key + "," + network_operator + ",GSM;" + mccmnc + "," + gps_timedate + "," + String(int_arfcn) + "," + String(int_rxlev) + "," + gps_loc + ",GSM";
              Serial.println(wigle);
              fh.println(wigle);
              fh.flush();
              celltowers.concat(wigle_cell_key+";");
              unique_celltowers++;
              filesize=fh.size();
            }
          }
          start=end+1;
        }
      }
    }
  // loop done  
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_WHITE,TFT_BLUE);
  M5.Lcd.fillRect(0, 0, 320, 26, TFT_BLUE);
  M5.Lcd.drawCentreString(gps_timedate,160,1,4);

  M5.Lcd.fillRect(0, 240-26, 320, 26, TFT_BLUE);
  M5.Lcd.drawCentreString("M5Stack GSM Wardriver R1",160,240-25,4);

  M5.Lcd.setTextColor(TFT_BLUE,TFT_BLACK);
  M5.Lcd.drawCentreString(gps_sat,160,40,4);
  M5.Lcd.drawCentreString("Unique Celltowers",160,180,4);

  M5.Lcd.setTextColor(TFT_WHITE,TFT_BLACK);
  M5.Lcd.setTextSize(3);
  M5.Lcd.drawCentreString(String(unique_celltowers),160,100,4);
  // Serial.println(response); 
  }
}
