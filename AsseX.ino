//////  ASSE X //////
///feed filo

#include <Wire.h>
#include <FastPID.h>
#include <TimerOne.h>
//#include <avr/wdt.h>

int debug = 1;

float Kp = 28, Ki = 0, Kd = 20.5, Hz = 10;				// debug
int output_bits = 16;									// impostazioni PID
bool output_signed = true;
FastPID myPID(Kp, Ki, Kd, Hz, output_bits, output_signed);
const int PidCorrection = 2580;							// costante aggiustamento output pid

volatile int setpoint = 0;								// di quanti click deve muoversi l'encoder

volatile int lastEncoded = 0;
volatile bool run = 0;									// gira o non gira motore
volatile long encoderValue = 0;

long lastencoderValue = 0;

int lastMSB = 0;
int lastLSB = 0;

bool check = 0;

///////////  definizione pin motore
const byte ENA = 7;										//pin enable
const byte DIR = 8;										//pin direzione
const byte STEPpin = 9;									//pin impulsi

//////////// definizione pin encoder
int encoderPin1 = 2;
int encoderPin2 = 3;

/////////// definizione duty cycle e frequenza pwm motore
int DUTY = 300;
int PWM = 80;											//(numero basso frequenza alta)

/////////// definizione indirizzo I2c
int I2cAddress = 11;
int I2cAddressMaster = 10;


void setup() {
	///////////   verifica debug
	pinMode(13, OUTPUT);
	Serial.begin(250000);

	//Wait for two seconds or till data is available on serial, 
	//whichever occurs first.
	while (Serial.available() == 0 && millis()<2000);
	if (Serial.available()>0) {
				//If data is available, we enter here.
		int test = Serial.read();			//We then clear the input buffer
		Serial.println("DEBUG");			//Give feedback indicating mode
		debug = 1;							//Enable debug
		digitalWrite(13, HIGH);
	}
	else {
		Serial.end();
		digitalWrite(13, LOW);
	}

	///////////   setup I2c
	Wire.begin(I2cAddress);					// join i2c bus with address #2
	Wire.onReceive(receiveEvent);			// register event
//	TWCR = 0;								// reset TwoWire Control Register to default, inactive state

	///////////////    setup encoder
	pinMode(encoderPin1, INPUT_PULLUP);
	pinMode(encoderPin2, INPUT_PULLUP);
	attachInterrupt(0, updateEncoder, CHANGE);
	attachInterrupt(1, updateEncoder, CHANGE);

	////////////////   setup pid
	myPID.setOutputRange(80, 2500);

	///////////// setup uscite motore
	pinMode(DIR, OUTPUT);					//direzione
	pinMode(ENA, OUTPUT);					//enable (attivo basso)
	digitalWrite(ENA, HIGH);				//disattiva motore
	digitalWrite(DIR, HIGH);				//imposta senso rotazione
	Timer1.initialize();
	Timer1.pwm(STEPpin, DUTY, PWM);			//inizzializza treno impulsi stepper 
	Timer1.stop();							//ferma treno impulsi

	////////////  debug
	if (debug == 1) {
		if (myPID.err()) {
			Serial.println("There is a PID configuration error!");
			for (;;) {}
		}
	}

}

void loop() {

	///////// calcolo con PID velocit� motore e la imposto in PWM
	uint16_t output = myPID.step(setpoint, encoderValue);
	PWM = (PidCorrection + (-output));

	if (run == 1){
      Timer1.pwm(STEPpin, DUTY, PWM);
	  ////////////  debug
	  if (debug == 1) {
		//  Serial.println(PWM);
		  Serial.println(encoderValue);
	  }
	}
	

	if (encoderValue >= setpoint && check == 0) {
		Timer1.stop();
		detachInterrupt(encoderPin1);
		detachInterrupt(encoderPin2);
		run = 0;
		setpoint = 0;
		encoderValue = setpoint;
		Wire.begin(I2cAddress);					// join i2c bus with address #2
		Wire.onReceive(receiveEvent);			// register event
		I2cSendOK();
		check = 1;

	}

	
}

void updateEncoder() {

	int MSB = digitalRead(encoderPin1);			//MSB = most significant bit
	int LSB = digitalRead(encoderPin2);			//LSB = least significant bit

	int encoded = (MSB << 1) | LSB;				//converting the 2 pin value to single number
	int sum = (lastEncoded << 2) | encoded;		//adding it to the previous encoded value

	if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue++;
	if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue--;

	lastEncoded = encoded;						//store this value for next time
}

void receiveEvent(int rcve) {
//////ricevo il nuovo setpoint via I2c
	check = 0;
	setpoint = 0;
	encoderValue = setpoint;
	int iRXVal;
	if (Wire.available() >= 2)                  // Make sure there are two bytes.
	{
		
		for (int i = 0; i < 2; i++)             // Receive and rebuild the 'int'.
			iRXVal += Wire.read() << (i * 8);   //    "     "     "     "    "
		
	}

	int x = iRXVal;								// receive byte as an integer

	////////////  debug
	if (debug == 1) {
	   Serial.println("ricevo via I2c");
	   Serial.println(iRXVal);					// Print the result.
	   Serial.println(x);						// print the integer
	}
	if (x == -32767) {
		///////////////  codice da eseguire quando ricevo via i2c un nuovo setpoint
		resetController();
	}

	if (x == -32768)
	{
		//////////////  codice da eseguire quando ricevo via I2c -32768
		digitalWrite(ENA, HIGH);				//disattiva motore
		I2cSendOK();
	}
	if (x > -32760){
		///////////////  codice da eseguire quando ricevo via i2c un nuovo setpoint
		setpoint = x;
		attachInterrupt(0, updateEncoder, CHANGE);
		attachInterrupt(1, updateEncoder, CHANGE);
		TWCR = 0;								// reset TwoWire Control Register to default, inactive state
		run = 1;
		digitalWrite(ENA, LOW);					//attiva motore
	}

	
	
}

void I2cSendOK() {
	//////invio -555 / comando eseguito via I2c
	int x = -555;
	Wire.beginTransmission(I2cAddressMaster);					// transmit to Master
	Wire.write((byte*) & x , 2);				// Transmit the 'int', one byte at a time.
	Wire.endTransmission();						// stop transmitting
	////////////  debug
	if (debug == 1) {
		Serial.println("inviato ok (-555)");
	}
}

void resetController() {
	wdt_disable();
	wdt_enable(WDTO_15MS);
	while (1) {}
}