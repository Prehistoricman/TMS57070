#include <FastGPIO.h> //https://github.com/pololu/fastgpio-arduino
#include <SPI.h>

#define SS_PIN 10
#define HX_PIN MISO
#define HR_PIN MOSI
#define HRBCK_PIN SCK
#define HXBCK_PIN SCK
#define CS_PIN 8
#define HOST_PIN 4
#define MUX_ENABLE_PIN 5

uint8_t received[3];

char nibbles[16] = "0123456789ABCDEF";
char Hex[9];


void DSPSend(uint8_t b) {
    FastGPIO::Pin<CS_PIN>::setOutputValue(0);
	delayMicroseconds(5);
	SPI.transfer(b);
	delayMicroseconds(5);
	FastGPIO::Pin<CS_PIN>::setOutputValue(1);
}

void DSPCommunication(uint16_t address, uint32_t data) { //address = 2 bytes, data = 3 bytes
    FastGPIO::Pin<CS_PIN>::setOutputValue(0);
	delayMicroseconds(5);
	
	SPI.transfer(0x3);
	SPI.transfer(address >> 8);
	SPI.transfer(address >> 0);
	SPI.transfer(data >> 16);
	SPI.transfer(data >> 8);
	SPI.transfer(data >> 0);
	
	delayMicroseconds(5);
	FastGPIO::Pin<CS_PIN>::setOutputValue(1);
}

void DSPReceive() {
	uint8_t buf[3];
	memset(&buf, 0, sizeof(buf)); //Must send zero to DSP while receiving
	
    FastGPIO::Pin<CS_PIN>::setOutputValue(0);
	delayMicroseconds(5); //Cannot be as low as 3us
	SPI.transfer(0x10);
	delayMicroseconds(1);
	FastGPIO::Pin<CS_PIN>::setOutputValue(1);
	
	delayMicroseconds(1);
	
	FastGPIO::Pin<HXBCK_PIN>::setOutputValue(0);
	SPCR &= ~(1 << 6); //Disable SPI and SCK becomes a normal output pin
	delayMicroseconds(20); //Needed for readout to be correct
	FastGPIO::Pin<HXBCK_PIN>::setOutputValue(1);
	delayMicroseconds(3);
	SPCR |= (1 << 6); //Re-enable SPI
	
	FastGPIO::Pin<CS_PIN>::setOutputValue(0);
	delayMicroseconds(3);
	SPI.transfer(&buf, 3);
	delayMicroseconds(3);
	FastGPIO::Pin<CS_PIN>::setOutputValue(1);
	
	memcpy(&received, &buf, sizeof(buf));
}

void Enable() { //Set all pins as outputs
	pinMode(SS_PIN, OUTPUT);
	digitalWrite(SS_PIN, 1); //SS has to be high for SPI to work
    
	pinMode(MUX_ENABLE_PIN, OUTPUT);
	digitalWrite(MUX_ENABLE_PIN, 1);
    pinMode(HRBCK_PIN, OUTPUT);
    pinMode(HXBCK_PIN, OUTPUT);
    pinMode(HR_PIN, OUTPUT);
    pinMode(CS_PIN, OUTPUT);
	FastGPIO::Pin<CS_PIN>::setOutputValue(1);
	
	SPISettings settings{(uint32_t)500 * 1000, LSBFIRST, SPI_MODE3};
	SPI.beginTransaction(settings); //125kHz is the slowest available
}

void Disable() { //Set all pins as inputs
	SPI.endTransaction();
	pinMode(MUX_ENABLE_PIN, INPUT);
	digitalWrite(MUX_ENABLE_PIN, 0);
    pinMode(HRBCK_PIN, INPUT);
    pinMode(HXBCK_PIN, INPUT);
    pinMode(HR_PIN, INPUT);
    pinMode(CS_PIN, INPUT);
}


void PMEM(uint8_t opcode) {
	//Serial.println("PMEM");
	FastGPIO::Pin<CS_PIN>::setOutputValue(0);
	delayMicroseconds(5);
	SPI.transfer(opcode);
	delayMicroseconds(25);
	
	//Copy hex chars into buffer until we reach a control character
	int icount = 0;
	while (true) {
		size_t length = Serial.readBytesUntil(',', Hex, 8);
		Hex[length] = '\0';
		if (length == 0)
			continue; //Why does this happen??
		
		if (Hex[0] == '!') {
			//Serial.println("End of PMEM");
			//Serial.println(icount);
			Serial.print("p!");
			break;
		}
		icount++;
		uint32_t pmem_word = strtoul(Hex, nullptr, 16);
		
		//MSByte first
		SPI.transfer(pmem_word >> 24);
		SPI.transfer(pmem_word >> 16);
		SPI.transfer(pmem_word >> 8);
		SPI.transfer(pmem_word >> 0);
	}

	delayMicroseconds(25);
    FastGPIO::Pin<CS_PIN>::setOutputValue(1);
}

