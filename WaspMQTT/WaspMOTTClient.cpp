/*
 *
 * 
 *  WaspMQTTClient.cpp
 *
 *  The WaspMQTTClient library implements the MQTT protocol to work
 *  with the waspmote's 3G/GPRS module. 
 *  Version:		1.1
 *  Modified by Simfony
*/


/*
 *  TODO: Implement unsubscribe function
*/

#include <Waspmote.h>
#include "WaspGPRS_Pro.h"
#include "WaspMQTTClient.h"



void WaspMQTTClient::test() {
  callback("topic", (uint8_t*)"message", 10);
}

/* - - - - - - - CONSTRUCTORS - - - - - - - */

WaspMQTTClient::WaspMQTTClient() {
  this->_client = NULL;
}

WaspMQTTClient::WaspMQTTClient(char* host, char* port, void(*callback)(char*, uint8_t*, unsigned int), Client& client) {
  this->_client = &client;
  this->callback = callback;
  this->host = host;
  this->port = port;
}
//Added by Simfony
WaspMQTTClient::WaspMQTTClient(char* host, char* port, void(*callback)(char*, uint8_t*, unsigned int), Client& client, char* APN, char* APN_USER, char* APN_PASS, char* PIN) {
  this->_client = &client;
  this->callback = callback;
  this->host = host;
  this->port = port;
  this->APN = APN;
  this->APN_USER = APN_USER;
  this->APN_PASS = APN_PASS;
  this->PIN = PIN;
  }

/* * * * * * * * * * * * * * * * * */
/* * *  MQTT CLIENT FUNCTIONS  * * */
/* * * * * * * * * * * * * * * * * */

/* - - - - - - CONNECT FUNCTIONS - - - - - - */

boolean WaspMQTTClient::connect(char *id, char *user, char *pass) {
  boolean answer;
  char idBuf[30];
  char userBuf[30];
  char passBuf[33];
  
  sprintf(idBuf,"%s", id);

  if(user == "") {
    answer = connect(idBuf, NULL, NULL, 0, 0, 0, 0);
  }
  else {
    sprintf(userBuf,"%s", user);
    
    if(pass != "") {
      sprintf(passBuf, "%s", pass);
      answer = connect(idBuf, userBuf, passBuf, 0, 0, 0, 0);
    }
    else {
      answer = connect(idBuf, userBuf, NULL, 0, 0, 0, 0);
    }
  }
  return answer;
}


/* TODO Implementations in connect:
 * - Enable user to set "Clean Session Flag" (default = 1)
 */
