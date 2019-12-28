#define MODNAME "sl811h" 

#ifdef CONFIG_USB_SL811H_DEBUG
//---------- global varibles used by gebugging dump_mem & dump_regs
unsigned char buf16[16];                    // dump_regs uses this storage
unsigned char buf240[240];                  // dump_mem uses this storage
int i;                                      // 
#endif

#define MAX_SL811H_CONTROLLERS   8          // change this if your system contains
                                            // more sl811h controllers

//---------------- internal memory map of sl811hs controller ----------------------
#define A_HOST_CONTROL           0x00         
#define A_HOST_BASE_ADDRESS      0x01
#define A_HOST_BASE_LENGTH       0x02
#define A_PID_ENDPOINT           0x03          // write
#define A_STATUS                 0x03          // read
#define A_DEVICE_ADDRESS         0x04          // write
#define A_TRANSFER_COUNT         0x04          // read
#define CONTROL1                 0x05
#define INTERRUPT_ENABLE         0x06
#define B_HOST_CONTROL           0x08
#define B_HOST_BASE_ADDRESS      0x09
#define B_HOST_BASE_LENGTH       0x0a
#define B_PID_ENDPOINT           0x0b           // write
#define B_STATUS                 0x0b           // read
#define B_DEVICE_ADDRESS         0x0c           // write
#define B_TRANSFER_COUNT         0x0c           // read
#define INTERRUPT_STATUS         0x0d 
#define SOF_COUNTER_LOW          0x0e           // SOF counter low on write
                                                // hw revision on read
#define SOF_COUNTER_HIGH_CONTROL2 0x0f          // SOF counter high and control2 on write
                                                // SOF counter higher 8 bits in read 
#define A_MEM_BEGIN              0x10
#define B_MEM_BEGIN              0x30
#define MEMORY_END               0xff

//---------------- paticular bits (masks) in sl811hs   registers------------------------
// bits in A, B control regs
#define ALLOW_TRANSFER           0x01
#define ENABLE_TRANSFER          0x02
#define DIRECTION                0x04           // '0'== in '1'==out !!check it
#define ALLOW_ISO                0x10
#define SYNC_WITH_SOF            0x20           // i failed to put this bit in work
#define DATA_0_1                 0x40
#define PREAMBLE                 0x80
// bits in A, B status regs 
#define ACK                      0x01
#define ERR                      0x02
#define TIMEOUT                  0x04
#define DATA_0_1_ST              0x08
#define SETUP                    0x10
#define OVERFLOW                 0x20
#define NAK                      0x40
#define STALL                    0x80
// bits in control1 reg
#define START_SOF_COUNTER        0x01
#define DONT_SEND_IN_EOF2        0x04
#define USB_ENGINE_RESET         0x08
#define JK_STATE_FORCE           0x10
#define SLOW_SPEED               0x20             // '0'==full '1'==slow 
#define SUSPEND                  0x40                           
#define INPUT_FREQ               0x80             // '0'==48Mgz '1'==12Mgz
// bits in interrupt enable /  interrupt status reg
#define USB_A                    0x01
#define USB_B                    0x02
#define BABBLE                   0x04
#define SOF_TIMER                0x10
#define INS_REM_DEVICE           0x20
#define USB_RESET_RESUME         0x40
#define D_PLUS_PIN_VALUE         0x80             // only read in interrupt status reg  
// bits in current data set / hardware revision reg
#define A_CURRENT_DATA           0x01
#define B_CURRENT_DATA           0x02
#define HW_REVISION              0xf0              // '0x0'==sl11h '0x1'==sl811hs
// bits in control2
#define POLARITY                 0x40
#define MASTER                   0x80

//-------------------- sl811h spcific time values --------------------------------------
// these values are experimental 
// SS -slow speed, FS - full speed
// 
#define EMPTY_TR_SS              90     // us; empty transaction means 
#define EMPTY_TR_FS              12     // any transaction with zero length data

#define BYTE_TR_SS               8     //   us; transaction with eight byte data will take   
#define BYTE_TR_FS               1     //  EMPTY_TR_?S + EIGHT_BYTE_TR_?S
                                        
