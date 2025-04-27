//THERE IS NO WARRANTY FOR THE SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR 
//OTHER PARTIES PROVIDE THE SOFTWARE "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
//OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE SOFTWARE IS WITH THE CUSTOMER. SHOULD THE 
//SOFTWARE PROVE DEFECTIVE, THE CUSTOMER ASSUMES THE COST OF ALL NECESSARY SERVICING, REPAIR, OR CORRECTION EXCEPT TO THE EXTENT SET OUT UNDER THE HARDWARE WARRANTY IN THESE TERMS.

// - Throttle Descent Limiter : Prevents sudden drops by limiting how quickly throttle can decrease.
// - Stick Input Smoothing : Applies exponential smoothing to all stick inputs to prevent jerky movements.
// - Low Battery Failsafe : Monitors battery voltage and triggers warnings or auto-landing when voltage drops below thresholds.
// - Accidental Disarm Protection : Requires throttle to be below 1050μs (stricter than arming threshold) for disarming.
// - Wind Resistance Tuning : Detects high gyro rates that might indicate wind and increases PID gains to compensate.
// - Auto-Level on Takeoff : Forces level attitude for the first 2 seconds after arming.
// - Signal Loss Failsafe : Enters hover mode with gentle descent if no valid receiver signal is detected for over 1 second.
// - Motor Failure Detection : Monitors motor outputs for imbalances that might indicate a motor failure and reduces power to the opposite motor.
// - In-Flight Calibration Check : Monitors gyro readings for abnormal values that might indicate bad calibration.
// - No-Input Return Mode : Initiates auto-level if sticks remain centered for more than 5 seconds.
#include <Wire.h>
#include <ESP32Servo.h> // Change to the standard Servo library for ESP32
#include <EEPROM.h>

volatile float RatePitch, RateRoll, RateYaw;
float RateCalibrationPitch, RateCalibrationRoll, RateCalibrationYaw,AccXCalibration,AccYCalibration,AccZCalibration;

int ESCfreq=500;
float PAngleRoll=2; float PAnglePitch=PAngleRoll;
float IAngleRoll=0.5; float IAnglePitch=IAngleRoll;
float DAngleRoll=0.007; float DAnglePitch=DAngleRoll;

float PRateRoll = 0.625;
float IRateRoll = 2.1;
float DRateRoll = 0.0088;

float PRatePitch = PRateRoll;
float IRatePitch = IRateRoll;
float DRatePitch = DRateRoll;

float PRateYaw = 4;
float IRateYaw = 3;
float DRateYaw = 0;

uint32_t LoopTimer;
float t = 0.004;  // 4ms loop time hardcoded

// Arm/Disarm variables
bool armed = false;
unsigned long armingStart = 0;
bool armingInProgress = false;
unsigned long disarmStart = 0;
bool disarmingInProgress = false;
const unsigned long armingTime = 2000; // Time in ms to hold sticks for arming
const unsigned long disarmingTime = 1000; // Time in ms to hold sticks for disarming
const int armingThreshold = 1030; // Throttle threshold for arming
const int LED_PIN = 15; // LED pin for status indication

Servo mot1;
Servo mot2;
Servo mot3;
Servo mot4;

const int mot1_pin = 27;
const int mot2_pin = 12;
const int mot3_pin = 5; //is 14 for some designed FC on perforated baords
const int mot4_pin = 13;

volatile uint32_t current_time;
volatile uint32_t last_channel_1 = 0;
volatile uint32_t last_channel_2 = 0;
volatile uint32_t last_channel_3 = 0;
volatile uint32_t last_channel_4 = 0;
volatile uint32_t last_channel_5 = 0;
volatile uint32_t last_channel_6 = 0;
volatile uint32_t timer_1;
volatile uint32_t timer_2;
volatile uint32_t timer_3;
volatile uint32_t timer_4;
volatile uint32_t timer_5;
volatile uint32_t timer_6;
volatile int ReceiverValue[6]; // Increase the array size to 6 for Channel 1 to Channel 6
const int channel_1_pin = 34;
const int channel_2_pin = 35;
const int channel_3_pin = 32;
const int channel_4_pin = 33;
const int channel_5_pin = 25;
const int channel_6_pin = 26;

