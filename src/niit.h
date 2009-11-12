/*
 * niit.h -- definitions for the SIIT module
 *
 *
 */


/*
 * Constants
 */


/* NIIT_ETH control the name of NIIT interface:
 * 0 - interface name is niit0,
 * 1 - interface name is ethX.
 */
#define NIIT_ETH 0

#define BUFF_SIZE 4096
#define FRAG_BUFF_SIZE 1232     /* IPv6 max fragment size without IPv6 header
                                 * to fragmanet IPv4 if result IPv6 packet will be > 1280
                                 */

#define NIIT_V6PREFIX_1 0x00000000
#define NIIT_V6PREFIX_2 0x00000000
#define NIIT_V6PREFIX_3 0x0000ffff


/*
 * Macros to help debugging
 */

#undef PDEBUG             /* undef it, just in case */
#ifdef NIIT_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk(KERN_DEBUG "niit " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...)







