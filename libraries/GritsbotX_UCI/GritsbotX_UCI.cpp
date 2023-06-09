#include "GritsbotX_UCI.h"

GritsbotX::GritsbotX():_motorLeft(_interruptL1, _interruptL2),_motorRight(_interruptR1, _interruptR2), \
_strip(2,10,NEO_GRB + NEO_KHZ800){

}

///////////////////////////////////////////////////////////
//Setup
///////////////////////////////////////////////////////////

void GritsbotX::SETUP(){

  ///////////////////////////////////////////////////////////
  //Set Pin Modes!
  ///////////////////////////////////////////////////////////

  //IR Sensor Input Pins (Analog)
  pinMode(_IRF, INPUT);//Set the Center IR Pin to Input
  pinMode(_IRLF, INPUT);//Set the Left Forward IR Pin to Input
  pinMode(_IRL, INPUT);//Set the Left IR Pin to Input
  pinMode(_IRLB, INPUT);//Set the Back Left IR Pin to Input
  pinMode(_IRRB, INPUT);//Set the Back Right IR Pin to Input
  pinMode(_IRR, INPUT);//Set the Right IR Pin to Input
  pinMode(_IRRF, INPUT);//Set the Right Forward IR Pin to Input

  //IR Sensor Enable Pins (Digital) Set High
  //pinMode(_enIRF, OUTPUT);//Set the Center Enable IR Pin to Output
  //digitalWrite(_enIRF, HIGH);
  //pinMode(_enIRLF, OUTPUT);//Set the Left Forward Enable IR Pin to Output
  //digitalWrite(_enIRLF, HIGH);
  //pinMode(_enIRL, OUTPUT);//Set the Left Enable IR Pin to Output
  //digitalWrite(_enIRL, HIGH);
  //pinMode(_enIRLB, OUTPUT);//Set the Back Left Enable IR Pin to Output
  //digitalWrite(_enIRLB, HIGH);
  //pinMode(_enIRRB, OUTPUT);//Set the Back Right Enable IR Pin to Output
  //digitalWrite(_enIRRB, HIGH);
  //pinMode(_enIRR, OUTPUT);//Set the Right Enable IR Pin to Output
  //digitalWrite(_enIRR, HIGH);
  //pinMode(_enIRRF, OUTPUT);//Set the Right Forward Enable IR Pin to Output
  //digitalWrite(_enIRRF, HIGH);

  //Battery Feedback Pins
  pinMode(_battChargingCheck, INPUT_PULLDOWN);//Set the Battery Charging Pin to Input,
  // use pulldown so when the charger is not connected the pin stays low.

  //Motor Control 
  pinMode(_STBY, OUTPUT);

  pinMode(_PWMR, OUTPUT); 
  pinMode(_PWML, OUTPUT);  
  
  pinMode(_RMotor1, OUTPUT); 
  pinMode(_RMotor2, OUTPUT); 
  
  pinMode(_LMotor1, OUTPUT); 
  pinMode(_LMotor2, OUTPUT);
  
  // Raspberry Pi Soft Start
  //pinMode(_enRPi, INPUT);
  pinMode(_enRPi, OUTPUT);
  digitalWrite(_enRPi, LOW);
  delay(5000);
  digitalWrite(_enRPi, HIGH);

  ///////////////////////////////////////////////////////////
  // Neo Pixel Setup
  ///////////////////////////////////////////////////////////

  _strip.begin(); // Initialize the neo pixel library
  _strip.setBrightness(64); // Set LEDs to 1/4 brightness.
  _strip.show(); // Turn LEDs off (initially set to off)

  ///////////////////////////////////////////////////////////
  //Start Timers for Control Loops
  ///////////////////////////////////////////////////////////
  _driveStart = false;
  _PIDMotorsTimeStart = millis();
  _positionUpdateTimeStart = millis();
  _communicationTimeout = millis();
}

///////////////////////////////////////////////////////////
//Conversion Functions
///////////////////////////////////////////////////////////

float GritsbotX::_deg2rad(float deg){
  //Converts degree to radians
  float output = deg * _pi / 180;
  return output;
}

float GritsbotX::_rad2deg(float rad){
  //Converts radians to degree
  float output = rad * 180 / _pi;
  return output;
}

