///////  Mega 2560
///////  Unit� centrale
///////  Ricevo da Pc i comandi, trasmetto ai rispettivi assi, attendo segnale di eseguito dall'asse, trasmetto ok al PC

#include <Wire.h>

const byte numChars = 32;
char receivedChars[numChars]; // an array to store the received data
char tempChars[numChars];        // temporary array for use by strtok() function
      // variables to hold the parsed data
char messageFromPC[numChars] = {0};
int integerFromPC = 0;
boolean newData = false;
boolean startY = 0;
boolean startZ = 0;

int iRXVal = 0;

/////////// definizione indirizzo I2c
int I2cAddress = 10;
int I2cAddressX = 11;
int I2cAddressY = 12;
int I2cAddressZ = 13;

void setup() {
  ///////////   Attivo porta seriale
	pinMode(2, OUTPUT);
	pinMode(3, OUTPUT);
	digitalWrite(2, LOW);
	digitalWrite(3, LOW);
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
    Serial.println("This demo expects 2 pieces of data - text, an integer and a floating point value");
    Serial.println("Enter data in this style <text,12>  ");
    Serial.println();
  ///////////   setup I2c
  Wire.begin(I2cAddress);         // join i2c bus with address 
  Wire.onReceive(receiveEvent);     // register event
  Serial.println("starting up");
  digitalWrite(2, HIGH);
  while (iRXVal != -555)
  {
  }
  iRXVal = 0;
  digitalWrite(3, HIGH);
  while (iRXVal != -555)
  {
  }
}



void loop() {

 recvWithStartEndMarkers();
    if (newData == true) { 
        strcpy(tempChars, receivedChars);
            // this temporary copy is necessary to protect the original data
            //   because strtok() replaces the commas with \0
        parseData(); 
		showParsedData();
		if (messageFromPC[0] != 'n') {
			routeData();
		}
        
        newData = false;
    }
	}

//============


void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;

    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

//============

void parseData() {

      // split the data into its parts
    char * strtokIndx; // this is used by strtok() as an index

    strtokIndx = strtok(tempChars,",");      // get the first part - the string
    strcpy(messageFromPC, strtokIndx); // copy it to messageFromPC
  
    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    integerFromPC = atoi(strtokIndx);     // convert this part to an integer

    //strtokIndx = strtok(NULL, ",");
    //floatFromPC = atof(strtokIndx);     // convert this part to a float

}

//============

void showParsedData() {
    Serial.print("Message ");
    Serial.println(messageFromPC);
    Serial.print("Integer ");
    Serial.println(integerFromPC);
   // Serial.print("Float ");
   // Serial.println(floatFromPC);
}

//============

void routeData() {
	if (messageFromPC[0] == 's'){
		sendCommands2byte(-32768, I2cAddressX);
		sendCommands2byte(-32768, I2cAddressY);
		sendCommands2byte(-32768, I2cAddressZ);
		messageFromPC[0] = 'n';
	}
	if (messageFromPC[0] == 'r') {
		sendCommands2byte(-32767, I2cAddressX);
		sendCommands2byte(-32767, I2cAddressY);
		sendCommands2byte(-32767, I2cAddressZ);
		messageFromPC[0] = 'n';
	}
	//ricevo comandi asse x
	if (messageFromPC[0] == 'x' || messageFromPC[0] == 'X') {
		Serial.println("ricevuto dati per x");
		sendCommands2byte(integerFromPC, I2cAddressX);
		messageFromPC[0] = 'n';
	}
	//ricevo comandi asse y
	if (messageFromPC[0] == 'y' || messageFromPC[0] == 'Y') {
		/// percorrere strada piu' breve
		if (messageFromPC[0] == 'y') {
			sendCommands3byte(integerFromPC, I2cAddressY, 1);
			messageFromPC[0] = 'n';
		}
		/// percorrere strada impostata 
		if (messageFromPC[0] == 'Y') {
			sendCommands3byte(integerFromPC, I2cAddressY, 0);
			messageFromPC[0] = 'n';
		}
		
	}

	//ricevo comandi asse z
	if (messageFromPC[0] == 'z' || messageFromPC[0] == 'Z') {
		/// percorrere strada piu' breve
		if (messageFromPC[0] == 'z') {
			sendCommands3byte(integerFromPC, I2cAddressZ, 1);
			messageFromPC[0] = 'n';
		}
		/// percorrere strada impostata 
		if (messageFromPC[0] == 'Z') {
			sendCommands3byte(integerFromPC, I2cAddressZ, 0);
			messageFromPC[0] = 'n';
		}

	}
}

//============

void sendCommands2byte(int a, int b) {
    //////invio intero via I2c
  Wire.beginTransmission(b);          // transmit to device #y
  Wire.write((byte*) & a , 2);        // Transmit x, one byte at a time.
  Wire.endTransmission();           // stop transmitting
  Serial.println("trasmesso allo slave 2 byte");
}

void sendCommands3byte(int a, int b, int c) {
	//////invio intero via I2c
	Wire.beginTransmission(b);          // transmit to device #y
	Wire.write((byte*)& a, 2);        // Transmit x, one byte at a time.
	Wire.write((byte*)& c, 1);
	Wire.endTransmission();           // stop transmitting
	Serial.println("trasmesso allo slave 3 byte");

}

void receiveEvent(int rcve) {
	//////ricevo via I2c
	
	if (Wire.available() >= 2)                  // Make sure there are two bytes.
	{

		for (int i = 0; i < 2; i++)             // Receive and rebuild the 'int'.
			iRXVal += Wire.read() << (i * 8);   //    "     "     "     "    "
		Serial.println("ricevuto via i2c");
	}

}