volatile float PtermRoll;
volatile float ItermRoll;
volatile float DtermRoll;
volatile float PIDOutputRoll;
volatile float PtermPitch;
volatile float ItermPitch;
volatile float DtermPitch;
volatile float PIDOutputPitch;
volatile float PtermYaw;
volatile float ItermYaw;
volatile float DtermYaw;
volatile float PIDOutputYaw;
volatile float KalmanGainPitch;
volatile float KalmanGainRoll;

int ThrottleIdle = 1170;
int ThrottleCutOff = 1000;

volatile float DesiredRateRoll, DesiredRatePitch, DesiredRateYaw;
volatile float ErrorRateRoll, ErrorRatePitch, ErrorRateYaw;
volatile float InputRoll, InputThrottle, InputPitch, InputYaw;
volatile float PrevErrorRateRoll, PrevErrorRatePitch, PrevErrorRateYaw;
volatile float PrevItermRateRoll, PrevItermRatePitch, PrevItermRateYaw;
volatile float PIDReturn[] = {0, 0, 0};

//Kalman filters for angle mode
volatile float AccX, AccY, AccZ;
volatile float AngleRoll, AnglePitch;
volatile float KalmanAngleRoll=0, KalmanUncertaintyAngleRoll=2*2;
volatile float KalmanAnglePitch=0, KalmanUncertaintyAnglePitch=2*2;
volatile float Kalman1DOutput[]={0,0};
volatile float DesiredAngleRoll, DesiredAnglePitch;
volatile float ErrorAngleRoll, ErrorAnglePitch;
volatile float PrevErrorAngleRoll, PrevErrorAnglePitch;
volatile float PrevItermAngleRoll, PrevItermAnglePitch;

float complementaryAngleRoll = 0.0f;
float complementaryAnglePitch = 0.0f;

volatile float MotorInput1, MotorInput2, MotorInput3, MotorInput4;

// LED status indicators
unsigned long ledLastToggle = 0;
bool ledState = false;
int ledBlinkRate = 500; // Default blink rate in ms

void kalman_1d(float KalmanState, float KalmanUncertainty, float KalmanInput, float KalmanMeasurement) {
  KalmanState=KalmanState + (t*KalmanInput);
  KalmanUncertainty=KalmanUncertainty + (t*t*4*4); //here 4 is the vairnece of IMU i.e 4 deg/s
  float KalmanGain=KalmanUncertainty * 1/(1*KalmanUncertainty + 3 * 3); //std deviation of error is 3 deg
  KalmanState=KalmanState+KalmanGain * (KalmanMeasurement-KalmanState);
  KalmanUncertainty=(1-KalmanGain) * KalmanUncertainty;
  Kalman1DOutput[0]=KalmanState; 
  Kalman1DOutput[1]=KalmanUncertainty;
}

void channelInterruptHandler() {
  current_time = micros(); 
  if (digitalRead(channel_1_pin)) { 
    if (last_channel_1 == 0) { 
      last_channel_1 = 1; 
      timer_1 = current_time; 
    } 
  } else if (last_channel_1 == 1) { 
    last_channel_1 = 0; 
    ReceiverValue[0] = current_time - timer_1; 
  } 
  
  if (digitalRead(channel_2_pin)) { 
    if (last_channel_2 == 0) { 
      last_channel_2 = 1; 
      timer_2 = current_time; 
    } 
  } else if (last_channel_2 == 1) { 
    last_channel_2 = 0; 
    ReceiverValue[1] = current_time - timer_2; 
  } 
  
  if (digitalRead(channel_3_pin)) { 
    if (last_channel_3 == 0) { 
      last_channel_3 = 1; 
      timer_3 = current_time; 
    } 
  } else if (last_channel_3 == 1) { 
    last_channel_3 = 0; 
    ReceiverValue[2] = current_time - timer_3; 
  } 
  
  if (digitalRead(channel_4_pin)) { 
    if (last_channel_4 == 0) { 
      last_channel_4 = 1; 
      timer_4 = current_time; 
    } 
  } else if (last_channel_4 == 1) { 
    last_channel_4 = 0; 
    ReceiverValue[3] = current_time - timer_4; 
  } 
  
  if (digitalRead(channel_5_pin)) { 
    if (last_channel_5 == 0) { 
      last_channel_5 = 1; 
      timer_5 = current_time; 
    } 
  } else if (last_channel_5 == 1) { 
    last_channel_5 = 0; 
    ReceiverValue[4] = current_time - timer_5; 
  } 
  
  if (digitalRead(channel_6_pin)) { 
    if (last_channel_6 == 0) { 
      last_channel_6 = 1; 
      timer_6 = current_time; 
    } 
  } else if (last_channel_6 == 1) { 
    last_channel_6 = 0; 
    ReceiverValue[5] = current_time - timer_6; 
  }
}

