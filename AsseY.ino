////////  ASSE Y ////////////

#include <Wire.h>        //modified
#include <Adafruit_ADS1015.h> // modified
#include <FastPID.h>
#include <TimerOne.h>
#include <SimpleKalmanFilter.h>
//#include <avr/wdt.h>

bool debug = 0;               // debug

							  ////// simple kalman filter to remove noise from angle sensor
							  /*  SimpleKalmanFilter(e_mea, e_est, q);
							  e_mea: Measurement Uncertainty
							  e_est : Estimation Uncertainty
							  q : Process Noise
							  */
SimpleKalmanFilter simpleKalmanFilter(2, 2, 0.1);  /// to be setted


												   ////// ADS1115
												   ////// adafruit library is edited to set sample rate of ADS1115 to 860Hz 
												   ////// Wire library is edidited for a frequency of 400kHz
Adafruit_ADS1115 ads(0x48);         // Use this for the 16-bit version 
int IngressoADC = 0;            // imposta canale adc sull'ADS1115

								// PID settings (to be modified)
float Kp = 15, Ki = 0, Kd = 0, Hz = 200;
int output_bits = 16;
bool output_signed = true;
FastPID myPID(Kp, Ki, Kd, Hz, output_bits, output_signed);
const int PidCorrection = 2580;       // PID correction output

									  // variables definition
volatile int setpoint = 0;          // where my motor has to move to
volatile uint16_t distance;         // distance from setpoint to target in circle calculation
const unsigned int diff_error = 10;     // max accattable error for hold or positioning. (to be modified)
bool check = 0;               // ramp up check

volatile bool hold = 0;           // hold in position or move
volatile bool directionChange = 0;      // serves to verify if direction has changed
bool direzione = 0;             // direction of stepper

volatile uint16_t sensorValueUnfiltered = 0;// sensor reading
volatile uint16_t sensorValue = 0;          // sensor reading filtered
volatile uint16_t sensorMapped = 0;     // sensor reading filtered

uint16_t sensorMAX = 0;           // max reading of sensor value
uint16_t sensorMIN = 32767;         // max reading of sensor value


									///////////  motor pin definition
const byte ENA = 7;             // enable pin
const byte DIR = 8;             // direction pin 
const byte STEPpin = 9;           // pulses pin

								  /////////// duty cycle and motor frequency
const unsigned int DUTY = 300;
unsigned int PWM = 140;               //(lower number higher frequency)

									  /////////// I2c address definition
int I2cAddress = 12;
int I2cAddressMaster = 10;

void setup(void)
{
	///////////  enter the debug? 
	pinMode(13, OUTPUT);
	Serial.begin(250000);

	//Wait for two seconds or till data is available on serial, 
	//whichever occurs first.
	while (Serial.available() == 0 && millis()<2000);
	if (Serial.available()>0) {
		//If data is available, enter here.
		int test = Serial.read(); // Clear the input buffer
		Serial.println("DEBUG");  // Give feedback indicating mode
		debug = 1; // Enable debug
		digitalWrite(13, HIGH);
	}
	else {             // disable debug
		Serial.end();
		digitalWrite(13, LOW);
	}

	///////////   setup I2c
	Wire.begin(I2cAddress);         // join i2c bus with address #2
	Wire.onReceive(receiveEvent);     // register event function

									  ////////////////  setup pid
	myPID.setOutputRange(250, 1900);

	///////////// setup stepper output
	pinMode(DIR, OUTPUT);         //direction
	pinMode(ENA, OUTPUT);         //enable (active low)
	digitalWrite(ENA, HIGH);        //motor off
	digitalWrite(DIR, HIGH);        //set direction
	Timer1.initialize();
	Timer1.pwm(STEPpin, DUTY, PWM);     //initialize stepper pulses (to be removed)
	Timer1.stop();              //stop stepper pulses

								////////////  debug
	if (debug == 1) {
		if (myPID.err()) {
			Serial.println("There is a configuration error!");
			for (;;) {}
		}
		Serial.println("Getting single-ended readings from AIN0");
		Serial.println("ADC Range: 2/3x gain +/- 6.144V  1 bit = 0.1875mV");
	}

	/////////// setup ADS1115
	ads.setGain(GAIN_TWOTHIRDS);  // set adc to 2/3
	ads.begin();

	/////////// find min ad max sensor values rotating at low speed
	digitalWrite(ENA, LOW);
	Timer1.pwm(STEPpin, DUTY, 4500);
	for (int i = 0; i < 32200; i++) {
		CalibrazioneSensore();
	}
	I2cSendOK();
	digitalWrite(ENA, HIGH);    //motor off
								////////////  debug
	if (debug == 1) {
		Serial.println("valore massimo sensore");
		Serial.println(sensorMAX);
		Serial.println("valore minimo sensore");
		Serial.println(sensorMIN);
	}
}

