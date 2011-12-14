//
// Metrics.H
//
// Copyright (c) 2011, Alan Pearson
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modifica-
// tion, are permitted provided that the following conditions are met:
//
// 1) Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2) Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSE-
// QUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//

#ifndef METRICS_H_INCLUDED
#define METRICS_H_INCLUDED

#include "SharedMemory.H"
#include <boost/shared_ptr.hpp>
#include <boost/variant.hpp>
#include <vector>
#include <map>
#include <arpa/inet.h>
#include <sys/time.h>

#ifdef DARWIN
#include <libkern/OSAtomic.h>
#endif

#ifdef LINUX
#include <atomic>
#endif

namespace metrics {

	typedef int COUNTERID;
	typedef int METRICSID;
	typedef int INSTANCEID;

	const int COUNTERID_NULL = 0;


	// a 32-bit counter
	const int COUNTER_TYPE_32BIT =          0x00000001;
	// a 64-bit counter
	const int COUNTER_TYPE_64BIT =          0x00000002;
	// an 8-byte string
	const int COUNTER_TYPE_TEXT =           0x00000004;
	// an 8-byte indentifier, displayed as 16 hex characters
	const int COUNTER_TYPE_IDENT =          0x00000008;

	// a mask which, when combined with the flags, will return only the TYPE bits
	const int COUNTER_TYPE_MASK =           0x0000000f;


	// a count counter should be shown as absolute value
	const int COUNTER_FORMAT_COUNT =        0x00000000;

	// a delta counter should be shown as the change in value between 2 samples: i.e., delta(value)
	const int COUNTER_FORMAT_DELTA =        0x00010000;

	// a rate counter should be divided and shown as delta(value) / delta(time)
	const int COUNTER_FORMAT_RATE =         0x00020000;

	// a ratio counter should be shown as value / related_value
	const int COUNTER_FORMAT_RATIO =        0x00040000;

	const int COUNTER_FORMAT_TIMER =        0x00080000;

	// a mask which, when combined with the flags, will return only the FORMAT bits
	const int COUNTER_FORMAT_MASK =         0x000f0000;


	// a monotonic counter should be always increasing. a special trick is performed with 64-bit monotonic counters
	// to try and read them correctly without resorting to locking
	const int COUNTER_FLAG_MONOTONIC =      0x00100000;

	// the value for this counter is taken from the previous counter.  this is useful
	// primarily for RATIO counters where you want to have the absolute value and also
	// a ratio
	const int COUNTER_FLAG_USEPRIORVALUE =  0x00200000;

	// multiply the value by 100
	const int COUNTER_FLAG_PCT =  			0x00400000;

	// flag indicating that an instance slot has been allocated
	const int INSTANCE_FLAG_LIVE =          0x00000001;


	// the base size of the metrics defintion is 3 ints:
	//   the metrics ID
	//   the number of defined counters
	//   the maximum number of instances
	const int METRICS_DEFINITION_HEADER_SIZE =  (3*sizeof(int));

	// the base size of the instance data is 2 ints:
	//   a flags int indicating if the instance slot is in use
	//   the instance ID
	const int METRICS_INSTANCE_HEADER_SIZE =    (2*sizeof(int));

	// In the shared memory, a counter definition takes 2 ints:
	//      the id of the counter
	//      the counter flags
	//		counter description (32 chars)
	const int COUNTER_DEFINITION_SIZE =         (2*sizeof(int)) + sizeof(COUNTERID) + 32;

	template <typename T> T idFromString(const std::string& s)
	{
		BOOST_ASSERT(s.length() == 4);
		return (T)ntohl(*(int *)s.c_str());
	}

	inline long long getCurrentTimestamp() {
		timeval tv;
		gettimeofday(&tv,NULL);
		return (long long)tv.tv_sec * 1000ll + (long long)tv.tv_usec / 1000ll;
	}

	class MetricsDefinition;

	class Exception : public std::exception {
	protected:
		std::string _error;
	public:
		Exception( std::string error ) : _error(error) {}
		~Exception() throw() {}
		const char* what() const throw() { return _error.c_str(); }
	};

	class CounterDefinition {
	private:
		COUNTERID   ctrId;
		std::string description;
		int         flags;
		int         index;
		int         offset;
		COUNTERID   relatedCounterId;

	public:
		CounterDefinition(COUNTERID ctrId_in, std::string description_in, int flags_in, int offset_in, int index_in, COUNTERID relatedCounterId_in = COUNTERID_NULL);
		CounterDefinition(char* p, int offset_in, int index_in);

		COUNTERID getId() const { return ctrId; }
		COUNTERID getRelatedCounterId() const { return relatedCounterId; }
		int getFlags() const { return flags; }
		int getIndex() const { return index; }
		int getDataType() const { return flags & COUNTER_TYPE_MASK; }
		int getFormat() const { return flags & COUNTER_FORMAT_MASK; }
		const std::string& getDescription() const { return description; }
		std::string getName() const;
		int getCounterSize() const;

