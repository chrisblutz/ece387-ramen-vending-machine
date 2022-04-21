// Cole Hengstebeck ECE514 Group project: Ramen Vending Machine
// Seasoning hopper servo control

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
#define SERVO_FREQ 50

const uint16_t SERVO_MINS[] = {110, 127, 120}; //affects down position
const uint16_t SERVO_MAXS[] = {485, 485, 600}; //affectss up position

void setup() {
  // put your setup code here, to run once:
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(750);
  servo_turn_up(2);
  delay(750);
  servo_turn_down(2);
  
}

void servo_turn_up(int servo_num) {
  for (uint16_t pulselen = SERVO_MINS[servo_num]; pulselen < SERVO_MAXS[servo_num]; pulselen++) {
    pwm.setPWM(servo_num, 0, pulselen);
    //delay(1);
  }
}

void servo_turn_down(int servo_num) {
  for (uint16_t pulselen = SERVO_MAXS[servo_num]; pulselen > SERVO_MINS[servo_num]; pulselen--) {
    pwm.setPWM(servo_num, 0, pulselen);
    //delay(1);
  }
}
