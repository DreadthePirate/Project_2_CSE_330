#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include <assert.h>

#include "../common.h"
#include "helper.h"

/* 
 * Description:
 * This test checks if allocation to a previously-mapped page is correctly
 * handled by the kernel module.
 */

int main(void);

bool test_allocate(void){
    struct alloc_info* allocCall;    
    allocCall = malloc(sizeof(struct alloc_info));
    allocCall->vaddr                = (unsigned long) &main >> 12 << 12;
    allocCall->num_pages            = 1;
    allocCall->write                = true;
    if (!ioctl(devfd, ALLOCATE, allocCall) != -1) {
        return false;
    }
    return true;
}

int main(void)
{
    printf("Executing: TEST1\n");

    if (!open_device_driver()) return -1;
    
    if(!test_allocate()) {
    	printf("Passed: TEST1\n");
        return -1;
    }

    printf("Failed: TEST1\n");
    close(devfd);
    return 0;
}