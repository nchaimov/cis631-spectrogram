#include <pngwriter.h>
#include <math.h>
#include <iostream.h>
#include <string>
#include <stdlib.h>
#include <omp.h>
#include <stdio.h>
#include <sndfile.h>

using namespace std;

// sampleRate: sample rate of the output wav file
const int sampleRate = 44100;

// durationPerLine:	duration in the sound file of each column in the	
//					input image
const double durationPerLine = 0.2;

// maxFreq: the maximum frequency in the output wav file
//			(height of the spectrogram)
const double maxFreq = 22000;

// bits:	how many bits per sample in the output wav file (8, 16, or 32)	
const int bits = 32;

// format:	the output format of the wav file. the number of bits in the
//			PCM encoding must match the number of bits above
const int format = SF_FORMAT_WAV | SF_FORMAT_PCM_32;

// colorCorrection:	adjustment to output color to increase contrast
//					(4.25 seems to work well)
const double colorCorrection = 4.25;

struct pixel {
	int red;
	int green;
	int blue;
};

struct image {
	int height;
	int width;
	pixel *pixels;
};

const double samplesPerLine = sampleRate * durationPerLine;
const double maxSound = pow(2.0, bits)/2.0;

image & readImage(const char * name) {
	pngwriter pngfile(1, 1, 1.0, "tmp.png");
	pngfile.readfromfile(name);
	const int width = pngfile.getwidth();
	const int height = pngfile.getheight();
	pixel *img = new pixel[(width+1)*(height+1)];
	for(int x = 1; x <= width; ++x) {
		for(int y = 1; y <= height; ++y) {
			pixel & q = img[x + (y*width)];
			q.red = pngfile.read(x, y, 1);
			q.green = pngfile.read(x, y, 2);
			q.blue = pngfile.read(x, y, 3);
		}
	}
	image * result = new image;
	result->height = height;
	result->width = width;
	result->pixels = img;
	return (*result);
}


int main(int argc, char ** argv)
{
	if(argc < 2) {
		cerr << "Usage: " << argv[0] << " inputFileName.png" << endl;
		exit(-1);
	}
	image img = readImage(argv[1]);
	
	const double duration = durationPerLine * img.width;
	const long frames = (long) ceil(duration * sampleRate);

	double * buffer = (double *) calloc(frames, sizeof(double));
	if(buffer == NULL) {
		perror("malloc");
		exit(-1);
	}

	const int width = img.width;
	const int height = img.height;
	const pixel * pixels = img.pixels;
	
	double maxColor = 0;
	
	for(int x = 0; x < width; ++x) {
		for(int y = 0; y < height; ++y) {
			const long cur =	pixels[x + (y*width)].red + 
								pixels[x + (y*width)].green + 
								pixels[x + (y*width)].blue;
			if (cur > maxColor) {
				maxColor = cur;
			}
		}
	}
	
	//long frame = 0;
	
	double freqs[height + 1];
	double color[height + 1];


	
	for(int y = 1; y < height; ++y) {
		freqs[y] = maxFreq - (((double)y/(double)height)*maxFreq);
	}

	for(int x = 1; x <= width; ++x) {
		const int offset = (int)((x-1)*samplesPerLine);
		for(int y = 1; y < height; ++y) {
			const pixel & q = pixels[x+((height-y)*width)];
			color[y] =	pow( 10.0, (double) (colorCorrection - colorCorrection*
						(q.red + q.green + q.blue) / maxColor) );
		}
		for(int pos = 0; pos < samplesPerLine; ++pos) {
			double outFreq = 0.0;
			for(int y = 1; y < height; ++y) {
				const double t = ((double)pos / (double)sampleRate) * freqs[y];
				outFreq += sin(2 * M_PI * t)/color[y];
			}
			outFreq /= height+1;
			buffer[offset + pos] = outFreq;
		}
	}

	SF_INFO info;
	info.format = format;
	info.channels = 1;
	info.samplerate = sampleRate;


	SNDFILE *sndFile = sf_open("out.wav", SFM_WRITE, &info);
	if(sndFile == NULL) {
		cerr << "Unable to open output file for writing" << endl;
		free(buffer);
		exit(-1);
	}

	long writtenFrames = sf_writef_double(sndFile, buffer, frames);

	if(writtenFrames != frames) {
		cerr << "Failed to write the correct number of frames to the output file" << endl;
		sf_close(sndFile);
		free(buffer);
		exit(-1);
	}

	sf_write_sync(sndFile);
	sf_close(sndFile);
	free(buffer);

	return 0;
}