void loop(void) {
	// hold value is established in receiveEvent() and modified in moveToTarget()
	if (hold == 0) {
		moveToTarget();
	}
	else {
		holdPosition();
	}
}

////  receive new setpoint
void receiveEvent(int rcve) {
	////  this function does: recive via i2c 3 bytes. first two are setpoint,
	////  third is to set for direction or let go to the shortest route.

	////// receive new setpoint and direction decision via I2c (3 bytes)
	int iRXVal;
	int iRYVal;
	bool shortestRoute;
	if (Wire.available() >= 2) {      //CAMBIATO NUMERO BYTE TEST//            // Make sure there are tree bytes.
		for (int i = 0; i < 2; i++) {            // Receive and rebuild the integer
			iRXVal += Wire.read() << (i * 8);
		}
		  iRYVal += Wire.read() << (3 * 8);   // the third byte if is equal to 255 will let the software decide the move direction
	}
	int x = iRXVal;               // receive byte as an integer
	if (iRYVal == 1) {
		shortestRoute = 1;
	}
	else {
		shortestRoute = 0;
	}
	//////////////  debug
	if (debug == 1) {
		Serial.println("ricevo via I2c ");
		Serial.print("iRXVal "); Serial.println(iRXVal);                 // Print the result.
		Serial.print("received data x "); Serial.println(x);            // print the integer
																		//  Serial.print("received data y "); Serial.println(y);
	}
	//////////////  what to do if I received a new value via I2c
	/////////  if I receive this -32768 via I2c disable motor
	if (x == -32768) {
		digitalWrite(ENA, HIGH);        //disattiva motore
		I2cSendOK();
		return;
	}

	if (x == -32767) {
		///////////////  codice da eseguire quando ricevo via i2c un nuovo setpoint
		resetController();
	}

	if (x > -32760) {
	///////////////  0 will mean don't move so hold position.
	if (x == 0) {
		digitalWrite(ENA, LOW);         //enable motor
		hold = 1;
		I2cSendOK();
		return;
	}
	//////////////  if Y is different than 255 the user is setting the direction of rotation
	if (shortestRoute == 0) {
		digitalWrite(ENA, LOW);         //enable motor
										//////////  positive and negative numbers are used to set the direction of rotation
										// set direction of rotation and new sepoint
		if (x > 0) {
			direzione = 1;      // direction established from sign
			setpoint = abs(x);    // new setpoint
			hold = 0;       // move to setpoint   
			return;
		}
		if (x < 0) {
			direzione = 0;      // direction established from sign
			setpoint = abs(x);    // new setpoint 
			hold = 0;       // move to setpoint 
			return;
		}
	}
	//// if Y equals to 255, set new setpoint and find the shortest route to setpoint 
	// find direction of rotation and new sepoint
	if (shortestRoute == 1) {
		setpoint = abs(x);                  // new setpoint 
		readSensor();                   // read filter map sensor
		direzione = FindTurnSide(sensorMapped, setpoint); // find shortest route
		hold = 0;                     // move to setpoint 
		return;
	}
	}
}

