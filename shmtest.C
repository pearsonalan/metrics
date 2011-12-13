#include <iostream>
#include "SharedMemory.H"

int main() {
	shmem::SharedMemory shm("foo",1024,shmem::OpenOrCreate);
	std::cout << "This process " << (shm.wasCreated() ? "created" : "attached to") << " the shared memory segment" << std::endl;
}