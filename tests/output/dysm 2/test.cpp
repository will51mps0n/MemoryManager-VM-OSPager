#include <iostream>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include "vm_pager.h"

using std::cout;
using std::endl;

// This test focuses on edge cases and validation
// It attempts to trigger various corner cases and error conditions
// to ensure the pager handles them gracefully

// Utility function to check if operation was successful
void check_result(void* ptr, const char* operation) {
    if (ptr == nullptr) {
        cout << operation << " correctly returned nullptr\n";
    } else {
        cout << operation << " unexpectedly succeeded (returned " << ptr << ")\n";
    }
}

// Checks if fault returns unexpected result
void check_fault(const void* addr, bool write, int expected_result, const char* description) {
    int result = vm_fault(addr, write);
    if (result == expected_result) {
        cout << "✓ vm_fault " << description << " correctly returned " << result << endl;
    } else {
        cout << "! vm_fault " << description << " returned " << result 
             << " (expected " << expected_result << ")" << endl;
    }
}

int main() {
    cout << "In main in the test. Beginning: \n";
    
    cout << "\n Testing null and invalid pointers: \n";
    
    // Map SB page and this will hold file names
    char* filename_buf = static_cast<char*>(vm_map(nullptr, 0));
    if (filename_buf == nullptr) {
        cout << "Failed to read the file names test should exit :(\n";
        return 1;
    }
    
    // map with invalid file because outside arena
    cout << "Testing vm map with invalid arena. Shouldnt work but pager should continue\n";
    char invalid_filename[20] = "papers.txt";
    check_result(vm_map(invalid_filename, 0), "vm_map test with a filename outside arena");
    
    // Test valid file and string
    cout << "Reading in file to swap block\n";
    strcpy(filename_buf, "papers.txt");
    cout << "Testing a vm map with a valid file name\n";
    void* file_page = vm_map(filename_buf, 0);
    if (file_page == nullptr) {
        cout << "Failed to map file page. Shouldnt happen >:(" << endl;
    }
    
    // Test mapping with invalid filename (non-existent file)
    strcpy(filename_buf, "nonexistent.txt");
    cout << "Testing vm_map with non-existent file..." << endl;
    void* nonexistent_page = vm_map(filename_buf, 0);
    if (nonexistent_page != nullptr) {
        cout << "Mapped non-existent file (this is allowed by spec)" << endl;
        // Force a fault to try to access this page - should fail
        check_fault(nonexistent_page, false, -1, "on non-existent file");
    }
    
    // SECTION 2: Test arena boundaries
    cout << "\n--- SECTION 2: Arena boundaries ---" << endl;
    
    // Test accessing memory just outside arena boundaries
    void* arena_start = VM_ARENA_BASEADDR;
    void* arena_end = static_cast<char*>(VM_ARENA_BASEADDR) + VM_ARENA_SIZE;
    
    // Try faulting on addresses just outside arena
    cout << "Testing vm_fault with address before arena start..." << endl;
    check_fault(static_cast<char*>(arena_start) - 1, false, -1, "before arena");
    
    cout << "Testing vm_fault with address at arena end..." << endl;
    check_fault(arena_end, false, -1, "at arena end");
    
    // SECTION 3: Test swap space exhaustion
    cout << "\n--- SECTION 3: Swap space exhaustion ---" << endl;
    
    // Map as many swap-backed pages as possible until failure
    cout << "Mapping swap-backed pages until failure..." << endl;
    void* swap_pages[1000]; // Large enough array
    int page_count = 0;
    
    while (true) {
        swap_pages[page_count] = vm_map(nullptr, 0);
        if (swap_pages[page_count] == nullptr) {
            cout << "✓ vm_map correctly failed after " << page_count << " swap pages" << endl;
            break;
        }
        page_count++;
    }
    
    // Verify we can still access already mapped pages
    cout << "Verifying access to already mapped pages..." << endl;
    if (page_count > 0) {
        // Write to the first page
        char* first_page = static_cast<char*>(swap_pages[0]);
        first_page[0] = 'X';
        
        // Write to the middle page if available
        if (page_count > 2) {
            char* middle_page = static_cast<char*>(swap_pages[page_count/2]);
            middle_page[0] = 'Y';
        }
        
        // Write to the last page
        char* last_page = static_cast<char*>(swap_pages[page_count-1]);
        last_page[0] = 'Z';
        
        // Verify values
        if (first_page[0] == 'X' && last_page[0] == 'Z') {
            cout << "✓ Successfully accessed already mapped pages after exhaustion" << endl;
        } else {
            cout << "! Failed to access already mapped pages after exhaustion" << endl;
        }
    }
    
    // SECTION 4: Test zero page behavior
    cout << "\n--- SECTION 4: Zero page behavior ---" << endl;
    
    // Map a new swap-backed page
    char* zero_test_page = static_cast<char*>(vm_map(nullptr, 0));
    
    if (zero_test_page != nullptr) {
        // Check if page is initialized to zero
        bool all_zeros = true;
        for (unsigned int i = 0; i < VM_PAGESIZE && all_zeros; i += 4096) {
            if (zero_test_page[i] != 0) {
                all_zeros = false;
            }
        }
        
        if (all_zeros) {
            cout << "✓ New swap-backed page correctly initialized to zeros" << endl;
        } else {
            cout << "! New swap-backed page not initialized to zeros" << endl;
        }
        
        // Modify page and then verify adjacent pages aren't affected
        zero_test_page[0] = 123;
        zero_test_page[VM_PAGESIZE-1] = 123;
        
        if (page_count > 1) {
            char* prev_page = static_cast<char*>(swap_pages[page_count-1]);
            if (prev_page[0] == 'Z') {
                cout << "✓ Adjacent page not affected by modification" << endl;
            } else {
                cout << "! Adjacent page modified incorrectly: " << (int)prev_page[0] << endl;
            }
        }
    }
    
    // SECTION 5: Test cross-page filename
    cout << "\n--- SECTION 5: Cross-page filename ---" << endl;
    
    // Allocate two consecutive pages
    char* name_page1 = static_cast<char*>(vm_map(nullptr, 0));
    char* name_page2 = static_cast<char*>(vm_map(nullptr, 0));
    
    if (name_page1 != nullptr && name_page2 != nullptr) {
        // Verify pages are consecutive
        if (reinterpret_cast<uintptr_t>(name_page2) - 
            reinterpret_cast<uintptr_t>(name_page1) == VM_PAGESIZE) {
            
            // Create a filename that spans both pages
            strcpy(name_page1 + VM_PAGESIZE - 5, "data1.bin");
            
            // Try to map with a filename that spans pages
            cout << "Testing vm_map with filename that spans pages..." << endl;
            void* span_page = vm_map(name_page1 + VM_PAGESIZE - 5, 0);
            
            if (span_page != nullptr) {
                cout << "✓ Successfully mapped file with cross-page filename" << endl;
                
                // Try accessing the page
                char first_char = static_cast<char*>(span_page)[0];
                cout << "  First character of cross-page file: " << first_char << endl;
            } else {
                cout << "! Failed to map file with cross-page filename" << endl;
            }
        } else {
            cout << "! Pages not consecutive, skipping cross-page filename test" << endl;
        }
    }
    
    // SECTION 6: Test maximum VPN
    cout << "\n--- SECTION 6: Maximum VPN test ---" << endl;
    
    // Calculate approximately how many pages fit in the arena
    unsigned int pages_in_arena = VM_ARENA_SIZE / VM_PAGESIZE;
    cout << "Arena can theoretically hold " << pages_in_arena << " pages" << endl;
    
    // Map until we hit the maximum arena size
    // We already mapped many pages above, so we'll continue from there
    cout << "Continuing to map pages until arena is full..." << endl;
    int additional_pages = 0;
    
    while (page_count + additional_pages < pages_in_arena) {
        void* page = vm_map(nullptr, 0);
        if (page == nullptr) {
            cout << "Arena full or swap space exhausted after " 
                 << (page_count + additional_pages) << " total pages" << endl;
            break;
        }
        additional_pages++;
    }
    
    // Try mapping one more page (should fail)
    check_result(vm_map(nullptr, 0), "vm_map after arena is full");
    
    cout << "=== EDGE CASES AND VALIDATION TEST COMPLETE ===" << endl;
    return 0;
}