//
// Metrics.C
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

#include <boost/assert.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <sys/time.h>

#include "Metrics.H"

namespace metrics {

	//------------------------------------------------------------------------------
	// CounterDefinition
	//

	CounterDefinition::CounterDefinition(COUNTERID ctrId, std::string description_in, int flags, int offset, int index, COUNTERID relatedCtrId) :
		ctrId(ctrId),
		description(description_in),
		flags(flags),
		index(index),
		offset(offset),
		relatedCounterId(relatedCtrId)
	{
	}

	CounterDefinition::CounterDefinition(char* p, int offset_in, int index_in) :
		ctrId(0),
		flags(0),
		index(index_in),
		offset(offset_in),
		relatedCounterId(COUNTERID_NULL)
	{
		int* c = (int*)p;
		ctrId = c[0];
		flags = c[1];
		relatedCounterId = c[2];

		char buf[33] = {0};
		strncpy(buf, p + 3*sizeof(int), 32);
		description = std::string(buf);
	}

	std::string CounterDefinition::getName() const 
	{
		char sz[5];
		*(int*)sz = htonl(ctrId);
		sz[4] = 0;
		return std::string(sz);
	}

	void CounterDefinition::storeDefinitionToMemory(void * p) const
	{
		int * c = (int*)p;
		c[0] = ctrId;
		c[1] = flags;
		c[2] = relatedCounterId;
		strncpy((char *)p + 3*sizeof(int),description.c_str(),32);
	}

	int CounterDefinition::getCounterSize() const
	{
		if (flags & COUNTER_TYPE_64BIT)
			return sizeof(long long);
		if (flags & COUNTER_TYPE_32BIT)
			return sizeof(int);
		if (flags & COUNTER_TYPE_TEXT)
			return 8;
		if (flags & COUNTER_TYPE_IDENT)
			return 8;
		// should not be reached if the counter flags are valid
		BOOST_ASSERT(false);
		throw Exception("Undefined counter type");
	}


	//------------------------------------------------------------------------------
	// Counter
	//

	Counter::Counter(CounterDefinitionPtr def, void* p) :
		definition(def),
		dataptr(p),
		datatype(definition->getFlags() & COUNTER_TYPE_MASK)
	{
	}

	Counter::~Counter()
	{
	}


	//------------------------------------------------------------------------------
	// TextCounter
	//

	std::string TextCounter::getValue() 
	{
		char buf[9] = {0};
		strncpy(buf, dptr(), 8);
		return std::string(buf);
	}

	void TextCounter::setValue(const std::string& value) 
	{
		strncpy(dptr(),value.c_str(),8);
	}


	//------------------------------------------------------------------------------
	// MetricsInstance
	//

	MetricsInstance::MetricsInstance(MetricsDefinition* mdef, void* p) :
		definition(mdef),
		instanceData(p),
		cleanupOnDealloc(false)
	{
		char* counterBuffer = (char*)p + METRICS_INSTANCE_HEADER_SIZE;
		
		// go through all of the counter definitions and allocate a counter for them
		const std::vector<CounterDefinitionPtr>& counterDefinitions = definition->getCounterDefinitions();
		BOOST_FOREACH(CounterDefinitionPtr ctrdef, counterDefinitions) {
			CounterPtr counter;

			switch (ctrdef->getFlags() & COUNTER_TYPE_MASK) {
			case COUNTER_TYPE_32BIT:
				counter = CounterPtr(new NumericCounter<int>(ctrdef,counterBuffer));
				break;
			case COUNTER_TYPE_64BIT:
				counter = CounterPtr(new NumericCounter<long long>(ctrdef,counterBuffer));
				break;
			case COUNTER_TYPE_TEXT:
				counter = CounterPtr(new TextCounter(ctrdef,counterBuffer));
				break;
			default:
				BOOST_ASSERT(false && "Invalid counter type");
				throw Exception("Invalid counter type");
			}

			BOOST_ASSERT(counter);
			counters.push_back(counter);
			counterBuffer += ctrdef->getCounterSize();
		}
	}

	MetricsInstance::~MetricsInstance() 
	{
		if (cleanupOnDealloc) {
			bzero(instanceData,definition->getInstanceSize());
		}
	}

	INSTANCEID MetricsInstance::getInstanceId() const
	{
		return *((int*)instanceData+1);
	}

	bool MetricsInstance::isAlive() const 
	{
		return (*((int*)instanceData) & INSTANCE_FLAG_LIVE) != 0;
	}

