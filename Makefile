PREFIX=/usr/local

DESTDIR=



ifdef P_FREETYPE
FT_ARG = -DNO_FREETYPE
else
FT_ARG = `freetype-config --cflags` `freetype-config --libs`
endif

CXX=g++
MPICXX=mpic++

CXXFLAGS= -O3 -Wall -Wno-deprecated $(FT_ARG)

INC=   -I$(PREFIX)/include/ -I/opt/local/include -I/mnt/netapp/home2/nchaimov/include

LIBS=  -L$(PREFIX)/lib/ -L/opt/local/lib -L/mnt/netapp/home2/nchaimov/lib -lz -lpng -lpngwriter -lsndfile -lm -Wl,-rpath,/mnt/netapp/home2/nchaimov/lib/ 

export TAU_MAKEFILE:=/mnt/netapp/home1/arash_khan/tau-2.19.1/x86_64/lib/Makefile.tau-pdt
TAUCXX=/mnt/netapp/home1/arash_khan/tau-2.19.1/x86_64/bin/tau_cxx.sh
TAUCC=/mnt/netapp/home1/arash_khan/tau-2.19.1/x86_64/bin/tau_cc.sh

INSTALL=install

SELF=make.include.linux

PRODUCTS=imgEncode imgEncode-tau imgEncode-omp imgEncode-omp-tau imgEncode-mpi
all: $(PRODUCTS)

imgEncode: imgEncode.cxx
	$(CXX) $(CXXFLAGS) $(INC) imgEncode.cxx -o imgEncode $(LIBS) 

imgEncode-tau: imgEncode.cxx
	$(TAUCXX) $(CXXFLAGS) $(INC) imgEncode.cxx -o imgEncode-tau $(LIBS) 

imgEncode-omp: imgEncode-omp.cxx
	$(CXX) -fopenmp $(CXXFLAGS) $(INC) imgEncode-omp.cxx -o imgEncode-omp $(LIBS) 

imgEncode-omp-tau: imgEncode-omp.cxx
	$(TAUCXX) -fopenmp $(CXXFLAGS) $(INC) imgEncode-omp.cxx -o imgEncode-omp-tau $(LIBS) 

imgEncode-mpi: imgEncode-mpi.cxx
	$(MPICXX) $(CXXFLAGS) $(INC) imgEncode-mpi.cxx -o imgEncode-mpi $(LIBS) 

.phony: clean all
	
clean:
	rm -f $(PRODUCTS) *.o imgEncode-mpi-tau
