/*
 * niit.h 
 *
 *
 */


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

#undef PDEBUG
#ifdef NIIT_DEBUG
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "niit " fmt, ## args)
#else
#  define PDEBUG(fmt, args...) 
#endif

