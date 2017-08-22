#include <ESP8266WiFi.h>
//#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <FS.h>
#include <WiFiUdp.h>


#define _OTA_
//#define SerialOn

#ifdef _OTA_
#include <ArduinoOTA.h>
#endif


WiFiManager wifiManager;


MDNSResponder mdns;
ESP8266WebServer server(80);

int SERIAL_SPEED = 115200;

int RECV_PIN = 0; //1:tx //an IR detector/demodulator is connected to GPIO  //D1
int SEND_PIN = 3; //3:rx // D2 paired with a 1.6 Ohm resistor

String CONFIG_PATH = "config.json";
String CONFIG_BACKUP_PATH = "config.bak";

File fsUploadFile;

IRrecv irrecv(RECV_PIN);
IRsend irsend(SEND_PIN);

decode_results  results1;        // Somewhere to store the results
decode_results  results;        // Somewhere to store the results

unsigned long StrToUL (String str) {
  char tarray[15];
  str.toCharArray(tarray, sizeof(tarray));
  unsigned long code = strtoul(tarray, NULL, 10);
  return (code);
}

unsigned long combineBytes(int a1, int a2, int a3, int a4) {
  unsigned long code = 0;
  code = code + a1;
  code = (code << 8) + a2;
  code = (code << 8) + a3;
  code = (code << 8) + a4;
  return (code);
}

int flipBits(unsigned char b) {
  return ( (b * 0x0202020202ULL & 0x010884422010ULL) % 1023);
}

void dump(int a1, int a2, int a3) {
  Serial.print(a1, HEX);
  Serial.print("-");
  Serial.print(a2, HEX);
  Serial.print("-");
  Serial.println(a3, HEX);
}

void handleRoot() {
  server.send(200, "text/html", "Please specify command! Form: /ir?code=xxx&bits=xx&protocol=x");
}

