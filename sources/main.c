#include <sys/time.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_RATE 44100
#define MAX_VOLUME 1
#define PI 3.1415926535

#define CLAMP(v, hi, lo) v > hi ? hi : (v < lo ? lo : v)

// returns an array of frequencies given specified starting frequency, octave, and step size between octaves
void generateScale(float* scale, int length, float frequency, int octave, int step){
	scale[0] = frequency;

	for(int i = 1; i <= length; i ++){
		if(i % step == 0)
			frequency *= octave;

		scale[i] = frequency + (i % step * (frequency * octave - frequency) * (1.0 / step));
	}
}

// write a sample to a file
// to play the file use aplay <filename> -r <samplerate> -f S16_LE
// pcm to flac ffmpeg -f s16le -ar 44.1k -ac 1 -i <file>.pcm <file>.flac
void writeSample(const char* filename, float* sample, int sampleLength){
	// open file
	FILE* f = fopen(filename, "wb");
	if(f == NULL){
		fputs("Failed to open file", stderr);
		exit(1);
	}

	for(int i = 0; i < sampleLength; i ++){
		// int16_t is a 16 bit integer but sample[i] is a 64 bit double -1 * MAX_VOLUME < x < MAX_VOLUME
		// 
		int16_t output = (int16_t)(0x7FFF * sample[i]);
		fwrite(&output, sizeof(char), sizeof(int16_t) * (1.0/sizeof(char)), f);
	}

	// close file
	fclose(f);
}

// applies an envelope to strength given time
float applyEnvelope(int i, int length, float strengthInit, float currStrength, float a, float s, float d, float r){
	float ca = 0;

	if(i < a && currStrength < strengthInit)
		ca = strengthInit * (1.0 / a);
	else if(i < a && i < length-a)
		ca = 0;

	if(i > a && i < d + a && (strengthInit < s ? currStrength < s : currStrength > s))
		ca = (s - strengthInit) * (1.0 / d);
	else if(i > a && i < d)
		ca = 0;
	if(i > length-r && currStrength > 0)
		ca = -1 * s * (1.0 / r);
	else if(i > length-r)
		ca = 0;

	return ca;
}

/*
################################
	TONE GENERATOR FUNCTIONS
################################
*/

// generates a tone from a tone generator function
void generateTone(float* samples, int filePos, int length, float frequency, float amplitude, float asdr[4], float (*toneGeneratorFunction)(int, float, float)){
	int i = 0;
	float dynAmp = 0.0;
	float ampTightness = 480;

	for(i = 0; i < length; i ++){
		samples[i] += dynAmp * toneGeneratorFunction(i+filePos, frequency, amplitude);
		dynAmp += applyEnvelope(
			i,
			length,
			amplitude,
			dynAmp,
			ampTightness + asdr[0],
			asdr[1],
			asdr[2],
			ampTightness + asdr[3]
		);
	}
}

// generate a sine tone
float generateSine(int i, float frequency, float amplitude){
	return sin(frequency * i * 2 * PI * (1.0 / SAMPLE_RATE));
}

// generate a square tone
float generateSquare(int i, float frequency, float amplitude){
	return (sin(frequency * i * (2 * PI) * (1.0 / SAMPLE_RATE)) > 0) ? (1) : (-1);
}

// generate a sawtooth tone
float generateSawtooth(int i, float frequency, float amplitude){
	return 0.636 * atan(tan(frequency * (i * (2 * PI) * (1.0 / SAMPLE_RATE))));
}

// generate noise
float generateNoise(int i, float frequency, float amplitude){
	return (float)(rand()) * (1.0 / RAND_MAX);
}

/*
################################
	Sequence Functions
################################
*/

// contains information about tone type, pitch, rhythm, and amplitude
struct Sequence {
	float (*toneGeneratorFunction)(int, float, float);
	float *scale;

	// in beats per second
	float tempo;

	// 1 repeat 0 no repeat
	int repeat;

	// length is the number of tones, totalLength is sum of the length of the tones
	int length;
	int totalLength;
	float (*asdr)[4];

	// tones[][0] = pitch
	// tones[][1] = rhythm (in fractions of a second: seconds / tones[][1])
	// tones[][2] = amplitude (max: 100, min: 0)
  float* (*tones)[3];
};

// contains information about a track's sequence and length for arrangement
struct Track {
	// the length of the track
	float length;

	// the sequence this track plays
	struct Sequence seq;
};

// calculates the length of a sequence given the rhythm lengths
static float calculateSequenceLength(float rhy[], int length){
	float totalLength = 0;
	for(int i = 0; i < length; i ++){
		totalLength += 1.0 / rhy[i];
	}

	return totalLength;
}

// initializes a sequence with given values
struct Sequence initializeSequence(float (*toneGeneratorFunction)(int, float, float), float* scale, float tempo, float repeat, int length, float (*asdr)[4], float* (*tones)[3]){
	return ((struct Sequence) {
		toneGeneratorFunction,
		scale,
		tempo,
		repeat,
		length,
		(int)(calculateSequenceLength((*tones)[1], length) * SAMPLE_RATE * 2),
	    asdr,
		tones,
	});
}

// writes a sequence to a sample buffer
void writeSequence(float* sample, float length, float sampleWidth, struct Sequence* seq){
	int bufferPos = 0;
	int nextNoteSpace = 0;

	for(int i = 0;
		bufferPos <= length - (nextNoteSpace = sampleWidth * (1.0 / (*(seq->tones))[1][i]));
		i = (i+1)%seq->length){
		generateTone(
			&sample[bufferPos],
			bufferPos,
			nextNoteSpace,
			seq->scale[(int)(*(seq->tones))[0][i]],
			(*(seq->tones))[2][i] * 0.01,
			*(seq->asdr),
			seq->toneGeneratorFunction
		);

		bufferPos += nextNoteSpace;
	}
}