#define BOGOTICK_LATANCY         6      // us;   time for register 0xf to decrement by 1
                                        // it's aproximatly 5.3 us

#define      MAX_CONTROL_TASK_TIME    100 // jiffies; if task failes to complete
#define      MAX_INT_TASK_TIME        2   // in specified period we have to abort it
#define      MAX_BULK_TASK_TIME       10  // 
#define      MAX_ISO_TASK_TIME        10

// Transfer descriptors (TD) structures
typedef struct sl811h_queue_head {
    int need_service;
    void* descs;
    signed int number_of_descs;
    signed int current_desc;
    signed int queue_tries;
    signed int desc_tries;
    signed int actual_length;
    struct list_head waiting_list;       // all pending urbs of that queue
    struct timer_list task_timer;        // function of this timer forces
                                         //completion of task if it hangs
    struct urb *urb;                     // current urb
} sl811h_queue_head_t;

#define CONTROL_QUEUE_TRIES       5
#define CONTROL_DESC_TRIES        10
#define SETUP_PACKET_SIZE         8
typedef struct sl811h_control_td {
    __u32 regs;                                                                    
    __u8 reg_main;
    unsigned char* data;
    int data_size;
} sl811h_control_td_t;


#define MIN_INT_INTERVAL         8      // must be 2^n
#define MAX_INT_INTERVAL         256
#define DEFAULT_INT_INTERVAL     MIN_INT_INTERVAL
typedef struct sl811h_int_td {
    __u32 regs;                                  // x_HOST_BASE_ADDRESS, x_HOST_BASE_LENGTH,
                                                 // x_PID_ENDPOINT, x_DEVICE_ADDRESS 
    __u8 reg_main;                               // x_HOST_CONTROL
    struct urb *urb;
    int interval;                                // polling interval
    int counter;                                 // 0 - it's time to initiate transfer 
    int toggle_bit;                              // 1 - DATA1, 0 - DATA0        
} sl811h_int_td_t;


#define BULK_DESC_TRIES          20
typedef struct sl811h_bulk_td {
    __u32 regs;                                                                      
    __u8 reg_main;
    unsigned char* data;
    int data_size;                          
} sl811h_bulk_td_t;

typedef struct sl811h_iso_td {
    __u32 regs;                                  
    __u8 reg_main;                            
} sl811h_iso_td_t;

#define CONTROL_QUEUE_LENGTH          40
#define INT_QUEUE_LENGTH              10
#define BULK_QUEUE_LENGTH             40
#define ISO_QUEUE_LENGTH              40

#define      MAX_CONTROL_TASK_TIME    100 // jiffies; if task failes to complete
#define      MAX_INT_TASK_TIME        2   // in specified period we have to abort it.
#define      MAX_BULK_TASK_TIME       10  // Task may fail to complete because of
#define      MAX_ISO_TASK_TIME        10  //bugs described at the bottom of this file 


////////////////////////////////////////////////////////////////////////////////////
// Explanations:
//   Controller has 4 queues of transfer descriptors, one per each transfer type.
// Descriptors in queue are filled by submit_x_urb functions.
// For each queue there is an associated function that serve it (sl811h_control_task   
// for control queue, etc). Function 'sl811h_scheduler', waken by SOF timer every 
// milisecond, decides what task to run. 
//
////////////////////////////////////////////////////////////////////////////////////


//------------------ virtual root hub ----------------------------------------------
struct virt_root_hub {
  int devnum;		// Address of Root Hub endpoint 
  void *urb;
  void *int_addr;
  int send;
  int interval;
  int numports;
  int c_p_r[8];
  int status;           // status of sl811hs's port (connect, power, slow speed, reset etc) 
  int chstatus;         // changes of status of sl811hs's port
  struct timer_list rh_int_timer;
};

typedef enum {
    NO_TASK, INT_TASK, CONTROL_TASK, ISO_TASK, BULK_TASK
} sl811h_task_type_t;

typedef struct sl811h sl811h_t;

typedef void (*sl811h_task_t)(sl811h_t*, int);

