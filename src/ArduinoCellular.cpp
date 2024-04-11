

#include "ArduinoCellular.h"

unsigned long ArduinoCellular::getTime() {
    int year, month, day, hour, minute, second;
    float tz;
    modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second, &tz);
    return Time(year, month, day, hour, minute, second).getUNIXTimestamp();
}

ArduinoCellular::ArduinoCellular() {
    
}

void ArduinoCellular::begin() {
    // set sim slot
    modem.init();

 
    String modemInfo = this ->sendATCommand("I");
    if(modemInfo.indexOf("EC200A") > 0){
        this->model = ModemModel::EC200;
    } else if (modemInfo.indexOf("EG95") > 0){
        this->model = ModemModel::EG95;
    } else {
        this->model = ModemModel::Unsupported;
    }

    // Set GSM module to text mode
    modem.sendAT("+CMGF=1");
    modem.waitResponse();

    modem.sendAT(GF("+CSCS=\"GSM\""));
    modem.waitResponse();

    // Send intrerupt when SMS has been received
    modem.sendAT("+CNMI=2,1,0,0,0");
    modem.waitResponse();


    ArduinoBearSSL.onGetTime(ArduinoCellular::getTime);

}

bool ArduinoCellular::connect(String apn, String gprsUser, String gprsPass, String pin){
    SimStatus simStatus = getSimStatus();
    if(simStatus == SimStatus::SIM_LOCKED && pin.length() > 0){
       unlockSIM(pin.c_str());
    }

    simStatus = getSimStatus();
    if(simStatus == SimStatus::SIM_READY) {
        if(awaitNetworkRegistration()){
            if(connectToGPRS(apn.c_str(), gprsUser.c_str(), gprsPass.c_str())){
                if(this->debugStream != nullptr){
                    this->debugStream->println("Setting DNS...");
                }
                
                auto response = this->sendATCommand("+QIDNSCFG=1,\"8.8.8.8\",\"8.8.4.4\"");
                
                if(this->debugStream != nullptr){
                    this->debugStream->println(response);
                }                
                return true;
            }
        } else {
            return false;
        }
    }
    
    return false;    
}


Location ArduinoCellular::getGPSLocation(unsigned long timeout){
    if (model == ModemModel::EG95){
        float latitude = 0.00000;
        float longitude = 0.00000;
        unsigned long startTime = millis();

        while((latitude == 0.00000 || longitude == 0.00000) && (millis() - startTime < timeout)) {
            modem.getGPS(&latitude, &longitude);
            delay(1000);
        }
        
        Location loc;
        loc.latitude = latitude;
        loc.longitude = longitude;

        return loc;
    } else {
        if(this->debugStream != nullptr){
            this->debugStream->println("Unsupported modem model");
        }
        return Location();
    }
}

Time ArduinoCellular::getGPSTime(){
    int year, month, day, hour, minute, second;
    modem.getGPSTime(&year, &month, &day, &hour, &minute, &second);
    return Time(year, month, day, hour, minute, second);
}

Time ArduinoCellular::getCellularTime(){
    int year, month, day, hour, minute, second;
    float tz;
    modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second, &tz);
    return Time(year, month, day, hour, minute, second);
}


void ArduinoCellular::sendSMS(String number, String message){
    modem.sendAT(GF("+CMGS=\""), number, GF("\""));
    if (modem.waitResponse(GF(">")) != 1) { }
    modem.stream->print(message);  // Actually send the message
    modem.stream->write(static_cast<char>(0x1A));  // Terminate the message
    modem.stream->flush();
    auto response = modem.waitResponse(10000L);
    
    if(this->debugStream != nullptr){
        this->debugStream->println("Response: " + String(response));
    }
  }


IPAddress ArduinoCellular::getIPAddress(){
    return modem.localIP();
}

int ArduinoCellular::getSignalQuality(){
    return modem.getSignalQuality();
}

TinyGsmClient ArduinoCellular::getNetworkClient(){
    return TinyGsmClient(modem);
}