boolean WaspMQTTClient::connect(char *id, char *user, char *pass, char *willTopic, uint8_t willQos, uint8_t willRetain, char *willMessage) {
  if(!connected()) {
    int result = 0;
    
    memset(&packetBuffer[0], 0, sizeof(packetBuffer));
    
   // result = _client->connect(this->host, this->port);
	//Added by Simfony
	 result = _client->connect(this->host, this->port, this->APN, this->APN_USER, this->APN_PASS, this->PIN);
	 if (result == 2){
		  return result;
		
	 }
     if(result) {
	  nextMsgId = 1;
      uint8_t d[9] = {0x00, 0x06, 'M', 'Q', 'I', 's', 'd', 'p', MQTT_PROTOCOL_VERSION};  // First 9 Bytes of Variable Header
      
      uint16_t length = 5;
      unsigned int j;
      
      for(j = 0; j < 9; j++) {
        packetBuffer[length++] = d[j];
      }
      
      uint8_t v;    // The 10th Byte of Variable Header
      if(willTopic) {
        v = 0x06 | (willQos << 3) | (willRetain << 5);  // If there is a topic, set "Will QoS" and "Will Retain" flags
      }
      else {
        v = 0x02;                                       // or nothing
      }

      if(user != NULL) {
        v = v | 0x80;                                   // If there is a user name, set the "User Name Flag" to 1
        
        if(pass != NULL & pass != "") {
          v = v | (0x80 >> 1);                          // If there is a password, set the "Password Flag" to 1
        }
      }

      packetBuffer[length++] = v;
      
      packetBuffer[length++] = ((MQTT_KEEPALIVE) >> 8);    // Set "Keep Alive" MSB
      packetBuffer[length++] = ((MQTT_KEEPALIVE) & 0xFF);  // Set "Keep Alive" LSB
      
      length = writeString(id, packetBuffer, length);

      if(willTopic) {
        length = writeString(willTopic, packetBuffer, length);
        length = writeString(willMessage, packetBuffer, length);
      }
      
      if(user != NULL) {
        length = writeString(user, packetBuffer, length);
        
        if(pass != NULL) {
          length = writeString(pass, packetBuffer, length);
        }
      }
      
      result = write(MQTT_CONNECT, packetBuffer, length-5);  // TODO: Must be check it's returns
      if(!result) {
		  USB.println(F("> Result NOK"));
        return false;
      }
      
      lastInActivity = lastOutActivity = millis();
      
      while(!_client->available()) {    // available() not implemented. Returns 1
        unsigned long t = millis();
        if(t - lastInActivity > MQTT_KEEPALIVE * 1000UL) {
          _client->stop();
		    USB.println(F("> Result NOK"));
          return false;
        }
      }
      
      int len = readPacket();
      
      if(len == 4 && packetBuffer[3] == 0) {
        lastInActivity = millis();
        pingOutstanding = false;
		return true;
      }
      else {
        char tmp[20];
        
        USB.println(F("Broker Fault Responce:"));
        
        sprintf(tmp, "Message Length: %d", len);
        USB.println(tmp);
        
        sprintf(tmp,"packetBuffer[0]: %x",packetBuffer[0]);
        USB.println(tmp);
        
        sprintf(tmp,"packetBuffer[1]: %x",packetBuffer[1]);
        USB.println(tmp);

	sprintf(tmp,"packetBuffer[2]: %x",packetBuffer[2]);
        USB.println(tmp);

	sprintf(tmp,"packetBuffer[3]: %x",packetBuffer[3]);
        USB.println(tmp);
          USB.println(F("> Result NOK"));
        return false;
      }
    }
    _client->stop();
  }
    USB.println(F("> Result NOK"));
  return false;
}


/* - - - - - - PUBLISH FUNCTIONS - - - - - - */

boolean WaspMQTTClient::publish(char* topic, char* message) {
  return publish(topic, (uint8_t*)message, strlen(message), false);
}

boolean WaspMQTTClient::publish(char* topic, uint8_t* message, unsigned int msgLength) {
  return publish(topic, message, msgLength, false);
}

boolean WaspMQTTClient::publish(char* domain, char* stuff, char* thing, char* message)
{
  char topic[MQTT_MAX_PACKET_SIZE];
  memset(&topic[0], 0, sizeof(topic));

  sprintf(topic, "%s%s", topic, domain);
  sprintf(topic, "%s%s", topic, "/");
  sprintf(topic, "%s%s", topic, stuff);
  sprintf(topic, "%s%s", topic, "/");
  sprintf(topic, "%s%s%c", topic, thing, '\0');

  USB.println(topic);
  return publish(topic, (uint8_t*)message, strlen(message), false);
}

boolean WaspMQTTClient::publish(char* topic, uint8_t* message, unsigned int msgLength, boolean retained)
{
  if(connected()) {
    uint16_t length = 5;  // Leave room in the buffer for header and variable length field
    uint16_t i;
    
    memset(&packetBuffer[0], 0, sizeof(packetBuffer));
    length = writeString(topic, packetBuffer, length);
    
    
    for(i = 0; i < msgLength; i++) {
      if(length >= MQTT_MAX_PACKET_SIZE) {
        USB.println(F("\n> (Overrun MAX_PACKET_SIZE definition. You can"));
        USB.println(F("> (increase MAX_PACKET_SIZE in WaspMQTTClient"));
        USB.println(F("> (header file, with your own responsibility.)"));
        return false;
      }
      packetBuffer[length++] = message[i];
    }
    uint8_t header = MQTT_PUBLISH;
    
    if(retained) {
      header |= 1;
    }
    return write(header, packetBuffer, length-5);    // CHECK
  }
  USB.println(F("> (There is no connected client found.)"));
  USB.println(F("> (Be sure you connect a client first.)"));
  USB.println(F("> ([Try method: connect(\"device_ID\", \"user\", \"password\");])"));
  return false;
}