//------------------ our controller private data structure --------------------------
struct sl811h {
    unsigned int io_addr;
    unsigned int io_size;
    unsigned int irq;
    unsigned int maxports;
    unsigned int allow_submit_urb;          // 1 - allow all urbs, 0 - only root hub's urb
    unsigned int ignore_ins_rem;            // 1 == do not handle ins rem events 
    unsigned long sched_time;               // scheduler uses it
    unsigned long task_time;                // scheduler uses it
  
    sl811h_task_t ins_rem_ihandler;         // pointers to controller's interrupt 
    sl811h_task_t a_done_ihandler;          // handlers
    sl811h_task_t b_done_ihandler;
    sl811h_task_t sof_timer_ihandler;
    
    volatile sl811h_task_type_t current_task;
    
    sl811h_queue_head_t *control_qh;
    sl811h_queue_head_t *int_qh;
    sl811h_queue_head_t *iso_qh;
    sl811h_queue_head_t *bulk_qh;
    
    struct timer_list timer0;               // if there is no irq, ...  
    struct timer_list timer1;               // connect_disconnect() uses it
    struct timer_list timer2;               // bug_handler()        uses it
      
    
    struct usb_bus *bus;	      // our bus
    struct virt_root_hub rh;	      //private data of the virtual root hub
    
    struct list_head other;           // to point to other sl811h if any
};


// ------------------------------------------------------------------------------------ 
//   Virtual Root HUB (inhereted from usb-uhci.h, slightly modified)
// ------------------------------------------------------------------------------------ 
   // destination of request
#define RH_INTERFACE               0x01
#define RH_ENDPOINT                0x02
#define RH_OTHER                   0x03

#define RH_CLASS                   0x20
#define RH_VENDOR                  0x40

// Requests: bRequest << 8 | bmRequestType 
#define RH_GET_STATUS           0x0080
#define RH_CLEAR_FEATURE        0x0100
#define RH_SET_FEATURE          0x0300
#define RH_SET_ADDRESS		0x0500
#define RH_GET_DESCRIPTOR	0x0680
#define RH_SET_DESCRIPTOR       0x0700
#define RH_GET_CONFIGURATION	0x0880
#define RH_SET_CONFIGURATION	0x0900
#define RH_GET_STATE            0x0280
#define RH_GET_INTERFACE        0x0A80
#define RH_SET_INTERFACE        0x0B00
#define RH_SYNC_FRAME           0x0C80
// Our Vendor Specific Request 
#define RH_SET_EP               0x2000
// Hub port features
#define RH_PORT_CONNECTION         0x00
#define RH_PORT_ENABLE             0x01
#define RH_PORT_SUSPEND            0x02
#define RH_PORT_OVER_CURRENT       0x03
#define RH_PORT_RESET              0x04
#define RH_PORT_POWER              0x08
#define RH_PORT_LOW_SPEED          0x09
#define RH_C_PORT_CONNECTION       0x10
#define RH_C_PORT_ENABLE           0x11
#define RH_C_PORT_SUSPEND          0x12
#define RH_C_PORT_OVER_CURRENT     0x13
#define RH_C_PORT_RESET            0x14
// wPortStatus bits 
#define USB_PORT_STAT_CONNECTION	0x0001
#define USB_PORT_STAT_ENABLE		0x0002
#define USB_PORT_STAT_SUSPEND		0x0004
#define USB_PORT_STAT_OVERCURRENT	0x0008
#define USB_PORT_STAT_RESET		0x0010
#define USB_PORT_STAT_POWER		0x0100
#define USB_PORT_STAT_LOW_SPEED		0x0200
// wPortChange bits 
#define USB_PORT_STAT_C_CONNECTION	0x0001
#define USB_PORT_STAT_C_ENABLE		0x0002
#define USB_PORT_STAT_C_SUSPEND		0x0004
#define USB_PORT_STAT_C_OVERCURRENT	0x0008
#define USB_PORT_STAT_C_RESET		0x0010
// Hub features 
#define RH_C_HUB_LOCAL_POWER       0x00
#define RH_C_HUB_OVER_CURRENT      0x01
#define RH_DEVICE_REMOTE_WAKEUP    0x00
#define RH_ENDPOINT_STALL          0x01
// Our Vendor Specific feature 
#define RH_REMOVE_EP               0x00
#define RH_ACK                     0x01
#define RH_REQ_ERR                 -1
#define RH_NACK                    0x00

