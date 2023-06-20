#ifndef ETGGEOPHONE_H__
#define ETGGEOPHONE_H__
    
#include <asm/ioctl.h>
#include <linux/types.h>
    
#ifdef __KERNEL__
#define uint32_t u32
#define uint16_t u16
#else	/* 
 */
#include <stdint.h>
#define u32 uint32_t
#define u16 uint16_t
#define bool int
#endif	/* 
 */
    
typedef struct geophone_data{
    u16 pin_number;
    u16 tofs;
    u16 iofs;
    u32 tot_count;
    u32 int_count;
    bool used;
} GEOPHONE_DATA;

typedef struct geophone_param{
    u16 pin_number;
} GEOPHONE_PARAMS;

#define GPIOC_ADDPIN        _IOW('G', 0, GEOPHONE_PARAMS)
#define GPIOC_GETDATA       _IOR('G', 1, GEOPHONE_DATA)
#define GPIOC_REMOVEPIN     _IOW('G', 2, GEOPHONE_PARAMS)

 
 
#endif // ETGGEOPHONE_H__