	CounterPtr MetricsInstance::getCounterByIndex(int index)
	{
		if (index < 0 || index >= (int) counters.size()) {
			throw Exception("Invalid index");
		}
		return counters[index];
	}

	CounterPtr MetricsInstance::getCounterById(COUNTERID id)
	{
		CounterDefinitionPtr cdef = definition->getCounterDefinitionById(id);
		if (!cdef) {
			throw Exception("Counter not found");
		}
		return counters[cdef->getIndex()];
	}

	bool MetricsInstance::sample(Sample& sample)
	{
		sample.setSampleTime();
		if ((*((int*)instanceData) & INSTANCE_FLAG_LIVE) == 0)
			return false;
		BOOST_FOREACH(CounterPtr ctr, counters) {
			CounterDefinitionPtr cdef = ctr->getDefinition();

			// if the counter has a related counter ID, we don't need to get the value for this 
			// counter since it gets its data from a different one
			if (cdef->getRelatedCounterId() != COUNTERID_NULL)
				continue;

			Variant v;
			switch (cdef->getDataType()) {
			case COUNTER_TYPE_32BIT:
			case COUNTER_TYPE_64BIT:
				v = Variant(ctr->asDouble());
				break;
			case COUNTER_TYPE_TEXT:
				v = Variant(boost::dynamic_pointer_cast<TextCounter>(ctr)->getValue());
				break;
			}
			sample.insert(std::make_pair(cdef->getId(),v));
		}
		return true;
	}


	//------------------------------------------------------------------------------
	// MetricsDefinition
	//

	MetricsDefinition::MetricsDefinition(METRICSID metId_in, int maxInstances_in) :
		metId(metId_in),
		definitionSize(0),
		instanceSize(0),
		maxInstances(maxInstances_in)
	{
		// convert the metrics ID into a name
		char sz[5];
		*(int*)sz = htonl(metId);
		sz[4] = 0;
		name = std::string(sz);

		// the base size of the definition is 3 ints:
		//	 the metrics ID
		//   the number of defined counters
		//   the maximum number of instances
		definitionSize = METRICS_DEFINITION_HEADER_SIZE;

		// the base size of the instance data is 2 ints:
		//   a flags int indicating if the instance slot is in use
		//   the instance ID
		instanceSize = METRICS_INSTANCE_HEADER_SIZE;
	}

	MetricsDefinition::MetricsDefinition(std::string name_in, int maxInstances_in) :
		metId(idFromString<METRICSID>(name_in)),
		definitionSize(0),
		instanceSize(0),
		maxInstances(maxInstances_in)
	{
		// convert the metrics ID into a name
		char sz[5];
		*(int*)sz = htonl(metId);
		sz[4] = 0;
		name = std::string(sz);

		// the base size of the definition is 3 ints:
		//	 the metrics ID
		//   the number of defined counters
		//   the maximum number of instances
		definitionSize = METRICS_DEFINITION_HEADER_SIZE;

		// the base size of the instance data is 2 ints:
		//   a flags int indicating if the instance slot is in use
		//   the instance ID
		instanceSize = METRICS_INSTANCE_HEADER_SIZE;
	}

	MetricsDefinition::~MetricsDefinition()
	{
	}

