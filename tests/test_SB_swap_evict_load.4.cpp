#include <iostream>
#include <cstring>
#include "vm_app.h"

/*
 * Map many swap back pages and write to them 
 * Try to cause eviction - maybe write dirty pages for all clock algo when we cal to vm destroy
 */
int main() {
    std::cout << "Creating a reference to pages at 4\n";
    char* pages[10];
    pages[8] = (char*) vm_map(nullptr, 0);
    std::cout << "here is a pin pointer " << pages[8][0] << "\n";

    std::cout << "Creating more pages, trigger write fault for each and evict\n";
    for (int i = 0; i < 8; ++i) {
        std::cout << "Creating page " << i << "\n";
        pages[i] = (char*) vm_map(nullptr, 0);
        std::cout << "Created swap back page " << i << "\n";
        // This should trigger faults (write fault i believe)
        std::cout << "Trigger a write fault for " << i << "\n";
        std::cout << pages[i][0] << "\n";  
        pages[i][0] = 'A' + i;  
        std::cout << pages[i][0] << "\n";
        if(i == 5) {
            std::cout << "Trigger a read fault for " << 4 << "\n";
            std::cout << pages[4][0] << "\n\n"; 
        } 
    }
    std::cout << "Done with write fault\n";

    pages[8] = (char*) vm_map(nullptr, 0);
    std::cout << "here is 9 " << pages[8][0] << "\n";

    // Force an eviction by adding the other vps
    std::cout << "Okay doing it again to make evictions creating new vpns " <<"\n";
    std::cout << "\n";
    for (int i = 0; i < 8; ++i) {
        std::cout << "Creating page and then reading from it to cout" << i << "\n";
        // If needed it will trigger a reload
        std::cout << pages[i][0] << "\n";  
        std::cout << "Done with page " << i << "\n";
    }
    std::cout << "here is 9 again " << pages[8][0] << "\n";
    std::cout << "Done with test\n";
    return 0;
}
