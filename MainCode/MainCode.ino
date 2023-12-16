#include <Wire.h>
//A4, A5 pin designated for MPU6050 I2C connection
//pin config: Vcc-3.3V SCL-A5 SDA-A4
#include <TinyMPU6050.h>
#include <SoftwareSerial.h>

MPU6050 mpu (Wire);//attach MPU6050 library to Wire.h

#define MPU6050     0x68 // MPU6050 address
#define BT_RX       12 //HM-10 TX pin num
#define BT_TX       13 //HM-10 RX pin num
#define IN3         6 //Motor Rotation Direction
#define IN4         7 //Motor Rotation Direction
#define PWM_pin     9 //Motor PWM pin
#define CDS1
#define CDS2
//include more pin numbers
const int16_t accelY_offset;
const int16_t gyroZ_offset;
unsigned int op_mode;
volatile float angle;
volatile float speed;
volatile float set_speed;
volatile float set_angle;
float cumulated_error;
const float err_ref = 0.25;//reference value for deciding steady-state
//unsigned long now;
const float Kp_v = 8.0;//P controller Gain for velocity
const float Ki_v = 0.5;//I controller Gain for velocity
const float Kp_a = 12.0;//P controller Gain for angle
const float Ki_a = 5.0;//I controller Gain for angle
int counter;//counter for selective PI control system
const int analogPins[] = {0, 1};                                       //define each analogue pin
const int numPins = sizeof(analogPins) / sizeof(analogPins[0]);        //define size of analogue pin
uint16_t Pin0 = 0;   //0번 핀 값 정의
uint16_t Pin1 = 0;   //1번 핀 값 정의
bool orientation_flag;//bool flag for whether system is oriented to set_angle
bool control_mod;//bool flag for whether system is in velocity control or angle control
//true: velocity  false: angle

SoftwareSerial BTSerial(BT_RX, BT_TX);//BTSerial: HM10 comm

void setup() {
  // put your setup code here, to run once:
  //set register
  
  //initializing MPU6050
  mpu.Initialize();
  //set pin mode
  
  //set motor to brake(STOP)
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, HIGH);
  delay(3000);//wait until motor stops
  //initiating Bluetooth Comm
  BTSerial.begin(38400);//Bluetooth HM10 Baudrate: 38400
  BTSerial.println("Sensor Calibration");
  BTSerial.println("DO NOT MOVE");
  mpu.Calibrate ();//calibrating MPU6050 for sensor offset value
  BTSerial.println("Operational");
  
  //initializing variables
  set_angle=0.0;
  set_speed = 0.0;
  cumulated_error = 0.0;
  op_mode = 0;//default set op_mode: Stabilization
  orientation_flag = false;
  counter = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  bool comm_flag = false;//bool flag for whether
  int command;
  if(BTSerial.available()){//get operation mode and set rpm from HM10
    String data = BTSerial.readStringUntil('\n');
    command = data.toInt();
    comm_flag = true;
  }
  if(comm_flag){//set operation mode according to command
      if(abs(command)<=180.0){
        op_mode = 1;
        set_angle = command;//get set angle
        BTSerial.print("Rotating to orientation:");
        BTSerial.println(set_angle);
      }
    else if(command == 20000){//mode0: stabilization
      op_mode=0;
      BTSerial.print("Stabilize");
      comm_flag = false;
    }
    else if(command == 25000){//mode1: moving to previous set_angle
      op_mode=1;
      orientation_flag = false;
      BTSerial.print("Rotating to orientation:");
      BTSerial.println(set_angle);
    }
    else if(command == 30000){//mode2: solar tracking
      op_mode=2;
      BTSerial.println("Tracking Sun");
    }
    else if(abs(command)<=180.0){//mode1: moving to set_angle
      op_mode = 1;
      set_angle = command;//get set angle
      BTSerial.print("Rotating to orientation:");
      BTSerial.println(set_angle);
    }
    else BTSerial.print("COMM FAIL");

    comm_flag = false;
  }

  //execute command for each mode
  if(op_mode == 0){
    stabilization();
  }
  else if(op_mode == 1){
    orientation();
  }
  else if(op_mode == 2){
    SolarTrack();
  }
}
//PI control with Anti-windup method
void PIcontrol(float setpoint, float currentvalue){
  float feedback;
  float error = setpoint - currentvalue;
  //float r_speed = abs(mpu.GetAngGyroZ())*3.14159/180;
  if(abs(error)<err_ref) counter ++;
  else counter = 0;//reset counter during transiet response

  if(counter>20){//start Integrator if entered steady-state
    cumulated_error += error;
  }

  if((counter>40)&(error<0.3)) orientation_flag = true;//whether set value is achieved

  else cumulated_error = 0;//reset integrator during transient response
  if(!control_mod){//velocity control mod
    feedback = Kp_v * error + constrain(Ki_v * cumulated_error,-60,60);//PIcontrol feedback value constrained
  }
  else{
    feedback = Kp_a * error + constrain(Ki_a * cumulated_error,-60,60);//PIcontrol feedback value
  }
  //need function to compensate NLD
  int pwm = constrain(abs(feedback),0,255);
  if(feedback>=0){
    digitalWrite(IN3, LOW); //CW rotation
    digitalWrite(IN4, HIGH);
    analogWrite(PWM_pin,pwm);
  }
  else{
    digitalWrite(IN3, HIGH); //CCW rotation
    digitalWrite(IN4, LOW);
    analogWrite(PWM_pin,pwm);//write absolute value of PWM into PWM pin
  }
  BTSerial.print(angle);
  BTSerial.print(", ");
  BTSerial.print(speed);
  BTSerial.print(", ");
  BTSerial.println(pwm);
}