	void MetricsDefinition::initialize()
	{
		int i;

		// compute the total size of the shared memory needed
		int totalSize = definitionSize + maxInstances * instanceSize;

		//std::cout << "total size for metrics = " << totalSize << std::endl;

		shmem = boost::shared_ptr<shmem::SharedMemory>(new shmem::SharedMemory(name,totalSize,counterDefs.size() == 0 ? shmem::OpenExisting : shmem::OpenOrCreate));
		if (shmem->wasCreated()) {
			//std::cout << "Created new shared mem" << std::endl;

			// if we created the shared memory, as opposed to opening an existing one, we need to init the memory
			char * p = (char*) shmem->getSharedMemory();
			bzero(p,totalSize);

			// store the ID, instance count, and number of counters
			int count = counterDefs.size();
			*((int*)p + 0) = metId;
			*((int*)p + 1) = count;
			*((int*)p + 2) = maxInstances;

			// now store all of the counter definitions
			p += METRICS_DEFINITION_HEADER_SIZE;
			for (i = 0; i < count; i++) {
				CounterDefinitionPtr ctrdef = counterDefs.at(i);
				ctrdef->storeDefinitionToMemory(p);
				p += COUNTER_DEFINITION_SIZE;
			}

			instanceData = p;

		} else {
			// std::cout << "Opened existing shared mem" << std::endl;

			// if we opened an existing shared memory check the values and init the counters 
			char* p = (char*) shmem->getSharedMemory();

			// validate the metrics ID in the shared memory
			if (*((int*)p + 0) != metId) 
				throw Exception("Invalid metric id");

			// we may still be attaching to an existing shared memory with a counter definition
			// or attaching to the shared memory with no counter definitions, in which case the
			// counter definitions are loaded from the memory
			int count = counterDefs.size();
			if (count) {
				// validate what is in the shared memory with what we were initialized with
				if (*((int*)p + 1) != count)
					throw Exception("Invalid counter count in metrics");
				if (*((int*)p + 2) != maxInstances)
					throw Exception("Invalid max instance count in metrics");

				// validate list of counters we were defined with against list of counters in the shared memory
				p += METRICS_DEFINITION_HEADER_SIZE;
				for (i = 0; i < count; i++) {
					CounterDefinitionPtr ctr = counterDefs.at(i);

					COUNTERID ctrid = *((int*)p + 0);
					int flags = *((int*)p + 1);

					if (ctrid != ctr->getId())
						throw Exception(std::string("Unexpected counter id in metrics definition: expected ") + boost::lexical_cast<std::string>(ctr->getId()) + " found " + boost::lexical_cast<std::string>(ctrid));
					if (flags != ctr->getFlags())
						throw Exception("Unexpected counter flags in metrics definition");

					p += COUNTER_DEFINITION_SIZE;
				}

				instanceData = p;

				// initialize all of the instance data to zero
				BOOST_ASSERT((char *)instanceData + (instanceSize * maxInstances) == (char *)shmem->getSharedMemory() + shmem->getSize());
				bzero(instanceData, instanceSize * maxInstances);

			} else {
				// std::cout << "Reading counter defintions from shared mem" << std::endl;

				// initialize the counter set from the shared memory
				count = *((int*)p + 1);
				maxInstances = *((int*)p + 2);

				p += METRICS_DEFINITION_HEADER_SIZE;
				int offset = METRICS_INSTANCE_HEADER_SIZE;
				for (i = 0; i < count; i++) {
					CounterDefinitionPtr ctrDef(new CounterDefinition(p,offset,i));

					// store the counter definition in the array of definitions and in the MAP
					counterDefs.push_back(ctrDef);
					counterMap[ctrDef->getId()] = ctrDef;

					p += COUNTER_DEFINITION_SIZE;
					offset += ctrDef->getCounterSize();

					// each counter adds a counter ID and flags to the definition chunk
					definitionSize += COUNTER_DEFINITION_SIZE;
					
					// each counter adds a location for the actual counter data. the size of the data depends on the type of the counter
					instanceSize += ctrDef->getCounterSize();
				}

				instanceData = p;
			}

			BOOST_ASSERT(instanceData != NULL);
			BOOST_ASSERT(count > 0);
		}
	}

	CounterDefinitionPtr MetricsDefinition::defineCounter(const std::string& ctrName, const std::string& description, int flags, COUNTERID relatedCounterId)
	{
		return defineCounter(idFromString<COUNTERID>(ctrName),description,flags,relatedCounterId);
	}

	CounterDefinitionPtr MetricsDefinition::defineCounter(COUNTERID ctrId, const std::string& description, int flags, COUNTERID relatedCounterId)
	{
		CounterDefinitionPtr ctrDef(new CounterDefinition(ctrId,description,flags,instanceSize,counterDefs.size(),relatedCounterId));

		// each counter adds a counter ID and flags to the definition chunk
		definitionSize += COUNTER_DEFINITION_SIZE;

		// each counter adds a location for the actual counter data. the size of the
		// data depends on the type of the counter
		instanceSize += ctrDef->getCounterSize();

		// store the counter definition in the array of definitions and in the MAP
		counterDefs.push_back(ctrDef);
		counterMap[ctrId] = ctrDef;

		return ctrDef;
	}

	CounterDefinitionPtr MetricsDefinition::getCounterDefinition(int index) 
	{
		return counterDefs[index];
	}

	CounterDefinitionPtr MetricsDefinition::getCounterDefinitionById(COUNTERID ctrId)
	{
		return counterMap[ctrId];
	}

