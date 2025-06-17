#include <iostream>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include "vm_app.h"

/*
 * This is an attempt at an exhaustive test
 * makes a child process of an emptry arena parent,
 * Then the parent and child access same file backed page
 * Allocates swap back and file backed pages
 * Filles and then modified them
 * Force evictions
 * Perform read write after eviction
 * For a child with empty arena
 * Go back to parent
 */
int main() {
    // fork an empty process
    if (fork() == 0) {
        // SB pages mapping
        char* swap_pages[4];
        for (int i = 0; i < 4; ++i) {
            swap_pages[i] = (char*) vm_map(nullptr, 0);
            assert(swap_pages[i] != nullptr);
            strcpy(swap_pages[i], "X.X.X.X.X");
        }
    
        // Write file name 
        char* filename_page = (char*) vm_map(nullptr, 0);
        assert(filename_page != nullptr);
        strcpy(filename_page, "data1.bin");
    
        // Mape multiple file back from pages
        char* file_pages[2];
        for (int i = 0; i < 2; ++i) {
            file_pages[i] = (char*) vm_map(filename_page, 0);
            assert(file_pages[i] != nullptr);
            // Write which should make them dirty
            file_pages[i][i] = 'Z'; 
        }

        // Eviction with more SB pages
        for (int i = 0; i < 6; ++i) {
            char* temp = (char*) vm_map(nullptr, 0);
            assert(temp != nullptr);
            // This should cause a writefault
            temp[0] = 'A' + i; 
        }

        // Access the earlier SB and FB pages to make us read them back in
        for (int i = 0; i < 4; ++i) {
            assert(strcmp(swap_pages[i], "X.X.X.X.X") == 0);
        }
        for (int i = 0; i < 2; ++i) {
            assert(file_pages[i][i] == 'Z');
        }

        for (int i = 0; i < 4; ++i) {
            swap_pages[i][0] = 'Y';
            assert(swap_pages[i][0] == 'Y');
        }
        for (int i = 0; i < 2; ++i) {
            file_pages[i][1] = 'Q' + i;
            assert(file_pages[i][1] == 'Q' + i);
        }
    }
    else {
        char* parentSB1 = (char*) vm_map(nullptr, 0);
        char* parentSB2 = (char*) vm_map(nullptr, 0);
        assert(parentSB1 && parentSB2);
        strcpy(parentSB1, "parentSB1");
        strcpy(parentSB2, "parentSB2");

        // This will allocate the same file backed pages as the chldren
        char* fn_page = (char*) vm_map(nullptr, 0);
        assert(fn_page != nullptr);
        strcpy(fn_page, "data1.bin");

        // Then map the same file back block that the child does (the if case before this else)
        char* shared_fb = (char*) vm_map(fn_page, 0);
        assert(shared_fb != nullptr);

        // Parent reads file back page - this is reading to shared memory between two processes
        char read_val = shared_fb[0];  
        // That will fault or load if evicted by child - may make this a different test case not sure of order rn
        std::cout << "Parent read from shared FB: " << read_val << std::endl;

        // Write to the shared page 
        shared_fb[2] = 'P';

        // Write into its own SB pages
        parentSB1[0] = 'X';
        parentSB2[1] = 'Y';

        assert(parentSB1[0] == 'X');
        assert(parentSB2[1] == 'Y');

        std::cout << "Done with parent case in test\n";

    }
    std::cout << "At the end of the test case! Should mean no failure if asserts are correct" << std::endl;
    return 0;
}