void handleIr() {
  String codestring = server.arg("code");
  String protocol = server.arg("protocol");
  String bitsstring = server.arg("bits");

  String deviceCode = server.arg("deviceCode");
  String subDeviceCode = server.arg("subDeviceCode");
  String obc = server.arg("obc");
  String pronto = server.arg("pronto");

  //  String webOutput = "Protocol: "+protocol+"; Code: "+codestring+"; Bits: "+bitsstring + " - ("+deviceCode + subDeviceCode + obc +")";
  String webOutput = "";

  unsigned long code = 0;
  int rc5_control_bit = 0;

  if ((codestring != "") && (bitsstring != "")) {
    //unsigned long code = codestring.toInt();
    char tarray[15];
    codestring.toCharArray(tarray, sizeof(tarray));
    code = strtoul(tarray, NULL, 16);
    // unsigned long code = atol(codestring);
  }
  int bits = bitsstring.toInt();


  if ((obc != "") && (deviceCode != "")) {
    //convert OBC & deviceCode to hex CodeString


    int iDeviceCode = StrToUL(deviceCode);
    int iDeviceCodeLSB = flipBits(iDeviceCode);
    int iDeviceCodeLSB_INV = iDeviceCodeLSB ^ 0xFF;
    dump(iDeviceCode, iDeviceCodeLSB, iDeviceCodeLSB_INV);

    int iSubDeviceCode;
    int iSubDeviceCodeLSB;
    int iSubDeviceCodeLSB_INV;
    if ((subDeviceCode == "")) {
      iSubDeviceCode = iDeviceCode;
      iSubDeviceCodeLSB = iDeviceCodeLSB;
      iSubDeviceCodeLSB_INV = iDeviceCodeLSB_INV;
    } else {
      iSubDeviceCode = StrToUL(subDeviceCode);
      iSubDeviceCodeLSB = flipBits(iSubDeviceCode);
      iSubDeviceCodeLSB_INV = iSubDeviceCodeLSB ^ 0xFF;
    }

    int iOBC = StrToUL(obc);
    int iOBCLSB = flipBits(iOBC);
    int iOBCLSB_INV = iOBCLSB ^ 0xFF;
    dump(iOBC, iOBCLSB, iOBCLSB_INV);

    Serial.println(iDeviceCodeLSB, HEX);
    Serial.println(iOBCLSB, HEX);
    Serial.println("----");
    if (protocol == "Samsung") {
      code = combineBytes(iDeviceCodeLSB, iSubDeviceCodeLSB, iOBCLSB, iOBCLSB_INV);
    } else if (protocol == "NEC" || protocol == "NECx2") {
      code = combineBytes(iDeviceCodeLSB, iDeviceCodeLSB_INV, iOBCLSB, iOBCLSB_INV);
    } else if (protocol == "RC6") {
      /*NOT TESTED*/
      code = combineBytes(0, 0, iDeviceCode, iOBC);
      bits = 20;
    }  else if (protocol == "RC5") {
      /*NOT TESTED*/
      /*control=1,device=5,command=6*/
      rc5_control_bit = abs(rc5_control_bit - 1);
      code = rc5_control_bit;
      code += code << 5 + iDeviceCodeLSB;
      code += code << 6 + iOBCLSB;
      bits = 12;
    } else if (protocol == "JVC") {
      /*NOT TESTED*/
      code = combineBytes(0, 0, iDeviceCodeLSB, iOBCLSB);
      bits = 16;
    }  else if (protocol == "Sony") {
      /*NOT TESTED & highly suspect, need to seem some example codes*/
      code = iOBCLSB;
      if (subDeviceCode != "") {
        bits = 20;
        code = code << 5 + iDeviceCodeLSB;
        code = code << 8 + iSubDeviceCodeLSB;

      } else if (iDeviceCode > 2 ^ 5) {
        bits = 15;
        code = code << 8 + iDeviceCodeLSB;
      } else {
        bits = 12;
        code = code << 5 + iDeviceCodeLSB;
      }
    } else  {
      code = 0;
      server.send(404, "text/html", "Protocol not implemented for OBC!");
    }

    Serial.println("---");
    Serial.println(code, HEX);
    Serial.println("---");

  }

  if (code != 0) {
    Serial.println(code, HEX);

    if (protocol == "NEC") {
      server.send(200, "text/html", webOutput);
      irsend.sendNEC(code, bits);
    }
    else if (protocol == "Sony") {
      server.send(200, "text/html", webOutput);
      irsend.sendSony(code, bits);
    }
    else if (protocol == "Whynter") {
      server.send(200, "text/html", webOutput);
      irsend.sendWhynter(code, bits);
    }
    else if (protocol == "LG") {
      server.send(200, "text/html", webOutput);
      irsend.sendLG(code, bits);
    }
    else if (protocol == "RC5") {
      server.send(200, "text/html", webOutput);
      irsend.sendRC5(code, bits);
    }
    else if (protocol == "RC6") {
      server.send(200, "text/html", webOutput);
      irsend.sendRC6(code, bits);
    }
    else if (protocol == "DISH") {
      server.send(200, "text/html", webOutput);
      irsend.sendDISH(code, bits);
    }
    else if (protocol == "SharpRaw") {
      server.send(200, "text/html", webOutput);
      irsend.sendSharpRaw(code, bits);
    }
    else if (protocol == "Samsung") {
      server.send(200, "text/html", webOutput);
      irsend.sendSAMSUNG(code, bits);
    }
    else {
      server.send(404, "text/html", "Protocol not implemented!");
    }
  }  else if (pronto != "") {
    //pronto code
    //blocks of 4 digits in hex
    //preample is 0000 FREQ LEN1 LEN2
    //followed by ON/OFF durations in FREQ cycles
    //Freq needs a multiplier
    //blocks seperated by %20
    //we are ignoring LEN1 & LEN2 for this use case as not allowing for repeats
    //just pumping all
    int spacing = 5;
    int len = pronto.length();
    int out_len = ((len - 4) / spacing) - 3;
    uint16_t prontoCode[out_len];
    unsigned long timeperiod;
    unsigned long multiplier = .241246 ;

    int pos = 0;
    unsigned long hz;
    if (pronto.substring(pos, 4) != "0000") {
      server.send(404, "text/html", "unknown pronto format!");
      //unknown pronto format
    } else {
      pos += spacing;

      hz = strtol(pronto.substring(pos, pos + 4).c_str(), NULL, 16);
      hz = (hz * .241246);
      hz = 1000000 / hz;
      //XXX TIMING IS OUT
      timeperiod = 1000000 / hz;
      pos += spacing; //hz
      pos += spacing; //LEN1
      pos += spacing; //LEN2
      delay(0);
      for (int i = 0; i < out_len; i++) {
        prontoCode[i] = (strtol(pronto.substring(pos, pos + 4).c_str(), NULL, 16) * timeperiod) + 0.5;
        pos += spacing;
      }
      //sendRaw
      yield();

      irsend.sendRaw(prontoCode, out_len, hz / 1000);
      server.send(200, "text/html", "pronto code!");
    }

  } else {
    server.send(404, "text/html", "Missing code or bits!");
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "404");
}

