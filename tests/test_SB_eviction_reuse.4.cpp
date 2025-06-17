#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cassert>
#include "vm_app.h"

/*
 * This tests a swap back page eviciton
 * This is followed by a reuse of the evicted swap block
*/
int main() {
    std::cout << "Creating 4 swap back pages\n";
    char* page0 = static_cast<char*>(vm_map(nullptr, 0));
    char* page1 = static_cast<char*>(vm_map(nullptr, 0));
    char* page2 = static_cast<char*>(vm_map(nullptr, 0));
    char* page3 = static_cast<char*>(vm_map(nullptr, 0));

    std::cout << "Filling the pages\n";
    strcpy(page0, "Sergey");
    strcpy(page1, "Ethan");
    strcpy(page2, "Adam");
    strcpy(page3, "Kitty");
    std::cout << "Done filling the pages\n";
    // This should make an eviction
    std::cout << "Making a new page. Swap back and empty\n";
    char* page4 = static_cast<char*>(vm_map(nullptr, 0));
    std::cout << "Write to the new page, which should evict page 0\n";
    strcpy(page4, "Meow");

    // Acces a page that has been evicted and ensure its brought back
    std::cout << "Read from the page 0 that was evicted. Should recover this swap backed page\n";

    std::cout << "Recovered the page: " << page2[0] << "\n";
    std::cout << "Reference the page: " << page1[0] << "\n";
    std::cout << "Recovered the 2nd page: " << page4[0] << "\n";

    std::cout << "Recovered the page: " << page2[0] << "\n";
    std::cout << "Reference the page: " << page1[0] << "\n";


    //std::cout << "Test done " << c << " " << y << " " << "\n";
    return 0;
}