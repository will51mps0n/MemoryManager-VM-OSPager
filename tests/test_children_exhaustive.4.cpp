#include <iostream>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include "vm_app.h"

/*
 * This test creates multiple children like a family of sorts
 * They all read and write to the same file backed file
 * Should ensure that the writes are consistent across files
 * And that the clock isnt reloaded for each one
 */
int main() {
    std::cout << "In main\n";
    std::cout << " \n";
    // fork an empty process
    
    if (fork() == 0) {
        if (fork() == 0) {
            std::cout << "In child 1\n";
            char* filename_page = (char*) vm_map(nullptr, 0);
            strcpy(filename_page, "data1.bin");
            std::cout << "Creating a FB file \n";
            char* fbPage = (char*) vm_map(filename_page, 0);
    
            std::cout << "Writing to shared file back \n";
            fbPage[0] = 'A';
    
            std::cout << "Reading to shared file back \n";
            std::cout << fbPage[0] << "\n";
        }
        else {
            std::cout << "In parent2\n";
            char* filename_page = (char*) vm_map(nullptr, 0);
            strcpy(filename_page, "data1.bin");
            std::cout << "Creating a FB file in parent to same file as child\n";
            char* fbPage = (char*) vm_map(filename_page, 0);
    
            std::cout << "Writing to shared file back \n";
            fbPage[0] = 'A';
    
            std::cout << "Reading from shared after write \n";
            std::cout << fbPage[0] << "\n";
        }

        std::cout << "In child 2\n";
        char* filename_page = (char*) vm_map(nullptr, 0);
        strcpy(filename_page, "data1.bin");
        std::cout << "Creating a FB file \n";
        char* fbPage = (char*) vm_map(filename_page, 0);

        std::cout << "Writing to shared file back \n";
        fbPage[0] = 'A';

        std::cout << "Reading to shared file back \n";
        std::cout << fbPage[0] << "\n";
    }
    else{ 
        std::cout << "In parent1\n";
        char* filename_page = (char*) vm_map(nullptr, 0);
        strcpy(filename_page, "data1.bin");
        std::cout << "Creating a FB file in parent to same file as child\n";
        char* fbPage = (char*) vm_map(filename_page, 0);

        std::cout << "Writing to shared file back \n";
        fbPage[0] = 'A';

        std::cout << "Reading from shared after write \n";
        std::cout << fbPage[0] << "\n";
    }

    std::cout << "ALL DONE\n";
}