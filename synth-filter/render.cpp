// Queen Mary University of London
// ECS7012 - Music and Audio Programming
// Spring 2021
//
// Assignment 1: Synth Filter
// This project contains template code for implementing a resonant filter with
// parameters adjustable in the Bela GUI, and waveform visible in the scope

#include <Bela.h>
#include <libraries/Gui/Gui.h>
#include <libraries/GuiController/GuiController.h>
#include <libraries/Scope/Scope.h>
#include <libraries/math_neon/math_neon.h>
#include <cmath>
#include "Wavetable.h"

// Browser-based GUI to adjust parameters
Gui gGui;
GuiController gGuiController;

// Browser-based oscilloscope to visualise signal
Scope gScope;

// Oscillator objects
Wavetable gSineOscillator, gSawtoothOscillator;

// Filter coefficients
float gA1 = 0;  			//gloabl variable to hold y[n-1] coef
float gB0 = 0, gB1 = 0;		//gloabl variable to hold x[n] and x[n-1] coef respectively

float gLastIn[]={0.0,0.0,0.0,0.0};   //global variable to hold x[n-i] values
float gLastOut[]={0.0,0.0,0.0,0.0};  //gloabl variable to hold y[n-i] values

float gGres=0.75;          // global variable to hold Gres


// Calculate filter coefficients given specifications
// frequencyHz -- filter frequency in Hertz (needs to be converted to discrete time frequency)
// resonance -- normalised parameter 0-1 which is related to filter Q

void calculate_coefficients(float sampleRate, float frequencyHz, float resonance)
{
	
	float wc = 2.0*(M_PI * frequencyHz / sampleRate); //wc is the cut off frequency in rad/sample
	
	//fourth-order polynomial equation to calculate g in order to determine a more adjusted cut off frequency 
    float g = 0.9892*wc - 0.4342*powf(wc,2) + 0.1381*powf(wc,3) - 0.0202*powf(wc,4);
   
    //Valimaki and Houvilainen's resonance correction 
    gGres = resonance*(1.0029 + 0.0526*wc - 0.0926*powf(wc,2) + 0.0218*powf(wc,3));
	
	//calculate filter coefficients
	gB0 = g*1/1.3;
    gB1 = g*0.3/1.3;
    gA1 = 1-g;
    
}


bool setup(BelaContext *context, void *userData)
{
	std::vector<float> wavetable;
	const unsigned int wavetableSize = 1024;
		
	// Populate a buffer with the first 64 harmonics of a sawtooth wave
	wavetable.resize(wavetableSize);
	
	// Generate a sawtooth wavetable (a ramp from -1 to 1)
	for(unsigned int n = 0; n < wavetableSize; n++) {
		wavetable[n] = -1.0 + 2.0 * (float)n / (float)(wavetableSize - 1);
	}
	
	// Initialise the sawtooth wavetable, passing the sample rate and the buffer
	gSawtoothOscillator.setup(context->audioSampleRate, wavetable);

	// Recalculate the wavetable for a sine
	for(unsigned int n = 0; n < wavetableSize; n++) {
		wavetable[n] = sin(2.0 * M_PI * (float)n / (float)wavetableSize);
	}	
	
	// Initialise the sine oscillator
	gSineOscillator.setup(context->audioSampleRate, wavetable);

	// Set up the GUI
	gGui.setup(context->projectName);
	gGuiController.setup(&gGui, "Oscillator and Filter Controls");	
	
	// Arguments: name, default value, minimum, maximum, increment
	// Create sliders for oscillator and filter settings
	gGuiController.addSlider("Oscillator Frequency", 100, 40, 8000, 0);
	gGuiController.addSlider("Oscillator Amplitude", 0.3, 0, 2.0, 0);
	gGuiController.addSlider("Oscillator Cutoff Frequency", 2000, 100, 5000, 0);
	gGuiController.addSlider("Oscillator Resonance", 0.9, 0.0, 1.1, 0);
	
	// Set up the scope
	gScope.setup(2, context->audioSampleRate);

	return true;
}

void render(BelaContext *context, void *userData)
{
	// Read the slider values
	float oscFrequency = gGuiController.getSliderValue(0);	
	float oscAmplitude = gGuiController.getSliderValue(1);	
	float oscCutOff = gGuiController.getSliderValue(2);
	float oscResonance = gGuiController.getSliderValue(3);
	
	// Set the value of Gcomp
	float gGcomp =0.5;
	
	// Set the oscillator frequency
	gSineOscillator.setFrequency(oscFrequency);
	gSawtoothOscillator.setFrequency(oscFrequency);

	// Calculate new filter coefficients
	calculate_coefficients(context->audioSampleRate, oscCutOff, oscResonance);
	
    for(unsigned int n = 0; n < context->audioFrames; n++) {
    	
    	//Declares input signal 
		//float in = oscAmplitude * gSineOscillator.process();
		float in = oscAmplitude * gSawtoothOscillator.process();
		//Adds non-linearity to the input signal
		float feedback = tanhf_neon(1.0*in + 4.0*in*gGres*gGcomp - 4.0*gGres*gLastOut[3]);
		
		// Calculate filter N=4
 
		float out1 = gB0*feedback  +  gB1*gLastIn[0]  + gA1*gLastOut[0];
		gLastOut[0] = out1;
		gLastIn[0] = feedback;
		
		float out2 = gB0*out1 +  gB1*gLastIn[1]  + gA1*gLastOut[1];
		gLastOut[1] = out2;
		gLastIn[1] =  out1;
		
		float out3 = gB0*out2 +  gB1*gLastIn[2]  + gA1*gLastOut[2];
		gLastOut[2] = out3;
		gLastIn[2] = out2;
		
		float out  = gB0*out3 +  gB1*gLastIn[3]  + gA1*gLastOut[3];
		gLastOut[3] = out;
		gLastIn[3]  = out3;
   
        // Write the output to every audio channel
    	for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
    		audioWrite(context, n, channel, out);
    	}
    	
    	gScope.log(in, out);
    }
}

void cleanup(BelaContext *context, void *userData)
{

}
