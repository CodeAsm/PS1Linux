/**********************************************************
/ file              sl811h.c
/ description       linux driver for SL811HS USB host controller
/ written by        Burmistrov. A, burmistrov_a@rambler.ru
 
/ history           15.05.2001 nothing 
                    01.06.2001 working control transfers
                    19.06.2001 working int transfers (mouse works)
                    27.06.2001 keyboard works
		    02.07.2001 multiple sl811hs controllers 
		    in one system supported
		    22.07.2001 keyboard works stable	
		    20.08.2001 remade almost thorowly (use interrupts)
		    kbd and mouse still work :)
/ todo              1.bulk
                    2.iso
		    3.SMP locks. Currently this driver can
                    work only on 1-processor system    
***********************************************************/
#include <linux/config.h>
#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/malloc.h>
#include <linux/ioport.h> 
#include <linux/errno.h>   
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/usb.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/ps/interrupts.h>   
#include <asm/ps/pio_extension.h>

#include "sl811h.h"

static struct list_head sl811h_controllers;

//////--------------- HARDWARE ACSESS FUNCTIONS --------------------------------------------

//--------------- read-write from(to) sl811hs controller functions -------------------------

unsigned char sl811h_byte_read(sl811h_t* controller, short internal_addr){
    int ret;
    int base;
    int size;
    // switch to PIO extension board registers page
   
    psx_page_switch_ret(REGS, base, size);
   

    outb(internal_addr, controller->io_addr);
    ret = inb(controller->io_addr + 1);
    
    // switch back
  
    psx_page_switch_to(base, size);

    return ret;
}

void sl811h_byte_write(sl811h_t* controller, short internal_addr, unsigned char data){
    int base;
    int size;
    // switch to PIO extension board registers page
 
    psx_page_switch_ret(REGS, base, size);
 
    
    outb(internal_addr, controller->io_addr);
    wmb();
    outb(data, controller->io_addr + 1);
    
    // switch back
 
    psx_page_switch_to(base, size);

}

int sl811h_buf_read(sl811h_t* controller, short internal_addr, int count, unsigned char* buffer){
    int base;
    int size;
    
    if(internal_addr + count > MEMORY_END){
        dbg("sl811h_buf_read: too much you want to read from sl811hs ");
	return -1;
    }
    
    // switch to PIO extension board registers page

    psx_page_switch_ret(REGS, base, size);
 
    
    outb(internal_addr, controller->io_addr);
    while(count--)
        *(buffer++) = inb(controller->io_addr + 1);
    
    // switch back
  
    psx_page_switch_to(base, size);

    return 0;
}

int sl811h_buf_write(sl811h_t* controller, short internal_addr, int count, unsigned char* buffer){
    int base;
    int size;
    
    if(internal_addr + count > MEMORY_END){
        dbg("sl811h_buf_write: too much you want to write to sl811hs ");
	return -1;
    }
    
    // switch to PIO extension board registers page
  
    psx_page_switch_ret(REGS, base, size);
  

    outb(internal_addr, controller->io_addr);
    wmb();
    while(count--)  
        outb(*(buffer++), controller->io_addr + 1);
    
    // switch back
  
    psx_page_switch_to(base, size);
  
    return 0;
}

void sl811h_bit_set(sl811h_t* controller, short reg, short bit_mask){
    sl811h_byte_write(controller, reg, sl811h_byte_read(controller, reg) | bit_mask);
}

void sl811h_bit_clear(sl811h_t* controller, short reg, short bit_mask){
    sl811h_byte_write(controller, reg, sl811h_byte_read(controller, reg) & ~bit_mask);
}
//
//------------------ reset-init  sl811hs controller functions ------------------------------

void reset_hc(sl811h_t* controller){
    int count = 13;                                // fill with zeros first 13 regs
    while(count--)
	sl811h_byte_write(controller, count, 0);
    
    sl811h_byte_write(controller, INTERRUPT_STATUS, 0xff);              // clear all irq events
    dbg("reset done\n");
    return;
}
 
void init_hc(sl811h_t* controller){
    sl811h_byte_write(controller, SOF_COUNTER_HIGH_CONTROL2, 0x2e | MASTER);
    sl811h_byte_write(controller, INTERRUPT_ENABLE, USB_A | USB_B | SOF_TIMER | INS_REM_DEVICE);
}
  
void reset_on_bus(sl811h_t* controller){
    sl811h_bit_set(controller, CONTROL1, USB_ENGINE_RESET);
    mdelay(10);
    sl811h_bit_clear(controller, CONTROL1, USB_ENGINE_RESET);
}


//---------------------------- speed detect-------------------------------------------------
//  The only garanty of that some device REALLY connected is working counter
// at the end of this function.
//  It seems counter couldn't be started (but could work ) with no device connected.
//  returns: 0 - nobody, 1 - slow speed, 2 - full speed
int speed_detect(sl811h_t* controller){
    unsigned char int_status;
    unsigned char counter;
    unsigned char counter_later;
    int retval; 
    
    // stop sof counter, clear control registers, clear irqs
    sl811h_byte_write(controller, CONTROL1, 0);
    sl811h_byte_write(controller, A_HOST_CONTROL, 0);
    sl811h_byte_write(controller, B_HOST_CONTROL, 0);
    sl811h_byte_write(controller, INTERRUPT_STATUS, 0xff);
    // we need this delay for device to be good inserted
    mdelay(300);
    reset_on_bus(controller);                         
    mdelay(1);					     
    int_status = sl811h_byte_read(controller, INTERRUPT_STATUS);	     
    sl811h_byte_write(controller, INTERRUPT_STATUS, 0xff);

    if ((int_status & D_PLUS_PIN_VALUE) == 0){   
	dbg("Slow Speed ? %x\n", int_status);
	sl811h_byte_write(controller, SOF_COUNTER_HIGH_CONTROL2, 0x2e | MASTER | POLARITY);    
	sl811h_byte_write(controller, SOF_COUNTER_LOW, 0xe0);
	sl811h_byte_write(controller, CONTROL1, SLOW_SPEED | START_SOF_COUNTER
#ifdef SL811H_12MHZ
			  | INPUT_FREQ_12MGZ
#endif
			  );
	//printk("slow\n");
	retval = 1;
    }
    else{   
        dbg("Full Speed ! %x\n", int_status);
        sl811h_byte_write(controller, SOF_COUNTER_HIGH_CONTROL2, 0x2e | MASTER);      
        sl811h_byte_write(controller, SOF_COUNTER_LOW, 0xe0);     
        sl811h_byte_write(controller, CONTROL1, DONT_SEND_IN_EOF2 | START_SOF_COUNTER
#ifdef SL811H_12MHZ
			  | INPUT_FREQ_12MGZ
#endif
			  );
	//printk("full\n");
	retval = 2;      
    }
    
    sl811h_byte_write(controller, A_DEVICE_ADDRESS, 0);
    sl811h_byte_write(controller, A_HOST_CONTROL, 0x01); 
    mdelay(100);                                // Hub required approx. 24.1mS
    
    counter = sl811h_byte_read(controller, SOF_COUNTER_HIGH_CONTROL2);
    udelay(10);
    counter_later = sl811h_byte_read(controller, SOF_COUNTER_HIGH_CONTROL2);
    // counter works - some device present on bus
    if(counter != counter_later)
        return retval;
    // no device present on bus
    else
      //printk("none\n");
    return 0;
}


//////----------------- MAIN DRIVER FUNCTIONALITY -----------------------------------------