/* - - - - - - SUBSCRIBE FUNCTION - - - - - - */

/* TODO Implementations in subscribe:
 * - Implement subscribes of QoS 1 and 2
 */
boolean WaspMQTTClient::subscribe(char* topic)
{
  if(connected())
  {
    uint16_t length = 5;  // Leave room in the buffer for header and variable length field
    memset(&packetBuffer[0], 0, sizeof(packetBuffer));
    
    nextMsgId++;
    if(nextMsgId == 0) {
      nextMsgId = 1;
    }

    packetBuffer[length++] = (nextMsgId >> 8);
    packetBuffer[length++] = (nextMsgId & 0xFF);

    length = writeString(topic, packetBuffer, length);

    if(nextMsgId == 0) {    // 0 is reserved as an invalid Message Id.
      nextMsgId = 1;
    }
    packetBuffer[length++] = 0;    // TODO: Only do QoS 0 subs. Further implementation needed.
 
    return write(MQTT_SUBSCRIBE, packetBuffer, length-5);
  }
  return false;
}


/* - - - - - - READ/WRITE FUNCTIONS - - - - - - */

uint16_t WaspMQTTClient::readPacket() {
  uint16_t answer = _client->read((char*)&packetBuffer);
  return answer;
}


boolean WaspMQTTClient::write(uint8_t header, uint8_t* buf, uint16_t length) {
  uint8_t lenBuf[4];
  uint8_t llen = 0;
  uint8_t digit;
  uint8_t pos = 0;
  uint8_t len = length;
  int8_t answer;

  do {
    digit = len % 128;
    len = len / 128;
    
    if(len > 0) {
      digit |= 0x80;
    }
    
    lenBuf[pos++] = digit;
    llen++;
  }while(len > 0);
  
  buf[4-llen] = header;
  
  for(int i = 0; i < llen; i++) {
    buf[5-llen+i] = lenBuf[i];    
  }

  answer = _client->write(buf + (4-llen), length + 1 + llen);
  
  lastOutActivity = millis();
  
  return answer;
}

uint16_t WaspMQTTClient::writeString(char* string, uint8_t* buf, uint16_t pos) {
  char* idp = string;
  uint16_t i = 0;
  pos += 2;
  
  while(*idp) {
    buf[pos++] = *idp++;
    i++;
  }
  buf[pos-i-2] = (i >> 8);
  buf[pos-i-1] = (i & 0xFF);
  return pos;
}


/* - - - - - - FUNCTIONALITY FUNCTIONS - - - - - - */

boolean WaspMQTTClient::commandCompare(char *cmd, byte *rcvdBuff, unsigned int len) {
  if(strlen(cmd) != len) {
    return false;
  }
  for(unsigned int i = 0; i < len; i++) {
    if(cmd[i] != (char)(rcvdBuff[i])) {
      return false;
    }
  }
  return true;
}


boolean WaspMQTTClient::connected() {
  boolean rc;
  int state;
  
  if(_client == NULL) {
    rc = false;
  }
  else {
    state = _client->connected();
    
    if(state == 1) {
      rc = true;
    }
    else {
      rc = false;
    }

    /*if(!rc) {
      _client->stop();
    } */
  }
  return rc;
}


void WaspMQTTClient::disconnect() {
  packetBuffer[0] = MQTT_DISCONNECT;
  packetBuffer[1] = 0;
  
  _client->write(packetBuffer, 2);
  _client->stop();
  lastInActivity = lastOutActivity = millis();
}


/* - - - - - - - - MQTT LOOP - - - - - - - - - */

