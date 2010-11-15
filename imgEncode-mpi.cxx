#include <pngwriter.h>
#include <math.h>
#include <iostream.h>
#include <string>
#include <stdlib.h>
#include <omp.h>
#include <stdio.h>
#include <sndfile.h>
#include <mpi.h>

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

int nprocs;
int myid;

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
			pixel & q = img[y + (x*height)];
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

double * buffer;
double duration;
long frames;


int main(int argc, char ** argv)
{
	MPI_Init(&argc, &argv);
	
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	
	image * img;
	
	int width;
	int height;
	double maxColor;
	pixel * pixels = NULL;
		
	if(myid == 0) {
		if(argc < 2) {
			cerr << "Usage: " << argv[0] << " inputFileName.png" << endl;
			MPI_Abort(MPI_COMM_WORLD, 2);
		}
		img = &readImage(argv[1]);
			
		duration = durationPerLine * img->width;
		frames = (long) ceil(duration * sampleRate);

		buffer = (double *) calloc(frames, sizeof(double));
		if(buffer == NULL) {
			perror("malloc");
			exit(-1);
		}

		width = img->width;
		height = img->height;
		pixels = img->pixels;
	
		maxColor = 0;
	
		for(int x = 0; x < width; ++x) {
			for(int y = 0; y < height; ++y) {
				const long cur =	pixels[y + (x*height)].red + 
									pixels[y + (x*height)].green + 
									pixels[y + (x*height)].blue;
				if (cur > maxColor) {
					maxColor = cur;
				}
			}
		}
	}
	
	MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&maxColor, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	
	
	int numLines = width/nprocs;	
	int myStart = (numLines*myid);
	if(myid == nprocs-1) {
		numLines += width % nprocs;
	}
	int myEnd = myStart + numLines - 1;
	
	pixel * myPixels;
	if(myid == 0) {
		myPixels = pixels;
	} else {
		myPixels = (pixel *) calloc((numLines+1) * (height+1), sizeof(pixel));
	}
	
	if(myid == 0) {
		for(int i = 1; i < nprocs-1; ++i) {
			MPI_Send(&(pixels[numLines*i*height]), numLines*height*3, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
		MPI_Send(&(pixels[numLines*(nprocs-1)*height]), (numLines + (width % nprocs))*height*3, MPI_INT, nprocs-1, 0, MPI_COMM_WORLD);
	} else {
		MPI_Status s;
		MPI_Recv(&(myPixels[height]), numLines*height*3, MPI_INT, 0, 0, MPI_COMM_WORLD, &s);
	}
	
	
	double freqs[height + 1];

	for(int y = 1; y < height; ++y) {
		freqs[y] = maxFreq - (((double)y/(double)height)*maxFreq);
	}
	
	if(myid != 0) {
		buffer = (double*) calloc(numLines*samplesPerLine, sizeof(double));
	}
	
	for(int x = 1; x <= numLines; ++x) {
		const int offset = (int)((x-1)*samplesPerLine);
		double color[height + 1];
		for(int y = 1; y < height; ++y) {
			const pixel & q = myPixels[(height-y)+(x*height)];
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
	
	cout << "Process " << myid << " finished calculating" << endl;
	
	if(myid == 0) {
		MPI_Status s;
		for(int i = 1; i < nprocs-1; ++i) {
			MPI_Recv(&(buffer[i*numLines*(int)samplesPerLine]), numLines*samplesPerLine, MPI_DOUBLE, i, 2, MPI_COMM_WORLD, &s);
		}
		MPI_Recv(&(buffer[(nprocs-1)*numLines*(int)samplesPerLine]), (numLines + (width % nprocs))*samplesPerLine, MPI_DOUBLE, nprocs-1, 2, MPI_COMM_WORLD, &s);
	} else {
		MPI_Send(buffer, numLines*samplesPerLine, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
	}
	
	if(myid == 0) {
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
	}
	free(buffer);
	
	
	MPI_Finalize();
	return 0;
}