void handleReset() {
  wifiManager.resetSettings();
}

void handleLoadConfig() {
  File f;
  if (!SPIFFS.exists(CONFIG_PATH)) {
    //doesn't exist create blank config
    f = SPIFFS.open(CONFIG_PATH, "w");
    f.println("{\"pages\":[{\"name\":\"New Page\",\"buttons\":[]}]}");
    Serial.println("CREATED");
    f.close();
  }
  f = SPIFFS.open(CONFIG_PATH, "r");
  String s = f.readString();
  Serial.println(s);
  f.close();
  String callback = server.arg("callback");
  server.send(200, "text/plain", callback + "(" + s + ")");
}

void handleLoadBackupConfig() {
  File f;
  if (!SPIFFS.exists(CONFIG_BACKUP_PATH)) {
    f = SPIFFS.open(CONFIG_BACKUP_PATH, "w");
    f.println("{\"pages\":[{\"name\":\"New Page\",\"buttons\":[]}]}");
    Serial.println("CREATED");
    f.close();
  }
  f = SPIFFS.open(CONFIG_BACKUP_PATH, "r");
  String s = f.readStringUntil('\n');
  Serial.println(s);
  f.close();
  String callback = server.arg("callback");
  server.send(200, "text/plain", callback + "(" + s + ")");

}

void handleSaveConfig() {
  Serial.println("Saving");
  File f;
  File f2;
  if (SPIFFS.exists(CONFIG_PATH)) {
    f = SPIFFS.open(CONFIG_PATH, "r");
    f2 = SPIFFS.open(CONFIG_BACKUP_PATH, "w");
    String s = f.readStringUntil('\n');
    f2.println(s);
    Serial.println("BACKED UP");
    f.close();
    f2.close();
    Serial.println("Config backuped");
  }
  f = SPIFFS.open(CONFIG_PATH, "w");
  String newConfig = server.arg("config");
  f.println(newConfig);
  Serial.println(newConfig);
  f.close();
  String callback = server.arg("callback");
  server.send(200, "text/plain", callback + "('SAVED')");

}

void handleDeleteConfig() {
  File f;
  String callback = server.arg("callback");
  if (!SPIFFS.exists(CONFIG_PATH)) {
    Serial.println("FILE NOT FOUND");
  } else {
    SPIFFS.remove(CONFIG_PATH);
    Serial.println("FILE DELETED");
  }
  server.send(200, "text/plain", callback + "(\"File Deleted\")");
};


void learnHandler() {
  Serial.println("In Learning Handling");
  String callback = server.arg("callback");

  String proto = "";
  { // Grab an IR code
    // dumpInfo(&results);           // Output the results
    switch (results.decode_type) {
      default:
      case UNKNOWN:      proto = ("UNKNOWN");       break ;
      case NEC:          proto = ("NEC");           break ;
      case SONY:         proto = ("Sony");          break ;
      case RC5:          proto = ("RC5");           break ;
      case RC6:          proto = ("RC6");           break ;
      case DISH:         proto = ("DISH");          break ;
      case SHARP:        proto = ("SHARP");         break ;
      case JVC:          proto = ("JVC");           break ;
      case SANYO:        proto = ("Sanyo");         break ;
      case MITSUBISHI:   proto = ("MITSUBISHI");    break ;
      case SAMSUNG:      proto = ("Samsung");       break ;
      case LG:           proto = ("LG");            break ;
      case WHYNTER:      proto = ("Whynter");       break ;
      // case AIWA_RC_T501: Serial.print("AIWA_RC_T501");  break ;
      case PANASONIC:    proto = ("PANASONIC");     break ;
    }
    //results->value
    //Serial.print(results->value, HEX);

    Serial.println("Here");           // Blank line between entries
    irrecv.resume();              // Prepare for the next value
    String output = callback + "({protocol:\"" + proto + "\", value:\"" + String((unsigned long)results.value, HEX) + "\", bits:\"" + String(results.bits) + "\"})";
    Serial.println(output);
    server.send(200, "text/html", output);
  }
}

