#include <WaspGPRS_SIM928A.h>
#include <WaspFrame.h>
#include <WaspMQTTClient.h>
#include <WaspMQTTUtils.h>
#include <Client.h>
#include <WaspGPRS_Pro.h>
#include <WaspSensorGas_Pro.h>

/*
	This program is an example code to be used with the SIMFONY Development Kit 
	available on the Libelium Market Place
    
	The program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 2.1 of the License, or
    (at your option) any later version.
   
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

	The code below is build for an Smart Environment Pro GPRS P&S equiped with
	an CO2 calibrated sensor and a Temperature, Humidity and Pressure sensor.
	You can change the sensor reading part to adapt it to any GPRS/3G P&S. 
	Running the code will make the P&S connect to the Simfony IoT cloud and periodically
	send the sensor readings using an MQTT connection.

*/

//GPRS settings
char apn[] = "internet.simfony.net"; //Depending on the type of SIM you have ordered
char login[] = "";  //Use only if APN security is enabled. If so use the same values as MQTT_USERNAME/ see device credentials
char password[] = ""; //Use only if APN security is enabled. If so use the same values as MQTT_PASSWORD/ see device credentials
char pin[]="";

//MQTT settings
char MQTT_USERNAME[] = "<USERNAME>";   // Used to authenticate the device. Check the device credentials in the cloud platform
char MQTT_PASSWORD[] = "<PASSWORD>";   // Used to authenticate the device. Check the device credentials in the cloud platform
char MQTT_DEVICE_ID[] = "<Device ID>";  // Used as client_id, if we suppose that one device represents one client. Check the device profile in the cloud platform
char MQTT_TOPIC[] = "<your dedicated topic>/<your choice of a subtopic>";  //The topic on which data is sent. The main topic is the customer ID and must correspond with 
																			//the value in the cloud account. The subtopic is your choice but make sure it has a corresponding
																			//IoT app listening
char MQTT_TOPIC_CONTROL[] = "<your dedicated topic>/<your choice of a subtopic>";							

char MQTT_HOST[] = "52.28.75.56";  //Simfony cloud address. Do not modify
char MQTT_PORT[] = "1883";          //Simfony cloud port. Do not modify

//GPS settings
char GPS_data[250];
char latitude[15], longitude[15], altitude[15], speedOG[15], courseOG[15], time[11], date[11];
int GPS_FIX_status = 0;
int GPS_status = 0;
//Miscellaneous
int battery;
int answer;
MQTTUtils j;
MQTTUtils j2;
WaspMQTTClient c;
Wasp3GMQTTClient client;

// Sensor settings. Add your sensors as nedeed
Gas CO2(SOCKET_B);
char humidity[15], pressure[15], temperature[15], co2[15];


void cmdCallBack(char *topic, uint8_t* message, unsigned int len);
void getSpeed();
void getGPSData(uint8_t silent);
void getSensorData();


void setup()
{   
	USB.ON();

    USB.println(F("************SETUP**************"));
    // Sets operator parameters
    GPRS_Pro.set_APN(apn, login, password);
	// And shows them
    GPRS_Pro.show_APN();    	
	boolean ans = false;
	//Getting Battery
	USB.print(F("Battery level: "));
	USB.print(PWR.getBatteryLevel(),DEC);
	int battery=(int)PWR.getBatteryLevel();
	
	//creating MQTT client
	client = Wasp3GMQTTClient();
    c = WaspMQTTClient(MQTT_HOST, MQTT_PORT, cmdCallBack, client,apn ,login, password, pin );
   	answer = 0;
    //TODO implement stop counter	
    while(answer != 1) {
        answer = c.connect(MQTT_DEVICE_ID, MQTT_USERNAME, MQTT_PASSWORD);
	   if(answer != 1)
       {
         USB.println(F("> Unable to connect to network."));
       }
	   if(answer == 2)
       {
         USB.println(F("> PIN ERROR...OPERATION CANCELED"));
       }
    }
  
	//Starting GPS
	 GPS_status = GPRS_SIM928A.GPS_ON();
        if (GPS_status == 1)
        { 
            USB.println(F("> GPS started"));
        }
        else
        {
            USB.println(F("> GPS NOT started"));   
        }
	//Waiting for GPS
    int countGPS = 16;	
	while((GPS_FIX_status == 0) and (GPS_status == 1)){
		USB.println(F("> Waiting for GPS signal"));
		GPS_FIX_status = GPRS_SIM928A.waitForGPSSignal(15);	
        if ( countGPS % 15 == 0 )
		{
			sendControl("gpsFail",0);
		}
		//Make sure we don't loose the MQTT connection
		if(!c.loop()) {
			USB.print(F("MQTT loop failure."));
		}
 		countGPS++;
	}
	USB.println(F("> GPS signal found"));
	
	//Starting sensors
	CO2.ON();
	// CO2 gas sensor needs a warm up time at least 60 seconds	
    // To reduce the battery consumption, use deepSleep instead delay
    // After 1 minute, Waspmote wakes up thanks to the RTC Alarm
	// Make sure the MQTT connection does not timeout while you wait (default 600 seconds)
	USB.println(F("Waiting for sensor warm-up..."));
    //PWR.deepSleep("00:00:01:10", RTC_OFFSET, RTC_ALM1_MODE1, ALL_ON);
	delay (70000);
	USB.println(F("\t\t\t\t\tDone."));
	USB.println(F("**********End SETUP**************"));
	USB.println(F(""));
    USB.println(F("**********Starting DATA acquisition **************"));
}
	