//-------------------- x_task functions ---------------------------------------------------
// x_task takes care of a queue of x descriptors, ie initiates transfer, look at it's result,
// call completion handler if one; x = control, int, bulk, iso.
static void sl811h_control_task(sl811h_t* controller, int called_from_ihandler){
    sl811h_queue_head_t *head;
    sl811h_control_td_t *desc;    
    urb_t *urb;
    unsigned char transfer_status;
    unsigned char transfer_count;
    unsigned char counter;
    unsigned int time_needed;
    
   
    head = controller->control_qh;
    desc = (sl811h_control_td_t*) head->descs + head->current_desc;
    if(head->urb)
	urb = head->urb;
    else //urb unlinked
	goto c_t_exit;
    
    
    if(called_from_ihandler){ // transfer complete
        // BUG 6 at the bottom of sl811h.h
        udelay(20);
	// look at a result of a complete transfer
	transfer_status = sl811h_byte_read(controller, A_STATUS);  
	if(transfer_status & ACK){                     // ACK
	    // read data if in packet 
	    if(!(desc->reg_main & DIRECTION)){
		transfer_count = sl811h_byte_read(controller, A_TRANSFER_COUNT); 
		if(transfer_count > desc->data_size){
		    // wrong hw behavior mustn't cause sw crash 
		    urb->status =  -EINVAL;
		    goto c_t_urb_complete;
		}		
		sl811h_buf_read(controller, A_MEM_BEGIN, desc->data_size - transfer_count,desc->data);
		head->actual_length += desc->data_size - transfer_count;
		if(transfer_count){ // short packet
		    head->current_desc = head->number_of_descs - 2;
		    desc = (sl811h_control_td_t*) head->descs + head->current_desc;
		}
	    }
	    head->current_desc++;
	    desc++;
	    head->desc_tries = CONTROL_DESC_TRIES;
	}
	if(transfer_status & STALL){                    // STALL
	    urb->status = -EPIPE;
	    goto c_t_urb_complete;
	}
	else{                                           // NAK, ERR, TIMEOUT, OVERFLOW
	    head->desc_tries--;
	    if(!(head->desc_tries)){
		head->queue_tries--;
		if(!(head->queue_tries)){
		    urb->status = -ETIMEDOUT;
		    goto c_t_urb_complete;  
		}
		else{
		    head->current_desc = 0;
		    head->desc_tries = CONTROL_DESC_TRIES;
		    head->actual_length = 0;
		    controller->current_task = NO_TASK;
		    return;
		}
	    }
	}
    }
    
    // prepare for initiating new transfer
    if(head->current_desc < head->number_of_descs){
	sl811h_buf_write(controller, A_HOST_BASE_ADDRESS, 4, (unsigned char*) &desc->regs);
	// write data to controller if out or setup packet
	if(desc->reg_main & DIRECTION){
	    sl811h_buf_write(controller, A_MEM_BEGIN, desc->data_size, desc->data);
	}
	// look how many time remains before end of frame (for not to bubble)
	time_needed = (usb_pipeslow(urb->pipe)) ? 
	    EMPTY_TR_SS + BYTE_TR_SS * desc->data_size :
	    EMPTY_TR_FS + BYTE_TR_FS * desc->data_size;
	counter = sl811h_byte_read(controller, SOF_COUNTER_HIGH_CONTROL2);
	// to syncronize transfer with the begining of next frame SYNC_WITH_SOF bit might be used
	// but it seems it doesn't work, so we need do this stupid delaing 
	if(counter * BOGOTICK_LATANCY < time_needed)
	    udelay(counter * BOGOTICK_LATANCY);
	// initiate
	sl811h_byte_write(controller, A_HOST_CONTROL, desc->reg_main);
	return;
    } 
    else{ // no more transfers have to be initiated
	urb->actual_length = head->actual_length;
	urb->status = 0;
    }
   
 c_t_urb_complete:
    if(urb->complete)
	urb->complete(urb);
 c_t_exit:
    del_timer(&head->task_timer);
    controller->current_task = NO_TASK;
    head->need_service = 0;   
}


void sl811h_int_task(sl811h_t* controller, int called_from_ihandler){
    sl811h_queue_head_t *head;
    sl811h_int_td_t *desc;
    urb_t* urb;
    unsigned char transfer_status;
    unsigned char counter;
    unsigned int time_needed;
    
    
    head = controller->int_qh;
    desc = (sl811h_int_td_t *) head->descs + head->current_desc;
    if(desc->urb)
	urb = desc->urb;
    else // urb unlinked
	goto i_t_look_for; 

    // look at a result of a previos transfer
    if(called_from_ihandler){
	transfer_status = sl811h_byte_read(controller, A_STATUS);
	if(transfer_status & ACK){
	    if(usb_pipein(urb->pipe)){
		sl811h_buf_read(controller, A_MEM_BEGIN, 
				urb->transfer_buffer_length, urb->transfer_buffer);
		
		urb->actual_length = urb->transfer_buffer_length;//??????
	    }
	    if(urb->complete){
		urb->status = 0;
		urb->complete(urb);
		urb->status = -EINPROGRESS;
	    }
	    desc->toggle_bit = (desc->toggle_bit) ^ 1;
	}
	desc->counter = desc->interval;
    }

 i_t_look_for:  
    // look for descriptor with zero counter
    for(;; head->current_desc++, desc++){
	if(head->current_desc >= INT_QUEUE_LENGTH){// all descs served - task complete
	    head->current_desc = 0;
	    head->need_service = 0;
	    controller->current_task = NO_TASK;
	    return;
	}
	if(desc->urb && !(desc->counter)){ // we found it
	    urb = desc->urb;
	    break;
	}
    }
    
    // prepare for initiating transfer
    sl811h_buf_write(controller, A_HOST_BASE_ADDRESS, 4, (unsigned char*) &desc->regs); 
    if(usb_pipeout(urb->pipe)){
       sl811h_buf_write(controller,A_MEM_BEGIN,urb->transfer_buffer_length,urb->transfer_buffer);
       urb->actual_length = urb->transfer_buffer_length;///???
    }
    // for not to bubble
    time_needed = (usb_pipeslow(urb->pipe)) ? 
	EMPTY_TR_SS + BYTE_TR_SS * urb->transfer_buffer_length :
	EMPTY_TR_FS + BYTE_TR_FS * urb->transfer_buffer_length;
    counter = sl811h_byte_read(controller, SOF_COUNTER_HIGH_CONTROL2);
    if(counter * BOGOTICK_LATANCY < time_needed)
        udelay(counter * BOGOTICK_LATANCY);
    // initiate
    sl811h_byte_write(controller, A_HOST_CONTROL, desc->reg_main | 
		      ((desc->toggle_bit) ? DATA_0_1 : 0));
}