	// for a single-instance metrics definition, get the single instance
	MetricsInstancePtr MetricsDefinition::getInstance()
	{
		BOOST_ASSERT(maxInstances == 1 && "Invalid to invoke this method on a multi-instance definition");
		BOOST_ASSERT(instanceData != NULL && "Invalid instance data pointer");

		int * p = (int*)instanceData;

		if (p[0] & INSTANCE_FLAG_LIVE) {
			// instance already live.
			BOOST_ASSERT(p[1] == metId && "Unexpected instance ID");
		} else {
			// instance is not currently live.  we are allocating it for the first time

			// clear the memory
			bzero(p,instanceSize);

			// set the flags and instance ID:
			p[0] = INSTANCE_FLAG_LIVE;
			p[1] = metId;			// for single instances, the instance ID is the same as the metrics ID
		}

		return MetricsInstancePtr(new MetricsInstance(this,p));
	}

	// for a multiple-instance metrics definition, allocate an instance
	MetricsInstancePtr MetricsDefinition::allocInstance(INSTANCEID instId)
	{
		BOOST_ASSERT(maxInstances > 1 && "Invalid to invoke this method on a single-instance definition");
		BOOST_ASSERT(instanceData != NULL && "Invalid instance data pointer");

		// iterate through all of the instance slots looking for an unused slot
		for (int i = 0; i < maxInstances; i++ ) {
			int * p = (int*)((char*)instanceData + (i * instanceSize));
			
			if (!(p[0] & INSTANCE_FLAG_LIVE)) {
				// clear the memory
				bzero(p,instanceSize);

				// set the flags and instance ID:
				p[0] = INSTANCE_FLAG_LIVE;
				p[1] = instId;

				MetricsInstancePtr inst(new MetricsInstance(this,p));
				inst->setCleanupOnDealloc(true);
				return inst;
			}
		}

		return MetricsInstancePtr();
	}

	// for a multiple-instance metrics definition, get the instance at a specific index
	MetricsInstancePtr MetricsDefinition::getInstanceByIndex(int index)
	{
		BOOST_ASSERT(maxInstances > 1 && "Invalid to invoke this method on a single-instance definition");
		BOOST_ASSERT(instanceData != NULL && "Invalid instance data pointer");
		if (index < 0 || index >= maxInstances)
			throw Exception("Invalid index");
		int * p = (int*)((char*)instanceData + (index * instanceSize));
		return MetricsInstancePtr( new MetricsInstance(this,p));
	}


	//------------------------------------------------------------------------------
	// Sample
	//

	Sample::Sample() 
	{
	}

	void Sample::setSampleTime()
	{
		timeval tv;
		gettimeofday(&tv,NULL);
		time = (long long)tv.tv_sec * 1000ll + (long long)tv.tv_usec / 1000ll;
	}

	void Sample::format(MetricsDefinition& mdef, Sample& prev) 
	{
		int index = 0;
		BOOST_FOREACH(CounterDefinitionPtr ctrdef, mdef.getCounterDefinitions()) {
			// don't do any formatting for TEXT
			if (ctrdef->getDataType() == COUNTER_TYPE_TEXT)
				continue;

			if (prev.size() == 0)
				continue;

			COUNTERID id = ctrdef->getId();
			COUNTERID relid = ctrdef->getRelatedCounterId();
			Variant val,prevval;

			if ((ctrdef->getFlags() & COUNTER_FLAG_USEPRIORVALUE) && index > 0) {
				CounterDefinitionPtr priordef = mdef.getCounterDefinition(index-1);
				id = priordef->getId();
			}

			if (ctrdef->getFormat() == COUNTER_FORMAT_RATIO) {
				val = this->operator[](id);
			} else {
				val = (relid == COUNTERID_NULL ? this->operator[](id) : this->operator[](relid));
				prevval = (relid == COUNTERID_NULL ? prev[id] : prev[relid]);
				if (prevval.which() != 2) prevval = Variant((double)0.0);
			}

			if (val.which() != 2) val = Variant((double)0.0);

			switch (ctrdef->getFormat()) {
			case COUNTER_FORMAT_COUNT:
				// nothing to do
				break;
			case COUNTER_FORMAT_DELTA:
				val = Variant(boost::get<double>(val) - boost::get<double>(prevval));
				break;
			case COUNTER_FORMAT_RATE:
				val = Variant((boost::get<double>(val) - boost::get<double>(prevval)) * 1000.0 / (double)(time - prev.time));
				break;
			case COUNTER_FORMAT_RATIO:
				if (boost::get<double>(this->operator[](relid)) != 0.0)
					val = Variant(boost::get<double>(val) / boost::get<double>(this->operator[](relid)));
				else
					val = Variant((double)0.0);
				break;
			}

			if (ctrdef->getFlags() & COUNTER_FLAG_PCT) {
				val = Variant(boost::get<double>(val) * 100.0);
			}

			this->operator[](ctrdef->getId()) = val;
			index++;
		}
	}
}