boolean WaspMQTTClient::loop()
{
  int answer;
  
  if(connected())
  {
    //USB.println(F(">> Client Connected."));
    unsigned long time = millis();
    
    if((time - lastInActivity > MQTT_KEEPALIVE * 1000UL / 2) || (time - lastOutActivity > MQTT_KEEPALIVE * 1000UL / 2))
    {
      if(pingOutstanding) {
        _client->stop();
        return false;
      }
      else {
        USB.println(F(">> Sending PINGREQ."));
        packetBuffer[0] = MQTT_PINGREQ;
        packetBuffer[1] = 0;
        
        answer = _client->write(packetBuffer, 2);
        if(answer != 1) {
          USB.println(F("> PINGREQ (answer != 1 received)."));
          disconnect();
          // Decomented
		  _client->stop();
          return false;
        }
        lastOutActivity = time;
        lastInActivity = time;
        pingOutstanding = true;
      }
    }
    
    //if(_client->available())    // available() not implemented. Returns 1
	//Simfony
   if (_client->available())
	{
      uint8_t lenLength;
      
      uint16_t len = readPacket();
 
      // TODO: Some kind of modulo must be applied in len variable
      // in order to find out how many bytes describe the length
      if(len < 126)        // TODO: This logic have to be tested.
      {
        lenLength = 1;
      }
      else {
        lenLength = 2;
      }
      
      if(len > 0)
      {
        lastInActivity = time;
        uint8_t type = GPRS_Pro.buffer_GPRS[0] & 0xF0;

        if(type == MQTT_PUBLISH)
        {
          USB.println(F("> PUBLISH message received."));
          if(callback)
          {
            //USB.println(F("CallBack."));
            uint16_t restLength;
            uint16_t topicLength;
            uint16_t msgLength;

            // Assumes that the bigger length description would be up to two bytes.
			// Technically, an MQTT packet for Waspmote 3G module, will never overrun
			// 512bytes (the number "512" can be represented by 2 bytes).
            if(lenLength == 1) {
              restLength = packetBuffer[lenLength+1];
              topicLength = ((packetBuffer[lenLength+2] << 8) | packetBuffer[lenLength+3]) + 2;
            }
            else if(lenLength == 2) {
              restLength = (packetBuffer[lenLength+1] << 8) | packetBuffer[lenLength+2];
              topicLength = ((packetBuffer[lenLength+3] << 8) | packetBuffer[lenLength+4]) + 2;
            }
            else {
              //USB.println(F("Variable \"lenLength\", above the limits."));
              return false;
            }
            
            msgLength = restLength - topicLength;
            
            char topic[topicLength];            // TODO: Make dynamic allocation
            uint8_t message[msgLength];         // TODO: Make dynamic allocation
            
            int msgIndex = 2 + lenLength + topicLength;
            int topicIndex = 2 + lenLength + 2;
			
            for(int i = 0; i < msgLength; i++) {
              message[i] = packetBuffer[msgIndex+i];
            }
            message[msgLength] = '\0';
            
            for(int i = 0; i < topicLength - 2; i++) {
              topic[i] = packetBuffer[topicIndex+i];
            }
            topic[topicLength-2] = '\0';

            packetBuffer[len] = 0;

            callback(topic, message, msgLength);
          }
        }
        else if(type == MQTT_PINGREQ)
        {
          USB.println(F(">> PINGREQ received."));
          packetBuffer[0] = MQTT_PINGRESP;
          packetBuffer[1] = 0;
          _client->write(packetBuffer, 2);
        }
        else if(type == MQTT_PINGRESP)
        {
          USB.println(F(">> PINGRESP received"));
          pingOutstanding = false;
        }
      }
      else {
        USB.println(F("No Data Received"));
      }
    }
    return true;
  }
  return false;
}


/* * * * * * * * * * * * * * * * */
/* * *  3G CLIENT FUNCTIONS  * * */
/* * * * * * * * * * * * * * * * */