void CMEM() {
	//Serial.println("CMEM");
	FastGPIO::Pin<CS_PIN>::setOutputValue(0);
	delayMicroseconds(5);
	SPI.transfer(0x5);
	delayMicroseconds(25);
	
	int icount = 0;
	//Copy hex chars into buffer until we reach a control character
	while (true) {
		size_t length = Serial.readBytesUntil(',', Hex, 8);
		Hex[length] = '\0';
		if (length == 0)
			continue; //Why does this happen??
		
		if (Hex[0] == '!') {
			//Serial.println("End of CMEM");
			//Serial.println(icount);
			Serial.print("d!");
			break;
		}
		uint32_t cmem_word = strtoul(Hex, nullptr, 16);
		icount++;
		
		//MSByte first
		SPI.transfer(cmem_word >> 16);
		SPI.transfer(cmem_word >> 8);
		SPI.transfer(cmem_word >> 0);
	}

	delayMicroseconds(25);
    FastGPIO::Pin<CS_PIN>::setOutputValue(1);
}

uint8_t ReadoutStart = 0;
uint8_t ReadoutEnd = 0;
void Readout() {
	Serial.print("r");
	Serial.print(ReadoutStart); //Print the starting address of this readout
	Serial.print(" ");
	
	Hex[6] = ' ';
	Hex[7] = '\0';
	
	//Loop over readout region and output hex
	for (int i = ReadoutStart; i <= ReadoutEnd; i++) {
		DSPCommunication(0, i);
		DSPReceive();
		GetHex();
		Serial.print(Hex);
	}
	Serial.print("!");
}

void ReadoutBounds() {
	//Query serial for two bytes of the bounds
	//ReadoutStart = Serial.read();
	//ReadoutEnd = Serial.read();
	
	size_t length = 0;
	while (length == 0)
		length = Serial.readBytesUntil(',', Hex, 2);
	Hex[length] = '\0';
	
	ReadoutStart = strtoul(Hex, nullptr, 16);
	
	//Serial.print("Got first number: ");
	//Serial.print(ReadoutStart);
	//Serial.println();
	
	length = 0;
	while (length == 0)
		length = Serial.readBytesUntil(',', Hex, 2);
	Hex[length] = '\0';
	
	ReadoutEnd = strtoul(Hex, nullptr, 16);
	
	//Serial.print("Got second number: ");
	//Serial.print(ReadoutEnd);
	//Serial.println();
	
	if (ReadoutEnd < ReadoutStart)
		ReadoutEnd = -1;
}

void Write() {
	//Write 24 bits to CMEM
	size_t length = 0;
	while (length == 0)
		length = Serial.readBytesUntil(',', Hex, 4);
	Hex[length] = '\0';
	
	uint16_t addr = strtoul(Hex, nullptr, 16);
	
	length = 0;
	while (length == 0)
		length = Serial.readBytesUntil('!', Hex, 6);
	Hex[length] = '\0';
	
	uint32_t word = strtoul(Hex, nullptr, 16);
	
	DSPCommunication(addr, word);
}

void GetHex() {
	for (int j = 0; j < 3; j++) { //Loop through 3 bytes
		for (int i = 0; i < 2; i++) { //Loop through 2 nibbles
			byte nibble = received[j] << (4 * i);
			nibble = nibble >> 4;
			Hex[i + j * 2] = nibbles[nibble];
		}
	}
}


void setup() {
	Disable();
	pinMode(HOST_PIN, INPUT);
	pinMode(HX_PIN, INPUT);
	
	Hex[6] = ' ';
	Hex[7] = 0;
	
	//serial
	Serial.begin(250000);
	Serial.println("Hello world!");
	
	pinMode(HOST_PIN, INPUT);
	//Wait for host to pass over control
	while (!digitalRead(HOST_PIN)) {}
	Enable();
	digitalWrite(HOST_PIN, 1);
	Serial.println("start");
}

void loop() {
	//Wait for a command from the UART
	//Serial.println("Waiting for input");
	while (!Serial.available()); //Wait for input
	
	char command = Serial.read();
	//Serial.print("Got command ");
	//Serial.write(command);
	//Serial.println();
	switch (command) {
	case 'p':
		PMEM(0x04); //Resets
		break;
	case 'q':
		PMEM(0x0C); //Waits
		break;
	case 'd':
		CMEM();
		break;
	case 'r':
		//Start readout
		Readout();
		break;
	case 's':
		//Set readout bounds
		ReadoutBounds();
		break;
	case 'w':
		Write();
		break;
	case '?':
		Serial.print("!");
		break; //For synchronisation
	}
}