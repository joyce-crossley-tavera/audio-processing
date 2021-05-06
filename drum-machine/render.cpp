/*
 * assignment2_drums
 * ECS7012 Music and Audio Programming
 *
 * Second assignment, to create a sequencer-based
 * drum machine which plays sampled drum sounds in loops.
 *
 * This code runs on the Bela embedded audio platform (bela.io).
 *
 * Andrew McPherson, Becky Stewart and Victor Zappi
 * 2015-2020
 */


#include <Bela.h>
#include <cmath>
#include "drums.h"
#include <libraries/Scope/Scope.h>


// Button pin 1 GPIO, LED pin 0
const int kButtonPin = 1;
const int kLedPin = 0;

// Button Status
int gLastButtonStatus=HIGH;

// Bela oscilloscope
Scope gScope;

// Buffer that holds the sound file
extern float *gDrumSampleBuffers[NUMBER_OF_DRUMS]; 
extern int gDrumSampleBufferLengths[NUMBER_OF_DRUMS];

//Position of the last frame we played
int gReadPointers[16] = {-1};  
// array that associates gReadPointers and gDrumSampleBuffers
int gDrumBufferForReadPointer[16]= {-1}; 


/* Patterns indicate which drum(s) should play on which beat.
 * Each element of gPatterns is an array, whose length is given
 * by gPatternLengths.
 */
extern int *gPatterns[NUMBER_OF_PATTERNS];
extern int gPatternLengths[NUMBER_OF_PATTERNS];

//These variables indicate which pattern we're playing, and where within the pattern we currently are.
int gCurrentPattern = 0;
int gCurrentIndexInPattern = 0;

//This variable holds the interval between events in **milliseconds**
int gEventIntervalMilliseconds = 250;

//Whether we should play or not. Implement this in Step 4b. */
int gIsPlaying = 0;			

// LED variables
unsigned int gLEDInterval = 4410;

// Metronome state machine variables
const int kMetronomeStateOff =-1;
//counts audio samples to decide when the next tick should be
int gMetronomeCounter = 0;
// the amount of samples we need to count up to start the next beat
int gMetronomeInterval = 0;
//state variable. It tells which beat of the bar we are in. Evry time a tick ocurrs, we need to increment IN 1.  
int gMetronomeBeat = kMetronomeStateOff; 


//Accelerometer thresholds
float gThresholdHigh_X = 0.2;
float gThresholdLow_X = -0.2;
float gThresholdHigh_Y = 0.3;
float gThresholdLow_Y = -0.1;
float gThresholdHigh_Z = 0.3;
float gThresholdLow_Z = -0.3;


// This variable indicates whether samples should be triggered or not. 
extern int gIsPlaying;

// This indicates whether we should play the samples backwards.
int gPlaysBackwards = 0;

int number_of_drums = 8;

bool setup(BelaContext *context, void *userData)
{
	// Check that audio and digital have the same number of frames
	// per block, an assumption made in render()
	if(context->audioFrames != context->digitalFrames) {
		rt_fprintf(stderr, "This example needs audio and digital running at the same rate.\n");
		return false;
	}
	

	// Initialise GPIO pins for the button and LED
	pinMode(context, 0, kButtonPin, INPUT);
	pinMode(context, 0, kLedPin, OUTPUT);
	
	// Calculate the LED interval in samples, given that it should stay lit 50 ms after every tick
	gLEDInterval = 0.05*context->analogSampleRate;

	// Initialise the Bela oscilloscope with 2 channels
	gScope.setup(2, context->audioSampleRate);
	
	return true;
}


