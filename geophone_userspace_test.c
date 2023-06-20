#include "etggeophone.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>

#define GPIONEXT_VV00         26
#define GPIONEXT_VV01         27

void print_data(GEOPHONE_DATA* data) {
    printf("pin_number: %d\n", data->pin_number);
    printf("tofs: %d\n", data->tofs);
    printf("iofs: %d\n", data->iofs);
    printf("tot_count: %d\n", data->tot_count);
    printf("int_count: %d\n", data->int_count);
    printf("used: %d\n", data->used);
    printf("\n\n\n");
}

int main (void) {
    int fd;
    int ret;
    int t;
    GEOPHONE_DATA data;

    GEOPHONE_PARAMS gp = {
        GPIONEXT_VV00
    };

    fd = open("/dev/geophone0", O_RDWR);
    if(fd < 0) {
        printf("Error opening\n");
        return -1;
    }
    
    ret = ioctl(fd, GPIOC_ADDPIN, &gp);
    //ret = 0;
    if(ret != 0) {
        printf("Error add pin\n");
        if(ret != 0) {
            printf("Try to remove pin\n");
            ret = ioctl(fd, GPIOC_REMOVEPIN, &gp);
            if(ret != 0) {
                printf("Error remove pin\n");
                return -1;
            }
            ret = ioctl(fd, GPIOC_ADDPIN, &gp);
            if(ret != 0) {
                printf("Errore rimozione pin\n");
                return -1;
            }
        } else {
            return -1;
        }
    }

    t = 0;
    while(1) {
        ret = ioctl(fd, GPIOC_GETDATA, &data);
        if(ret != 0) {
            printf("Error getdata\n");
            break;
        }
        print_data(&data);
        sleep(1);
        t++;
        if(t == 60) {
            break;
        }
    }

    ret = ioctl(fd, GPIOC_REMOVEPIN, NULL);
    if(ret != 0) {
        printf("Error remove pin\n");
    }

    return 0;
}