static void sl811h_bulk_task(sl811h_t* controller, int called_from_ihandler){
    sl811h_queue_head_t *head;
    sl811h_bulk_td_t *desc;    
    urb_t *urb;
    int pipe;
    unsigned char transfer_status;
    unsigned char transfer_count;
    unsigned char counter;
    unsigned int time_needed;
    
    
    dbg("b_t");
    head = controller->bulk_qh;
    desc = (sl811h_bulk_td_t*) head->descs + head->current_desc;
    if(head->urb){
	urb = head->urb;
	pipe = urb->pipe;
    }
    else //urb unlinked
	goto b_t_exit;
    
    
    if(called_from_ihandler){ // transfer complete
	dump_regs();
	// look at a result of a complete transfer
	transfer_status = sl811h_byte_read(controller, B_STATUS);  
	
	if(transfer_status & ACK){                                   // ACK
	    usb_dotoggle(urb->dev, usb_pipeendpoint(pipe), usb_pipeout(pipe));
	    // read data if 'in' packet 
	    if(!(desc->reg_main & DIRECTION)){
		transfer_count = sl811h_byte_read(controller, B_TRANSFER_COUNT); 
		if(transfer_count > desc->data_size){
		    // wrong hw behavior mustn't cause sw crash 
		    urb->status =  -EPIPE;
		    goto b_t_urb_complete;
		}		
		sl811h_buf_read(controller, B_MEM_BEGIN, 
				desc->data_size - transfer_count,desc->data);
		head->actual_length += desc->data_size - transfer_count;
		if(transfer_count){ // short packet - last packet
		    urb->actual_length = head->actual_length;
		    urb->status =  0;
		    goto b_t_urb_complete; 
		}
	    }
	    
	    head->current_desc++;
	    desc++;
	    head->desc_tries = BULK_DESC_TRIES;
	    goto b_t_initiate_new;
	}
	
	if(transfer_status & STALL){                                 // STALL
	    dbg("b_t: endpoint stalled, pipe %x", pipe);
	    usb_endpoint_halt(urb->dev, usb_pipeendpoint(pipe), usb_pipeout(pipe));
	    urb->status = -EPIPE;
	    goto b_t_urb_complete;
	}
	
	if(transfer_status & (NAK | ERR | TIMEOUT | OVERFLOW)){     // NAK,ERR,TIMEOUT
	    if(--(head->desc_tries)){
		controller->current_task = NO_TASK;
		// bulk_qh still 'need_service'
		// scheduler will call bulk task again
		return;
	    }
	    else{// no more tries
		dbg("b_t: too many consecutive errors, pipe %x", pipe);
		urb->status = -ETIMEDOUT; 
		goto b_t_urb_complete;   
	    }
	}
	
    }// end of "if(called_from_ihandler)"

 b_t_initiate_new:
    // prepare for initiating new transfer
    if(head->current_desc < head->number_of_descs){
	sl811h_buf_write(controller, B_HOST_BASE_ADDRESS, 4, (unsigned char*) &desc->regs);
	// write data to controller if out or setup packet
	if(desc->reg_main & DIRECTION){
	    sl811h_buf_write(controller, B_MEM_BEGIN, desc->data_size, desc->data);
	}
	// look how many time remains before end of frame (for not to bubble)
	time_needed = (usb_pipeslow(pipe)) ? 
	    EMPTY_TR_SS + BYTE_TR_SS * desc->data_size :
	    EMPTY_TR_FS + BYTE_TR_FS * desc->data_size;
	counter = sl811h_byte_read(controller, SOF_COUNTER_HIGH_CONTROL2);
	// to syncronize transfer with the begining of next frame SYNC_WITH_SOF bit might be used
	// but it seems it doesn't work, so we need do this stupid delaing 
	if(counter * BOGOTICK_LATANCY < time_needed)
	    udelay(counter * BOGOTICK_LATANCY);
	// initiate
	sl811h_byte_write(controller, B_HOST_CONTROL, desc->reg_main | 
	    ((usb_gettoggle(urb->dev, usb_pipeendpoint(pipe), usb_pipeout(pipe))) ? 
	     DATA_0_1 : 0));
	return;
    } 
    else{ // no more transfers have to be initiated
	urb->actual_length = head->actual_length;
	urb->status = 0;
    }
   
 b_t_urb_complete:
    dbg("b_t: calling complete");
    if(urb->complete)
	urb->complete(urb);
 b_t_exit:
    dbg("b_t: exit");
    del_timer(&head->task_timer);
    controller->current_task = NO_TASK;
    head->need_service = 0;   
}


void sl811h_iso_task(sl811h_t* controller, int called_from_ihandler){

}
int sl811h_unlink_urb(urb_t* urb);
//----------------- called by task_timer if task fails to complete in a time -----------
static void sl811h_abort_task(unsigned long __controller){
    sl811h_t* controller;
    sl811h_queue_head_t* head;
    urb_t* urb;
    
    controller =  (sl811h_t*) __controller;
    switch(controller->current_task){
    case CONTROL_TASK:
	dbg("aborting control task");
	head = controller->control_qh;
	urb = head->urb;
	sl811h_unlink_urb(urb);
	controller->current_task = NO_TASK;
	head->need_service = 0;
    return;
    
    case BULK_TASK:
	dbg("aborting bulk task");
	head = controller->bulk_qh;
	urb = head->urb;
	sl811h_unlink_urb(urb);
	controller->current_task = NO_TASK;
	head->need_service = 0;
    return;
    
    case INT_TASK:
	dbg("aborting int task");
    return;
    
    case ISO_TASK:
	dbg("aborting iso task");
    return;
    
    case NO_TASK:
	dbg("in abort task while NO_TASK state");
    return;
    }
}

//------------------ functions handling (dis)connections of devices----------------------
#define TIME_TO_SET_ADDRESS       1       // sec.
void clear_queue_heads(sl811h_t* controller);


void sl811h_allow_connect_disconnect(unsigned long __controller){
    sl811h_t* controller = (sl811h_t*) __controller; 
    
    controller->ignore_ins_rem = 0; 
}


void connect_disconnect_bh(unsigned long __controller){
    sl811h_t* controller;
    urb_t* urb;
    int tries;
    struct timer_list* timer1;
    int speed;
   
    controller = (sl811h_t*) __controller;
    urb = controller->rh.urb;
    tries = 3;
    timer1 = &controller->timer1;
    init_timer (timer1);
    
    //printk("____BH\n");
    // look what happaned on a bus 
    controller->rh.chstatus = controller->rh.chstatus | USB_PORT_STAT_C_CONNECTION;
    while(tries--){
	if((speed = speed_detect(controller)) != 0){
	    dbg("somebody connected");
	    // in case there is some garbidge, unlinked old urbs
	    clear_queue_heads(controller);
	    controller->allow_submit_urb = 1;
	    controller->current_task = NO_TASK;
	    controller->rh.status = (controller->rh.status | USB_PORT_STAT_CONNECTION |
				     ((speed == 1) ? USB_PORT_STAT_LOW_SPEED : 0));
	    break;
	}
	else
	    if(!tries){
		dbg("somebody disconnected");
		controller->allow_submit_urb = 0;
		controller->current_task = NO_TASK;
		controller->rh.status = (controller->rh.status & ~USB_PORT_STAT_CONNECTION );
	    }
    }
    // hub driver will handle our event
   
    if(urb->complete)
        urb->complete(urb);
    
    timer1->function = sl811h_allow_connect_disconnect;
    timer1->data = (unsigned long) controller;
    timer1->expires = jiffies + HZ * TIME_TO_SET_ADDRESS;
    add_timer (timer1); 
    return;
}		       

DECLARE_TASKLET (sl811h_connect_tasklet, connect_disconnect_bh, 0);

void sl811h_connect_disconnect(sl811h_t* controller, int unused){
    // ignoring new ins rem events for a while is needed for some odd devices, 
    //that keep RESET on bus untill they are assigned an address
  //printk("____UH\n");
    controller->ignore_ins_rem = 1;
    //
    sl811h_connect_tasklet.data = (unsigned long) controller;
    tasklet_schedule(&sl811h_connect_tasklet);
} 
 


//  sl811h_scheduler call this function before calling new x_task 
// to be sure that transfers of previos x_task is complete
// return -1 - incomplete transfer, 0 - ok
int wait_till_complete(sl811h_t* controller){
    int ret = 0;
    
    if(sl811h_byte_read(controller, A_HOST_CONTROL) & ALLOW_TRANSFER){
        ret = -1;
	dbg("     ! a incomplete");    
    }
    if(sl811h_byte_read(controller, B_HOST_CONTROL) & ALLOW_TRANSFER){
	ret = -1;
	dbg("     ! b incomplete");    
    }   
    return ret;
}

//------------------ scheduler -----------------------------------------------------
#define      SCHEDULER_INTERVAL       MIN_INT_INTERVAL