void render(BelaContext *context, void *userData) {

	
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		float out =0;
		//Read the button
		int status = digitalRead(context, n, kButtonPin);
		//Read the analog inputs to get the current tempo
		float input = analogRead(context, n/2, 0);
		
		//Get the voltage which corresponds to acceleration in each axis.
		float position_X =analogRead(context, n/2, 1);
		float position_Y =analogRead(context, n/2, 2);
		float position_Z =analogRead(context, n/2, 3);
		
		// Mapping the output voltage readings to -1.5 to 1.5 g. The min and max voltage readings are taken
		// by measuring the first samples in flat position.
		float X= map(position_X, 0.20, 0.60, -1.5, 1.5);
		float Y= map(position_Y, 0.24, 0.64, -1.5, 1.5);
		float Z= map(position_Z, 0.16, 0.56, -1.5, 1.5);
		
		//rt_printf( "%f %f %f\n", position_X, position_Y, position_Z);
		//rt_printf( "%f %f %f\n", X, Y, Z);

		//Hysteresis Comparator

		if ((X > gThresholdLow_X  && X < gThresholdHigh_X ) && (Y > gThresholdLow_Y && Y < gThresholdHigh_Y) && (Z > gThresholdHigh_Z)) {
			gCurrentPattern = 0;
			//rt_printf( "Resting flat");
		}
		 
		if ((X < gThresholdLow_X) && (Y > gThresholdLow_Y && Y < gThresholdHigh_Y) && (Z > -0.3 && Z < gThresholdHigh_Z)){
			gCurrentPattern = 1;
			//rt_printf( "Turned vertically on left side");
		}		
		if ((X > gThresholdHigh_X) && (Y > gThresholdLow_Y && Y < gThresholdHigh_Y) && (Z > -0.3 && Z < gThresholdHigh_Z)){
			gCurrentPattern = 2;
			//rt_printf( "Turned vertically on right side");
		}
		if ((X > gThresholdLow_X && X < gThresholdHigh_X) && (Y < gThresholdLow_Y) && (Z > -0.3 && Z < gThresholdHigh_Z)){
			gCurrentPattern = 3;
			//rt_printf( "Turned vertically on front side");
		}	
		if ((X > gThresholdLow_X && X < gThresholdHigh_X) && (Y > gThresholdHigh_Y) && (Z > -0.3 && Z < gThresholdHigh_Z)){
			gCurrentPattern = 4;
			//rt_printf( "Turned vertically on back side");
		}
		
		if ((X > gThresholdLow_X && X < gThresholdHigh_X) && (Y > gThresholdLow_Y && Y < 0.3) && (Z < gThresholdLow_Z)){
			gCurrentPattern = 1;
			//Trigger the sound backwards
			if (gPlaysBackwards == 1) {
				gPlaysBackwards = 0;
			}
			else {
				gPlaysBackwards = 1;
			}
			//rt_printf( "Turned the board upsidedown");
		}		

		//Falling edge detection
		if(status == LOW && gLastButtonStatus == HIGH) {
		   	if (gIsPlaying==1) {
		   		gIsPlaying = 0;
			}
			else{
				gIsPlaying = 1;
			}
		}
		
		if (gIsPlaying == 1) {
			float gEventIntervalMilliseconds = map(input, 0, 3.3/4.096, 50, 1000);
			// Calculate the metronome interval in samples.
			gMetronomeInterval = gEventIntervalMilliseconds*0.001* context->audioSampleRate;
		
			if (++gMetronomeCounter >= gMetronomeInterval) {
				//Reset the counter and trigger the sample
				gMetronomeCounter = 0;
				startNextEvent();
			}
			
			//Check if counter is within the LED blink interval
			if(gMetronomeCounter < gLEDInterval) {
				digitalWriteOnce(context, n, kLedPin, HIGH);
			}
			else {
				digitalWriteOnce(context, n, kLedPin, LOW);
			}
			
			// If the drum sample is playing forward
			if (gPlaysBackwards ==0) {
				
				for (unsigned int j= 0; j < 16; j++) {
					
					//Check if there is a drum index associated to the pointer. 
						if (gDrumBufferForReadPointer[j]!=-1) {
							// Has it reached to the end of the drum sample?
					 			if ( gReadPointers[j] >= gDrumSampleBufferLengths[gDrumBufferForReadPointer[j]] ) {
									gDrumBufferForReadPointer[j] = -1;
									gReadPointers[j]=-1;
								}
								else {
									// Output is divided  by the number of gReadPointers in order to avoid clipping
									//out += gDrumSampleBuffers[gDrumBufferForReadPointer[j]][gReadPointers[j]]/sizeof(gReadPointers);
									out += gDrumSampleBuffers[gDrumBufferForReadPointer[j]][gReadPointers[j]];
									gReadPointers[j]++;
								}
						}
						
				}
			}
			// If the drum sample is playing backwards
/*			if (gPlaysBackwards == 1) {
				
				for (unsigned int j= 16; j > 0; j--) {
					
					//Check if there is a drum index associated to the pointer. 
						if (gDrumBufferForReadPointer[j]!=-1) {
							// Has it reached to the end of the drum sample? 
							//TODO: not sure how to check if it has reached the length of the drum sample when is running backwards
					 			if ( gReadPointers[j] >= gDrumSampleBufferLengths[gDrumBufferForReadPointer[j]] ) {
									gDrumBufferForReadPointer[j] = -1;
									gReadPointers[j]=-1;
								}
								else {
									// Output is divided by the number of gReadPointers in order to avoid clipping
									out += gDrumSampleBuffers[gDrumBufferForReadPointer[j]][gReadPointers[j]]/sizeof(gReadPointers);
									gReadPointers[j]++;
								}
						}
						
				}
				
			}
*/			
			// Write the sample to every audio output channel            
			for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
				audioWrite(context, n, channel, out);
			}
		}
		// Update gLastButtonStatus
		gLastButtonStatus = status;	
		
	}
		
}



/* Start playing a particular drum sound given by drumIndex.
 */
void startPlayingDrum(int drumIndex) {
	for (int i =0; i<16; i++) {
			// check for the first gReadPointer which is available
			if (gReadPointers[i]==-1) {
				//Assign a drum sound to that gReadPointer
				gDrumBufferForReadPointer[i]= drumIndex;
				//Activate that gReadPointer
				gReadPointers[i] = 0;
				break;
			}
	}
}
	
	
	
/* Start playing the next event in the pattern */
void startNextEvent() {
	// Iterate over the drum samples
	for (int i = 0; i< number_of_drums; i++) {
		// if the current event contains that drum sound, then it plays it.
		if (eventContainsDrum(gPatterns[gCurrentPattern][gCurrentIndexInPattern],i) == 1) {
			startPlayingDrum(i);
		}
		gCurrentIndexInPattern++;
		if (gCurrentIndexInPattern >= gPatternLengths[gCurrentPattern]) {
			//Reset the index pattern
			gCurrentIndexInPattern=0;
		}
	}
}



/* Returns whether the given event contains the given drum sound */
int eventContainsDrum(int event, int drum) {
	if(event & (1 << drum))
		return 1;
	return 0;
}



// cleanup_render() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in initialise_render().

void cleanup(BelaContext *context, void *userData) {

}