//------------------------usefull functions ---------------------------------------------------
    
#define min(a,b) (((a)<(b))?(a):(b))

#ifdef CONFIG_USB_SL811H_DEBUG
#define dump_regs() sl811h_buf_read(controller, 0, 16, buf16); \
                    printk("regs:  ");\
                    for(i = 0; i < 16; i++)\
                        printk("%x:%x ", i, buf16[i]);\
                    printk("\n");  

#define dump_mem(n) sl811h_buf_read(controller, MEMORY_BEGIN, n, buf240);\
                    printk("memory:\n");\
                    for(i = 0; i < n ; i = i + 16)\
                       printk("%x   %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x\n", i + MEMORY_BEGIN, buf240[i], buf240[i+1], buf240[i+2], buf240[i+3], buf240[i+4], buf240[i+5], buf240[i+6], buf240[i+7], buf240[i+8], buf240[i+9], buf240[i+10], buf240[i+11], buf240[i+12], buf240[i+13], buf240[i+14], buf240[i+15]);

#define dbg(format, arg...) printk(format "\n" , ## arg)

#else // CONFIG_USB_SL811H_DEBUG
#define dump_regs() do {} while(0)
#define dump_mem(n) do {} while(0)
#define dbg(format, arg...) do {} while(0)

#endif // CONFIG_USB_SL811H_DEBUG

static __inline__ void sl811_wait_ms(unsigned int ms)
{
	if(!in_interrupt())
	{
		current->state = TASK_UNINTERRUPTIBLE;
		schedule_timeout(1 + ms * HZ / 1000);
	}
	else
		mdelay(ms);
}
               
#define err(format, arg...) printk(KERN_ERR __FILE__ ": " format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO __FILE__ ": " format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING __FILE__ ": " format "\n" , ## arg)

////////////////////////////// BUGS ////////////////////////////////////////////////////
// working with sl811hs controller I've encountered with following cases of ill behavior 
// of it's hardware
//  1. Symptom: control write to keyboard (command 0921 0200 0000 0001, turn in led) result 
// in error(s) on data stage - bit ERR in A_STATUS set
//     Cause: unknown
//     Workaround: usual retries of control_task 
//     03.10.2001
//
//  2. Symptom: control write to keyboard (command 0921 0200 0000 0001) sometime fails
//to complete. Setup stage pass ok, data stage transfer (OUT) initiated but irq on it's   
//completion (USB_A) isn't handled / isn't generated.
//     Cause: unknown, rather HW. 
//     Workaround: no 
//     03.10.2001
//  
//  3. Symptom: initiated transfer fails to complete, bit ALLOW_TRANSFER isn't cleared
//in A[B]_HOST_CONTROL. BUG2 may be a case of this
//     Cause: SL811H HW, transfer must complete, successfully or not, but it must complete
//     Workaround: SW solution coming soon
//     03.10.2001
//
//  4. Symptom: keyboard can silently die, espesialy when nobody touch it for a time.
//it can be waken up by plug off - plug in or by control write - command 0921 0200 0000 0001, 
//turn in led. Command can be issued by pressing CapsLock on another keyboard (I have two)
//     Cause: may be it's not bug but a suspend mode, that have to be cleared           
//     Workaround: 
//     03.10.2001
//
// 5. Symptom: errors in bulk transfers. It's a big trouble !
//    Cause: Space rays
//    Workaround: impossible
//    03.10.2001 
//
// 6. Symptom: PlayStation version only
//    after change from 1.4 to 1.5 in sl811h.c in PSXLinux (new addresses of registers,
//    new style of registers access) usb.c fails (timeout) to read dev descriptor from
//    keyboard if debug messagess turned off.
//    Cause: unknown
//    Workaround: udelay(10); in control_task
//