static void decrement_int_counters(sl811h_t* controller);
// every SCHEDULER_INTERVAL miliseconds this function decrement counters of int descriptors
// and look what task to run next; normally task should terminate in time less than
// MAX_x_TASK_TIME, if no - scheduler forces NO_TASK state.
static void sl811h_scheduler(sl811h_t* __controller, int called_from_ihandler){
    sl811h_t* controller = __controller;
    sl811h_task_type_t running_task;

    running_task = controller->current_task; 
    // keep track of scheduler's time
    if(--(controller->sched_time))
	if(running_task == NO_TASK)
	    goto choose_task;
	else
	    return;
    else
	controller->sched_time = SCHEDULER_INTERVAL;
    
    // decrement int counters
    decrement_int_counters(controller);
   
    // choose task to run
 choose_task:
    if(controller->control_qh->need_service)
	goto run_control_task;
    
    if(controller->int_qh->need_service)
	goto run_int_task;
    
    if(controller->bulk_qh->need_service)
	goto run_bulk_task;
    
    if(controller->iso_qh->need_service)
	goto run_iso_task;
    
    goto run_no_task;
    
    // run choosed task
 run_control_task:
    if(running_task == CONTROL_TASK)
	return;
    if(wait_till_complete(controller))
	return;
    controller->current_task = CONTROL_TASK;
    controller->b_done_ihandler = NULL;
    controller->a_done_ihandler = sl811h_control_task;
    sl811h_control_task(controller, 0);
    return;
 
 run_int_task:    
    if(running_task == INT_TASK)
	return;
    if(wait_till_complete(controller))
	return;
    controller->current_task = INT_TASK;
    controller->a_done_ihandler = sl811h_int_task;
    controller->b_done_ihandler = NULL;
    sl811h_int_task(controller, 0);
    return;
    
 run_bulk_task:    
    if(running_task == BULK_TASK)
	return;
    if(wait_till_complete(controller))
	return;
    controller->current_task = BULK_TASK;
    controller->a_done_ihandler = NULL;
    controller->b_done_ihandler = sl811h_bulk_task;
    sl811h_bulk_task(controller, 0);
    return;
   
 run_iso_task:  
    // not ready yet
    return;
    
 run_no_task:
    controller->current_task = NO_TASK; 
    controller->a_done_ihandler = NULL;
    controller->b_done_ihandler = NULL;
    return;
}

static void decrement_int_counters(sl811h_t* controller){
    sl811h_int_td_t* desc;
    int n;
    
    for(n = 0, desc = (sl811h_int_td_t*) controller->int_qh->descs; 
	n < INT_QUEUE_LENGTH; n++, desc++){
	if(desc->urb){ // not empty descriptor
	    desc->counter = 
	    (desc->counter > 0) ? (desc->counter - MIN_INT_INTERVAL) : 0;
	    if(!(desc->counter))
		controller->int_qh->need_service = 1;  
	}
    } 
}


//__________________ controller's interrupt handler ---------------------------------------
void sl811h_interrupt(int irq, void *__sl811h, struct pt_regs *regs){
    sl811h_t* controller;
    unsigned char istatus;
    int base;
    int size;
    int irq;
   
    controller = (sl811h_t*) __sl811h;
    // check if it's realy USB's interrupt
    // we share PIO irq line with Smart Media controller on PIO extension board
   
    psx_page_switch_ret(REGS, base, size);
  
    irq = inb(PSX_INTERRUPT_REQUEST);
   
    psx_page_switch_to(base, size);
 
    if(!(irq & 0x2))
	return;
    
    istatus = sl811h_byte_read(controller, INTERRUPT_STATUS);
    sl811h_byte_write(controller, INTERRUPT_STATUS, 0xff);
    
    if(istatus & SOF_TIMER){
	if(controller->sof_timer_ihandler)
	    (*(controller->sof_timer_ihandler))(controller, 1);
	else
	    dbg("NULL sof_ih");
    }
    
    if(istatus & USB_A){
	if(controller->a_done_ihandler)
	    (*(controller->a_done_ihandler))(controller, 1);
	else
	    dbg("NULL a_d_ih");
    }
    
    if(istatus & USB_B){
	if(controller->b_done_ihandler)
	    (*(controller->b_done_ihandler))(controller, 1);
	else
	    dbg("NULL b_d_ih");
    }
    
    if((istatus & INS_REM_DEVICE) && !(controller->ignore_ins_rem)){
      //printk("connect\n");
	if(controller->ins_rem_ihandler)
	   (*(controller->ins_rem_ihandler))(controller, 1);
	else
	    dbg("NULL ins_rem_ih");
    } 
   
    sl811h_byte_write(controller, INTERRUPT_STATUS, 0xff);
}


//---------------------- submit_x_urb functions -----------------------------------------------

static int submit_iso_urb (urb_t *urb){
    sl811h_t* controller; 

    controller = urb->dev->bus->hcpriv;
    err("sorry, yet driver doesn't support iso transfers\n");
    return -1;
}

// can be called from sl811h_submit_urb and from bulk_task (with *urb == 0)
// if bulk chain is free - fill descriptors, mark chain as 'need_servise'
// if no - do some check and place urb in a waiting list
static int submit_bulk_urb (urb_t *urb, sl811h_t *__controller){
    sl811h_t* controller;
    sl811h_queue_head_t* head;
    sl811h_bulk_td_t* desc;
    int pipe;
    int maxsze;
    int pid;         
    unsigned char* data;
    unsigned long len; 
    __u32 regs;
    
    if(!urb){ // if NULL argument try to get urb from list
	if(!__controller)
	    return -EINVAL;
	controller = __controller;
	head = controller->bulk_qh;
	if(!list_empty(&head->waiting_list)){
	    urb = list_entry(head->waiting_list.next, urb_t, urb_list);
	    list_del(&urb->urb_list);
	}
	else
	    return 0;
    }
    else{
	controller = urb->dev->bus->hcpriv;
	head = controller->bulk_qh;
    } 
    
    desc = (sl811h_bulk_td_t*) head->descs;
    pipe = urb->pipe;
    maxsze = usb_maxpacket(urb->dev, pipe, usb_pipeout(pipe));
   
    if(usb_endpoint_halted (urb->dev, usb_pipeendpoint(pipe), usb_pipeout(pipe)))
	return -EPIPE;

    if(urb->transfer_buffer_length < 0) {
	err("Negative transfer length in submit_bulk");
	return -EINVAL;
    }
    
    if(!maxsze) {
	err("submit_bulk_urb: pipesize for pipe %x is zero", pipe);
	return -EINVAL;
    }
    
    if(head->need_service){ // can't build descriptors right now
	if(urb->transfer_flags & USB_QUEUE_BULK){
	    list_add_tail(&urb->urb_list, &head->waiting_list);
	    urb->status = -EINPROGRESS;
	    return 0;
	}
	else{
	    dbg("can't queue bulk urb");
	    return -ENXIO;
	}
    }
   
    // initilize head
    head->number_of_descs = 0;
    head->current_desc = 0;
    head->desc_tries = BULK_DESC_TRIES;
    head->actual_length = 0;
    head->urb = urb;
    
    // fill descriptors
    len = urb->transfer_buffer_length;
    data = urb->transfer_buffer;
    while(len > 0){
	desc->data = data;    
	desc->data_size = (len < maxsze) ? len : maxsze;
	pid = ((usb_pipeout(pipe)) ? USB_PID_OUT : USB_PID_IN) & 0x0f;
	regs = (B_MEM_BEGIN) |             
	       (desc->data_size << 8)  |  
	       (((pid << 4) | usb_pipeendpoint(pipe)) << 16) |   
	       (usb_pipedevice(pipe) << 24);                      
	desc->regs = cpu_to_le32(regs);
	desc->reg_main = (ALLOW_TRANSFER | ENABLE_TRANSFER |
                           ((usb_pipeout(pipe)) ? DIRECTION : 0));
	dbg("bulk desc: regs %04x  reg_main %01x", desc->regs, desc->reg_main);
	
	head->number_of_descs++;
	len -= desc->data_size;
	if((head->number_of_descs >= BULK_QUEUE_LENGTH) && (len > 0)){ 
	    dbg("submit_bulk: overflow of bulk queue");
	    return -EINVAL;
	}
	data += desc->data_size;
	desc++;
    }    
    
    // descriptors successfully filled 
    urb->status = -EINPROGRESS;
    head->task_timer.data = (unsigned long) controller;
    head->task_timer.expires = jiffies + MAX_BULK_TASK_TIME;
    add_timer(&head->task_timer);
    head->need_service = 1;
    return 0;
}