/* - - OPEN 3G SOCKET - - */
int8_t Wasp3GMQTTClient::connect(const char *host, const char *port) {
  int8_t answer;
  
  USB.println();
  USB.print(F("> Opening GPRS module... "));
  
  GPRS_Pro.set_APN("internet.simfony.net", "", "");
  answer = GPRS_Pro.ON(); ;
  if ((answer == 1) || (answer == -3))
  {
    USB.println(F("\t\t\tDone."));
    
    USB.print(F("> Connect GPRS module to network... "));
	//answer = GPRS_Pro.setPIN(PIN); // Sets PIN = 1234 to the SIM
	//if (answer == 1) 
   // {
   //     USB.println(F("GPRS_Pro module SIM ok..."));
	//}
	
	//else
	//{
		USB.println(F("PIN code incorrect"));
	//}	
    answer = GPRS_Pro.check(60);    
    if (answer == 1)
    {
      USB.println(F("\t\tDone."));

      USB.print(F("> Setting connection... "));
	  	  
      answer = GPRS_Pro.configureGPRS_TCP_UDP(SINGLE_CONNECTION, NON_TRANSPARENT);;
      if (answer == 1)
      {
        USB.println(F("\t\tDone."));
        
        USB.print(F("> Opening TCP socket... "));

        answer = GPRS_Pro.createSocket(TCP_CLIENT, host, "1883"); 
        if (answer == 1)
        {
          USB.println(F("\t\tDone."));
          USB.println(F(""));
          isConnected = 1;
          return answer;
        }
        else {
          USB.println(F("\t\tFail."));
        }
      }
      else {
        USB.println(F("\t\tFail."));
      }
    }
    else {
      USB.println(F("\t\tFail."));
    }
  }
  else {
    USB.println(F("\t\tFail."));
  }
  return 0;
}

//Added by Simfony to implement APN and PIN
int8_t Wasp3GMQTTClient::connect(const char *host, const char *port, char *APN, char *APN_USER, char *APN_PASS, char *PIN) {
  int8_t answer;
  
  USB.println();
  USB.print(F("> Opening GPRS module... "));
  GPRS_Pro.set_APN(APN, APN_USER, APN_PASS);
  answer = GPRS_Pro.ON(); ;
  if ((answer == 1) || (answer == -3))
  {
    USB.println(F("\t\t\tDone."));
    
    
	//Added by Simfony
	if ((strlen(PIN) > 1) and (pinRetries < 1) )
	{
		USB.print(F("> Setting PIN..."));
		answer = GPRS_Pro.setPIN(PIN); // Sets PIN 
		if (answer == 1) 
		{
			USB.println(F("\t\t SIM ok."));
		}
		else
		{
			USB.println(F("\t\t PIN code incorrect."));
			//Added by Simfony. Return 2 if PIN code is wrong
			pinRetries++;
			//return 0;
		}
    }
	USB.print(F("> Connect GPRS module to network... "));
    answer = GPRS_Pro.check(60);    
    if (answer)
    {
      USB.println(F("\tDone."));

      USB.print(F("> Setting connection... "));
	  	  
      answer = GPRS_Pro.configureGPRS_TCP_UDP(SINGLE_CONNECTION, NON_TRANSPARENT);;
      if (answer)
      {
        USB.println(F("\t\tDone."));
        
        USB.print(F("> Opening TCP socket... "));

        answer = GPRS_Pro.createSocket(TCP_CLIENT, host, "1883"); 
        if (answer)
        {
          USB.println(F("\t\tDone."));
          USB.println(F(""));
          isConnected = 1;
          return answer;
        }
        else {
          USB.println(F("\t\tFail."));
        }
      }
      else {
        USB.println(F("\t\tFail."));
      }
    }
    else {
      USB.println(F("\t\tFail."));
    }
  }
  else {
    USB.println(F("\t\tFail."));
  }
  return 0;
}

