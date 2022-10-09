const String projectName = "SIM7600 CELL PHONE";

//REPLACE WITH YOUR NUMBER HERE
const String defaultTelephoneNumber = "17045551212";//<<<<<<<<<<<<REPLACE WITH YOUR NUMBER HERE
//REPLACE WITH YOUR NUMBER HERE

// the number of the LED pin
const int ledPin =  13; 

// send call push button
const int callButtonPin = 3; 
int callButtonState = LOW; // low if not pressed, high if pressed 

// send GPS link text button
const int textButtonPin = 4; 
int textButtonState = LOW; // low if not pressed, high if pressed

long callLastDebounceTime = millis();  // the last time the output pin was toggled
long textLastDebounceTime = millis();  // the last time the output pin was toggled
long lastGPS = millis(); // last time GPS position was requested from SIM7600

const int debounceDelay = 1000;    // the debounce time; increase if the output flickers
const int gpsWaitPeriodSeconds = 30; // wait time between requesting GPS from SIM7600
long requestCount = 0; // how many times GPS was requested from SIM7600

boolean isTextActive = true; // disable only for test or dev
boolean isCallActive = true; // disable only for test or dev
boolean isGPSActive = true; // disable only for test or dev

boolean isGPSObtained = false;
boolean isGPSCurrent = false;
boolean isSendGPSRequested = true; // default true so that phone will send an initial GPS link when first turned on
boolean isGPSSent = false; 

long firstGPSTime = 0; // how many requests before GPS was obtained
long firstGPSCount = 0; // how many milliseconds before GPS was obtained
int bodyMinSizeGPS = 80; // GPS string output from phone is between 80 and 90 characters when valid
long aquiredGPSCount = 0; // count how many times GPS has been valid

String bodyGPS;
String west;
String north;
String decimalMinutes;
String positionDegrees;
String linkGPS;

void setup() {
  // setup pins
  pinMode(ledPin, OUTPUT);
  pinMode(textButtonPin, INPUT);
  pinMode(callButtonPin, INPUT);

  bodyGPS.reserve(26);
  west.reserve(11);
  north.reserve(11);
  linkGPS.reserve(60);
  positionDegrees.reserve(3);
  decimalMinutes.reserve(10);

  // begin serial comms with IDE and SIM7600
  Serial.begin(9600);// serial from arduino ide
  Serial.setTimeout(250);
  
  Serial1.begin(9600);//serial to SIM7600 cellular board
  Serial1.setTimeout(250);

  Serial.print(projectName);
  Serial.println(" started");  
}

void loop() {
  // checks if it is able to send GPS (button pressed and GPS obtained
  if(canSendGPS()){
    isSendGPSRequested = false;
    isGPSSent = true;
    sendTextToDefaultNumber(linkGPS);
  }

  // check if send GPS button pressed
  if(isTextButtonPressed()){
     isSendGPSRequested = true;
     isGPSSent = false;
  }

  // check if call button is pressed
  if(isCallButtonPressed()){
     callDefaultNumber();
  }
  // ------------------------------------------

  // ----------------------------------------
  // read data from arduino ide, FOR TESTING ONLY, NOT REQUIRED 
  // this will allow you to ask the SIM7600 questions via AT commands
  // several examples below 
  // ------------------------------------------
  checkForIDEMessages();// comment out if not needed
  // ------------------------------------------ 

  // ------------------------------------------  
  // read all incoming data from SIM7600 HAT
  // ------------------------------------------ 
  if (Serial1.available() > 0) {
    String messageFromSIM7600 = Serial1.readString();
    //remove incoming end \r\n from the message
    messageFromSIM7600.trim();

    if(isGPSActive){
      // see if the response from the SIM7600 contains the prefix that indicates a GPS response message
      String gpsInfo = getBodyByPrefix("+CGNSSINFO: ", messageFromSIM7600);

      // if the gpsInfo is not blank then there is a message, may not be valid yet
      if(gpsInfo.length() != 0) {
        // a GPS Info message received from SIM7600
        Serial.print("gpsInfo: ");
        Serial.println(gpsInfo);  
        // Serial.print("gpsInfo.length(): ");   
        // Serial.println(gpsInfo.length());  

        // test to see if the message length indicates that the GPS message actually contains valid GPS position
        if(gpsInfo.length() >= bodyMinSizeGPS && gpsInfo.length() < bodyMinSizeGPS + 10){// small buffer for size
          // Get N and W values from the string by looking for a terminating character combo
          north = getPointByTerminatingCharacter(gpsInfo, ",N,");
          west =  getPointByTerminatingCharacter(gpsInfo, ",W,"); 
          
          // has valid data, generate the DDM link for Google Maps
          createGPSLink();
          isGPSCurrent = true;
          aquiredGPSCount++;

          // mark frist time GPS aquired
          if(isGPSObtained == false){
            isGPSObtained = true;
            firstGPSTime = millis();
            firstGPSCount = requestCount;
          }
        }else{
           isGPSCurrent = false;
        }
        // print data if there is any
        printGPSData();
        
      }else{
        // not GPS Info message from SIM7600, so print to see what it was
        Serial.println(messageFromSIM7600);
      }
    }
    //Serial.println(messageFromSIM7600.length());
  }
  // ------------------------------------------ 

  // check to see if it is time to request a GPS position from the cell phone
  checkGPSTimer();
  
  //-------------------------------------------
  // LOOP COMPLETE
}