void loop() {

   memset(GPS_data, '\0', sizeof(GPS_data));
   //Making sure MQTT will stay on. The c.loop() function will send the keepalive it the timer is up.
   if(!c.loop()) {
   
		USB.print(F("MQTT loop failure."));
	}
   
   USB.println(F(">>>> Starting a new read cycle"));
   USB.println(F("> Getting sensor data..."));
   getSensorData();
   getGPSData(1);
   answer = sendData();
    if (answer){
	
    //treat error if needed
	}
	else{
	
    //treat error if needed
 	}
 
	//Getting Battery level and send a warning if low
	battery=(int)PWR.getBatteryLevel();
	if ( battery < 15 )
		{
		USB.print(F("> Low batter..."));
		USB.print(battery);
	    USB.print(F(" %"));
		sendControl("batteryWarning",battery);
	}

	//Make sure we don't loose the MQTT connection
	if(!c.loop()) {
		USB.print(F("MQTT loop failure."));
	}
	
    //Going to sleep until next sample
	// Don't exceed MQTT timeout period (default 600 seconds)
	USB.println(F(">>>>   Going to sleep for the next sample"));
	delay (30000);
	//PWR.deepSleep("00:00:05:00", RTC_OFFSET, RTC_ALM1_MODE1, ALL_ON);
	
	//clear memory  
	//USB.print(F("Free Memory:"));
	USB.println(freeMemory());	
}

//Gets all the GPS data
void getGPSData(uint8_t silent){
	
	 if (GPS_status == 1)
    {
        GPS_FIX_status = GPRS_SIM928A.waitForGPSSignal(15);
        if (GPS_FIX_status == 1)
        {
            answer = GPRS_SIM928A.getGPSData(1);
            if (answer == 1)
            {           
		         //put data in global vars
                dtostrf(GPRS_SIM928A.latitude, 8 , 5, latitude);
				dtostrf(GPRS_SIM928A.longitude, 8 , 5, longitude);
				dtostrf(GPRS_SIM928A.altitude, 8 , 5, altitude);
				dtostrf(GPRS_SIM928A.speedOG, 3 , 2, speedOG);
				dtostrf(GPRS_SIM928A.courseOG, 3 , 2, courseOG);
				
				// show data when enabled so
				if ( silent != 1){
					USB.print(F("\t\t Date (ddmmyy):"));
					USB.println(GPRS_SIM928A.date);
					USB.print(F("\t\t time (UTC_time):"));
					USB.println(GPRS_SIM928A.UTC_time);
					USB.print(F("\t\t Latitude (degrees):"));
					USB.println(GPRS_SIM928A.latitude);
					USB.print(F("\t\t Longitude (degrees):"));
					USB.println(GPRS_SIM928A.longitude);
					USB.print(F("\t\t Speed over the ground"));
					USB.println(GPRS_SIM928A.speedOG);
					USB.print(F("\t\t Course over the ground:"));
					USB.println(GPRS_SIM928A.courseOG);
					USB.print(F("\t\t altitude (m):"));
					USB.println(GPRS_SIM928A.altitude);
				}
								
            }
            else
            { 
                GPS_FIX_status = 0;
                // 6b. add not GPS data string
                USB.println(F("> GPS data not available"));  
            }
        }
        else
        {
            GPS_FIX_status = 0;
            // 6c. add not GPS fixed string
            USB.println(F("> GPS not fixed")); 		
        }
    }
    else
    {
        GPS_FIX_status = 0;
        // 6d. add not GPS started string
        USB.println(F("> GPS not started. Restarting"));
        GPS_status = GPRS_SIM928A.GPS_ON();
    }
}


