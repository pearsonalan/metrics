//
// SharedMemory.C
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

#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "SharedMemory.H"

namespace shmem {

    const char * kSharedMemoryDirectory = "/tmp";

    SharedMemory::SharedMemory() :
        size(0),
        shmkey(0),
        shmid(0),
        created(false),
        mem(NULL)
    {
    }

    SharedMemory::SharedMemory( const std::string& name, int size, OMODE omode ) :
        size(size),
        name(name),
        shmkey(0),
        shmid(0),
        created(false),
        mem(NULL)
    {
        open(omode);
    }

    SharedMemory::~SharedMemory() {
        struct shmid_ds ds;
        
        if (mem) {
            //std::cout << "Detaching shared memory " << std::hex << shmkey << std::endl;;
            shmdt(mem);
            mem = NULL;
        }
        
        if (shmctl(shmid, IPC_STAT, &ds) == 0) {
            //std::cout << "Shared memory " << std::hex << shmkey << " has " << std::dec << ds.shm_nattch << " attaches" << std::endl;
        }
        
        if (shmid != -1 && ds.shm_nattch == 0) {
            //std::cout << "Removing shared memory " << std::hex << shmkey << " (shmid " << std::dec << shmid << ")" << std::endl;
            shmctl(shmid, IPC_RMID, NULL);
            shmid = -1;
            unlink(filename.c_str());
        }
    }

    void SharedMemory::open(OMODE omode)
    {
        struct stat s;

        // check for existance of the directory
        if (stat(kSharedMemoryDirectory,&s) != 0) {
            // try to create the directory
            if (mkdir(kSharedMemoryDirectory, 0755) != 0) {
                throw Exception(std::string("Cannot create directory for shared memory file at ") + kSharedMemoryDirectory);
            }
        } else {
            if ((s.st_mode & S_IFDIR) != S_IFDIR) {
                throw Exception(std::string("Path for shared memory ") + kSharedMemoryDirectory + " is not a directory");
            } 
        }

        filename = std::string(kSharedMemoryDirectory) + "/" + name;
        
        if (access(filename.c_str(), W_OK) == -1) {
            int fd = creat(filename.c_str(), 0644 );
            if (fd == -1) 
                throw Exception(std::string("Cannot create ") + filename);
            close(fd);
        }
        
        shmkey = ftok(filename.c_str(), 1);
        if (shmkey == -1) 
            throw Exception(std::string("Cannot convert ") + filename + " to IPC token");
        
        int mode ;
        switch (omode) {
        case Create:
            mode = IPC_CREAT | IPC_EXCL | 0644;
            break;
        case OpenOrCreate:
            mode = IPC_CREAT | 0644;
            break;
        case OpenExisting:
            mode = 0644;
            break;
        }

        //std::cout << "Opening shmkey " << std::hex << shmkey << " for " << name << " (" << filename << ")" << std::endl;

        shmid = shmget(shmkey, size, mode) ;
        if (shmid == -1) {
            if (errno == EEXIST && omode == Create)
                throw Exception(std::string("Shared memory ") + name + " already exists");
            if (errno == ENOENT && omode == OpenExisting)
                throw Exception(std::string("Shared memory segment ") + name + " does not exist");
            std::ostringstream oss;
            oss << "Cannot open shared memory " << name << ". Cannot get shmid for shmkey " << std::hex << shmkey << ". Error " << std::dec << errno << ".";
            throw Exception(oss.str());
        }
        
        struct shmid_ds ds;
        switch (omode) {
        case Create:
            created = true;
            break;
        case OpenOrCreate:
            if (shmctl(shmid, IPC_STAT, &ds) != 0)
                throw Exception("Cannot stat shared memory segment");
            if (ds.shm_cpid == getpid())
                created = true;
            else
                created = false;
            break;
        case OpenExisting:
            created = false;
            break;
        }
        
        mem = shmat(shmid, NULL, 0);
        if ((intptr_t)mem == -1) {
            mem = 0;
            std::ostringstream oss;
            oss << "Cannot open shared memory " << name << ". Cannot attach to shmid " << shmid << ". Error " << errno << ".";
            throw Exception(oss.str());
        }

        //std::cout << "Successfylly openend shm " << std::hex << shmkey << " for " << name << std::endl;
    }

}

