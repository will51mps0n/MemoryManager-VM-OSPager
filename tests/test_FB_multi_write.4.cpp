#include <iostream>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include "vm_app.h"

int main() {
    std::cout << "Mapping file name to sb file \n";
    char* filename = (char*) vm_map(nullptr, 0);
    strcpy(filename, "papers.txt");

    std::cout << "Attempting to map 10 consecutive files from papers.txt \n";
    char* file_pages[10];
    for (int i = 0; i < 10; ++i) {
        std::cout << "Mapping page " << i << "\n";
        file_pages[i] = (char*) vm_map(filename, i % 2);
        std::cout << "Mapping page " << i << " success \n";
    }
    std::cout << "Mapping all 10 files successful \n";
    std::cout << "Now attempting to read a block from each page\n";
    for (int i = 0; i < 6; ++i) {
        std::cout << "First char of page " << i << ": " << file_pages[i][0] << "\n";
    }
    std::cout << "Done reading pages. Assuming block size 4, blocks 6 7 8 and 9 should be resident\n";

    for (int i = 0; i < 4; ++i) {
        std::cout << "Writing to page " << i << "\n";
        file_pages[i][0] = 'X';
    }

    std::cout << "If block size = 4, pages 0 - 4 should now be resident as we just wrote to them. All should be dirty but not written yet\n";

    std::cout << "Making 2 new pages from 0 and 1 block of file name. Both resident so write sholdnt have happened yet\n";
    char* fbPage0Cpy = (char*) vm_map(filename, 0);
    char* fbPage1Cpy = (char*) vm_map(filename, 1);

    std::cout << "New page to same file, block 0 char = " << fbPage0Cpy[0]
    << " old page to file block 0, which should be different as we wrote to it = " << file_pages[0][0] << "\n";

    std::cout << "New page to same file, block 1 char = " << fbPage1Cpy[1]
    << " old page to file block 1, which should be different as we wrote to it = " << file_pages[1][0] << "\n";
    
    std::cout << "Reading from each page so the dirty gets written\n";
    for (int i = 0; i < 10; ++i) {
        std::cout << "First char of page " << i << ": " << file_pages[i][0] << "\n";
    }
    std::cout << "First char of page0 copy " << fbPage0Cpy[0] << " - First char of page1 copy " << fbPage0Cpy[1] << "\n";

    std::cout << "Making 2 new pages from 2, 3, and 10 block of file name. Both should have been evicted so write sholdnt have happened yet\n";
    char* fbPage2Cpy = (char*) vm_map(filename, 2);
    char* fbPage3Cpy = (char*) vm_map(filename, 1);
    char* fbPage10Cpy = (char*) vm_map(filename, 0);
    std::cout << "First char of page 2 copy: " << fbPage2Cpy[0] << "\n";
    std::cout << "First char of page 3 copy: " << fbPage2Cpy[0] << "\n";
    std::cout << "First char of page 10 copy: " << fbPage2Cpy[0] << "\n";
    std:: cout << "Writing A to pg 2 copy\n"; 

    fbPage2Cpy[0] = 'A';
    std:: cout << "First char of page2 copy: " << fbPage2Cpy[0] << "\n";
    std:: cout << "First char of page2: " << file_pages[0] << "\n";

    std:: cout << "Now for page 10 copy, write the same thing to memory thats in the original page already\n";
    fbPage10Cpy[0] = file_pages[9][0];
    std:: cout << "First char of page10 copy: " << fbPage10Cpy[0] << "\n";
    std:: cout << "First char of page10: " << file_pages[9][0] << "\n";

    std:: cout << "First char of page3 copy: " << fbPage3Cpy[0] << "\n";

    char* finally = (char*) vm_map(nullptr, 0);
    finally[0] = 'A';
    std::cout << "one more test " << finally[0] << "\n";
    
    std:: cout << "Done with tests\n";

    return 0;
}