HttpClient ArduinoCellular::getHTTPClient(const char * server, const int port){
    return HttpClient(* new TinyGsmClient(modem), server, port);
}

HttpClient ArduinoCellular::getHTTPSClient(const char * server, const int port){
    return HttpClient(* new BearSSLClient(* new TinyGsmClient(modem)), server, port);
}

BearSSLClient ArduinoCellular::getSecureNetworkClient(){
    return BearSSLClient(* new TinyGsmClient(modem));
}

bool ArduinoCellular::isConnectedToOperator(){
    return modem.isNetworkConnected();
}

bool ArduinoCellular::connectToGPRS(const char * apn, const char * gprsUser, const char * gprsPass){
    if(this->debugStream != nullptr){
        this->debugStream->println("Connecting to 4G network...");
    }
    while(!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        if(this->debugStream != nullptr){
            this->debugStream->print(".");
        }
        delay(2000);
    }
    return true;
}

bool ArduinoCellular::isConnectedToInternet(){
    return modem.isGprsConnected();
}

SimStatus ArduinoCellular::getSimStatus(){
    int simStatus = modem.getSimStatus();
    if(this->debugStream != nullptr){
        this->debugStream->println("SIM Status: " + String(simStatus));
    }

    if (modem.getSimStatus() == 0) {
        return SimStatus::SIM_ERROR;
    } else if (modem.getSimStatus() == 1) {
        return SimStatus::SIM_READY;
    } else if (modem.getSimStatus() == 2) {
        return SimStatus::SIM_LOCKED;
    } else if (modem.getSimStatus() == 3) {
        return SimStatus::SIM_ANTITHEFT_LOCKED;
    } else {
        return SimStatus::SIM_ERROR;
    }
}

bool ArduinoCellular::unlockSIM(const char * pin){
    if(this->debugStream != nullptr){
        this->debugStream->println("Unlocking SIM...");
    }
    return modem.simUnlock(pin); 
}

bool ArduinoCellular::awaitNetworkRegistration(){
    if(this->debugStream != nullptr){
        this->debugStream->println("Waiting for network registration...");
    }
    while (!modem.waitForNetwork()) {
        if(this->debugStream != nullptr){
            this->debugStream->print(".");
        }
        delay(2000);
    } 
    return true;
}

bool ArduinoCellular::enableGPS(bool assisted){
    if(this->debugStream != nullptr){
        this->debugStream->println("Enabling GPS...");
    }

    if(assisted){
        sendATCommand("AT+QGPSCFG=\"agpsposmode\",33488767");
    } else {
        sendATCommand("AT+QGPSCFG=\"agpsposmode\",8388608");
    }

    //modem.sendAT(GF("+UTIME=1,1"));
    //modem.waitResponse();
    //modem.sendAT(GF("+UGPIOC=23,0,1"));

    //modem.waitResponse();

    return modem.enableGPS();
    //delay(10000);
}

String ArduinoCellular::sendATCommand( char * command, unsigned long timeout){
    String resp;
    modem.sendAT(const_cast<char *>(command)); 
    modem.waitResponse(timeout, resp);
    return resp;
}