float GritsbotX::_wrapToPi(float rad){
  //Converts radian angle (-pi,pi]  myRobot.moveR(255);  
  //myRobot.moveL(255);
  float output = atan2(sin(rad),cos(rad));
  return output;
}

float GritsbotX::_wrapTo2Pi(float rad){
  //Converts radian angle [0,2*pi)
  float output = _wrapToPi(rad);
  if (output < 0){
    output = output + 2*_pi;
  }
  return output;
}

///////////////////////////////////////////////////////////
// Unicycle Model Conversions
///////////////////////////////////////////////////////////

float GritsbotX::convertUnicycleToRightMotor(float vel, float w){
  float output = (2.0*vel+w*_axelLength)/(_wheelDiameter);
  return output;
}

float GritsbotX::convertUnicycleToLeftMotor(float vel, float w){
  float output = (2.0*vel-w*_axelLength)/(_wheelDiameter);
  return output;
}

///////////////////////////////////////////////////////////
// JSON Serial Messaging
///////////////////////////////////////////////////////////

void GritsbotX::jsonSerialRead(){
  if(Serial.available()){
    JsonObject& jsonIn = _jsonBufferIn.parseObject(Serial);
    JsonObject& jsonOut = _jsonBufferOut.createObject();
    JsonArray& statusArray = jsonOut.createNestedArray("status");
    JsonArray& bodyArray = jsonOut.createNestedArray("body");
    JsonArray& requestArray = jsonIn["request"];
    JsonArray& ifaceArray = jsonIn["iface"];
    int requestArraySize = requestArray.size();//Same number of requests as iface.
    for(int i=0;i<requestArraySize;i++){
      const char* requestStr = requestArray[i];
      if(strcmp(requestStr,"read") == 0){
        _method = 0;
      }
      else if(strcmp(requestStr,"write") == 0){
        _method = 1;
      }
      switch(_method){
        case 0://read
        {
          JsonObject& body = bodyArray.createNestedObject();
          const char* ifaceStr = ifaceArray[i];
          if (strcmp(ifaceStr,"batt_volt") == 0){
            statusArray.add(1);
            body["batt_volt"] = checkBattVoltage();
          }
          else if (strcmp(ifaceStr,"charge_status") == 0){
            statusArray.add(1);
            body["charge_status"] = checkCharging();
          }
          break;
        }
        case 1://write
        {
          JsonObject& body = bodyArray.createNestedObject();
          const char* ifaceStr = ifaceArray[i];
          if (strcmp(ifaceStr,"motor") == 0){
            statusArray.add(1);
            _v = jsonIn["body"][i]["v"];            
            _w = jsonIn["body"][i]["w"];
          }
          else if (strcmp(ifaceStr,"left_led") == 0){
            statusArray.add(1);
            JsonArray& _leftLED = jsonIn["body"][i]["rgb"];//Left LED RGB value
            setSingleLED(1,_leftLED[0],_leftLED[1],_leftLED[2]);
          }
          else if (strcmp(ifaceStr,"right_led") == 0){
            statusArray.add(1);
            JsonArray& _rightLED = jsonIn["body"][i]["rgb"];//Right LED RGB value
            setSingleLED(0,_rightLED[0],_rightLED[1],_rightLED[2]);
          }

          _communicationTimeout = millis();
          break;
        }
      }  
    }
    if(Serial.dtr()){
      jsonOut.printTo(Serial);      
      _jsonBufferOut.clear();
    }
    _jsonBufferIn.clear();   
  }
}

void GritsbotX::communicationCheck(float communicationWaitTime){
  if (millis() - _communicationTimeout > communicationWaitTime){
    brake();
    _v=0;
    _w=0;
  }
}

void GritsbotX::followCommands(){
  if (_v == float(0) and _w == float(0)){
    brake();
    _v=0;
    _w=0;
    return;
  }
  PIDMotorControl(convertUnicycleToLeftMotor(_v,_w),convertUnicycleToRightMotor(_v,_w));
}

///////////////////////////////////////////////////////////
//Neo Pixel Functions
///////////////////////////////////////////////////////////

