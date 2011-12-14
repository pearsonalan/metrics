OSTYPE = $(shell uname -s)

ifneq ($(findstring MINGW,$(OSTYPE)),)
  OSTYPE = Win32
else
endif

all: shmtest ctrtest ctrview atomictest

clean:
	-rm *.o ctrtest shmtest ctrview atomictest

CXXFLAGS = -Wall -g
LINKFLAGS = -g

ifeq ($(OSTYPE),Darwin)
  CXXFLAGS += -DDARWIN
endif

ifeq ($(OSTYPE),Linux)
  CXXFLAGS += -DLINUX -Wno-multichar -std=gnu++0x
endif

SHMEMOBJS=SharedMemory.o shmtest.o 
CTRTESTOBJS=Metrics.o SharedMemory.o ctrtest.o
CTRVIEWOBJS=Metrics.o SharedMemory.o ctrview.o
ATOMICTESTOBJS=SharedMemory.o atomictest.o

shmtest: $(SHMEMOBJS)
	g++ $(LINKFLAGS) -o $@ $^

ctrtest: $(CTRTESTOBJS)
	g++ $(LINKFLAGS) -o $@ $^ -lcurses

ctrview: $(CTRVIEWOBJS)
	g++ $(LINKFLAGS) -o $@ $^ -lcurses

atomictest: $(ATOMICTESTOBJS)
	g++ $(LINKFLAGS) -o $@ $^ -lboost_thread

%.o: %.C
	g++ -c $(CXXFLAGS) -o $@ $^