		void storeDefinitionToMemory(void* p) const;
	};

	typedef boost::shared_ptr<CounterDefinition> CounterDefinitionPtr;

	typedef boost::variant<int, long long, double, std::string> Variant;

	class Counter {
	protected:
		CounterDefinitionPtr definition;
		volatile void* dataptr;
		int datatype;

	public:
		Counter(CounterDefinitionPtr def, void* p);
		virtual ~Counter() = 0;
		virtual int asInt() { return 0; }
		virtual long long asLongLong() { return 0; }
		virtual double asDouble() { return 0.0; }
		CounterDefinitionPtr getDefinition() const { return definition; }
	};

	typedef boost::shared_ptr<Counter> CounterPtr;

	template <typename T> class AtomicOperation {
	public:
		static T get(volatile T* p) ;
		static T set(volatile T* p, T val) ;
		static T add(volatile T* p, T amt) ;
		static T increment(volatile T* p) ;
		static T decrement(volatile T* p) ;
	};

#ifdef DARWIN
	// atomic get
	template <> inline int AtomicOperation<int>::get(volatile int * p) { return OSAtomicAdd32(0,p); }
	template <> inline long long AtomicOperation<long long>::get(volatile long long * p) { return OSAtomicAdd64(0,p); }

	// atomic set
	template <> inline int AtomicOperation<int>::set(volatile int * p, int val) {
		int old;
		do {
			old = OSAtomicAdd32(0,p);
		} while(!OSAtomicCompareAndSwap32(old,val,p));
		return val;
	}
	template <> inline long long AtomicOperation<long long>::set(volatile long long * p, long long val) {
		long long old;
		do {
			old = OSAtomicAdd64(0,p);
		} while(!OSAtomicCompareAndSwap64(old,val,p));
		return val;
	}

	// atomic add
	template <> inline int AtomicOperation<int>::add(volatile int * p, int amt) { return OSAtomicAdd32(amt, p); }
	template <> inline long long AtomicOperation<long long>::add(volatile long long * p, long long amt) { return OSAtomicAdd64(amt,p); }

	// atomic increment
	template <> inline int AtomicOperation<int>::increment(volatile int * p) { return OSAtomicIncrement32(p); }
	template <> inline long long AtomicOperation<long long>::increment(volatile long long * p) { return OSAtomicIncrement64(p); }

	// atomic decrement
	template <> inline int AtomicOperation<int>::decrement(volatile int * p) { return OSAtomicDecrement32(p); }
	template <> inline long long AtomicOperation<long long>::decrement(volatile long long * p) { return OSAtomicDecrement64(p); }
#endif

#ifdef LINUX
	// atomic get
	template <typename T> inline T AtomicOperation<T>::get(volatile T * p) { return *(std::atomic<T>*)p; }
	// atomic set
	template <typename T> inline T AtomicOperation<T>::set(volatile T * p, T val) { return *((std::atomic<T>*)p) = val; }
	// atomic add
	template <typename T> inline T AtomicOperation<T>::add(volatile T * p, T val) { return *((std::atomic<T>*)p) += val; }
	// atomic increment
	template <typename T> inline T AtomicOperation<T>::increment(volatile T * p) { return ++(*((std::atomic<T>*)p)); }
	// atomic decrement
	template <typename T> inline T AtomicOperation<T>::decrement(volatile T * p) { return --(*((std::atomic<T>*)p)); }
#endif


	template <typename T> class NumericCounter : public Counter {
	protected:
		volatile T* dptr() { return reinterpret_cast<volatile T*>(dataptr); }
	public:
		NumericCounter(CounterDefinitionPtr def, void* p) : Counter(def,p) {
			BOOST_STATIC_ASSERT(boost::is_arithmetic<T>::value);
			BOOST_ASSERT(datatype == COUNTER_TYPE_32BIT || datatype == COUNTER_TYPE_64BIT);
		}
		virtual ~NumericCounter() {}
		T getValue() { return (dptr() ? AtomicOperation<T>::get(dptr()) : 0); }
		void setValue(T value) { if (dptr()) AtomicOperation<T>::set(dptr(),value); }
		void incrementBy(int value) { if (dptr()) AtomicOperation<T>::add(dptr(),value); }
		void increment() { if (dptr()) AtomicOperation<T>::increment(dptr()); }
		void decrement() { if (dptr()) AtomicOperation<T>::decrement(dptr()); }
		virtual int asInt() { return static_cast<int>(getValue()); }
		virtual long long asLongLong() { return static_cast<long long>(getValue()); }
		virtual double asDouble() { return static_cast<double>(getValue()); }
	};

	typedef boost::shared_ptr<NumericCounter<int> > IntCounterPtr;
	typedef boost::shared_ptr<NumericCounter<long long> > LargeCounterPtr;


