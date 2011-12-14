// atomictest.C

#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/thread/thread_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
#include "SharedMemory.H"

#ifdef DARWIN
#include <libkern/OSAtomic.h>
#endif

#ifdef LINUX
#include <atomic>
#endif

//#define USEATOMIC

boost::mutex ioMutex;
volatile long long * volatile n;

class WriterThread {
protected:
	std::string name;

public:
	WriterThread(std::string name) : name(name) {};
	void operator()() {
		{
			boost::mutex::scoped_lock lock(ioMutex);
			std::cout << "starting thread " << name << std::endl;
		}

		for (int i = 0; i < 25000000; i++) {
#ifdef USEATOMIC
#ifdef DARWIN
			OSAtomicAdd64(1000,n);
#endif
#ifdef LINUX
			(*(std::atomic<long long>*)n) += 1000;
#endif
#else
			*n += 1000;
#endif
		}

		{
			boost::mutex::scoped_lock lock(ioMutex);
			std::cout << "exiting thread " << name << std::endl;
		}
	}	
};

class ReaderThread {
protected:
	std::string name;
	bool isTerminating;

public:
	ReaderThread(std::string name) : name(name), isTerminating(false) {};
	void terminate() { isTerminating = true; }

	void operator()() {
		{
			boost::mutex::scoped_lock lock(ioMutex);
			std::cout << "starting thread " << name << std::endl;
		}

		while (!isTerminating) {
			boost::this_thread::sleep(boost::posix_time::milliseconds(100));
			int64_t val;

#ifdef USEATOMIC			
#ifdef DARWIN
			val = OSAtomicAdd64(0,n);
#endif
#ifdef LINUX
			val = *((std::atomic<long long>*)n);
#endif
#else
			val = *n;
#endif

			boost::mutex::scoped_lock lock(ioMutex);
			std::cout << "value = " << val << std::endl;
		}

		{
			boost::mutex::scoped_lock lock(ioMutex);
			std::cout << "exiting thread " << name << std::endl;
		}
	}	
};

int main() {
	boost::thread_group threads;

	shmem::SharedMemory shm("atomic-test",12,shmem::OpenOrCreate);
	n = (long long *) shm.getSharedMemory();

	for (int i = 0; i < 8; i++) {
		// launch threads
		threads.create_thread(WriterThread(std::string("worker-") + boost::lexical_cast<std::string>(i)));
	}

	ReaderThread r("reader");
	boost::thread readerThread(boost::ref(r));

	threads.join_all();
	r.terminate();
	readerThread.join();

	std::cout <<  "result = " << *n << std::endl;
	return 0;
}