/* - - READ FROM 3G - - */
uint16_t Wasp3GMQTTClient::read(char *buf) {
  int answer;
  long unsigned int packetLength;
  int k;
  int point = 0;
  delay(100);
  
  memset(&GPRS_Pro.buffer_GPRS[0], 0, sizeof(GPRS_Pro.buffer_GPRS));
  memset(&buf[0], 0, sizeof(buf));

  //USB.println(F("> Scan Receiver..."));
  
  answer = GPRS_Pro.manageIncomingData();
  
  //USB.println(F("> Stop Scanning."));
  if(answer == 3) {

    point = parse("IPD");
	
	//added by  Simfony: otherwise it failed
	point=1;

    if (point > 0) {

      packetLength = GPRS_Pro.buffer_GPRS[1] + 2;
      
      k = countDigits(packetLength);
      
      int j = 0;
      for(int i = point + k + 1; i < point + k + 2 + packetLength; i++, j++) {
        buf[j] = GPRS_Pro.buffer_GPRS[i];
      }
      buf[j] = '\0';
      
      return packetLength;
    }
    else {
      //USB.println(F("IPD NOT FOUND."));
      USB.println(F("> No IP data found."));
      return 0;
    }
  }  
  else if(answer == 0) {
    //USB.println(F("> Error Occured -or- No Data Received."));
    return 0;
  }
  else {
    USB.println(F("> Data type other than IP data. Ignored."));
    return 0;
  }
}

/* - - WRITE TO 3G - - */
boolean Wasp3GMQTTClient::write(uint8_t *buf, uint16_t size) {
  int8_t answer;

  answer = GPRS_Pro.sendData(buf, size);
  if(answer == 1){
    return true;
  }
  else if(answer == 0) {
    USB.println(F("> Error Occurred on Sending Data."));
    //return false;
  }
  else if((answer == -2) || (answer == -3)) {
    USB.print(F("> Sending Data Fail. Error code: \t"));
    USB.println(answer, DEC);
    USB.print(F("> CME or IP error code: \t\t"));
    USB.println(GPRS_Pro.CME_CMS_code, DEC);
    if(GPRS_Pro.CME_CMS_code == 0) {
      stop();
    }
    //return false;
  }
  else {
    USB.print(F("> Unexpected error with code: \t"));
    USB.println(answer);
    stop();
  }
  return false;
}

/* - - STOP 3G SOCKET - - */
void Wasp3GMQTTClient::stop() {
  int answer;

  USB.print(F("> Stopping Client... "));
  answer = GPRS_Pro.closeSocket();
  if(answer == 1) {
    isConnected = 0;
    USB.println(F("\t\t\tDone."));
  }
  else {
    USB.println(F("\t\t\tFail."));
    SREG = 0;
    PWR.reboot();
  }
}

/* - - CONSTRUCTOR - - */
Wasp3GMQTTClient::Wasp3GMQTTClient(){
  isConnected = 0;
  pinRetries = 0;
  
  //Added by Simfony
  //isConnectedAddress = &isConnected;
}


uint8_t Wasp3GMQTTClient::connected() {
  return isConnected;
}


//Added by Simfony

void Wasp3GMQTTClient::setConnected(uint8_t isConnectedValue){
	
	isConnected = isConnectedValue;
}

/* * * * * * * * * * * * * * * */
/* * *  GENERAL FUNCTIONS  * * */
/* * * * * * * * * * * * * * * */

int parse(const char* data_expected) {
  uint16_t count1 = 0;
  uint16_t count2 = 0;
  int ok;
  //USB.print(F("DATA EXPECTED \t\t"));
  //USB.println(data_expected[count2]);
  
  while(count1 < BUFFER_SIZE){    
    if(GPRS_Pro.buffer_GPRS[count1] == data_expected[count2]) {
      count2 = 0;

      while(data_expected[count2] != '\0') {
        if (GPRS_Pro.buffer_GPRS[count1+count2] == data_expected[count2]) {
          count2++;
          ok = 1;
        }
        else {
          ok = 0;
          break;
        }
      }
      if(ok == 1) {
        return (uint16_t) count1 + count2;
      }
    }
    count1++;
    count2 = 0;
  }
  return -1;
}


int countDigits(uint16_t number) {
  long x = 10;
  int count = 1;
  
  /*
  USB.print(F("Enter countDigits. Number: "));
  USB.println(number, DEC);*/
  
  while(true) {
    if(number < x) {
    return count;
    }
    else {
      x *= 10;
      count++;
    }
  }
}

