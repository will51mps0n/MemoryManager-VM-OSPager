#include "vm_app.h"
#include <cstring>
#include <iostream>
/*
 * Tests the eviction of a dirty swap backed page
 Creates, writes, fills memory, and then forces an evviction
 **/
using std::cout;

int main() {
    std::cout << "Creating 4 swap back pages\n";
    auto* p1 = static_cast<char*>(vm_map(nullptr, 0));
    auto* p2 = static_cast<char*>(vm_map(nullptr, 0));
    auto* p3 = static_cast<char*>(vm_map(nullptr, 0));
    auto* p4 = static_cast<char*>(vm_map(nullptr, 0)); 

    std::cout << "Reading into Adam page\n";
    strcpy(p1, "Adam"); 
    std::cout << "Reading into Sergster page\n";
    strcpy(p2, "Sergester");
    std::cout << "Reading into Comini page\n";
    strcpy(p3, "Ethan"); 

    // This shoudl reorder the clock queue
    std::cout << "Calling to reorder the clock queue\n";
    (void)p1[0]; 
    (void)p2[0]; 
    (void)p3[0];
    
    // Andrew dingus should force an eviction
    std::cout << "Andy dingus needs to be read in, causing an eviction. Thanks alot dingus\n";
    strcpy(p4, "AndrewDingus");

    std::cout << "Andy dingus test complete.\n";
    return 0;
}