////  move to the target
void moveToTarget() {

	while (hold == 0) {

		readSensor();       ///  read sensor, map from 0-3600, filter
							///  calculate distance from target  
							//// to be checked: direction might be reversed

		if (direzione != directionChange) {
			directionChange = direzione;
			digitalWrite(DIR, direzione);
		}
		distance = ShorterDistance(sensorMapped, setpoint);

		///////// find PWM output for stepper using PID

		//PWM = (PidCorrection + (-output));
		uint16_t output = myPID.step(150, distance);
		if (output <= PWM && check == 0) {
			PWM = PWM - 6;
		}
		else {
			check = 1;
			PWM = output;
		}


		////////  set speed
		Timer1.pwm(STEPpin, DUTY, PWM);
		////////////  debug
		if (debug == 1) {
			Serial.print("distanza "); Serial.println(distance);
			Serial.print("PWM "); Serial.println(PWM);
			//Serial.print("sensorValueUnfiltered "); Serial.println(sensorValueUnfiltered);
			//Serial.print("sensorValue "); Serial.println(sensorValue);
			//Serial.print("sensorMapped "); Serial.println(sensorMapped);
		}

		//if (sensorMapped <= setpoint - diff_error || sensorMapped >= setpoint + diff_error) {
		if (distance <= 7) {
			/////////////// condition to be checked
			/////////////// what should I do if motor has reached target
			Timer1.stop();
			I2cSendOK();
			hold = 1;
			if (debug == 1) {
				Serial.print("PWM TARGET REACHED "); Serial.println(PWM);
				Serial.print("sensorMapped  after hold"); Serial.println(sensorMapped);
			}
			check = 0;
		}
	}
}
////  hold position like a servo
void holdPosition() {
	readSensor();
	direzione = FindTurnSide(sensorMapped, setpoint);
	distance = ShorterDistance(sensorMapped, setpoint);
	if (distance > 3) {  // vesione 1

						 //run motor
		digitalWrite(DIR, direzione);
		Timer1.pwm(STEPpin, DUTY, 1900);
		CalibrazioneSensore();  // keep veryfy if sensor has new range of values
		if (debug == 1) {
			Serial.print("PWM  While HOLDPOSITION "); Serial.println(PWM);
			//Serial.print("sensorMapped  while holding "); Serial.println(sensorMapped);
		}
	}
	Timer1.stop();  // stop pulses to motor
}

//// send setpoint reached - movement completed (-555 / 2 bytes)
void I2cSendOK() {
	////// send -555 / setpoint reached
	int x = -555;
	Wire.beginTransmission(I2cAddressMaster);        // transmit to device  (master)
	Wire.write((byte*)& x, 2);        // Transmit the 'int', one byte at a time.
	Wire.endTransmission();         // stop transmitting
									////////////  debug
	if (debug == 1) {
		Serial.println("inviato ok (-555)");
	}
}

////  calibrate minimum and maximum sensor values with raw sensor values
void CalibrazioneSensore() {
	sensorValueUnfiltered = ads.readADC_SingleEnded(IngressoADC);
	if (sensorValueUnfiltered > sensorMAX) {
		sensorMAX = sensorValueUnfiltered;
	}
	if (sensorValueUnfiltered < sensorMIN) {
		sensorMIN = sensorValueUnfiltered;
	}
}

////  read sensor, filter, map from 0 to 3600 -- sensorMapped is the output global variable
void readSensor() {
	sensorValueUnfiltered = ads.readADC_SingleEnded(IngressoADC);
	sensorValue = simpleKalmanFilter.updateEstimate(sensorValueUnfiltered);
	sensorMapped = map(sensorValue, sensorMIN, sensorMAX, 0, 3600);
}


//// calculate shorter distance between two angles
int ShorterDistance(int x, int y) {
	int z = x - y;
	int a = abs(z);
	int b = a - 1800;
	return 1800 - abs(b);
}

/// find if shorter distance is clockwise or anticlockwise between two angles
bool FindTurnSide(int current, int target)
{
	int diff = target - current;
	if (diff < 0)
		diff += 3600;
	if (diff > 1800)
		return 0;
	else
		return 1;
}

// sum/subtraction of angles, used to calculate distance from target
uint16_t SumSubtraction(int x, int m) {
	int r = x%m;
	return r<0 ? r + m : r;
}

void resetController() {
	wdt_disable();
	wdt_enable(WDTO_15MS);
	while (1) {}
}