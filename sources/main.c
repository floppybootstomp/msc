#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_RATE 44100
#define PI 3.1415926535
#define CLAMP(x, y, z) x > y ? (x < z ? x : z) : y

// returns an array of frequencies given specified starting frequency, octave, and step size between octaves
void generateScale(double* scale, int length, double frequency, int octave, int step){
	scale[0] = frequency;

	for(int i = 1; i <= length; i ++){
		if(i % step == 0) { frequency *= octave; }

		scale[i] = frequency + (i % step * (frequency * octave - frequency) / step);
	}
}

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
        // int16_t is a 16 bit integer but sample[i] is a 64 bit double 0 < x < 1
        // 
		int16_t output = (int16_t)(0x7FFF * sample[i]);
		fwrite(&output, sizeof(char), sizeof(int16_t)/sizeof(char), f);
	}

	// close file
	fclose(f);
}

// TODO: attack and release do not work, cause extreme clipping
// applies an envelope to strength given time
double applyEnvelope(int i, double length, double strengthInit, double a, double s, double d, double r){
	double iSeconds = (double)i / SAMPLE_RATE;

	if(iSeconds < a){
		return fmax(fmin((strengthInit - s + iSeconds) / a, strengthInit), 0);
	}
	else if(iSeconds <= d){
		return (s - strengthInit + iSeconds) / d;
	}
	else if(iSeconds >= length - r && iSeconds < length){
		return fmax(fmin(-1 * (s + iSeconds) / r, 1), 0);
	}

	printf("sus\n");
	return 0;
}

/*
################################
	TONE GENERATOR FUNCTIONS
################################
*/

// generates a tone from a tone generator function
void generateTone(double* samples, double length, double frequency, double amplitude, double (*toneGeneratorFunction)(int, double, double)){
	//amplitude = 0;
	for(int i = 0; i < length; i ++){
		samples[i] = amplitude * toneGeneratorFunction(i, frequency, amplitude);
		// frequencyfrequency -= 1.01;
        // if(amplitude > 0) { amplitude -= 4*(amplitude/length); } else { amplitude = 0; }
		
		amplitude += applyEnvelope(
								  i,
								  length,
								  amplitude,
								  0.0,
								  0.0,
								  length/4,
								  0.0);
		
	}
}

// generate a sine tone
double generateSine(int i, double frequency, double amplitude){
	return sin(frequency * (i * (2 * PI) / SAMPLE_RATE));
}

// generate a square tone
double generateSquare(int i, double frequency, double amplitude){
	return (sin(frequency * i * (2 * PI) / SAMPLE_RATE) > 0) ? (1) : (-1);
}

// generate a sawtooth tone
double generateSawtooth(int i, double frequency, double amplitude){
	return 0.636 * atan(tan(frequency * (i * (2 * PI) / SAMPLE_RATE)));
}

// generate noise
double generateNoise(int i, double frequency, double amplitude){
	return (double)(rand())/ RAND_MAX;
}

// contains information about tone type, pitch, rhythm, and amplitude
struct Sequence {
	double (*toneGeneratorFunction)(int, double, double);
	double *scale;

	int length;
	// tones[][0] = pitch
	// tones[][1] = rhythm (in fractions of a second: seconds / tones[][1])
	// tones[][2] = amplitude (max: 100, min: 0)
	int* tones[3];
};

// writes a sequence to a sample buffer
void writeSequence(double* sample, double length, struct Sequence* seq){
	int bufferPos = 0;
	for(int i = 0; i < seq->length; i ++){
		generateTone(
					 &sample[bufferPos],
					 SAMPLE_RATE/seq->tones[1][i],
					 seq->scale[seq->tones[0][i]],
					 (double)seq->tones[2][i]/100,
					 seq->toneGeneratorFunction
					 );

		bufferPos += (SAMPLE_RATE/seq->tones[1][i]);
	}
}

int main(int argv, char* argc[]){
	double duration = 3;
	double frequency = 110;
	double amplitude = 0.8;
	double* samples = (double*)malloc(sizeof(double) * duration * SAMPLE_RATE);
	double *s = (double*)malloc(sizeof(double) * 24);
	generateScale(s, 24, 220, 2, 12);

	char* outputFile = "./data/prunes.pcm";

	int ton[12] = { 12, 10, 8 , 6 , 8 , 10, 8 , 6 , 4 , 6 , 8 , 6  };
	int rhy[12] = { 4 , 8 , 4 , 4 , 4 , 8 , 4 , 4 , 3 , 3 , 3 , 8  };
	int amp[12] = { 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80 };

	struct Sequence seq = {
		&generateSine,
		s,
		12,
		{ton,
		 rhy,
		 amp}
	};

	// write sequence
	writeSequence(samples, duration, &seq);

	// generate sine wave
	// generateSine(samples, duration * SAMPLE_RATE, frequency, amplitude);	
	/*
	for(int i = 0; i < duration*4; i ++){
		generateTone(&samples[i * SAMPLE_RATE/4], SAMPLE_RATE/4, frequency, amplitude, &generateSine);

		frequency *= 1.2;
	}
	*/

	// write sample
	writeSample(outputFile, samples, SAMPLE_RATE * duration);

	free(samples);

	return 0;
}