void GritsbotX::setSingleLED(int pos, int r, int g, int b){
  //Sets the color of one LED
  if (pos >= _strip.numPixels()){ // Controlling a non-existant LED (as defined in SETUP).
    return;
  } 
  _strip.setPixelColor(pos, r, g, b);
  _strip.show();
}

void GritsbotX::setAllLED(int r, int g, int b){
  //Sets the color of both LEDs
  for (int i = 0; i < _strip.numPixels(); i++){
    _strip.setPixelColor(i, r, g, b);
  }
  _strip.show();
}

void GritsbotX::turnOffLED(){
  //Turns off both LEDs
  for (int i = 0; i < _strip.numPixels(); i++){
    _strip.setPixelColor(i, 0, 0, 0);
  }
  _strip.show();
}

void GritsbotX::rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<_strip.numPixels(); i++) {
      _strip.setPixelColor(i, _wheel((i+j) & 255));
    }
    _strip.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t GritsbotX::_wheel(byte wheelPos) {
  wheelPos = 255 - wheelPos;
  if(wheelPos < 85) {
    return _strip.Color(255 - wheelPos * 3, 0, wheelPos * 3);
  }
  if(wheelPos < 170) {
    wheelPos -= 85;
    return _strip.Color(0, wheelPos * 3, 255 - wheelPos * 3);
  }
  wheelPos -= 170;
  return _strip.Color(wheelPos * 3, 255 - wheelPos * 3, 0);
}

///////////////////////////////////////////////////////////
//Read Sensors
///////////////////////////////////////////////////////////

bool GritsbotX::checkCharging(){
  //Determines if the robot is charging its battery
  bool chargingStatus = digitalRead(_battChargingCheck);
  return chargingStatus;
}

float GritsbotX::checkBattVoltage(){
  //Determines if the robot is charging its battery
  float input = analogRead(_battVoltage);

  //Scale the reading from the voltage divider to determine the correct reading.
  float battVoltage = input * 3.3/1023.0 * ((5.0+15.0)/15.0);
  return battVoltage;
}

void GritsbotX::disableIR(){ //Turns off the emitter of the IR sensors (Used for lowering power consumption).
  digitalWrite(_enIRF,LOW);
  digitalWrite(_enIRRF,LOW);
  digitalWrite(_enIRR,LOW);
  digitalWrite(_enIRRB,LOW);
  digitalWrite(_enIRLB,LOW);
  digitalWrite(_enIRL,LOW);
  digitalWrite(_enIRLF,LOW);
}

void GritsbotX::enableIR(){ //Turns on the emitter of the IR sensors..
  digitalWrite(_enIRF,HIGH);
  digitalWrite(_enIRRF,HIGH);
  digitalWrite(_enIRR,HIGH);
  digitalWrite(_enIRRB,HIGH);
  digitalWrite(_enIRLB,HIGH);
  digitalWrite(_enIRL,HIGH);
  digitalWrite(_enIRLF,HIGH);
}

    //TODO MAKE SURE CALIBRATION IS CORRECT