static int submit_int_urb (urb_t *urb){
    sl811h_t* controller;
    sl811h_queue_head_t* head;
    sl811h_int_td_t* desc;
    int maxsze;
    int pid;
    int n;
    __u32 regs;
    
    controller = urb->dev->bus->hcpriv;
    head = controller->int_qh; 
    maxsze = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));
    
    if(urb->transfer_buffer_length > maxsze)
	return  -EINVAL;    
    
    if (urb->interval < 0 || urb->interval >= MAX_INT_INTERVAL)
	return -EINVAL;
    
    // look for a first free desc
    for(desc = (sl811h_int_td_t*) head->descs, n = 0; ; desc++, n++){
	if(n >= INT_QUEUE_LENGTH)// too many int endpoints
	    return   -EBUSY; 
	if(!(desc->urb)) // free descriptor 
	    break;
    }
    
    pid = ((usb_pipein(urb->pipe)) ? USB_PID_IN : USB_PID_OUT) & 0x0f;
    regs = (A_MEM_BEGIN) |                         
	   (urb->transfer_buffer_length << 8)  |            
	   (((pid << 4) | usb_pipeendpoint(urb->pipe)) << 16) | 
	   (usb_pipedevice(urb->pipe) << 24);                     
    desc->regs = cpu_to_le32(regs);   
    desc->reg_main = (ALLOW_TRANSFER | ENABLE_TRANSFER | 
		       ((usb_pipein(urb->pipe)) ? 0 : DIRECTION));
    desc->urb = urb;
    // round down interval to 2^n but not lees then MIN_INT_INTERVAL
    if(urb->interval <= MIN_INT_INTERVAL)
	desc->interval = DEFAULT_INT_INTERVAL;
    else
	for(n = MIN_INT_INTERVAL; n <= MAX_INT_INTERVAL; n += n)
	    if(urb->interval < n){
		desc->interval = n / 2;
		break;
	    }
    desc->counter = desc->interval;
    desc->toggle_bit = 0;
    urb->status = -EINPROGRESS;
    return 0;
} 

static int submit_control_urb (urb_t *urb){
    sl811h_t* controller;
    sl811h_queue_head_t* head;
    sl811h_control_td_t* desc;
    int maxsze;
    int pid;
    int parity = 0;
    unsigned char* data;
    unsigned long len;      
    __u32 regs; 
    
    ////////////////// DEBUGGING /////////////////////////////////////
    devrequest *cmd = (devrequest *) urb->setup_packet;
    __u16 bmRType_bReq;
    __u16 wValue;
    __u16 wIndex;
    __u16 wLength; 
    ////////////////////////////////////////////////////////////////////
    //printk("s_c_urb\n");
    controller = (sl811h_t*) urb->dev->bus->hcpriv; 
    head = controller->control_qh;
    desc = (sl811h_control_td_t*) head->descs;
    maxsze = usb_maxpacket (urb->dev, urb->pipe, usb_pipeout (urb->pipe));

    //look if the control is busy 
    if(head->need_service){
	dbg("submit_control_urb: control chain is busy");
        return -EBUSY;       
    }
    
    if (!maxsze) {
	err("submit_control_urb: pipesize for pipe %x is zero", urb->pipe);
	return -EINVAL;
    }
    
    head->number_of_descs = 0;
    head->current_desc = 0;
    head->queue_tries = CONTROL_QUEUE_TRIES;
    head->desc_tries = CONTROL_DESC_TRIES;
    head->actual_length = 0;
    head->urb = urb;
    

    ////////////////// DEBUGGING /////////////////////////////////////
    bmRType_bReq = cmd->requesttype | cmd->request << 8;
    wValue = le16_to_cpu (cmd->value);
    wIndex = le16_to_cpu (cmd->index);
    wLength = le16_to_cpu (cmd->length);
    dbg("submit control: adr: %2x cmd(%1x): %04x %04x %04x %04x",
	     usb_pipedevice(urb->pipe), 8, bmRType_bReq, wValue, wIndex, wLength);
   
    ////////////////////////////////////////////////////////////////////
      
    // build TD for setup stage
    desc->data = urb->setup_packet;   
    desc->data_size = SETUP_PACKET_SIZE; 
    pid = USB_PID_SETUP & 0x0f;
    regs = (A_MEM_BEGIN) |                        
	   (desc->data_size << 8)  |          
	   (((pid << 4) | usb_pipeendpoint(urb->pipe)) << 16) |
	   (usb_pipedevice(urb->pipe) << 24);                     
    desc->regs = cpu_to_le32(regs);              
    desc->reg_main = (ALLOW_TRANSFER | ENABLE_TRANSFER | DIRECTION);
    (head->number_of_descs)++;
    dbg("setup stage: regs %04x  reg_main %01x ", desc->regs, desc->reg_main);
    desc++;
    
    // build TDs for data stage (if needed)
    len = urb->transfer_buffer_length;
    data = urb->transfer_buffer;
 
    while(len > 0){
	desc->data = data;    
	desc->data_size = (len < maxsze) ? len : maxsze;
	pid = ((usb_pipeout(urb->pipe)) ? USB_PID_OUT : USB_PID_IN) & 0x0f;
	regs = (A_MEM_BEGIN) |             
	       (desc->data_size << 8)  |  
	       (((pid << 4) | usb_pipeendpoint(urb->pipe)) << 16) |   
	       (usb_pipedevice(urb->pipe) << 24);                      
	desc->regs = cpu_to_le32(regs);
	desc->reg_main = (ALLOW_TRANSFER | ENABLE_TRANSFER |
			   ((parity) ?  0 : DATA_0_1) |
                           ((usb_pipeout(urb->pipe)) ? DIRECTION : 0));
	dbg("data sage: regs %04x  reg_main %01x", desc->regs, desc->reg_main);
	
	head->number_of_descs++;
	len -= desc->data_size;
	if((head->number_of_descs >= CONTROL_QUEUE_LENGTH - 1) && (len > 0)){
	    dbg("submit_control: overflow of control queue");
	    desc++;
	    break;
	}
	data += desc->data_size;
	desc++;
	parity = parity^1;
    }

    // build TD for status stage
    desc->data = NULL;    
    desc->data_size = 0;      
    pid = ((usb_pipeout(urb->pipe)) ?  USB_PID_IN : USB_PID_OUT) & 0x0f;
    regs = (A_MEM_BEGIN) |             
	   (desc->data_size << 8)  |  
	   (((pid << 4) | usb_pipeendpoint(urb->pipe)) << 16) |   
	   (usb_pipedevice(urb->pipe) << 24);                     
    desc->regs = cpu_to_le32(regs);
    desc->reg_main = (ALLOW_TRANSFER | ENABLE_TRANSFER |
		        DATA_0_1 |
		       ((usb_pipein(urb->pipe)) ? DIRECTION : 0));
    
    head->number_of_descs++;
    dbg("status stage: regs %04x  reg_main %01x ", desc->regs, desc->reg_main);
    urb->status = -EINPROGRESS;
    head->task_timer.data = (unsigned long) controller;
    head->task_timer.expires = jiffies + MAX_CONTROL_TASK_TIME;
    add_timer(&head->task_timer);
    head->need_service = 1;
    return 0; 
}

int rh_submit_urb (urb_t*);  

//------ usb-core uses these 5 functions to interface with any usb host controller driver -------