void stabilization(){//stabilization mode function
  set_speed = 0;
  Update_MPU();
  control_mod = false;
  PIcontrol(set_speed, speed);
}

void orientation(){//constant RPM mode function
  //should update to set orientation_flag if orientation is complete
  Update_MPU();
  control_mod = true;
  PIcontrol(set_angle, angle);
}

void SolarTrack(){//solar tracking mode function
  init_CDS_ADC();
  Update_MPU();
  int max_illuminance = 0;//maximum illuminance
  int illuminance;
  float origin_angle = angle;
  float sun_orientation;//angle of maximum illuminance(=angle of sun)
  bool orientation_flag = false;//bool flag for whether system is oriented to the sun
  set_speed = 10;
  while(abs(angle-origin_angle)<0.5){//rotate 360 degree and find maximum illuminance angle
    Update_MPU();
    PIcontrol(set_speed, speed);
    illuminance = measure_CDS();
    if(illuminance>max_illuminance){
      max_illuminance = illuminance;
      sun_orientation = angle;
    }
  }
  set_angle=sun_orientation;
  orientation_flag = false;
  while(!(orientation_flag)){
    orientation();
  }
  BTSerial.println("Orientated to Sun");
}

void Update_MPU(){//fetch speed & angle from MPU6050
  mpu.Execute();
  speed = mpu.GetGyroZ();//speed in deg/sec
  angle = mpu.GetAngZ();//angle in deg
}

void init_CDS_ADC(){                                     //initiation for CDS ADC
  ADMUX |= (0<<REFS1) | (1<<REFS0);
  ADMUX |= (0<<ADLAR);                                   //8bit resolution
  ADCSRA |= (1<<ADEN);                                   //enable ADC
  ADCSRA |= (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);        //Prescaler=128
}

int measure_CDS(){//function to measure CDS_value
  //measure average of two bottom mounted CDS sensor
  //in the future, should come up with an algorithm to preclude when top mounted CDS sensor measures over certain value
  //(=interference detected)
  uint16_t Pin0, Pin1;
    for (int i =0; i < numPins; i++){
    ADMUX = (ADMUX & 0xF0) | (analogPins[i] & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    uint16_t value = ADC;
    if (i == 0) {
      Pin0 = value;
    }
    else if (i == 1){
      Pin1 = value;
      } 
    }  
  return (Pin0 + Pin1) / 2;
}