void GritsbotX::readIR(float irData[]){
  // Read and store the IR sensors in a clockwise fashion starting 
  // from the front (curved edge) [Front, Right Forward, Right, 
  // Back Right, Back Left, Left, Front Left].
  enableIR();
  _RDistance = analogRead(_IRR);
  _RFDistance = analogRead(_IRRF);
  _FDistance = analogRead(_IRF);
  _LFDistance = analogRead(_IRLF);
  _LDistance = analogRead(_IRL);
  _LBDistance = analogRead(_IRLB);
  _RBDistance = analogRead(_IRRB);

  irData[0] = _FDistance;
  irData[1] = _RFDistance;
  irData[2] = _RDistance;
  irData[3] = _RBDistance;
  irData[4] = _LBDistance;
  irData[5] = _LDistance;
  irData[6] = _LFDistance;

/* 
// Takes the value from the IR and converts it to a voltage.
  
  float voltsR = analogRead(_IRR)* 0.00322265625;   // value from right sensor * (5/1024) - if running 3.3.volts then change 5 to 3.3
  float voltsC = analogRead(IRC)* 0.00322265625;   // value from center sensor * (5/1024) - if running 3.3.volts then change 5 to 3.3
  float voltsL = analogRead(IRL)* 0.00322265625;   // value from left sensor * (5/1024) - if running 3.3.volts then change 5 to 3.3
  float voltsLF = analogRead(IRLF)* 0.00322265625;   // value from left forward sensor * (5/1024) - if running 3.3.volts then change 5 to 3.3
  float voltsRF = analogRead(IRRF)* 0.00322265625;   // value from right forward sensor * (5/1024) - if running 3.3.volts then change 5 to 3.3
  float voltsB = analogRead(IRB)* 0.00322265625;   // value from right forward sensor * (5/1024) - if running 3.3.volts then change 5 to 3.3
*/  
  
  /* This calibration is for the Sharp GP2Y0A41SK0F IR distance sensor.(http://www.pololu.com/product/2464)
  
  This measures to the back of the sensor! If you want the distance to the lense subtract off the width of the sensor.
    The units are in CM.
  
  This sensor has good accuracy from 4~30 cm then starts to get fuzzy.
  
  D=a/(V-b);
  
  a=15.68;
  b=-0.03907;
 
  Width of the Sensor=1.3;
  */
/*
     RDistance = a/(voltsR - b);
     CDistance = a/(voltsC - b);
     LDistance = a/(voltsL - b);
     LFDistance = a/(voltsLF - b);
     RFDistance = a/(voltsRF - b);
     BDistance = a/(voltsB - b); 
*/
}

void GritsbotX::_readEncoders(){
  // Reads the encoder and returns the current count.
  _encoderCountR = -_motorRight.read();
  _encoderCountL = -_motorLeft.read();
}

void GritsbotX::getEncoderCounts(int encoderData[]){
  // Reads the current encoder count and stores it in the array argument. [Left, Right]
  _readEncoders();
  encoderData[0] = _encoderCountL;
  encoderData[1] = _encoderCountR;
}

///////////////////////////////////////////////////////////
//Estimators
///////////////////////////////////////////////////////////

void GritsbotX::getGlobalPosition(float state[]){ //Returns the global position of the robot [x,y,theta].
  state[0] = _botXPos;
  state[1] = _botYPos;
  state[2] = _botA;
}

void GritsbotX::setGlobalPosition(float x, float y, float a){ // Set the global state of the robot.
  _botXPos = x;
  _botYPos = y;
  _botA = a;
}

void GritsbotX::encoderPositionUpdate(float timeStep){

  if (millis() - _positionUpdateTimeStart >= timeStep){
    timeStep = (millis() - _positionUpdateTimeStart)/1000; //Convert ms to s
    _positionUpdateTimeStart = millis();

    _readEncoders();
    int countL = _encoderCountL;
    int countR = _encoderCountR;

    float Dl = _pi*_wheelDiameter * (countL - _oldEPUEncoderCountL) / (_encoderCountsPerRotation * _motorGearRatio); // Linear distance left wheel has rotated.
    float Dr = _pi*_wheelDiameter * (countR - _oldEPUEncoderCountR) / (_encoderCountsPerRotation * _motorGearRatio); // Linear distance left wheel has rotated.
    //Check integer roll over!
    if (countL < 0 && _oldEPUEncoderCountL > 0){
      Dl = _pi*_wheelDiameter * ((countL - (-32768)) + (32767 - _oldEPUEncoderCountL)) / (_encoderCountsPerRotation * _motorGearRatio); // Linear distance left wheel has rotated.
    }
    if (countR < 0 && _oldEPUEncoderCountR > 0){
      Dr = _pi*_wheelDiameter * ((countR - (-32768)) + (32767 - _oldEPUEncoderCountR)) / (_encoderCountsPerRotation * _motorGearRatio); // Linear distance left wheel has rotated.
    }
    float Dc = (Dr + Dl)/2; //Distance center of bot has moved read by encoders.
    _oldEPUEncoderCountR = countR;
    _oldEPUEncoderCountL = countL;
    
    _botA += (Dr - Dl)/_axelLength;

    _botXPos += Dc * cos(_botA0 + (_botA-_botA0)/2);
    _botYPos += Dc * sin(_botA0 + (_botA-_botA0)/2);

    _botVel = (Dr + Dl)/(2*timeStep);
    _botA0 = _botA;
  }  
}