// writes a the sequences into a buffer based on the track arrangement for that sequence
void writeTrackBuffer(float* sampleBuffer, float length, int songLength, struct Track tracks[songLength]){
	float trackLengthWritten = 0;
	float sampleWidth = 0;
	for(int trackPos = 0; trackPos < songLength; trackPos ++){
		sampleWidth = SAMPLE_RATE * (1.0 / tracks[trackPos].seq.tempo);

		if(trackPos > 0){
			trackLengthWritten += tracks[trackPos-1].length > 0 ? tracks[trackPos-1].length : -1 * tracks[trackPos-1].length;
		}

		// write sequence
		if(tracks[trackPos].length > 0)
			writeSequence(
				&sampleBuffer[(int)(trackLengthWritten * sampleWidth)],
				tracks[trackPos].length * sampleWidth,
				sampleWidth,
				&tracks[trackPos].seq
			);
	}
}

// combines sequence buffers into master sequence
void writeMaster(float* master, float length, int numTracks, int songLength, struct Track tracks[numTracks][songLength]){
	for(int i = 0; i < length * SAMPLE_RATE; i ++)
		master[i] = 0;

	for(int trax = 0; trax < numTracks; trax ++){
		writeTrackBuffer(master, length, songLength, &tracks[trax][0]);
	}

	for(int i = 0; i < length * SAMPLE_RATE; i ++){
		master[i] = CLAMP(master[i], 1, -1);
	}
}

int main(int argc, char* argv[]){
	struct timeval s1, s2;
	int timing = 0;
	if(argc > 1 && strcmp(argv[1], "-t") == 0)
		timing = 1;

	if(timing == 1)
		gettimeofday(&s1, NULL);

	float duration = 17 * 6;
	int	songLength = 4;
	float* master = (float*)malloc(sizeof(float) * duration * songLength * SAMPLE_RATE);
	float* s = (float*)malloc(sizeof(float) * 24);
	generateScale(s, 24, 220, 2, 12);

	char* outputFile = "./data/prunes.pcm";

	float de = 8.0/3.0;
	float aa = 60;
	float ton[38] = { 17, 15, 14, 12, 14, 15, 14, 12, 10, 10, 12, 14, 15, 14, 17, 12, 12, 17, 15, 14,
					  17, 15, 14, 12, 14, 15, 14, 12, 10, 10, 12, 14, 15, 14, 17, 12, 12, 12 };
	float rhy[38] = { 4 , 8 , 4 , 4 , 4 , 8 , 8 , 8 , 4 , de, de, 8 , de, 4 , 4 , de, 2 , 4 , 8 , 2 ,
					  4 , 8 , 4 , 4 , 4 , 8 , 8 , 8 , 4 , de, de, 8 , de, 4 , 4 , de, 2 , 8.0/7.0 };
	float amp[38] = { aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa,
					  aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa, aa };

	float* tones[3] = {ton, rhy, amp};
	float asdr[4] = { 0, aa * 0.01, 0, 0 };
	struct Sequence seq = initializeSequence(
		&generateSine,
		s,
		0.5,
		1,
		38,
		&asdr,
		&tones
	);

	float ab = 80-aa;
	float ac = ab * 0.5;
	float ad = ab * 0.25;
	float tonR[8] = { 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0  };
	float rhyR[8] = { 4 , 8 , 4 , 4 , 4 , 8 , 4 , 4  };
	float ampR[8] = { ab, ad, ac, ac, ab, ad, ac, ad };

	float* tonesR[3] = {tonR, rhyR, ampR};
	float asdrR[4] = { 0, 0, SAMPLE_RATE * 0.0625, 0 };
	struct Sequence seqR = initializeSequence(
		&generateNoise,
		s,
		0.5,
		1,
		8,
		&asdrR,
		&tonesR
	);

	// order tracks
	float sht = 2.625;	// 10.5 / 4
	float lng = 10.5;	// 42 / 4

	struct Track mp = { lng, seq };
	struct Track ms = { -1 * sht, seq };
	struct Track rp = { lng, seqR };
	struct Track rs = { sht, seqR };

	struct Track tracks[2][11] = {
		{ ms, mp, mp, ms, mp, mp, ms },
		{ rs, rp, rp, rs, rp, rp, rs }
	};

	if(timing == 1)
		gettimeofday(&s2, NULL);

	long unsigned int paid;

	if(timing == 1){
		paid = (s2.tv_usec - s1.tv_usec) * 0.001;
		printf("allocation took %lu milliseconds\n", paid);
		gettimeofday(&s1, NULL);
	}

	writeMaster(master, duration, 2, 11, tracks);

	if(timing == 1){
		gettimeofday(&s2, NULL);
		paid = (s2.tv_usec - s1.tv_usec) * 0.001;
		printf("writeMaster took %lu milliseconds\n", paid);
		gettimeofday(&s1, NULL);
	}

	// write sample
	writeSample(outputFile, master, (int)(SAMPLE_RATE * duration));

	if(timing == 1){
		gettimeofday(&s2, NULL);
		paid = (s2.tv_usec - s1.tv_usec) * 0.001;
		printf("writeSample took %lu milliseconds\n", paid);
	}

	free(s);
	free(master);

	return 0;
}