int sl811h_submit_urb (urb_t *urb){
    sl811h_t* controller;
    
    if (!urb->dev || !urb->dev->bus || !urb->dev->bus->hcpriv)
	return -ENODEV;
    
    controller = urb->dev->bus->hcpriv;
    
    if (usb_pipedevice (urb->pipe) == controller->rh.devnum)
	return rh_submit_urb (urb);	
    
    if (!controller->allow_submit_urb)
	return -ENODEV;          
  
    if (usb_pipeisoc(urb->pipe))
	return submit_iso_urb (urb);
  
    if (usb_pipeint(urb->pipe))
	return submit_int_urb (urb); 
  
    if (usb_pipecontrol(urb->pipe))
	return submit_control_urb (urb);
  
    if (usb_pipebulk(urb->pipe))
	return submit_bulk_urb (urb, NULL);

    err("unknown type of urb: pipe == %d\n", urb->pipe);
    return -1;
}

int sl811h_alloc_dev (struct usb_device *usb_dev){
    return 0;
}

int sl811h_free_dev (struct usb_device *usb_dev){
    dbg("i am freeing dev!\n");
    return 0;
}

int sl811h_get_current_frame_number (struct usb_device *usb_dev){
    return 0;
}

// makes all queues free 
void clear_queue_heads(sl811h_t* controller){
    sl811h_queue_head_t* head;
    
    head = controller->control_qh;
    head->urb = NULL; 
    head->need_service = 0; 
   
    head = controller->int_qh;
    head->current_desc = 0;
    head->need_service = 0;
    
    head = controller->bulk_qh;
    // not ready
    head = controller->iso_qh;
    // not ready
}
 

void run_urb_completes(unsigned long __pasync_unlinked_urbs){
    struct list_head* pasync_unlinked_urbs;
    struct list_head* p;
    urb_t* urb;
    
    pasync_unlinked_urbs = (struct list_head*) __pasync_unlinked_urbs;
    p = pasync_unlinked_urbs->next;
    
    while(p != pasync_unlinked_urbs){
	urb = list_entry(p, urb_t, urb_list);
	p = p->next;
	list_del(&urb->urb_list);
	if(urb->complete){
	    urb->status = -ECONNRESET; // mark as asynchronously killed
	    urb->dev = NULL;
	    urb->complete(urb);
	}
    }
}


int sl811h_unlink_urb(urb_t *urb){
    sl811h_t* controller;
    sl811h_queue_head_t* head;
    sl811h_int_td_t* desc;
    struct timer_list* timer0;
    static struct list_head async_unlinked_urbs = {&async_unlinked_urbs, &async_unlinked_urbs};
    int n;

    controller = urb->dev->bus->hcpriv;
    
    dbg("sl811h_unlink_urb");
    
    if(urb->status != -EINPROGRESS) //already unlinked
	return 0;
    
    switch(usb_pipetype(urb->pipe)){
    
    case PIPE_CONTROL:
	head = controller->control_qh;
	if(urb == head->urb){
	    head->urb = NULL;
	    dbg("control urb unlinked");
	}
	break; 
    
    case PIPE_INTERRUPT:
	head = controller->int_qh;
	for(n = 0, desc = (sl811h_int_td_t*) head->descs;
		n < INT_QUEUE_LENGTH; n++, desc++){
	    if(urb == desc->urb){
		desc->urb = NULL;
		dbg("int urb unlinked");
	    } 
	}
	break;
    
    case PIPE_BULK:
	head = controller->bulk_qh;
	if(urb == head->urb){
	    head->urb = NULL;
	    dbg("bulk urb unlinked");
	}
	break;
    
    case PIPE_ISOCHRONOUS:
	head = controller->iso_qh;
	if(urb == head->urb){
	    head->urb = NULL;
	    dbg("iso urb unlinked");
	}
	break;
    }
   
    if(urb->transfer_flags & USB_ASYNC_UNLINK){ // async unlink
	urb->status = -ECONNABORTED;	// mark urb as "waiting to be killed"
	list_add(&urb->urb_list, &async_unlinked_urbs);
	timer0 = &controller->timer0;
	del_timer(timer0);              // in case it's already added
	init_timer (timer0);
	timer0->function = run_urb_completes;
	timer0->data = (unsigned long) &async_unlinked_urbs;
	timer0->expires = jiffies + 2;
	add_timer (timer0);
	return -EINPROGRESS;
    }
    else{ //sync unlink
	urb->status = -ENOENT;	        // mark urb as killed	
	if (urb->complete) {
	    urb->dev = NULL;
	    urb->complete(urb);
	}
	return 0;
    }	
}    


static struct usb_operations bus_operations =
{
    sl811h_alloc_dev,
    sl811h_free_dev,
    sl811h_get_current_frame_number,
    sl811h_submit_urb,
    sl811h_unlink_urb
};


//////----------------------- ROOT HUB ---------------------------------------------

__u8 root_hub_dev_des[] =
{
	0x12,			/*  __u8  bLength; */
	0x01,			/*  __u8  bDescriptorType; Device */
	0x00,			/*  __u16 bcdUSB; v1.0 */
	0x01,
	0x09,			/*  __u8  bDeviceClass; HUB_CLASSCODE */
	0x00,			/*  __u8  bDeviceSubClass; */
	0x00,			/*  __u8  bDeviceProtocol; */
	0x08,			/*  __u8  bMaxPacketSize0; 8 Bytes */
	0x00,			/*  __u16 idVendor; */
	0x00,
	0x00,			/*  __u16 idProduct; */
	0x00,
	0x00,			/*  __u16 bcdDevice; */
	0x00,
	0x00,			/*  __u8  iManufacturer; */
	0x02,			/*  __u8  iProduct; */
	0x01,			/*  __u8  iSerialNumber; */
	0x01			/*  __u8  bNumConfigurations; */
};


/* Configuration descriptor */
__u8 root_hub_config_des[] =
{
	0x09,			/*  __u8  bLength; */
	0x02,			/*  __u8  bDescriptorType; Configuration */
	0x19,			/*  __u16 wTotalLength; */
	0x00,
	0x01,			/*  __u8  bNumInterfaces; */
	0x01,			/*  __u8  bConfigurationValue; */
	0x00,			/*  __u8  iConfiguration; */
	0x40,			/*  __u8  bmAttributes; 
				   Bit 7: Bus-powered, 6: Self-powered, 5 Remote-wakwup, 4..0: resvd */
	0x00,			/*  __u8  MaxPower; */

     /* interface */
	0x09,			/*  __u8  if_bLength; */
	0x04,			/*  __u8  if_bDescriptorType; Interface */
	0x00,			/*  __u8  if_bInterfaceNumber; */
	0x00,			/*  __u8  if_bAlternateSetting; */
	0x01,			/*  __u8  if_bNumEndpoints; */
	0x09,			/*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
	0x00,			/*  __u8  if_bInterfaceSubClass; */
	0x00,			/*  __u8  if_bInterfaceProtocol; */
	0x00,			/*  __u8  if_iInterface; */

     /* endpoint */
	0x07,			/*  __u8  ep_bLength; */
	0x05,			/*  __u8  ep_bDescriptorType; Endpoint */
	0x81,			/*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
	0x03,			/*  __u8  ep_bmAttributes; Interrupt */
	0x08,			/*  __u16 ep_wMaxPacketSize; 8 Bytes */
	0x00,
	0xff			/*  __u8  ep_bInterval; 255 ms */
};


__u8 root_hub_hub_des[] =
{
	0x09,			/*  __u8  bLength; */
	0x29,			/*  __u8  bDescriptorType; Hub-descriptor */
	0x01,			/*  __u8  bNbrPorts; */
	0x00,			/* __u16  wHubCharacteristics; */
	0x00,
	0x01,			/*  __u8  bPwrOn2pwrGood; 2ms */
	0x00,			/*  __u8  bHubContrCurrent; 0 mA */
	0x00,			/*  __u8  DeviceRemovable; *** 7 Ports max *** */
	0xff			/*  __u8  PortPwrCtrlMask; *** 7 ports max *** */
};

//-----------------------------------
#define OK(x) 			len = (x); break

