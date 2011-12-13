//
// SharedMemory.h
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

#ifndef SHAREDMEMORY_H_INCLUDED
#define SHAREDMEMORY_H_INCLUDED

#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdexcept>
#include <boost/noncopyable.hpp>

namespace shmem {
    
    typedef enum {
        Create,
        OpenOrCreate,
        OpenExisting
    } OMODE;

    class Exception : public std::exception {
    protected:
        std::string _error;
    public:
        Exception( std::string error ) : _error(error) {}
        ~Exception() throw() {}
        const char* what() const throw() { return _error.c_str(); }
    };

    class SharedMemory : boost::noncopyable {
    private:
        int             size;
        std::string     name;
        std::string     filename;

        // key to shared memory segment
        key_t           shmkey;
        int             shmid;
        bool            created;
        void*           mem;

        void open(OMODE omode);

    public:
        SharedMemory();
        SharedMemory(const std::string& name, int size, OMODE omode = OpenOrCreate) ;

        ~SharedMemory() ;

        void init( const std::string& name, int size, OMODE omode = OpenOrCreate) ;

        int getSize() const { return size; }
        bool wasCreated() const { return created; }
        void* getSharedMemory() { return mem; }
        const std::string& getName() const { return name; }
        void setName(const std::string& value) { name = value; }
    };

}


#endif
