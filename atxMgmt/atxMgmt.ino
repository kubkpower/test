/*
 Name:		atxMgmt.ino
 Created:	22/02/2018 20:42:26
 Author:	kevin
 
 Code pour carte de management imprimante 3D DiscoEasy200
 
 Gestion de bouton en façade pour l'allumage de l'alimentation ATX
 Le bouton demande aussi l'arrêt propre du raspberry (octoprint)
 Attente du retour du raspberry (ou timeout) pour arrêt de l'alimentation
 
 */

#include "resources.h"
//#define DEBUG

// Constants
const byte PSU_SWITCH_PIN = 13;             // Connected to the ATX GREEN CABLE (LOW enables the PSU)
const byte OUT_SHUTDOWN_REQUEST_PIN = 12;   // To the RPI to request proper shutdown
const byte IN_STATUS_PIN = 3;              // TO the RPI (LOW=RPi not ready) / pin 2 or 3 for IRQ
const byte POWER_BTN_PIN = 2;               // pin 2 or 3 for IRQ
const byte RGB_RED_PIN = 7;
const byte RGB_GREEN_PIN = 6;
const byte RGB_BLUE_PIN = 5;

const unsigned long SHUTDOWN_TIMEOUT = 240000; //the timeout in milliseconds
const int DEBOUNCE_TIME = 15;                 // ms
const int BLINK_SPEED = 500;                  // ms

// Variables

bool psu_state_on = false;
bool rpi_started = false;
bool enable_blink = false;

unsigned long startingTime = 0; //used to store the start of shutdown request
unsigned long blinkTime = 0;

// Need to be volatile to be accessible from IRQ function
volatile unsigned long lastBtnPressed = 0;
volatile unsigned long lastRpiStateReport = 0;
volatile bool btn_pressed = false;
volatile bool rpi_status_triggered = false;

// Managing RGB LED
ledColor currentColor = ledColor::NONE;
bool ledOn = true;

// Routine to set the front LED color (RGB LED)
void setLedColor(ledColor color) {
	switch (color) {
	case ledColor::NONE:
		digitalWrite(RGB_RED_PIN, LOW);
		digitalWrite(RGB_GREEN_PIN, LOW);
		digitalWrite(RGB_BLUE_PIN, LOW);
		break;
	case ledColor::RED:
		digitalWrite(RGB_GREEN_PIN, LOW);
		digitalWrite(RGB_BLUE_PIN, LOW);
		digitalWrite(RGB_RED_PIN, HIGH);
		currentColor = ledColor::RED;
		break;
	case ledColor::BLUE:
		digitalWrite(RGB_RED_PIN, LOW);
		digitalWrite(RGB_GREEN_PIN, LOW);
		digitalWrite(RGB_BLUE_PIN, HIGH);
		currentColor = ledColor::BLUE;
		break;
	case ledColor::GREEN:
		digitalWrite(RGB_RED_PIN, LOW);
		digitalWrite(RGB_BLUE_PIN, LOW);
		digitalWrite(RGB_GREEN_PIN, HIGH);
		currentColor = ledColor::GREEN;
		break;
	}
}

// Set LED Color according to actions & states
void updateLedStatus() {
	// Update LED status
	if (psu_state_on) {
		if (rpi_started) {
			setLedColor(ledColor::GREEN);
		}
		else {
			setLedColor(ledColor::RED);
		}
	}
	else {
		setLedColor(ledColor::BLUE);
	}
}

// Routine to manager LED blinking
void blinkLed() {
	if ((enable_blink) && ((millis() - blinkTime) > BLINK_SPEED)) {
		if (ledOn) {
			setLedColor(ledColor::NONE);
		}
		else {
			setLedColor(currentColor);
		}
		ledOn = !ledOn;
		blinkTime = millis();
	}
}