#define CLR_RH_PORTSTAT(x) \
		status = inw(io_addr+USBPORTSC1+2*(wIndex-1)); \
		status = (status & 0xfff5) & ~(x); \
		outw(status, io_addr+USBPORTSC1+2*(wIndex-1))

#define SET_RH_PORTSTAT(x) \
		status = inw(io_addr+USBPORTSC1+2*(wIndex-1)); \
		status = (status & 0xfff5) | (x); \
		outw(status, io_addr+USBPORTSC1+2*(wIndex-1))
//-----------------------------------

int rh_submit_int_urb (urb_t *urb){
    sl811h_t* controller;

    controller = urb->dev->bus->hcpriv;
    dbg("rh_submit_int_urb speaks\n");
    controller->rh.urb = urb;
    controller->rh.send = 1;
    controller->rh.interval = urb->interval;
    return 0;
}

int rh_submit_control_urb (urb_t *urb){
    sl811h_t* controller;
    devrequest *cmd = (devrequest *) urb->setup_packet;
    void *data = urb->transfer_buffer;
    int leni = urb->transfer_buffer_length;
    int len = 0;
    int status = 0;
    int stat = 0;
    int i;
    __u16 cstatus;
    __u16 bmRType_bReq;
    __u16 wValue;
    __u16 wIndex;
    __u16 wLength;

    controller = urb->dev->bus->hcpriv;
    
    bmRType_bReq = cmd->requesttype | cmd->request << 8;
    wValue = le16_to_cpu (cmd->value);
    wIndex = le16_to_cpu (cmd->index);
    wLength = le16_to_cpu (cmd->length);

    for (i = 0; i < 8; i++)
	controller->rh.c_p_r[i] = 0;  // ???????????????

    ////////////////////// DEBUGGING ////////////////////////////////
    dbg("Root-Hub: adr: %2x cmd(%1x): %04x %04x %04x %04x",
	     controller->rh.devnum, 8, bmRType_bReq, wValue, wIndex, wLength);
    /////////////////////////////////////////////////////////////////
    
    switch (bmRType_bReq) {
	// Request Destination:
	//   without flags: Device, 
	//   RH_INTERFACE: interface, 
	//   RH_ENDPOINT: endpoint,
	//   RH_CLASS means HUB here, 
	//   RH_OTHER | RH_CLASS  almost ever means HUB_PORT here 
	
    case RH_GET_STATUS:
	*(__u16 *) data = cpu_to_le16 (1);
	OK (2);
    case RH_GET_STATUS | RH_INTERFACE:
	*(__u16 *) data = cpu_to_le16 (0);
	OK (2);
    case RH_GET_STATUS | RH_ENDPOINT:
	*(__u16 *) data = cpu_to_le16 (0);
	OK (2);
    case RH_GET_STATUS | RH_CLASS:
	*(__u32 *) data = cpu_to_le32 (0);
	OK (4);		//hub power 
    case RH_GET_STATUS | RH_OTHER | RH_CLASS:
	status = controller->rh.status;
	cstatus = controller->rh.chstatus;
	
	*(__u16 *) data = cpu_to_le16 (status);
	*(__u16 *) (data + 2) = cpu_to_le16 (cstatus);
	OK (4);
	
    case RH_CLEAR_FEATURE | RH_ENDPOINT:
	switch (wValue) {
	case (RH_ENDPOINT_STALL):
	    OK (0);
	}
	break;
	
    case RH_CLEAR_FEATURE | RH_CLASS:
	    switch (wValue) {
	    case (RH_C_HUB_OVER_CURRENT):
		OK (0);	// hub power over current
	    }
	    break;

    case RH_CLEAR_FEATURE | RH_OTHER | RH_CLASS:
	switch (wValue) {
	
	case (RH_PORT_ENABLE):
	    dbg("clearing feature PORT_ENABLE ");
	    controller->rh.status = (controller->rh.status & ~USB_PORT_STAT_ENABLE);
	    OK (0);
	
	case (RH_PORT_SUSPEND):
	    // yet not ready
	    OK (0);
	
	case (RH_PORT_POWER):
	    OK (0);	// port power
	
	case (RH_C_PORT_CONNECTION):
	    dbg("clearing feature C_PORT_CONNECTION ");
	    controller->rh.chstatus = (controller->rh.chstatus & ~USB_PORT_STAT_C_CONNECTION );
	    OK (0);
	
	case (RH_C_PORT_ENABLE):
	    // yet nothing we need here
	    OK (0);
	
	case (RH_C_PORT_SUSPEND):
	    // notready
	    OK (0);
	
	case (RH_C_PORT_OVER_CURRENT):
	    OK (0);	// port power over current 
	
	case (RH_C_PORT_RESET):
	    OK (0);
	}
	break;
	
    case RH_SET_FEATURE | RH_OTHER | RH_CLASS:
	switch (wValue) {
	case (RH_PORT_SUSPEND):
	    // yet not ready
	    OK (0);
	        
	case (RH_PORT_RESET):
	    dbg("setting feature PORT_RESET ");
	    // no real reset perfomed here
	    controller->rh.status = (controller->rh.status | USB_PORT_STAT_ENABLE
				     | USB_PORT_STAT_OVERCURRENT);
	    OK (0);
	
	case (RH_PORT_POWER):
	    OK (0);	// port power
	case (RH_PORT_ENABLE): 
	    dbg("setting feature PORT_ENABLE ");
	    controller->rh.status = (controller->rh.status | USB_PORT_STAT_ENABLE);
	    OK (0);
	}
	break;                                                
	                                                                                       
    case RH_SET_ADDRESS: 
	controller->rh.devnum = wValue;
	dbg("addr %d  assigned to root hub", controller->rh.devnum);    
	OK (0);

    case RH_GET_DESCRIPTOR: 
	switch ((wValue & 0xff00) >> 8) {
	case (0x01):	// device descriptor 
	  len = min (leni, min (sizeof (root_hub_dev_des), wLength));
	  memcpy (data, root_hub_dev_des, len);
	  dbg("len = %d", len);
	  OK (len);
	case (0x02):	// configuration descriptor 
	  len = min (leni, min (sizeof (root_hub_config_des), wLength));
	  memcpy (data, root_hub_config_des, len);
	  OK (len);
	case (0x03):	// string descriptors 
	  len = usb_root_hub_string (wValue & 0xff,
				     controller->io_addr, "ScanLogic",
				     data, wLength);
	  if (len > 0) {
	    OK (min (leni, len));
	  } else 
	    stat = -EPIPE;
	}
	break;
	
    case RH_GET_DESCRIPTOR | RH_CLASS:
	root_hub_hub_des[2] = controller->rh.numports;
	len = min (leni, min (sizeof (root_hub_hub_des), wLength));
	memcpy (data, root_hub_hub_des, len);
	OK (len);
	
    case RH_GET_CONFIGURATION:
      *(__u8 *) data = 0x01;
      OK (1);
	 
    case RH_SET_CONFIGURATION:
      OK (0);                        
    default:
	stat = -EPIPE;
    }
    
    urb->actual_length = len;
    urb->status = stat;
    urb->dev=NULL;
    
    return 0;
}    
    
int rh_submit_urb (urb_t *urb){
  //    dbg("rh_submit_urb speaks");
    if (usb_pipeint(urb->pipe))
	return rh_submit_int_urb (urb);
  
    if (usb_pipecontrol(urb->pipe))
	return rh_submit_control_urb (urb);

    err("unknown type of urb for root hub: pipe == %d\n", urb->pipe);
    return -1;
}


//////------------- INITIALIZATION STAFF ------------------------------------------------------

