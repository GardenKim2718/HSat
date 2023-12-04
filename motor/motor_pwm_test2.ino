# define breakPin 8  // Connect the yellow break line to pin 8
# define motorPin 9  // Connect the green PWM line to pin 9
// # define directionPin 7 // Connect the blue direction line to pin 7

void setup() {
  pinMode(breakPin, OUTPUT);
  pinMode(motorPin, OUTPUT);

  digitalWrite(breakPin, HIGH);  // Set the brakePin to HIGH(Release)

  // Configure Timer 1 for 20kHz PWM
  TCCR1A = bit(COM1A1) | bit(WGM11);  // Clear OC1A on compare match, set at TOP    Timing System - Fast PWM mode
  TCCR1B = bit(WGM13) | bit(WGM12) | bit(CS10);  // Fast PWM, no prescaler
  ICR1 = 799;  // TOP value for 20kHz PWM    PWM_frequency = clock_speed / [Prescaler_value * (1 + TOP_Value)]

  Serial.begin(9600);
}

void loop() {
  if (Serial.available() > 0) {
    int dutyCycle = Serial.parseInt();

    dutyCycle = constrain(dutyCycle, 0, 799); // duty cycle value range

    OCR1A = dutyCycle; // Set the duty cycle to write OCR1A

    Serial.print("Duty Cycle set to: ");
    Serial.println(dutyCycle);
  }
  delay(100);
}


// 1st try
//
// void loop() {
//   analogWrite(motorPin, 200);
// }


// void loop() {
//   // Change the duty cycle
//   int dutyCycle = 799;  // Set duty cycle between 0 and 799

//   // Set the duty cycle by writing to OCR1A
//   OCR1A = dutyCycle;

//   delay(500);
// }


// void loop() {
//   // Drive the motor from 0 to 255 sequentially
//   for (int i = 0; i <= 255; i++) {
//     analogWrite(motorPin, i);
//     delay(10);
//   }

//   // Drive the motor from 255 to 0 sequentially
//   for (int i = 255; i >= 0; i--) {
//     analogWrite(motorPin, i);
//     delay(10);
//   }
// }