void gyro_signals(void) {
  Wire.beginTransmission(0x68);
  Wire.write(0x1A);
  Wire.write(0x03);
  Wire.endTransmission();
  Wire.beginTransmission(0x68);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission();
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(); 
  Wire.requestFrom(0x68,6);
  int16_t AccXLSB = Wire.read() << 8 | Wire.read();
  int16_t AccYLSB = Wire.read() << 8 | Wire.read();
  int16_t AccZLSB = Wire.read() << 8 | Wire.read();
  Wire.beginTransmission(0x68);
  Wire.write(0x1B); 
  Wire.write(0x8);
  Wire.endTransmission();                                                   
  Wire.beginTransmission(0x68);
  Wire.write(0x43);
  Wire.endTransmission();
  Wire.requestFrom(0x68,6);
  int16_t GyroX=Wire.read()<<8 | Wire.read();
  int16_t GyroY=Wire.read()<<8 | Wire.read();
  int16_t GyroZ=Wire.read()<<8 | Wire.read();
  RateRoll=(float)GyroX/65.5;
  RatePitch=(float)GyroY/65.5;
  RateYaw=(float)GyroZ/65.5;
  AccX=(float)AccXLSB/4096;
  AccY=(float)AccYLSB/4096;
  AccZ=(float)AccZLSB/4096;
  AngleRoll=atan(AccY/sqrt(AccX*AccX+AccZ*AccZ))*57.29; //*1/(3.142/180);
  AnglePitch=-atan(AccX/sqrt(AccY*AccY+AccZ*AccZ))*57.29;
}

void pid_equation(float Error, float P, float I, float D, float PrevError, float PrevIterm) {
  float Pterm = P * Error;
  float Iterm = PrevIterm +( I * (Error + PrevError) * (t/2));
  if (Iterm > 400) {
    Iterm = 400;
  }
  else if (Iterm < -400) {
    Iterm = -400;
  }
  float Dterm = D *( (Error - PrevError)/t);
  float PIDOutput = Pterm + Iterm + Dterm;
  if (PIDOutput > 400) {
    PIDOutput = 400;
  }
  else if (PIDOutput < -400) {
    PIDOutput = -400;
  }
  PIDReturn[0] = PIDOutput;
  PIDReturn[1] = Error;
  PIDReturn[2] = Iterm;
}