//------------- building skeleton of transfer descriptors--------------------------------------
static int init_skel(sl811h_t* controller){
    void* p;
    struct timer_list* timer;
    
    if(!(p = (sl811h_queue_head_t*) kmalloc (4 * sizeof(sl811h_queue_head_t), GFP_KERNEL)))
	return -1;
    else{
	memset(p, 0, 4 * sizeof(sl811h_queue_head_t));
	// 
	controller->control_qh = p;
	timer = &controller->control_qh->task_timer;
	init_timer(timer);
	timer->function = sl811h_abort_task;
	p = p + sizeof(sl811h_queue_head_t);
	//
	controller->int_qh = p;
	timer = &controller->int_qh->task_timer;
	init_timer(timer);
	timer->function = sl811h_abort_task;
	p = p + sizeof(sl811h_queue_head_t);
	//
	controller->bulk_qh = p;
	timer = &controller->bulk_qh->task_timer;
	init_timer(timer);
	timer->function = sl811h_abort_task;
	// we use list for bulk urbs
	INIT_LIST_HEAD(&controller->bulk_qh->waiting_list);
	p = p + sizeof(sl811h_queue_head_t);   
	//
	controller->iso_qh = p;
	timer = &controller->iso_qh->task_timer;
	init_timer(timer);
	timer->function = sl811h_abort_task;
	
    }
    
    if(!(p = kmalloc (CONTROL_QUEUE_LENGTH * sizeof(sl811h_control_td_t), GFP_KERNEL)))
	return -1;
    else{
	memset(p, 0, CONTROL_QUEUE_LENGTH * sizeof(sl811h_control_td_t));
	controller->control_qh->descs = p;
    }

    if(!(p = kmalloc (INT_QUEUE_LENGTH * sizeof(sl811h_int_td_t), GFP_KERNEL)))
	return -1;
    else{
	memset(p, 0, INT_QUEUE_LENGTH * sizeof(sl811h_int_td_t));
	controller->int_qh->descs = p;
    }
   
    if(!(p = kmalloc (BULK_QUEUE_LENGTH * sizeof(sl811h_bulk_td_t), GFP_KERNEL)))
	return -1;
    else{
	memset(p, 0, BULK_QUEUE_LENGTH * sizeof(sl811h_bulk_td_t));
	controller->bulk_qh->descs = p;
    }

    if(!(p = kmalloc (ISO_QUEUE_LENGTH * sizeof(sl811h_iso_td_t), GFP_KERNEL)))
	return -1;
    else{
	memset(p, 0, ISO_QUEUE_LENGTH * sizeof(sl811h_iso_td_t));
	controller->iso_qh->descs = p;
    }
    return 0;
}


void cleanup_skel(sl811h_t* controller){  
    //////////// INCOMPLETE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    
}

sl811h_t* __init alloc_sl811h (int io_addr, int io_size, int irq){
    sl811h_t* controller;
  
    if(!(controller = kmalloc (sizeof (sl811h_t), GFP_KERNEL)))
	return NULL;
    memset (controller, 0, sizeof (sl811h_t));
    controller->io_addr = io_addr;
    controller->io_size = io_size;
    controller->irq = irq;
    controller->ignore_ins_rem = 0;
    controller->ins_rem_ihandler = sl811h_connect_disconnect;   
    controller->a_done_ihandler = NULL;    
    controller->b_done_ihandler = NULL;
    controller->sof_timer_ihandler = sl811h_scheduler;
    controller->sched_time = SCHEDULER_INTERVAL;
    controller->task_time = 0;
    controller->current_task = NO_TASK;
    init_timer (&controller->timer0);
    init_timer (&controller->timer1);
    init_timer (&controller->timer2);
    INIT_LIST_HEAD (&controller->other);
    return controller;
}

//-------------------- prepare2work -------------------------------------------------
// in this function we try to obtain nessesary resourses for our driver to work,
// then reset and init controller, then connect virtual root hub,

int __init  prepare2work(int io_addr, int io_size, int irq)
{    
    sl811h_t* controller;

    if((controller = alloc_sl811h(io_addr, io_size, irq)) == NULL){    
	err("memory allocation for controller private data failed!");
     return -1;
    }

    if(init_skel(controller)){
        err("memory allocation for transfer descriptors failed!");
	kfree(controller);
    }
    
    // on PSX box this check fail, so we comment it
    //if(check_region (controller->io_addr, controller->io_size)){
    //  err("resource is busy: io_addr %d, io_size %d", controller->io_addr, controller->io_size);
    //cleanup_skel(controller);
    //kfree(controller);
    //return -1;
    //}
    //else
    //  request_region (controller->io_addr, controller->io_size, MODNAME);
    
    reset_hc(controller);
    // no interrupt occures untill init_hc() and reset_on_bus()
    // near the end of this function
    if (request_irq (controller->irq, sl811h_interrupt, SA_INTERRUPT, MODNAME, controller)) {
	err("request_irq %d failed!",controller->irq);	
	release_region (controller->io_addr, controller->io_size);
	cleanup_skel(controller);
	kfree(controller);
	return -1;
    }
    // registring our bus
    if(!(controller->bus = usb_alloc_bus(&bus_operations))){
	err("memory allocation for usb_bus data structure failed!");
	free_irq (controller->irq, controller);
	release_region (controller->io_addr, controller->io_size);
	cleanup_skel(controller);
	kfree(controller);
	return -1;
    }
    else{
	controller->bus->hcpriv = controller;
	usb_register_bus (controller->bus);
    }
    // allocating virtual root hub
    if(!(controller->bus->root_hub = usb_alloc_dev (NULL, controller->bus))){
	err("memory allocation for root_hub data structure failed!");
	usb_deregister_bus (controller->bus);
	usb_free_bus (controller->bus);
	free_irq (controller->irq, controller);
	release_region (controller->io_addr, controller->io_size);
	cleanup_skel(controller);
	kfree(controller);
	return -1;
    }    
    // connect means they give us devnum (or addres if you like) 
    usb_connect (controller->bus->root_hub);
    // makes root hub to response on first setaddr request 
    controller->rh.devnum = 0;  
    // sl811hs controller has a single port 
    controller->rh.numports = 1;
    // now they send us control urbs to set address (devnum) to root hub and configure it    
    if (usb_new_device (controller->bus->root_hub)) {
	usb_disconnect (&(controller->bus->root_hub));
	usb_deregister_bus (controller->bus);
	usb_free_bus (controller->bus);
	free_irq (controller->irq, controller);
	release_region (controller->io_addr, controller->io_size);
	cleanup_skel(controller);
	kfree(controller);
	return -1;
    }
    list_add(&controller->other, &sl811h_controllers);
    init_hc(controller);
    reset_on_bus(controller);
    return 0;
}


///// debugging
struct timer_list itimer;
    
/////

static int __init sl811h_init (void) {
    int io_addr;
    int io_size;
    int base;
    int size;
    int irq;
    
    INIT_LIST_HEAD(&sl811h_controllers);
    // on a pio extention board we have single USB controller
    io_addr = PSX_USB_0;
    io_size = 0x2;
    irq = PIO;
    // bit 1 in PSX_INTERRUPT_MASK masks USB interrupt, we unmask it
 
    psx_page_switch_ret(REGS, base, size);
  
    outb(inb(PSX_INTERRUPT_MASK) | 0x2, PSX_INTERRUPT_MASK);
  
    psx_page_switch_to(base, size);

    return prepare2work(io_addr, io_size, irq);
}

static void  sl811h_cleanup (void){
    sl811h_t* controller;
    struct list_head* p;
    
    p = sl811h_controllers.next;
    while(p != &sl811h_controllers){
	controller = list_entry(p, sl811h_t, other);
	controller->allow_submit_urb = 0;		       
	usb_disconnect (&(controller->bus->root_hub));  
	usb_deregister_bus (controller->bus);      
	usb_free_bus (controller->bus); 
	release_region (controller->io_addr, controller->io_size);
	free_irq (controller->irq, controller);
	//cleanup_skel(controller); DEBUG THIS !
	//kfree (controller);
	p = p->next; 
    }     
    //kfree (&sl811h_controllers);
    dbg(" good bye.");
}

module_init (sl811h_init);
module_exit (sl811h_cleanup);



