all: shmtest ctrtest ctrview

clean:
	-rm *.o ctrtest shmtest ctrview

CXXFLAGS = -Wall -g
LINKFLAGS = -g

SHMEMOBJS=SharedMemory.o shmtest.o 
CTRTESTOBJS=Metrics.o SharedMemory.o ctrtest.o
CTRVIEWOBJS=Metrics.o SharedMemory.o ctrview.o

shmtest: $(SHMEMOBJS)
	g++ $(LINKFLAGS) -o $@ $^

ctrtest: $(CTRTESTOBJS)
	g++ $(LINKFLAGS) -o $@ $^ -lcurses

ctrview: $(CTRVIEWOBJS)
	g++ $(LINKFLAGS) -o $@ $^ -lcurses

%.o: %.C
	g++ -c $(CXXFLAGS) -o $@ $^