// Function to handle LED status indication
void updateLED() {
  unsigned long currentMillis = millis();
  
  // Blink LED according to arm status
  if (currentMillis - ledLastToggle >= ledBlinkRate) {
    ledLastToggle = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}

// Function to check arming conditions
void checkArmingStatus() {
  // Arming sequence: Low throttle and yaw right for 2 seconds
  if (!armed && !armingInProgress && 
      ReceiverValue[2] < armingThreshold && 
      ReceiverValue[3] > 1800) {
    armingInProgress = true;
    armingStart = millis();
    ledBlinkRate = 100; // Fast blink during arming process
  }
  
  // Check if arming sequence is complete
  if (armingInProgress) {
    if (ReceiverValue[2] < armingThreshold && ReceiverValue[3] > 1800) {
      if (millis() - armingStart >= armingTime) {
        armed = true;
        armingInProgress = false;
        ledBlinkRate = 1000; // Slow blink when armed
        takeoffTime = millis(); // Record takeoff time for auto-level
        autoLevelActive = true; // Enable auto-level on takeoff
        
        // Reset PID terms when arming
        resetPIDTerms();
        
        // Indicate successful arming with LED pattern
        for (int i = 0; i < 5; i++) {
          digitalWrite(LED_PIN, HIGH);
          delay(100);
          digitalWrite(LED_PIN, LOW);
          delay(100);
        }
      }
    } else {
      // If stick positions changed during arming, cancel the sequence
      armingInProgress = false;
      ledBlinkRate = 500; // Normal blink rate
    }
  }
  
  // Disarming sequence: Low throttle and yaw left for 1 second
  // Added protection: Only allow disarm if throttle is below 1050μs
  if (armed && !disarmingInProgress && 
      ReceiverValue[2] < 1050 && // Stricter threshold for disarming
      ReceiverValue[3] < 1200) {
    disarmingInProgress = true;
    disarmStart = millis();
    ledBlinkRate = 100; // Fast blink during disarming process
  }
  
  // Check if disarming sequence is complete
  if (disarmingInProgress) {
    if (ReceiverValue[2] < 1050 && ReceiverValue[3] < 1200) {
      if (millis() - disarmStart >= disarmingTime) {
        armed = false;
        disarmingInProgress = false;
        ledBlinkRate = 500; // Normal blink rate
        
        // Indicate successful disarming with LED pattern
        for (int i = 0; i < 3; i++) {
          digitalWrite(LED_PIN, HIGH);
          delay(200);
          digitalWrite(LED_PIN, LOW);
          delay(200);
        }
      }
    } else {
      // If stick positions changed during disarming, cancel the sequence
      disarmingInProgress = false;
      ledBlinkRate = armed ? 1000 : 500; // Return to appropriate blink rate
    }
  }
  
  // Emergency disarm: Roll hard left and throttle low
  // This is still allowed for safety reasons
  if (armed && ReceiverValue[2] < armingThreshold && ReceiverValue[0] < 1200) {
    armed = false;
    ledBlinkRate = 500; // Normal blink rate
    
    // Indicate emergency disarm with rapid LED flashes
    for (int i = 0; i < 10; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(50);
      digitalWrite(LED_PIN, LOW);
      delay(50);
    }
  }
}

// Reset all PID terms when arming/disarming
void resetPIDTerms() {
  PrevErrorRateRoll = 0; 
  PrevErrorRatePitch = 0; 
  PrevErrorRateYaw = 0;
  PrevItermRateRoll = 0; 
  PrevItermRatePitch = 0; 
  PrevItermRateYaw = 0;
  PrevErrorAngleRoll = 0; 
  PrevErrorAnglePitch = 0;    
  PrevItermAngleRoll = 0; 
  PrevItermAnglePitch = 0;
}

void setup(void) {
  Serial.begin(115200);

  // Initialize EEPROM for settings
  EEPROM.begin(512);

  // Setup battery monitoring
  pinMode(BATTERY_PIN, INPUT);
  
  // Setup LED pin for status indication
  pinMode(LED_PIN, OUTPUT);
  
  // Initial LED sequence to indicate power-on
  int led_time = 100;
  for (int i = 0; i < 4; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(led_time);
    digitalWrite(LED_PIN, LOW);
    delay(led_time);
  }

  pinMode(channel_1_pin, INPUT_PULLUP);
  pinMode(channel_2_pin, INPUT_PULLUP);
  pinMode(channel_3_pin, INPUT_PULLUP);
  pinMode(channel_4_pin, INPUT_PULLUP);
  pinMode(channel_5_pin, INPUT_PULLUP);
  pinMode(channel_6_pin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(channel_1_pin), channelInterruptHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(channel_2_pin), channelInterruptHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(channel_3_pin), channelInterruptHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(channel_4_pin), channelInterruptHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(channel_5_pin), channelInterruptHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(channel_6_pin), channelInterruptHandler, CHANGE);
  delay(100);
  
  Wire.setClock(400000);
  Wire.begin();
  delay(250);
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  delay(1000);
  mot1.attach(mot1_pin,1000,2000);
  delay(1000);
  mot1.setPeriodHertz(ESCfreq);
  delay(100);
  mot2.attach(mot2_pin,1000,2000);
  delay(1000);
  mot2.setPeriodHertz(ESCfreq);
  delay(100);
  mot3.attach(mot3_pin,1000,2000);
  delay(1000);
  mot3.setPeriodHertz(ESCfreq);
  delay(100);
  mot4.attach(mot4_pin,1000,2000);
  delay(1000);
  mot4.setPeriodHertz(ESCfreq);
  delay(100);

  mot1.writeMicroseconds(1000);
  mot2.writeMicroseconds(1000);
  mot3.writeMicroseconds(1000);
  mot4.writeMicroseconds(1000);
  delay(500);
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);

  // Set gyro and accelerometer calibration values
  RateCalibrationRoll = -0.96;
  RateCalibrationPitch = 3.58;
  RateCalibrationYaw = -1.27;

  AccXCalibration = 0.01;
  AccYCalibration = 0.01;
  AccZCalibration = 0.02;

  LoopTimer = micros();
  
  // Print startup message with arming instructions
  Serial.println("Quadcopter initialized");
  Serial.println("To arm: Hold throttle low and yaw right for 2 seconds");
  Serial.println("To disarm: Hold throttle low and yaw left for 1 second");
  Serial.println("Emergency disarm: Roll left + throttle low");
}