// test by time period for GPS
void checkGPSTimer() {
  if (isGPSActive && ( millis() - lastGPS >= gpsWaitPeriodSeconds * 1000 )) {
    // Remember when we published
    lastGPS = millis();
    //turn gps off
    Serial.println("GPS update timer");
    Serial.print("millis: ");       
    Serial.print(lastGPS);
    Serial.print(": period: ");
    Serial.print(gpsWaitPeriodSeconds);   
     
    if(isGPSObtained == true){
       Serial.print(": firstGPSTime: ");
       Serial.print(firstGPSTime);  
       Serial.print(": firstGPSCount: ");
       Serial.print(firstGPSCount);   
    }   
        
    Serial.print(": aquiredGPSCount: ");
    Serial.print(aquiredGPSCount);  
    Serial.print("/");
    Serial.print(++requestCount);  
    Serial.println(" requests");
    
    requestGPSData();
  }
}

// check to see if SIM7600 response message starts with a specific prefix
int checkPrefix(String message, String fullString){
  // +CGNSSINFO:
  int index = fullString.indexOf(message);
  if (index == -1) return index; // full string doesnt contain message
  return index + message.length(); 
}

// get the SIM7600 response message after a specified prefix, if that prefix exists
String getBodyByPrefix(String messagePrefix, String fullString){
  int start = checkPrefix(messagePrefix, fullString);

  if (start == -1)return "";
  
  return getMessageBody(start, fullString);
}

// get the body after the identified prefix end position
String getMessageBody(int start, String fullString){
  return fullString.substring(start, fullString.length() - 5); //remove OK and newline characters
}

// find the string point characters by looking for the identifying character right after them (known message structure)
String getPointByTerminatingCharacter(String body, String terminatingCharacters){
  int index = body.indexOf(terminatingCharacters);

  if (index == -1)return "";
  
  return body.substring(index - 11, index);
}

// convert a string GPS point into a DMM GPS string that can be used by Google Maps
String convertToGPSDMMfromPoint(String point){
  // Degrees and decimal minutes (DMM): 41 24.2028, 2 10.4418
  // https://support.google.com/maps/answer/18539?hl=en&co=GENIE.Platform%3DDesktop
  // INPUT:   08052.284623 (GPS Point from cell phone)
  // OUTPUT:  80%2052.284623 (needs to be formatted as DDM for Google Maps)
  int dotIndex = point.indexOf(".");

  positionDegrees = point.substring(0, 2);
  
  decimalMinutes = point.substring(dotIndex - 2, point.length());

  return positionDegrees + "%20" + decimalMinutes;
}

// create a Google Map string by converting the cell phone format Degrees and decimal minutes to 
// the Google Map required DMM format to be used in a link sent via SMS
void createGPSLink(){
  // example:  https://www.google.com/maps/place/41%2024.2028,2%2010.4418
  linkGPS = "https://www.google.com/maps/place/" + convertToGPSDMMfromPoint(north) + ",-" + convertToGPSDMMfromPoint(west);
  // NOTE: my W Degrees are negative 80 (-80) in the link above, if you live elsewhere check your link conversion
}

// determine if a GPS link should be sent
bool canSendGPS() {
  // texting is turned on (config)
  // GPS is turned on (config)
  // has a GPS signal
  // has requested GPS be sent (button)
  // have not sent the GPS link since it was requested
  return isTextActive && isGPSActive && isGPSObtained && isSendGPSRequested && !isGPSSent;
}

bool isTextButtonPressed(){
  long current = millis();    

  if ((current - textLastDebounceTime) > debounceDelay && digitalRead(textButtonPin) == HIGH) {
      Serial.print("text buttonState:");
      Serial.println(digitalRead(textButtonPin) == HIGH);      
     textLastDebounceTime = current + 1000; //set the current time     
     return true;
  }

  return false;
}

