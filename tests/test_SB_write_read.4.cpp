#include "vm_app.h"
#include <iostream>
#include <unistd.h>
#include <cstring>

/*
 * Simply tests a swap back page
 */
int main() {
    // Mapping a swap backed page
    std::cout << "In main, creating a swap back page\n";
    void *addr = vm_map(nullptr, 0); 
    if (addr == nullptr) {
        std::cout << "Mapping failed!" << std::endl;
        return 1;
    }

    std::cout << "Mapped swap-backed page at " << addr << std::endl;
    std::cout.flush();

    char *data = static_cast<char*>(addr);
    // Write which will trigger a fault 
    strcpy(data, "this is the data we WRITE! This Should trigger the writefault and be output");

    // Read the new data back
    printf("reading the data: %s\n", data);
    std::cout << "Done with test. Returning\n";
    return 0;
}