void getSensorData(){
   
   //Reads the data from the sensors. 
   //Add your own code here depending on the sensor you read.
   
  float measurement_humidity = CO2.getHumidity();
  float measurement_pressure = CO2.getPressure();
  float measurement_temperature = CO2.getTemp();
  float measurement_co2=CO2.getConc();
  dtostrf(measurement_temperature, 6 , 2, temperature);
  dtostrf(measurement_pressure, 8 , 2, pressure);
  dtostrf(measurement_humidity, 6 , 2, humidity);
  dtostrf(measurement_co2, 6 , 2, co2);
  
  USB.println(F("\t\t BME280 Measure performed"));
  USB.print(F("\t\t Humidity: "));
  USB.print(measurement_humidity);
  USB.println(F(" %"));
  USB.print(F("\t\t Pressure: "));
  USB.print(measurement_pressure);
  USB.println(F(" Pascal"));
  USB.print(F("\t\t Temperature: "));
  USB.print(measurement_temperature);
  USB.println(F("  Degrees Celsius"));
  USB.print(F("\t\t CO2 contentration: "));
  USB.print(measurement_co2);
  USB.println(F("  Degrees Celsius"));
	
}

void sendControl(const char *command, int battery){
	//Used to sent control commands
	
	j.startJsonObject(true);
	if ( command == "batteryWarning")
	{
		j.addKeyValuePair("cmd","batteryWarning");
		j.addKeyValuePair("battery",battery);
	}
	
    // You can add other control commands here	
	
	j.endJsonObject();
	
	USB.println(F("> Message to send: "));
	USB.println(j.getMessage());
	answer = c.publish(MQTT_TOPIC_CONTROL,j.getMessage());
    if(answer) {
      USB.print(F("\t\t Successfully sent data set "));
   
    }
    else {
      USB.println(F("\t\t Can't send data."));
	 
	}
}

boolean sendData(){
	
	//Start JSON
	j.startJsonObject(true);
	j.addKeyValuePair("long",longitude);
	j.addKeyValuePair("lat",latitude);
	j.addKeyValuePair("alt",altitude);
	j.addKeyValuePair("spd",speedOG);
	j.addKeyValuePair("hum",humidity);
	j.addKeyValuePair("prs",pressure);
	j.addKeyValuePair("tmp",temperature);
	j.addKeyValuePair("co2",co2);
	j.addKeyValuePair("time",GPRS_SIM928A.UTC_time);
	j.addKeyValuePair("date",GPRS_SIM928A.date);
	j.endJsonObject();
	//
	USB.println(F("> Message to send: "));
	USB.println(j.getMessage());
	USB.print(F("> Sending data set"));
	int answer = c.publish(MQTT_TOPIC,j.getMessage());
    //answer=1;
	if(answer) {
      USB.println(F("\t\t Successfully sent data set "));
      return true;
    }
    else {
       USB.println(F("\t\t Can't send data."));
	   USB.print(F(">> Closing socket"));
	   answer = GPRS_Pro.closeSocket();
		if(answer) {
			client.setConnected(0);
			USB.println(F("\t\t\tDone."));
		}
		  else 
		{
			USB.println(F("\t\t\tFail."));
			USB.println(F(">>Restarting socket"));
			GPRS_Pro.OFF();
			GPRS_Pro.ON();
		}

	   answer = c.connect(MQTT_DEVICE_ID, MQTT_USERNAME, MQTT_PASSWORD);
	   if(!answer)
       {
         USB.println(F("> Unable to connect to network."));
		 return false;
       }
	   else
	   {
		   return true;
	   }   
	}
	
}

void cmdCallBack(char *inTopic, uint8_t *message, unsigned int len) {
  boolean lightOn;
  boolean lightOff;
  
  //USB.println(F("Command Received"));
  
  j.setTopic("a/b/lights");  
  USB.print(inTopic);  
  USB.print(F(" "));
  for(int i = 0; i < len; i++) {
    USB.print((byte)message[i]);
  }
  USB.println();


}
