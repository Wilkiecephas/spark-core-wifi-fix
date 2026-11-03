/*
 * Kyambogo University - Dept of Electrical & Electronic Engineering
 * TETE 3102 Microcontrollers and Applications
 * Task 7: Arduino Uno I2C Slave (Address 0x08)
 * Authors: Group 4 (Mukama Robert, Nabudere Zerubabel, Nanfuka Shakirah, Kidega Wilkie Cephas)
 *
 * Receives: Command byte from STM32 Master to control Arduino pin 13 LED.
 * Sends: 2 bytes representing 10-bit analog reading from Potentiometer (A0).
 */

#include <Wire.h>

#define SLAVE_ADDRESS 0x08
#define POT_PIN       A0
#define LED_PIN       13

volatile uint16_t latestADC = 0;
volatile uint8_t  ledState  = 0;

void requestEvent() {
  // Read current analog value
  latestADC = analogRead(POT_PIN);
  uint8_t msb = (latestADC >> 8) & 0xFF;
  uint8_t lsb = latestADC & 0xFF;

  // Transmit 2 bytes back to Master
  Wire.write(msb);
  Wire.write(lsb);
}

void receiveEvent(int bytes) {
  while (Wire.available()) {
    ledState = Wire.read();
    digitalWrite(LED_PIN, (ledState == 0x01) ? HIGH : LOW);
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(SLAVE_ADDRESS);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  Serial.begin(115200);
  Serial.println("Arduino Uno I2C Slave initialized at 0x08");
}

void loop() {
  delay(100);
}