void setup() {
	#ifdef DEBUG
		Serial.begin(9600);
	#endif
	// Prepare pin state
	pinMode(PSU_SWITCH_PIN, OUTPUT);
	pinMode(OUT_SHUTDOWN_REQUEST_PIN, OUTPUT);

	pinMode(IN_STATUS_PIN, INPUT_PULLUP);
	pinMode(POWER_BTN_PIN, INPUT_PULLUP);

	pinMode(RGB_RED_PIN, OUTPUT);
	pinMode(RGB_GREEN_PIN, OUTPUT);
	pinMode(RGB_BLUE_PIN, OUTPUT);

	// System starts with PSU OFF
	digitalWrite(PSU_SWITCH_PIN, HIGH);
	// No shutdown request
	digitalWrite(OUT_SHUTDOWN_REQUEST_PIN, HIGH);
	// Bind interrupt to power button
	attachInterrupt(digitalPinToInterrupt(POWER_BTN_PIN), btnPress, RISING);
	// Bind interrupt to rpi state pin (RISING means RPI is completed started. FALLING means RPI is properly shut.)
	attachInterrupt(digitalPinToInterrupt(IN_STATUS_PIN), rpiState, CHANGE);
	#ifdef DEBUG
		Serial.println("Initialization complete");
	#endif 
	updateLedStatus();

}

void loop() {

	if (btn_pressed) {
		// reset btn state
		btn_pressed = false;
		if (!psu_state_on) {
			// Start PSU
			digitalWrite(PSU_SWITCH_PIN, LOW);
			psu_state_on = true;
			#ifdef DEBUG
				Serial.println("PSU Started");
			#endif
			enable_blink = false;
		}
		else {
			// Request proper shutdown to RPI
			digitalWrite(OUT_SHUTDOWN_REQUEST_PIN, LOW);
			// start timeout counter
			startingTime = millis();
			#ifdef DEBUG
				Serial.println("Starting timeout");
			#endif
			enable_blink = true;
		}
		updateLedStatus();
	}

	// Shutdown requested because timeout is running
	if (startingTime >0) {
		// Is timeout reached ?
		if ((millis() - startingTime) > SHUTDOWN_TIMEOUT) {
			// Abruptly shut PSU OFF
			digitalWrite(PSU_SWITCH_PIN, HIGH);
			psu_state_on = false;
			enable_blink = false;
			startingTime = 0;
			#ifdef DEBUG
				Serial.println("Shutdown timeout reached. Stopping PSU");
			#endif
			updateLedStatus();
		}
		// is RPI properly shut (or still not completely up) ?
		if (!rpi_started) {
			// shut PSU OFF
			digitalWrite(PSU_SWITCH_PIN, HIGH);
			psu_state_on = false;
			enable_blink = false;
			startingTime = 0;
			#ifdef DEBUG
				Serial.println("Stopping PSU.");
			#endif
			updateLedStatus();
		}
	}

	if (rpi_status_triggered) {
		rpi_status_triggered = false;
		if (digitalRead(IN_STATUS_PIN) == HIGH) {
			rpi_started = true;
			#ifdef DEBUG
				Serial.println("RPI startup completed.");
			#endif
		}
		else {
			rpi_started = false;
			#ifdef DEBUG
				Serial.println("RPI shutdown completed.");
			#endif
		}
		updateLedStatus();
	}

	blinkLed();

}


// Interrupt handler for power button
void btnPress() {
	// Debounce for inteferences (a few ms)
	if ((millis() - lastBtnPressed) > DEBOUNCE_TIME) {
		lastBtnPressed = millis();
		#ifdef DEBUG
			Serial.println("BTN INTERRUPT TRIGGERED !");
		#endif
		btn_pressed = true;
	}
}

// Interrupt handler for RPI state change
void rpiState() {
	// Debounce for inteferences
	if ((millis() - lastRpiStateReport) > DEBOUNCE_TIME) {
		lastRpiStateReport = millis();
		#ifdef DEBUG
			Serial.println("RPI INTERRUPT TRIGGERED !");
		#endif
		rpi_status_triggered = true;
	}
}