Time parseTimestamp(const String &timestampStr) {
  int hour, minute, second, day, month, year, offset;
  
  int commaIndex = timestampStr.indexOf(',');
  String date = timestampStr.substring(0, commaIndex);
  String time = timestampStr.substring(commaIndex + 1, timestampStr.indexOf('+'));
  String offsetStr = timestampStr.substring(timestampStr.indexOf('+') + 1);

  int firstSlashIndex = date.indexOf('/');
  int secondSlashIndex = date.lastIndexOf('/');
  day = date.substring(0, firstSlashIndex).toInt();
  month = date.substring(firstSlashIndex + 1, secondSlashIndex).toInt();
  year = date.substring(secondSlashIndex + 1).toInt() + 2000;

  int firstColonIndex = time.indexOf(':');
  int secondColonIndex = time.lastIndexOf(':');
  hour = time.substring(0, firstColonIndex).toInt();
  minute = time.substring(firstColonIndex + 1, secondColonIndex).toInt();
  second = time.substring(secondColonIndex + 1).toInt();

  offset = offsetStr.toInt();

  return Time(year, month, day, hour, minute, second, offset);
}
// Parses a single SMS entry from the data
SMS parseSMSEntry(const String& entry, const String& message) {
  SMS sms;
  int firstQuoteIndex = entry.indexOf('"');
  int secondQuoteIndex = entry.indexOf('"', firstQuoteIndex + 1);
  int thirdQuoteIndex = entry.indexOf('"', secondQuoteIndex + 1);
  int fourthQuoteIndex = entry.indexOf('"', thirdQuoteIndex + 1);
  int commaIndexBeforeTimestamp = entry.lastIndexOf(',', entry.lastIndexOf(',') - 1);

  String command = "+CMGL: ";
  // Index is between "+CMGL: " and first "," symbol
  sms.index = entry.substring(entry.indexOf(command) + command.length(), entry.indexOf(',')).toInt();

  // Extracting number and raw timestamp
  sms.number = entry.substring(thirdQuoteIndex + 1, fourthQuoteIndex);
  String rawTimestamp = entry.substring(commaIndexBeforeTimestamp + 2, entry.indexOf('+', commaIndexBeforeTimestamp) + 3);

  // Parse the timestamp
  sms.timestamp = parseTimestamp(rawTimestamp);
  
  sms.message = message;
  return sms;
}

// Function to split a string into lines based on a delimiter character
// Filters out empty lines
std::vector<String> splitStringByLines(const String& input, char delimiter = '\n') {
    std::vector<String> lines;
    int startIndex = 0;
    while (startIndex < input.length()) {
        int endIndex = input.indexOf(delimiter, startIndex);
        if (endIndex == -1)
            endIndex = input.length();
        String line = input.substring(startIndex, endIndex);
        // Remove trailing \r if it exists
        if (line.endsWith("\r")) {
            line.remove(line.length() - 1);
        }
        lines.push_back(line);
        startIndex = endIndex + 1;
    }
    return lines;
}

// Splits the entire message string into individual SMS entries and parses them
std::vector<SMS> parseSMSData(const String& data) {
    std::vector<SMS> smsList = std::vector<SMS>();    
    std::vector<String> lines = splitStringByLines(data);

    // Remove first line if it's empty
    if (lines.size() > 0 && lines[0] == "") {
        lines.erase(lines.begin());
    }

    // Remove last 2 lines if second to last line is "OK"
    if (lines.size() >= 2 && lines.back() == "OK") {
        lines.erase(lines.end() - 2, lines.end());
    }
    
    for(int i = 0; i < lines.size(); i ++){
        if (lines[i].startsWith("+CMGL:")) {
            String entry = lines[i];
            String message = "";
            // Loop through the lines until the next +CMGL line and extract the message by concatenating the lines
            for (int j = i + 1; j < lines.size(); j++) {
                if (lines[j].startsWith("+CMGL:")) {
                    i = j - 1;
                    break;
                }
                message += lines[j] + "\n";
            }
            SMS sms = parseSMSEntry(entry, message);
            smsList.push_back(sms);
        }
    }

  return smsList;
}

std::vector<SMS> ArduinoCellular::getReadSMS(){
    String rawMessages = sendATCommand("+CMGL=\"REC READ\"");
    if(rawMessages.indexOf("OK") == -1){
        return std::vector<SMS>();
    } else if (rawMessages.indexOf("ERROR") != -1){
        return std::vector<SMS>();
    } else {
        return parseSMSData(rawMessages);
    }
}

std::vector<SMS> ArduinoCellular::getUnreadSMS(){
    String rawMessages = sendATCommand("+CMGL=\"REC UNREAD\"");
    if(rawMessages.indexOf("OK") == -1){
        return std::vector<SMS>();
    } else if (rawMessages.indexOf("ERROR") != -1){
        return std::vector<SMS>();
    } else {
        return parseSMSData(rawMessages);
    }
}

void ArduinoCellular::setDebugStream(Stream &stream){
    this->debugStream = &stream;
}
