#include "protocol.h"

//constants
#define LEFT_CONST 320
#define RIGHT_CONST 390
#define NET_DELAY 400
#define NET_WAITING 3
#define MIN_FORCE 80
#define TOUCH_DELAY 20
#define FSR_NUM 4
#define KEEP_ALIVE_TIME 4000
#define SAMPLINGS 100
#define ANALOG_MAX 1023
#define INVALID_FORCE ANALOG_MAX/2
#define MIN_FORCE_ADDITION 20

//global variables
unsigned short netDetect = 0;
unsigned long startNetDetect = 0;
bool conn = false, calibrated = false;
unsigned short minForce[] = {0, 0, 0, 0, 0, 0};
unsigned long lastSent, startDetect[] = {0, 0, 0, 0, 0, 0};
bool detected[] = {false, false, false, false, false, false};

//function declarations
bool establishConn();
bool calibrate();
unsigned short sendAndReceive(unsigned short toSend);
int checkLineHit();
int checkNetHit();

void setup()
{
  //setup of serial communication
  Serial.begin(9600);
  Serial1.begin(9600);
  pinMode(19, INPUT);  
  digitalWrite(19, HIGH);

  if (!calibrate) //try to calibrate the sensors
  {
    //if the calibration failed, use default minimum force for each sensor
    for (unsigned short i = 0; i < FSR_NUM; i++)
    {
      minForce[i] = MIN_FORCE;
    }
  }
}

void loop()
{
  if (!conn)
  {
    conn = establishConn(); //wait until connection is established
  }
  else
  {
    int lineHit;
    if((lineHit = checkLineHit()) != 0) //if line was hit
    {
      //send line number and wait for acknowledgement
      unsigned short received = sendAndReceive(lineHit);
      if (received == DISCONN)
        return;
      if (received % 10000 != lineHit % 10000 + ACK) {
        Serial.println("ERROR: ack for line hit was not received \n");
      }
    }
    
    int netHit;
    if((netHit = checkNetHit()) != 0) //if net was hit
    {
      //send net side and wait for acknowledgement
      unsigned short received = sendAndReceive(netHit);
      if (received == DISCONN)
        return;
      if (received % 10000 != netHit % 10000 + ACK) {
        Serial.println("ERROR: ack for net hit was not received \n");
      }
    }
    
    if(millis() - lastSent >= KEEP_ALIVE_TIME) //if a few seconds passed and no message was sent
    {
      //make sure to keep the connection alive
      sendAndReceive(KEEP_ALIVE);
    }
  }
}

//waits until the app sends a connection establishment message
//sends connection ack message and returns true when connection is established
bool establishConn()
{
  unsigned short received;
  Serial.print("Waiting for connection...");
  //wait for receiving a connection request
  do
  {
    String resStr = Serial1.readStringUntil('~');
    received = resStr.toInt();
    if (received != 0)
    {
      Serial.print("Received: ");
      Serial.println(received);
      Serial.println();
      if(received != CONN_REQ) //if the app transferred data before asking for connection
      {
        unsigned short err = ERR_ARD + ERR_CONN;
        Serial1.print(err);
        Serial1.print("~");
        Serial.println("ERROR: app transferred data before asking for connection");
        Serial.print("Sent: ");
        Serial.println(err);
        lastSent = millis();
      }
    }
  }
  while(received != CONN_REQ);

  //send ack for connection request
  Serial1.print(CONN_ACK);
  Serial1.print("~");
  Serial.print("Sent: ");
  Serial.println(CONN_ACK);
  Serial.println("Connected \n");
  lastSent = millis();
  return true;
}

//calibrates each of the FSRs so that they will ignore unwanted noise
//returns true if calibration was successful, or false if an error occured
bool calibrate()
{
  for (unsigned short i = 0; i < SAMPLINGS; i++) {
    for (unsigned short i = 0; i < FSR_NUM; i++) {
      unsigned short samplingValue;
      if ((samplingValue = analogRead(i)) > minForce[i] - MIN_FORCE_ADDITION) {
        if (samplingValue >= INVALID_FORCE) {
          return false;
        }
        minForce[i] = samplingValue + MIN_FORCE_ADDITION;
      }
    }
  }
  return true;
}

//sends a message according to the protocol and returns the response received from the app
unsigned short sendAndReceive(unsigned short toSend)
{
  //send message
  Serial1.print(toSend);
  Serial1.print("~");
  Serial.print("Sent: ");
  Serial.println(toSend);

  //receive response
  String resStr = Serial1.readStringUntil('~');
  unsigned short received = resStr.toInt();
  Serial.print("Received: ");
  Serial.println(received);
  Serial.println();

  if (received == DISCONN)
    conn = false;

  //handle errors
  unsigned short err_val = received % 100;
  if (received - err_val == ERR_APP) {
    switch (err_val) {
      case ERR_PROTO:
        Serial.println("ERROR: message not according to protocol \n");
        break;
      case ERR_ACK:
        Serial.println("ERROR: acknowledgement doesn't match sent code \n");
        break;
      case ERR_CONN:
        Serial.println("ERROR: transferred data before asking for connection \n");
        break;
      default:
        Serial.print("ERROR: unknown error (");
        Serial.print(err_val);
        Serial.println(") \n");
    }
  }
  
  lastSent = millis();
  return received;
}

//returns a message according to the protocol for regular/ball touch, or 0 if the line wasn't hit
int checkLineHit()
{
  for (unsigned short i = 0; i < FSR_NUM; i++) { //for each FSR
    //if force was detected on the line, start measuring time
    if (!detected[i]) {
      if (analogRead(i) > minForce[i]) {
        startDetect[i] = millis();
        detected[i] = true;
      }
    }

    //after enough time passed since the force was detected, check the type of the hit
    else if (millis() - startDetect[i] >= TOUCH_DELAY) {
      startDetect[i] = 0;
      detected[i] = false;
      if (analogRead(i) > minForce[i]) {
        return REG_TOUCH + i + 1; //if the force is still strong, it is a regular touch
      }
      return BALL_TOUCH + i + 1; //otherwise, the ball hit the line
    }
  }
  return 0;
}

//returns a message according to the protocol for the side of the net that was hit, or 0 if it wasn't hit
int checkNetHit()
{
  unsigned int analogIn = analogRead(A8);

  if (analogIn < LEFT_CONST) { //if sensing acceleration from the left
    if (netDetect == 0) { //if it is the beginning of an oscillation, wait to see if the amplitude changes direction
      netDetect = NET_LEFT;
      startNetDetect = millis();
    }
    else if (netDetect == NET_RIGHT) { //if the amplitude changed direction within the time limit, it means that the net was hit
      netDetect = NET_WAITING; //wait a while until the oscilation stops
      startNetDetect = millis();
      return NET_TOUCH + NET_RIGHT;
    }
  }
  
  else if (analogIn > RIGHT_CONST) { //if sensing acceleration from the right
    if (netDetect == 0) { //if it is the beginning of an oscillation, wait to see if the amplitude changes direction
      netDetect = NET_RIGHT;
      startNetDetect = millis();
    }
    else if (netDetect == NET_LEFT) { //if the amplitude changed direction within the time limit, it means that the net was hit
      netDetect = NET_WAITING; //wait a while until the oscilation stops
      startNetDetect = millis();
      return NET_TOUCH + NET_LEFT;
    }
  }
  
  if (netDetect != 0 && millis() - startNetDetect > NET_DELAY) { //clear the oscillation delay timer after enough time passed 
    netDetect = 0;
  }

  return 0;
}
