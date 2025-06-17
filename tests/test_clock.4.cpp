#include <iostream>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include "vm_app.h"

using std::cout;
using std::endl;

void validate_clock_access(char* page, char expected, const char* error) {
    if (page[0] != expected) {
        cout << "Error: " << error << " - The expected value was: " << expected 
             << " we got :( " << page[0] << "\n";
    } else {
        cout << "Validation passed: " << expected << "\n";
    }
}

int main() {
    cout << "In main. Clock algorithm test beginning \n";
    
    
    cout << "Attempting to create 5 swap back pages\n";
    char* swap_pages[5];
    for (int i = 0; i < 5; i++) {
        swap_pages[i] = static_cast<char*>(vm_map(nullptr, 0));
        cout << "  Successfully created page " << i << " at the location " << (void*)swap_pages[i] << "\n";
    }
    
    
    cout << "Done with the swap page init. Creating swap pge with unique values A b c d e ;)\n";
    for (int i = 0; i < 5; i++) {
        swap_pages[i][0] = 'A' + i;  
        cout << "  Successfully write " << swap_pages[i][0] << "' to the page " << i << "\n";
    }
    
    cout << "Attempting to create file buffer\n";
    char* filename_buf = static_cast<char*>(vm_map(nullptr, 0));
    strcpy(filename_buf, "papers.txt");
    cout << "  created a buffer succeeded at loc: " << (void*)filename_buf << "\n";
    
    cout << "Attempting to create 3 file backed pages" << endl;
    char* file_pages[3];
    for (int i = 0; i < 3; i++) {
        file_pages[i] = static_cast<char*>(vm_map(filename_buf, i));
        cout << "  succeeded makin da file num: " << i << " at location " << (void*)file_pages[i] << "\n";
    }
    
    cout << "Accessing the file pages. This should make them resident\n";
    for (int i = 0; i < 3; i++) {
        char value = file_pages[i][0];
        cout << "  File page access for" << i << " ; the first byte is: '" << value << "'\n";
    }
    
   
    cout << "Modifying swap pages in the order 1 3 and then FB page 0 to make them dirty. Making them equal to X Y Z\n";
    swap_pages[1][0] = 'X';
    swap_pages[3][0] = 'Y';
    file_pages[0][0] = 'Z';
    cout << "  Succeeded modifying swap pages 1, 3 and file page 0\n";
    
    
    cout << "Force the clock evictions. Access pages in order 0 to 4 for swap pages\n";
    
    for (int i = 0; i < 5; i++) {
        char value = swap_pages[i][0];
        cout << "  Swap page " << i << " has value: '" << value << "'\n";
    }
    
    cout << "Verifying the evicted pages are properly reloaded\n";
    validate_clock_access(swap_pages[0], 'A', "Swap page 0 should still be 'A'");
    validate_clock_access(swap_pages[1], 'X', "Swap page 1 should be 'X' (modified)");
    validate_clock_access(swap_pages[2], 'C', "Swap page 2 should still be 'C'");
    validate_clock_access(swap_pages[3], 'Y', "Swap page 3 should be 'Y' (modified)");
    validate_clock_access(swap_pages[4], 'E', "Swap page 4 should still be 'E'");
    
    // Check that FB pages are correctly loaded back
    cout << "Verifying the file backed pages:\n";
    validate_clock_access(file_pages[0], 'Z', "File page 0 should be 'Z' (modified)");
    
    // more SB pages to cause more evictions
    cout << "Creating 3 more swap backed pages" << endl;
    char* more_swap_pages[3];
    for (int i = 0; i < 3; i++) {
        more_swap_pages[i] = static_cast<char*>(vm_map(nullptr, 0));
        more_swap_pages[i][0] = '1' + i;  // '1', '2', '3'
        cout << "  Created and writed '" << more_swap_pages[i][0] << "' to an additional swap page " << i <<"\n";
    }
    
    cout << "Attempting to verify the contents of all pages:\n";
    
    for (int i = 0; i < 5; i++) {
        char expected;
        const char* msg;
        
        if (i == 1) {
            expected = 'X';
            msg = "Modified swap page 1 (X)";
        } else if (i == 3) {
            expected = 'Y';
            msg = "Modified swap page 3 (Y)";
        } else {
            expected = 'A' + i;
            msg = "Original swap page";
        }
        
        validate_clock_access(swap_pages[i], expected, msg);
    }
    
    std::cout << "Checking the other swap backed pages: \n";
    for (int i = 0; i < 3; i++) {
        validate_clock_access(more_swap_pages[i], '1' + i, "Additional swap page");
    }
    
    validate_clock_access(file_pages[0], 'Z', "Modified file page 0");
    
    cout << "Modifying all of the pages to be a b c d e of the swap pages 0 to 5:\n"; 
    for (int i = 0; i < 5; i++) {
        swap_pages[i][0] = 'a' + i;  
    }

    cout << "Modifying all of the pages to be 4 5 6 of the swap pages 0 to 3:\n"; 
    for (int i = 0; i < 3; i++) {
        more_swap_pages[i][0] = '4' + i;  
    }
    
    cout << "Validating the second round number 2 of the file changes\n";
    
    for (int i = 0; i < 5; i++) {
        validate_clock_access(swap_pages[i], 'a' + i, "Second-modified swap page");
    }
    
    cout << "Validating the second round number 2 (number 2) of the file changes\n";
    for (int i = 0; i < 3; i++) {
        validate_clock_access(more_swap_pages[i], '4' + i, "Second-modified additional swap page");
    }
    
    cout << "Test complete lets go baby" << endl;
    return 0;
}