	class TextCounter : public Counter {
	protected:
		volatile char* dptr() { return reinterpret_cast<volatile char*>(dataptr); }
	public:
		TextCounter(CounterDefinitionPtr def, void* p) : Counter(def,p) {}
		std::string getValue();
		void setValue(const std::string& value);
	};

	typedef boost::shared_ptr<TextCounter> TextCounterPtr;


	class Sample : public std::map<COUNTERID, Variant> {
	protected:
		long long time;
	public:
		Sample();
		void setSampleTime();
		long long getTime() const { return time; }
		void format(MetricsDefinition&, Sample& previousSample);
	};


	class MetricsInstance {
	private:
		MetricsDefinition * definition;
		std::vector<CounterPtr> counters;
		void * instanceData;
		bool cleanupOnDealloc;

	public:
		MetricsInstance(MetricsDefinition* def, void * p);
		~MetricsInstance();

		bool getCleanupOnDealloc() const { return cleanupOnDealloc; }
		void setCleanupOnDealloc(bool value) { cleanupOnDealloc = value; }

		INSTANCEID getInstanceId() const;
		bool isAlive() const ;

		CounterPtr getCounterByIndex(int index);
		CounterPtr getCounterById(COUNTERID id);
		CounterPtr getCounterForDefinition(CounterDefinitionPtr cdef) { return getCounterByIndex(cdef->getIndex()); }

		IntCounterPtr getIntCounterById(COUNTERID id) { return boost::dynamic_pointer_cast<NumericCounter<int> >(getCounterById(id)); }
		LargeCounterPtr getLargeCounterById(COUNTERID id) { return boost::dynamic_pointer_cast<NumericCounter<long long> >(getCounterById(id)); }
		TextCounterPtr getTextCounterById(COUNTERID id) { return boost::dynamic_pointer_cast<TextCounter>(getCounterById(id)); }

		// if instance is alive, reads all of the counters and puts their values into 
		// the map. if instance is not alive, the map is not updated, and false is returned
		bool sample(Sample& sample);
	};

	typedef boost::shared_ptr<MetricsInstance> MetricsInstancePtr;


	class MetricsDefinition {
	private:
		METRICSID metId;
		std::string name;
		std::vector<CounterDefinitionPtr> counterDefs;
		std::map<COUNTERID,CounterDefinitionPtr> counterMap;
		boost::shared_ptr<shmem::SharedMemory> shmem;
		int definitionSize;
		int instanceSize;
		int maxInstances;
		
		// pointer to the first instance within the shared memory
		void * instanceData;

	public:
		// initialize a MetricsDefinition object
		MetricsDefinition(METRICSID metId, int maxInstances=1);
		MetricsDefinition(std::string name, int maxInstances=1);
		~MetricsDefinition();

		// accessors
		METRICSID getMetricsId() const { return metId; }
		const std::string& getName() const { return name; }
		int getMaxInstances() const { return maxInstances; }
		int getInstanceSize() const { return instanceSize; }

		// initialize the shared memory with the counter definitions or attach to
		// an existing shared memory and load the counter definitions from it
		void initialize();

		// for a single-instance metrics definition, get the single instance
		MetricsInstancePtr getInstance();

		// for a multiple-instance metrics definition, allocate an instance
		MetricsInstancePtr allocInstance(INSTANCEID instId);

		// for a multiple-instance metrics definition, get the instance at a specific index
		MetricsInstancePtr getInstanceByIndex(int index);

		// add a counter definition to the metrics definition
		CounterDefinitionPtr defineCounter(const std::string& ctrName, const std::string& description, int flags, COUNTERID relatedCounterId = COUNTERID_NULL);
		CounterDefinitionPtr defineCounter(COUNTERID ctrId, const std::string& description, int flags, COUNTERID relatedCounterId = COUNTERID_NULL);

		const std::vector<CounterDefinitionPtr>& getCounterDefinitions() const { return counterDefs; }

		CounterDefinitionPtr getCounterDefinition(int index);
		CounterDefinitionPtr getCounterDefinitionById(COUNTERID ctrId);
	};

	typedef boost::shared_ptr<MetricsDefinition> MetricsDefinitionPtr;


	//-----------------------------------------------------------------
	// ScopeTimer
	//
	// Utility class to measure how much time is spent within a block
	// of code and report that value in a counter.
	//
	class ScopeTimer {
	protected:
		LargeCounterPtr ctr;
		long long startTime;
	public:
		ScopeTimer(LargeCounterPtr ctr) : ctr(ctr) {
			// validate that the counter is the correct type of counter
			//BOOST_ASSERT(ctr->getDefinition()->getFlags() == (COUNTER_TYPE_64BIT | COUNTER_FLAG_MONOTONIC | COUNTER_FORMAT_RATE));
			startTime = getCurrentTimestamp();
		}
		~ScopeTimer() {
			long long scopeTime = getCurrentTimestamp() - startTime;
			ctr->incrementBy(scopeTime);
		}
	};

}

#endif

