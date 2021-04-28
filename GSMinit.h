#define TINY_GSM_MODEM_SIM800 //Ensure GSM modem model is defined
#include <TinyGsmClient.h> //Library used to interface with GSM modem
#include <PubSubClient.h> //Library used to interface with MQTT broker

TinyGsm modem(Serial2); //Establish communication link between GSM module and Teensy using UART on Serial2, ie. Pin 9 (RX2) and Pin 10 (TX2) on Teensy 3.6
TinyGsmClient GSMclient(modem); //Configure GSM modem as a client 
PubSubClient mqttClient(GSMclient); //Establish MQTT communication between MQTT broker and Teensy via GSM modem

void bootGSM(){
  //Function used to intitialize GSM modem as primary Internet communication device
  Serial.println("Initializing modem");
  modem.restart(); //Reboot GSM modem (ensures the module doesnt have any lingering connections)
  Serial.println("Modem: " + modem.getModemInfo()); //Details modem model and airtime balance
  Serial.print("Waiting for network ");
  if (!modem.waitForNetwork()){ 
    //Modem fails to connect to a network
    Serial.println("Fail");
    bootGSM();
  }
  Serial.println("OK");
  Serial.print("Connecting to internet ");
  if (!modem.gprsConnect("internet", "", "")) { //gprsConnect(apr, username, password)
    //Modem fails to access the Internet
    Serial.println("Fail");
    bootGSM();
  }
  Serial.println("OK");
}
