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

namespace metrics {

	typedef int COUNTERID;
	typedef int METRICSID;
	typedef int INSTANCEID;

	// a 32-bit counter
	const int COUNTER_TYPE_32BIT =          0x00000001;
	// a 64-bit counter
	const int COUNTER_TYPE_64BIT =          0x00000002;
	// an 8-byte string
	const int COUNTER_TYPE_TEXT =           0x00000004;
	// an 8-byte indentifier, displayed as 16 hex characters
	const int COUNTER_TYPE_IDENT =          0x00000008;

	// a mask which, when combined with the flags, will return onlythe TYPE bits
	const int COUNTER_TYPE_MASK =           0x0000000f;

	// a rate counter should be divided and shown as delta(value) / delta(time)
	const int COUNTER_FLAG_RATE =           0x00010000;

	// a monotonic counter should be always increasing. a special trick is performed with 64-bit monotonic counters
	// to try and read them correctly without resorting to locking
	const int COUNTER_FLAG_MONOTONIC =      0x00020000;

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
	const int COUNTER_DEFINITION_SIZE =         (2*sizeof(int));

	template <typename T> T idFromString(const std::string& s)
	{
		BOOST_ASSERT(s.length() == 4);
		return (T)ntohl(*(int *)s.c_str());
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
		int         flags;
		int         index;
		int         offset;

	public:
		CounterDefinition(COUNTERID ctrId_in, int flags_in, int offset_in, int index_in);
		CounterDefinition(char* p, int offset_in, int index_in);

		COUNTERID getId() const { return ctrId; }
		int getFlags() const { return flags; }
		int getIndex() const { return index; }
		int getDataType() const { return flags & COUNTER_TYPE_MASK; }
		std::string getName() const;
		int getCounterSize() const;

		void storeDefinitionToMemory(void* p) const;
	};

	typedef boost::shared_ptr<CounterDefinition> CounterDefinitionPtr;

	typedef boost::variant<int, long long, std::string> Variant;

	class Counter {
	protected:
		CounterDefinitionPtr definition;
		void* dataptr;
		int datatype;

	public:
		Counter(CounterDefinitionPtr def, void* p);
		virtual ~Counter() = 0;
		CounterDefinitionPtr getDefinition() const { return definition; }
	};

	typedef boost::shared_ptr<Counter> CounterPtr;

	template <typename T> class NumericCounter : public Counter {
	protected:
		T previousValue;
		T* dptr() { return reinterpret_cast<T*>(dataptr); }
	public:
		NumericCounter(CounterDefinitionPtr def, void* p) : Counter(def,p), previousValue(0) {
			BOOST_STATIC_ASSERT(boost::is_arithmetic<T>::value);
			BOOST_ASSERT(datatype == COUNTER_TYPE_32BIT || datatype == COUNTER_TYPE_64BIT);
		}
		virtual ~NumericCounter() {}
		T getValue() { return previousValue = (dptr() ? *dptr() : 0); }
		T getPreviousValue() { return previousValue; }
		void setValue(T value) { if (dptr()) *dptr() = value; }
		void incrementBy(int value) { if (dptr()) *dptr() += value; }
		void increment() { if (dptr()) (*dptr())++; }
		void decrement() { if (dptr()) (*dptr())--; }
	};

	// template specialization for 64 bit numeric counter
	template <> inline long long NumericCounter<long long>::getValue() {
		if (!dataptr) 
			return 0;
		long long value = *dptr();
		// special handling if the value is monotonically increasing
		if (definition->getFlags() & COUNTER_FLAG_MONOTONIC) {
			// re-read if we got a bad read
			if (((unsigned long long)value > ((unsigned long long)previousValue) + 0x100000000ll) || 
				((unsigned long long)value < (unsigned long long)previousValue)) {
				int r = 0;
				long long badvalue = value;
				// loop until we get a new read or we try 10 times
				while (value == badvalue && r < 10) {
					usleep(1);
					value = *dptr();
					r ++;
				}
			}
		}
		previousValue = value;
		return value;
	}

	typedef boost::shared_ptr<NumericCounter<int> > IntCounterPtr;
	typedef boost::shared_ptr<NumericCounter<long long> > LargeCounterPtr;


	class TextCounter : public Counter {
	protected:
		char* dptr() { return reinterpret_cast<char*>(dataptr); }
	public:
		TextCounter(CounterDefinitionPtr def, void* p) : Counter(def,p) {}
		std::string getValue();
		void setValue(const std::string& value);
	};

	typedef boost::shared_ptr<TextCounter> TextCounterPtr;


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
		bool sample(std::map<COUNTERID, Variant>& data);
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
		CounterDefinitionPtr defineCounter(std::string ctrName, int flags);
		CounterDefinitionPtr defineCounter(COUNTERID ctrId, int flags);

		const std::vector<CounterDefinitionPtr>& getCounterDefinitions() const { return counterDefs; }

		CounterDefinitionPtr getCounterDefinition(int index);
		CounterDefinitionPtr getCounterDefinitionById(COUNTERID ctrId);
	};
}

#endif