///////////////////////////////////////////////////////////
//Motor Functions
///////////////////////////////////////////////////////////

void GritsbotX::noMotion(){
  //Stop the motor board from supplying current to motor.
  digitalWrite(_STBY, LOW);
  _readEncoders();

  // Make sure PID controller variables reset (Integral windup, time step being large).
  _driveStart = false;
}

void GritsbotX::brake(){
  //Brakes the motors. (Locked to not rotate passively, this can be overcome by enough torque)
  digitalWrite(_STBY, HIGH);

  //analogWrite(_PWMR, LOW);
  analogWrite(_RMotor1, 0);
  //digitalWrite(_RMotor2, LOW);
  analogWrite(_RMotor2, 0);
  
  //analogWrite(_PWML, LOW);
  analogWrite(_LMotor2, 0);
  //digitalWrite(_RMotor2, LOW);
  analogWrite(_LMotor1, 0);


  _readEncoders();

  // Make sure PID controller variables reset (Integral windup, time step being large).
  _driveStart = false;
}

void GritsbotX::moveR(int motorSpeed){
  digitalWrite(_STBY, HIGH);

  //Assume forward at first
  bool in1 = HIGH;
  bool in2 = LOW;

  if (motorSpeed > 255) {motorSpeed = 255;}
  if (motorSpeed < -255) {motorSpeed = -255;}
  

  //If motor speed is negative we want to rotate wheel backwards
  if (motorSpeed < 0){
    digitalWrite(_RMotor2, in2);
    analogWrite(_RMotor1, abs(motorSpeed));
  }
  else {
    digitalWrite(_RMotor1, in2);
    analogWrite(_RMotor2, abs(motorSpeed));
  }
}

void GritsbotX::moveL(int motorSpeed){
  digitalWrite(_STBY, HIGH);

  //Assume forward at first
  bool in1 = HIGH;
  bool in2 = LOW;

  if (motorSpeed > 255) {motorSpeed = 255;}
  if (motorSpeed < -255) {motorSpeed = -255;}
  

  //If motor speed is negative we want to rotate wheel backwards
  if (motorSpeed < 0){
    digitalWrite(_LMotor1, in2);
    analogWrite(_LMotor2, abs(motorSpeed));
  }
  else {
    digitalWrite(_LMotor2, in2);
    analogWrite(_LMotor1, abs(motorSpeed));
  }
}

///////////////////////////////////////////////////////////
//Controllers
///////////////////////////////////////////////////////////