void handleUploadRequest() {
  Serial.println("in Upload Request");
  server.send(200, "text/html", "");
}

void handleFileUpload() { // upload a new file to the SPIFFS
  //if (server.uri() != "/upload") return;
  Serial.println("in upload");
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);

    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                   // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }

  }
}

void setup(void) {
  irsend.begin();
#ifdef SerialOn
  Serial.begin(SERIAL_SPEED, SERIAL_8N1, SERIAL_TX_ONLY);
#endif
  SPIFFS.begin();

  Serial.println("v5.2");

  WiFi.hostname("ESP8266_IR");
  wifiManager.autoConnect("ESP8266_IR", "1234567890");

#ifdef _OTA_
//OTA

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"irsvr");


  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
#endif

  server.on("/ir", handleIr);
  server.on("/reset", handleReset);
  server.on("/learn", learnHandler);
  server.on("/test", handleRoot);

  server.on("/loadConfig", handleLoadConfig);
  server.on("/loadBackupConfig", handleLoadBackupConfig);

  server.on("/saveConfig", handleSaveConfig);
  server.on("/deleteConfig", handleDeleteConfig);
  server.onFileUpload(handleFileUpload   );
  server.on("/upload", HTTP_POST, handleUploadRequest,    handleFileUpload   );

  server.serveStatic("/", SPIFFS, "/index.html");
  server.serveStatic("/index.html", SPIFFS, "/index.html");
  server.serveStatic("/app.js", SPIFFS, "/app.js");
  server.serveStatic("/style.css", SPIFFS, "/style.css");
  server.serveStatic("/success.html", SPIFFS, "/success.html");
  server.serveStatic("/config.json", SPIFFS, "/config.json");

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
  irrecv.enableIRIn();


  if (mdns.begin("irsvr", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }
}

void loop(void) {
  server.handleClient();
  mdns.update();

#ifdef _OTA_
  ArduinoOTA.handle();
#endif

  if (irrecv.decode(&results1)) {
    if(results1.value==0xffffffff){
      Serial.println("ffffffff recieved ignoring");
    } else {
      String proto = "";
        { // Grab an IR code
          // dumpInfo(&results);           // Output the results
          switch (results1.decode_type) {
            default:
            case UNKNOWN:      proto = ("UNKNOWN");       break ;
            case NEC:          proto = ("NEC");           break ;
            case SONY:         proto = ("Sony");          break ;
            case RC5:          proto = ("RC5");           break ;
            case RC6:          proto = ("RC6");           break ;
            case DISH:         proto = ("DISH");          break ;
            case SHARP:        proto = ("SHARP");         break ;
            case JVC:          proto = ("JVC");           break ;
            case SANYO:        proto = ("Sanyo");         break ;
            case MITSUBISHI:   proto = ("MITSUBISHI");    break ;
            case SAMSUNG:      proto = ("Samsung");       break ;
            case LG:           proto = ("LG");            break ;
            case WHYNTER:      proto = ("Whynter");       break ;
            // case AIWA_RC_T501: Serial.print("AIWA_RC_T501");  break ;
            case PANASONIC:    proto = ("PANASONIC");     break ;
          }
        }
        Serial.print("Signal recveived "+ proto+ " " + results1.bits + " " );
        Serial.println((unsigned long)results1.value, HEX);

      irrecv.decode(&results);
    }
    irrecv.resume();
  }

}
