#include <Arduino.h>
#include "WiFi.h"
#include <esp_now.h>
//client CC:DB:A7:32:B7:BC
//mater C4:D8:D5:95:A7:D8


uint8_t broadcastAddress1[] = {0xcc,0xdb,0xa7,0x32,0xb7,0xbc}; //DeviceA (with GPS)

uint8_t broadcastAddress2[] = {0xf8,0xb3,0xb7,0x4e,0xfb,0x38}; //DeviceB
//
uint8_t broadcastAddress3[] = {0xf4,0x65,0x0b,0x42,0xf2,0x88}; //DeviceC

String msg_2;
TaskHandle_t Task1;
TaskHandle_t Task2;

// 👉 เพิ่มตรงนี้
void Task1code(void * pvParameters);
void Task2code(void * pvParameters);
// Register peer
esp_now_peer_info_t peerInfo;
  
#define RXD2 16
#define TXD2 17
#define RXD1 18 //Readout
#define TXD1 19 //Readout

void wink(){
  digitalWrite(2 , HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(100);                       // wait for a second
  digitalWrite(2 , LOW);    // turn the LED off by making the voltage LOW
  delay(100);   
}
void trig_rec(){
  digitalWrite(4 , HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(100);                       // wait for a second
  digitalWrite(4 , LOW);    // turn the LED off by making the voltage LOW
  delay(100);   
  digitalWrite(4 , HIGH);
}


//Sent function
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
     /*
      if (status ==0){
        Serial.println("OK");  
      }
      else{
        Serial.println("FAIL");  
      }*/
     trig_rec();
    //Do nothing
}


//Receive function
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  
  String dataIn;
  for (int i=0;i<len;i++) {
    dataIn += (char)incomingData[i];
  }

  Serial.print(dataIn);
  Serial.flush();
  delay(10);

}



void setup() {
  //Serial.begin(115200); 
  Serial.begin(9600); 
  Serial1.begin(9600, SERIAL_8N1, RXD1, TXD1);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  pinMode(2, OUTPUT);
  pinMode(4, OUTPUT);
  
  
  WiFi.mode(WIFI_STA);
  Serial.println(WiFi.macAddress());

//Initialize ESPNOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  //Define function Send&Receive
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);


  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  // register first peer  
  memcpy(peerInfo.peer_addr, broadcastAddress1, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer1");
    return;
  }
  // register second peer  
  
  memcpy(peerInfo.peer_addr, broadcastAddress2, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer2");
    return;
  } 

  // register third peer 
  memcpy(peerInfo.peer_addr, broadcastAddress3, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer2");
    return;
  } 


  //----------------------------------------------Task----------------------------
  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    Task2code,   /* Task function. */
                    "Task2",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
    delay(500); 
    
    //Blink Test
    digitalWrite(4 , HIGH);
    delay(500);
    digitalWrite(4, LOW);
    delay(500);
    digitalWrite(4 , HIGH);
    delay(500);
    digitalWrite(4, LOW);
    delay(500);
    digitalWrite(4 , HIGH);
    delay(500);
    
}


//Task1
void Task1code( void * pvParameters ){
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    wink();
  
    //delay(500);
          
  } //End for loop
      
} // End Task1


//Task2
void Task2code( void * pvParameters ){
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
      //wail data from keyboard then send to client
      if(Serial.available() > 0){
            String msg = Serial.readStringUntil('\r');
            //send data to Node(n) by N1+command ex: N1Fetch@1 -> Fetch@1 will be sent to client
            if(msg.substring(0,2) == "N1"){
              msg.remove(0,2);
              String data_fetch = msg + "\r\n"; 
              esp_err_t result = esp_now_send( broadcastAddress1, (uint8_t*)data_fetch.c_str(), data_fetch.length());
            }
           else if(msg.substring(0,2) == "N2"){
             msg.remove(0,2);
             String data_fetch2 = msg + "\r\n"; 
             esp_err_t result2 = esp_now_send( broadcastAddress2, (uint8_t*)data_fetch2.c_str(), data_fetch2.length());
           }
           else if(msg.substring(0,2) == "N3"){
             msg.remove(0,2);
             String data_fetch3 = msg + "\r\n"; 
             esp_err_t result2 = esp_now_send( broadcastAddress3, (uint8_t*)data_fetch3.c_str(), data_fetch3.length());
           }
            
            
            delay(100);
      }
  
    
    }
  delay(10);
  //End for loop
  
}//End task2


void loop() {
  delay(100);  //Do nothing on core 1
}