void GritsbotX::PIDMotorControl(float desLVelInput, float desRVelInput){
    /*Keeps the rotational speeds of the individual motors at setpoints desLVel and desRVel (rad/s).*/

    float timeStep = 10;

    //Prefilter
    float a = 0.18;//Slightly tweaked to lower overshoot.

    if(!_driveStart){//We recently stopped motion and didn't call the PID loop, reset the PID timers/variables.
      _driveStart = true;
      _PIDMotorsTimeStart = millis();
      _oldMotorPIDEncoderCountL = _encoderCountL;
      _oldMotorPIDEncoderCountR = _encoderCountR;
      _integralL = 0;
      _integralR = 0; 
      _oldErrorL = 0;
      _oldErrorR = 0;
    }

    if(millis() - _PIDMotorsTimeStart >= 3*timeStep){//There's been a large time delay, reset the timer and current encoder error
      _PIDMotorsTimeStart = millis();                //to avoide integral windup.
      _oldMotorPIDEncoderCountL = _encoderCountL;
      _oldMotorPIDEncoderCountR = _encoderCountR;
    }

    if (millis() - _PIDMotorsTimeStart >= timeStep){
      _desVelR = (1-a)*_desVelR+a*desRVelInput;
      _desVelL = (1-a)*_desVelL+a*desLVelInput;
      float desLVel = _desVelL;
      float desRVel = _desVelR;
      float PIDTimeStep = (millis() - _PIDMotorsTimeStart)/1000;//Time step for controller to work on (s).

      _readEncoders();
      int countL = _encoderCountL;
      int countR = _encoderCountR;

      // Error on individual motors for vel control
      float errorL = desLVel - 2.0 * _pi * (countL - _oldMotorPIDEncoderCountL) / (_encoderCountsPerRotation * _motorGearRatio * PIDTimeStep);
      float errorR = desRVel - 2.0 * _pi * (countR - _oldMotorPIDEncoderCountR) / (_encoderCountsPerRotation * _motorGearRatio * PIDTimeStep);
      
      // Check and correct for rollover
      if (countL < 0 && _oldMotorPIDEncoderCountL > 0 && _oldMotorPIDEncoderCountL > 20000){
        errorL = desLVel - 2.0 * _pi * ((countL - (-32768)) - (32767 - _oldMotorPIDEncoderCountL)) / (_encoderCountsPerRotation * _motorGearRatio * PIDTimeStep);
      }
      if (countL > 0 && _oldMotorPIDEncoderCountL < 0 && _oldMotorPIDEncoderCountL < -20000){
        errorL = desLVel - 2.0 * _pi * ((32767 - countL) - (_oldMotorPIDEncoderCountL - (-32768))) / (_encoderCountsPerRotation * _motorGearRatio * PIDTimeStep);
      }

      if (countR < 0 && _oldMotorPIDEncoderCountR > 0 && _oldMotorPIDEncoderCountR > 20000){
        errorR = desRVel - 2.0 * _pi * ((countR - (-32768)) - (32767 - _oldMotorPIDEncoderCountR)) / (_encoderCountsPerRotation * _motorGearRatio * PIDTimeStep);
      }
      if (countR > 0 && _oldMotorPIDEncoderCountR < 0 && _oldMotorPIDEncoderCountR < -20000){
        errorR = desRVel - 2.0 * _pi * ((32767 - countR) - (_oldMotorPIDEncoderCountR - (-32768))) / (_encoderCountsPerRotation * _motorGearRatio * PIDTimeStep);
      }

      _integralL = _integralL + errorL * PIDTimeStep;
      _integralR = _integralR + errorR * PIDTimeStep;
      float diffL = (_oldErrorL - errorL) / PIDTimeStep;
      float diffR = (_oldErrorR - errorR) / PIDTimeStep;
      _oldErrorL = errorL;
      _oldErrorR = errorR;
      _oldMotorPIDEncoderCountL = countL;
      _oldMotorPIDEncoderCountR = countR;


      //Get rid of integral windup with feedback loop.
      if(_satR){
        _motorR += int(_motorDigitalK*(_kpMotor*errorR + _kiMotor*(_integralR + _satRVal*PIDTimeStep) + _kdMotor*diffR));
        _satR = false;
      }
      else{
        _motorR += int(_motorDigitalK*(_kpMotor*errorR + _kiMotor*_integralR + _kdMotor*diffR));
      }

      if(_satL){
        _motorL += int(_motorDigitalK*(_kpMotor*errorL + _kiMotor*(_integralL + _satLVal*PIDTimeStep) + _kdMotor*diffL));
        _satL = false;
      }
      else{
        _motorL += int(_motorDigitalK*(_kpMotor*errorL + _kiMotor*_integralL + _kdMotor*diffL));
      }
      
      //Check and deal with motor saturation.
      if (_motorL>255){
        _integralL -= (errorL*PIDTimeStep); //Don't integrate at saturation
        _satLVal = (255 - _motorL);
        _satL = true;
        _motorL=255;
      }
      if (_motorR>255){
        _integralR -= (errorR*PIDTimeStep); // Don't integrate at saturation
        _satRVal = (255 - _motorR);
        _satR = true;
        _motorR=255;
      }
      if (_motorL<-255){
        _integralL -= (errorL*PIDTimeStep); // Don't integrate at saturation
        _satLVal = (-255 - _motorL);
        _satL = true;
        _motorL=-255;
      }
      if (_motorR<-255){
        _integralR -= (errorR*PIDTimeStep); // Don't integrate at saturation
        _satRVal = (-255 - _motorR);
        _satR = true;
        _motorR=-255;
      }

      moveL(_motorL);
      moveR(_motorR);

      _PIDMotorsTimeStart = millis();
    }
}
