#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"
/*
 * Simply tests a file back page
 * Create valid file back, initial read fault
 **/
using std::cout;

int main() {
    /* Allocate swap-backed page from the arena */
    std::cout << "In main, allocating swap back page from arena \n";
    char* filename = static_cast<char *>(vm_map(nullptr, 0));

    /* Write the name of the file that will be mapped */
    std::cout << "Writing name of file to be mapped \n";
    strcpy(filename, "papers.txt");

    /* Map a page from the specified file */
    std::cout << "Map page from specified file \n";
    char* p = static_cast<char *>(vm_map (filename, 0));

    std::cout << "print the first part of the paper \n";
    /* Print the first part of the paper */
    for (unsigned int i=0; i<1930; i++) {
	cout << p[i];
    }
    std::cout << "\nTest done \n";
    return 0;
}