bool isCallButtonPressed(){
  long current = millis();
  //(current - callLastDebounceTime) > debounceDelay && 
  if ((current - callLastDebounceTime) > debounceDelay && digitalRead(callButtonPin) == HIGH) {
     Serial.print("call buttonState:");
     Serial.println(digitalRead(callButtonPin) == HIGH);     
     callLastDebounceTime = current + 1000; //set the current time
     return true;
  }

  return false;
}

void callDefaultNumber() {
  Serial.print("MAKE CALL VIA BUTTON TO: ");
  Serial.println(defaultTelephoneNumber);
  // Serial.print(millis());  
  // Serial.print(": callLastDebounceTime: ");  
  // Serial.println(callLastDebounceTime);  
  
  if(isCallActive == true) {
    Serial1.print("ATD+");
    Serial1.print(defaultTelephoneNumber);
    Serial1.println(";");
  }
}

void sendTextToDefaultNumber(String message) {
  Serial.print("SEND TEXT VIA BUTTON TO: ");
  Serial.println(defaultTelephoneNumber);
  // Serial.print(millis());  
  // Serial.print(": textLastDebounceTime: ");  
  // Serial.println(textLastDebounceTime);  

  if(isTextActive == true){
    Serial1.print("AT+CMGS=\"");// must have quotes on number
    Serial1.print(defaultTelephoneNumber);
    Serial1.println("\"");// must have quotes on number
    
    //wait for > response from SIM7600
    delay(200);
    
    // send message to SIM7600
    Serial1.print(message);
    Serial1.print(" @");
    Serial1.print(millis());
    
    // log text part of sent message
    Serial.print("sent: ");
    Serial.println(message);
    
    // close message sending process with control-z character
    Serial1.write(0x1A);  // sends ctrl+z end of message
  }
}

// format GPS link data for serial debugging display (NOT REQUIRED FOR OPERATION)
void printGPSData() {
  if ( aquiredGPSCount > 0){
    if(!isGPSCurrent) Serial.print("LAST ");
   
    Serial.print("north: ");
    Serial.print(north);
    Serial.print(" west: ");
    Serial.println(west);
    Serial.print("link: ");
    Serial.println(linkGPS);
  }
}

// for test communication via arduino serial, use if you plan on controlling/communicating with phone via 
// the arduino IDE or other serial communication tools (FOR TESTING OR DEV NOT REQUIRED FOR OPERATION)
void checkForIDEMessages() {
  // ------------------------------------------
  if (Serial.available() > 0)
  {
    String incoming = Serial.readString();
    incoming.trim();

    if(incoming == "c") {
        ///callDefaultNumber();
        Serial.println("CALL NUMBER - disabled");
    }else if(incoming == "t") {
        Serial.println("SEND TEXT - disabled");
    }else if(incoming == "g") {
        Serial.println("GET GPS");
        requestGPSData();
    }else if(incoming == "x") {
        Serial.println("GET Connection");
        requestCurrentConnection();
    }else if(incoming == "d") {
        Serial.println("GET Device Capabilites");
        requestDeviceCapabilities();
    }else if(incoming == "p") {
        Serial.println("GET Product Info");
        requestProductInfo();
    }else if(incoming == "#") {
        Serial.println("GET Phone Number");
        requestPhoneNumber();
    }else{
        Serial.print("incoming:");
        Serial.print(incoming);      
        Serial.print("/");   
        Serial.println(incoming.length());
    }
    // end if    
  }
  // ------------------------------------------ 
}

// SIM7600 AT COMMANDS
// ------------------------------------------------------------
void requestGPSData() {
  Serial.print("requestGPSData: ");
  Serial.println(millis());  
  Serial1.println("AT+CGNSSINFO");
  // Serial.println("COMPLETED");
}

void requestCurrentConnection() {
  Serial.print("requestCurrentConnection: ");
  Serial.println(millis());  
  Serial1.println("AT+COPS?");
  // Serial.println("COMPLETED");
}

void requestDeviceCapabilities() {
  Serial.print("requestDeviceCapabilities: ");
  Serial.println(millis());  
  Serial1.println("AT+GCAP");
  // Serial.println("COMPLETED");
}

void requestProductInfo() {
  Serial.print("requestProductInfo: ");
  Serial.println(millis());  
  Serial1.println("ATI");
  // Serial.println("COMPLETED");
}

void requestPhoneNumber() {
  Serial.print("requestPhoneNumber: ");
  Serial.println(millis());  
  Serial1.println("AT+CNUM");
  // Serial.println("COMPLETED");
}

