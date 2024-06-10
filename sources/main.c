#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SAMPLE_RATE 44100
#define DURATION 4
#define PI 3.1415926535

// write a sample to a file
// to play the file use aplay <filename> -r <samplerate> -f S16_LE
// pcm to flac ffmpeg -f s16le -ar 44.1k -ac 2 -i <file>.pcm <file>.flac
void writeSample(const char* filename, double* sample, int sampleLength){
	// open file
	FILE* f = fopen(filename, "wb");
	if(f == NULL){
		fputs("Failed to open file", stderr);
		exit(1);
	}

	for(int i = 0; i < sampleLength; i ++){
		int16_t output = (int16_t)(0x7FFF * sample[i]);
		fwrite(&output, sizeof(char), sizeof(int16_t)/sizeof(char), f);
	}

	// close file
	fclose(f);
}

// generates a tone from a tone generator function
void generateTone(double* samples, double length, double frequency, double amplitude, double (*toneGeneratorFunction)(int, double, double)){
	for(int i = 0; i < length; i ++){
		samples[i] = toneGeneratorFunction(i, frequency, amplitude);
		frequency -= 0.01;
	}
}

/*############################
	TONE GENERATOR FUNCTIONS
##############################*/

// generate a sine tone
double generateSine(int i, double frequency, double amplitude){
	return amplitude * sin(frequency * (i * (2 * PI) / SAMPLE_RATE));
}

// generate a square tone
double generateSquare(int i, double frequency, double amplitude){
	return (sin(frequency * i * (2 * PI) / SAMPLE_RATE) > 0) ? (amplitude) : (-1 * amplitude);
}

// generate a sawtooth tone
double generateSawtooth(int i, double frequency, double amplitude){
	return amplitude * 0.636 * atan(tan(frequency * (i * (2 * PI) / SAMPLE_RATE)));
}

// generate noise
double generateNoise(int i, double frequency, double amplitude){
	return (amplitude * rand()) / RAND_MAX;
}

int main(int argv, char* argc[]){
	double duration = 4.0;
	double frequency = 110;
	double amplitude = 0.8;
	double* samples = (double*)malloc(sizeof(double) * duration * SAMPLE_RATE);

	char* outputFile = "./data/prunes.pcm";

	// generate sine wave
	// generateSine(samples, duration * SAMPLE_RATE, frequency, amplitude);	
	for(int i = 0; i < duration*4; i ++){
		generateTone(&samples[i * SAMPLE_RATE/4], SAMPLE_RATE/4, frequency, amplitude, &generateSine);

		frequency *= 1.2;
	}

	// write sample
	writeSample(outputFile, samples, SAMPLE_RATE * DURATION);

	free(samples);

	return 0;
}
