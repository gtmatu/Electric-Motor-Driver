// Import std libraries
#include <stdint.h>
#include <cstdio>
#include <inttypes.h>
#include <iomanip>
#include <stdlib.h>
#include <string>
#include <algorithm>
#include <sstream>
#include "mbed.h"
#include "SHA256.h"

//////////////////////////////////
// GLOBAL VARIABLES
int8_t orState = 0;    //Rotot offset at motor state 0
int8_t intState = 0;
int8_t intStateOld = 0;
RawSerial pc(SERIAL_TX, SERIAL_RX);
Thread threadReceive, threadDecode, threadSpeed, threadRotation;
Mutex NewKey_Mutex;
Timer t;
char* msg_text;
string temp_str;
int stepCount;
int direction;
int velocity;
float revs;
float vel;
Thread motorCtrlT(osPriorityNormal, 1024);
int oldPosition;
int revsCount = 0;
////HASHING GLOBAL
uint8_t sequence[] = {  0x45,0x6D,0x62,0x65,0x64,0x64,0x65,0x64,
                        0x20,0x53,0x79,0x73,0x74,0x65,0x6D,0x73,
                        0x20,0x61,0x72,0x65,0x20,0x66,0x75,0x6E,
                        0x20,0x61,0x6E,0x64,0x20,0x64,0x6F,0x20,
                        0x61,0x77,0x65,0x73,0x6F,0x6D,0x65,0x20,
                        0x74,0x68,0x69,0x6E,0x67,0x73,0x21,0x20,
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint64_t* key = (uint64_t*)((int)sequence + 48);
uint64_t* nonce = (uint64_t*)((int)sequence + 56);
//////////////////////////////////


//////////////////////////////////
// Func Declarations
void photoISR();
void putMessage(int n, char* s, uint64_t keyP);
//////////////////////////////////

// Import custom libraries
#include "Motor.h"
#include "Coms.h"

void photoISR(){
    intState = readRotorState();
    motorOut((intState-orState+lead+6)%6); //+6 to make sure the remainder is positive
    if(intState == orState) revsCount++;
    // Get direction
    int diff = intState - intStateOld;
    if (diff > 0){
        if (diff == 5) direction  = -1;
        else           direction  = 1;
    }
    else if (diff < 0){
        if (diff == -5) direction  = 1;
        else           direction  = -1;
    }    
    stepCount += direction;
    intStateOld = intState;
}



    
//Main
int main() {
    
    // Initialize PWM_OUT params
    pwm_out.period(0.002f);
    pwm_out = 1.0f;
    //Initialise the serial port
    pc.printf("Hello\n");
    
    threadReceive.start(callback(Receiver));
    threadDecode.start(callback(decodeSerialInput));
    threadSpeed.start(callback(set_velocity));
    threadRotation.start(callback(Rotate));
    motorCtrlT.start(callback(motorCtrlFn));
    
        
    //Run the motor synchronisation
    orState = motorHome();
    
    /// ATTACH INTERRUPTS
    I1.rise(&photoISR);
    I2.rise(&photoISR);
    I3.rise(&photoISR);
    I1.fall(&photoISR);
    I2.fall(&photoISR);
    I3.fall(&photoISR);
    CHA.rise(&encISR0);
    CHB.rise(&encISR1);
    CHA.fall(&encISR2);
    CHB.fall(&encISR3);
    //

    
    // HASHING FUNCTION //
    
    SHA256 mySHA256 = SHA256();
    
    uint8_t hash[32];
    
    int counter = 0;
    t.start();
    while (1) {
        
        mySHA256.computeHash(hash,sequence,sizeof(sequence));
        counter++;
        if(t.read() >= 1){  //if a second has passed
            t.stop();
            t.reset();
            t.start();

            msg_text = "hashes per second\n\r";
            putMessage(counter, msg_text,0);
            counter = 0;
            }
        if((hash[0] == 0) && (hash[1] == 0)){
            putMessage(0, "SUCCESS \n\r", 0);
            putMessage(0, "Nonce found: ", *nonce);
            putMessage(0, "\n\r", 0);
        }
        *nonce+=1;
    }
}