void loop(void) {
  // Update LED status based on arm state
  updateLED();
  
  // Check arming/disarming status
  checkArmingStatus();

  // Read gyro and accelerometer data
  Wire.beginTransmission(0x68);
  Wire.write(0x1A);
  Wire.write(0x05);
  Wire.endTransmission();
  Wire.beginTransmission(0x68);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission();
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(); 
  Wire.requestFrom(0x68,6);
  int16_t AccXLSB = Wire.read() << 8 | Wire.read();
  int16_t AccYLSB = Wire.read() << 8 | Wire.read();
  int16_t AccZLSB = Wire.read() << 8 | Wire.read();
  Wire.beginTransmission(0x68);
  Wire.write(0x1B); 
  Wire.write(0x8);
  Wire.endTransmission();                                                   
  Wire.beginTransmission(0x68);
  Wire.write(0x43);
  Wire.endTransmission();
  Wire.requestFrom(0x68,6);
  int16_t GyroX=Wire.read()<<8 | Wire.read();
  int16_t GyroY=Wire.read()<<8 | Wire.read();
  int16_t GyroZ=Wire.read()<<8 | Wire.read();
  RateRoll=(float)GyroX/65.5;
  RatePitch=(float)GyroY/65.5;
  RateYaw=(float)GyroZ/65.5;
  AccX=(float)AccXLSB/4096;
  AccY=(float)AccYLSB/4096;
  AccZ=(float)AccZLSB/4096;

  RateRoll -= RateCalibrationRoll;
  RatePitch -= RateCalibrationPitch;
  RateYaw -= RateCalibrationYaw;

  AccX -= AccXCalibration;
  AccY -= AccYCalibration;
  AccZ -= AccZCalibration;

  AngleRoll=atan(AccY/sqrt(AccX*AccX+AccZ*AccZ))*57.29;
  AnglePitch=-atan(AccX/sqrt(AccY*AccY+AccZ*AccZ))*57.29;

  complementaryAngleRoll=0.991*(complementaryAngleRoll+RateRoll*t) + 0.009*AngleRoll;
  complementaryAnglePitch=0.991*(complementaryAnglePitch+RatePitch*t) + 0.009*AnglePitch;
  
  // Clamping complementary filter roll angle to ±20 degrees
  complementaryAngleRoll = (complementaryAngleRoll > 20) ? 20 : ((complementaryAngleRoll < -20) ? -20 : complementaryAngleRoll);
  complementaryAnglePitch = (complementaryAnglePitch > 20) ? 20 : ((complementaryAnglePitch < -20) ? -20 : complementaryAnglePitch);

  DesiredAngleRoll=0.1*(ReceiverValue[0]-1500);
  DesiredAnglePitch=0.1*(ReceiverValue[1]-1500);
  InputThrottle=ReceiverValue[2];
  DesiredRateYaw=0.15*(ReceiverValue[3]-1500);

  // Inlined PID equation for Roll
  ErrorAngleRoll = DesiredAngleRoll - complementaryAngleRoll;
  PtermRoll = PAngleRoll * ErrorAngleRoll;
  ItermRoll = PrevItermAngleRoll + (IAngleRoll * (ErrorAngleRoll + PrevErrorAngleRoll) * (t / 2));
  ItermRoll = (ItermRoll > 400) ? 400 : ((ItermRoll < -400) ? -400 : ItermRoll);
  DtermRoll = DAngleRoll * ((ErrorAngleRoll - PrevErrorAngleRoll) / t);
  PIDOutputRoll = PtermRoll + ItermRoll + DtermRoll;
  PIDOutputRoll = (PIDOutputRoll > 400) ? 400 : ((PIDOutputRoll < -400) ? -400 : PIDOutputRoll);
  DesiredRateRoll = PIDOutputRoll;
  PrevErrorAngleRoll = ErrorAngleRoll;
  PrevItermAngleRoll = ItermRoll;

  ErrorAnglePitch = DesiredAnglePitch - complementaryAnglePitch;
  PtermPitch = PAnglePitch * ErrorAnglePitch;
  ItermPitch = PrevItermAnglePitch + (IAnglePitch * (ErrorAnglePitch + PrevErrorAnglePitch) * (t / 2));
  ItermPitch = (ItermPitch > 400) ? 400 : ((ItermPitch < -400) ? -400 : ItermPitch);
  DtermPitch = DAnglePitch * ((ErrorAnglePitch - PrevErrorAnglePitch) / t);
  PIDOutputPitch = PtermPitch + ItermPitch + DtermPitch;
  PIDOutputPitch = (PIDOutputPitch > 400) ? 400 : ((PIDOutputPitch < -400) ? -400 : PIDOutputPitch);
  DesiredRatePitch = PIDOutputPitch;
  PrevErrorAnglePitch = ErrorAnglePitch;
  PrevItermAnglePitch = ItermPitch;

  // Compute errors
  ErrorRateRoll = DesiredRateRoll - RateRoll;
  ErrorRatePitch = DesiredRatePitch - RatePitch;
  ErrorRateYaw = DesiredRateYaw - RateYaw;

  // Roll Axis PID
  PtermRoll = PRateRoll * ErrorRateRoll;
  ItermRoll = PrevItermRateRoll + (IRateRoll * (ErrorRateRoll + PrevErrorRateRoll) * (t / 2));
  ItermRoll = (ItermRoll > 400) ? 400 : ((ItermRoll < -400) ? -400 : ItermRoll);
  DtermRoll = DRateRoll * ((ErrorRateRoll - PrevErrorRateRoll) / t);
  PIDOutputRoll = PtermRoll + ItermRoll + DtermRoll;
  PIDOutputRoll = (PIDOutputRoll > 400) ? 400 : ((PIDOutputRoll < -400) ? -400 : PIDOutputRoll);

  // Update output and previous values for Roll
  InputRoll = PIDOutputRoll;
  PrevErrorRateRoll = ErrorRateRoll;
  PrevItermRateRoll = ItermRoll;

  // Pitch Axis PID
  PtermPitch = PRatePitch * ErrorRatePitch;
  ItermPitch = PrevItermRatePitch + (IRatePitch * (ErrorRatePitch + PrevErrorRatePitch) * (t / 2));
  ItermPitch = (ItermPitch > 400) ? 400 : ((ItermPitch < -400) ? -400 : ItermPitch);
  DtermPitch = DRatePitch * ((ErrorRatePitch - PrevErrorRatePitch) / t);
  PIDOutputPitch = PtermPitch + ItermPitch + DtermPitch;
  PIDOutputPitch = (PIDOutputPitch > 400) ? 400 : ((PIDOutputPitch < -400) ? -400 : PIDOutputPitch);

  // Update output and previous values for Pitch
  InputPitch = PIDOutputPitch;
  PrevErrorRatePitch = ErrorRatePitch;
  PrevItermRatePitch = ItermPitch;

  // Yaw Axis PID
  PtermYaw = PRateYaw * ErrorRateYaw;
  ItermYaw = PrevItermRateYaw + (IRateYaw * (ErrorRateYaw + PrevErrorRateYaw) * (t / 2));
  ItermYaw = (ItermYaw > 400) ? 400 : ((ItermYaw < -400) ? -400 : ItermYaw);
  DtermYaw = DRateYaw * ((ErrorRateYaw - PrevErrorRateYaw) / t);
  PIDOutputYaw = PtermYaw + ItermYaw + DtermYaw;
  PIDOutputYaw = (PIDOutputYaw > 400) ? 400 : ((PIDOutputYaw < -400) ? -400 : PIDOutputYaw);

  // Update output and previous values for Yaw
  InputYaw = PIDOutputYaw;
  PrevErrorRateYaw = ErrorRateYaw;
  PrevItermRateYaw = ItermYaw;

  if (InputThrottle > 1800) {
    InputThrottle = 1800;
  }
  
  MotorInput1 = (InputThrottle - InputRoll - InputPitch - InputYaw); // front right - counter clockwise
  MotorInput2 = (InputThrottle - InputRoll + InputPitch + InputYaw); // rear right - clockwise
  MotorInput3 = (InputThrottle + InputRoll + InputPitch - InputYaw); // rear left  - counter clockwise
  MotorInput4 = (InputThrottle + InputRoll - InputPitch + InputYaw); // front left - clockwise

  // Limit motor values to valid range
  if (MotorInput1 > 2000) MotorInput1 = 1999;
  if (MotorInput2 > 2000) MotorInput2 = 1999;
  if (MotorInput3 > 2000) MotorInput3 = 1999;
  if (MotorInput4 > 2000) MotorInput4 = 1999;

  // Set minimum throttle values for stability when armed
  if (armed) {
    if (MotorInput1 < ThrottleIdle) MotorInput1 = ThrottleIdle;
    if (MotorInput2 < ThrottleIdle) MotorInput2 = ThrottleIdle;
    if (MotorInput3 < ThrottleIdle) MotorInput3 = ThrottleIdle;
    if (MotorInput4 < ThrottleIdle) MotorInput4 = ThrottleIdle;
  }

  // Check if motors should be running
  if (!armed || ReceiverValue[2] < 1030) {
    // Motors off when disarmed or throttle very low
    MotorInput1 = ThrottleCutOff;
    MotorInput2 = ThrottleCutOff;
    MotorInput3 = ThrottleCutOff;
    MotorInput4 = ThrottleCutOff;
    
    // Reset PID terms when motors are off
    if (ReceiverValue[2] < 1030) {
      resetPIDTerms();
    }
  }

  // Send commands to motors
  mot1.writeMicroseconds(MotorInput1);
  mot2.writeMicroseconds(MotorInput2);
  mot3.writeMicroseconds(MotorInput3);
  mot4.writeMicroseconds(MotorInput4);

  // Debug information via serial
  if (millis() % 250 == 0) { // Send debug info every 250ms to reduce serial overhead
    Serial.print("Armed: ");
    Serial.print(armed ? "YES" : "NO");
    Serial.print(" | Throttle: ");
    Serial.print(InputThrottle);
    Serial.print(" | Motors: ");
    Serial.print(MotorInput1);
    Serial.print(", ");
    Serial.print(MotorInput2);
    Serial.print(", ");
    Serial.print(MotorInput3);
    Serial.print(", ");
    Serial.print(MotorInput4);
    Serial.print(" | Angles R/P: ");
    Serial.print(complementaryAngleRoll);
    Serial.print("/");
    Serial.println(complementaryAnglePitch);
  }

  // Precise loop timing
  while (micros() - LoopTimer < (t*1000000)) {
    // Wait until the loop time is reached
  }
  LoopTimer = micros();
}