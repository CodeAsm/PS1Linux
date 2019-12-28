#include <asm/regdef.h>
#include <asm/ps/gpu.h>

#define HPOS 688

//---------- WaitGPU - waits until GPU ready to receive commands

			.global	WaitGPU
			
WaitGPU:   
	     		li		 t2,PSX_HW
			li     t0,0x10000000 // load bit $1c

WaitGPU1:  lw 		t1,GPU1(t2)  // load status word from GPU
           and 	 	t1,t1,t0		 // and out all but bit $1c
           beqz 	t1,WaitGPU1	// bit $1c = 0 -> GPU is busy
           jr 		ra


#---------- WaitIdle - waits until GPU is idle      
			
			.global	WaitIdle

WaitIdle:
	    li		 t2,PSX_HW	
	    li 		t1,0x04000000
WaitIdle1:  lw 		t0,GPU1(t2)
            and 	 t0,t0,t1
            beqz 	t0, WaitIdle1
            jr 		ra

//-------------------------------------------------------------
// gpu_dma_gpu_idle - waits for DMA & GPU to be idle.
//-------------------------------------------------------------
// void gpu_dma_gpu_idle( void )

			.global	gpu_dma_gpu_idle
			
gpu_dma_gpu_idle:
				li		 t2,PSX_HW	
        lui    t1,0x0100        	      					
loop_wd1:
	lw     t0, D2_CHCR(t2)
        and    t0, t0, t1
        bne    t0, zero, loop_wd1
        
        lui    t1, 0x0400
//loop_wd2:
	lw     t0, GPU1(t2)
        and    t0, t0, t1
        beq    t0, zero, gpu_dma_gpu_idle
        nop
        jr      ra
        
//-------------------------------------------------------------
// gpu_dma_idle - waits for GPU's DMA idle.
//-------------------------------------------------------------
// void gpu_dma_idle( void )
				.global gpu_dma_idle
				
gpu_dma_idle:
        li      t2, PSX_HW
        lui     t1, 0x0100
        
loop_gpu_dma_idle:
			  lw      t0, D2_CHCR(t2)
        and     t0, t0, t1
        bne     t0, zero, loop_gpu_dma_idle
        jr      ra

#----------- InitGPU - basic GPU init routine ----------------------------------

#in: a0: display mode

			.global	InitGPU

InitGPU:    subu	sp,sp,8
			sw		ra,(sp)
			sw		s0,4(sp)
			
			li		s0,0x1f800000
            sw 		zero,GPU1(s0)       # reset

            li 		t3,0x03000001
            sw 		t3,GPU1(s0)         # disable display

            li 		t3,0x05000000		# display offset
            sw 		t3,GPU1(s0)

#            li 		t3,0x06c40240       # horizontal start/end    
            li		t3,HPOS|((HPOS+2560)<<12)|0x06000000
            sw 		t3,GPU1(s0)

            li 		t3,0x07049025    	# vertical start/end
            sw 		t3,GPU1(s0)

            li 		t3,0xe1000686    	# draw mode, texture page = (8bit,320,0)
            jal 	WaitGPU
            sw 		t3,GPU0(s0)

#???PSX TEST            
#	    li 		t3,0xe2000685    	# draw mode, texture page = (8bit,320,0)
#            jal 	WaitGPU
#            sw 		t3,GPU0(s0)

            jal 	WaitGPU
            li 		t3,0xe3000000      	# clip start
            sw 		t3,GPU0(s0)

            li 		t3,0xe4000000+(512<<10)+640       # clip w, h
            jal 	WaitGPU
            sw 		t3,GPU0(s0)

            li 		t3,0xe5000000       # draw offset
            jal 	WaitGPU
            sw 		t3,GPU0(s0)

            sw 		a0,GPU1(s0)          # set display mode
            nop

            li 		t3,0x03000000
            sw 		t3,GPU1(s0)          # enable display

			lw		ra,(sp)
			lw		s0,4(sp)
			addu	sp,sp,8
            jr 		ra



#---------- MEM2VRAM_IO - transfer graphic data to VRAM (IO mode)
# in: a0: source address
#     a1: x,y    (y << 16 | x)
#     a2: w,h    (h << 16 | w)
#     a3: amount to copy (in words)

			.global	mem2vram

mem2vram:   move 	t3,ra
            jal 	WaitIdle
            li		t4,0x1f800000
            li 		t2,0x04000000
            sw 		t2,GPU1(t4)
            li 		t0,0x01000000
            li 		t1,0xa0000000
            sw 		t0,GPU1(t4)
            sw 		t1,GPU0(t4)
            sw 		a1,GPU0(t4)
            sw 		a2,GPU0(t4)

            move 	t0,a3           	# anzahl words
            move 	t2,a0               # source

m2v_io_loop:lw 		t1,(t2)  			# get source word
            subu 	t0,t0,1             # i--;
            sw 		t1,GPU0(t4)         # write to vram
            addu 	t2,t2,4             # next word
            bne 	zero,t0,m2v_io_loop

            move 	ra,t3
            jr 		ra
            

#---------- SendList - sends a list of primitives to GPU
# in: a0: address of list
			
			.global	SendList
			
SendList:   move 	t9,ra
            jal 	 WaitGPU
	
	    li		 t4,0x1f800000
            li 		t2,0x04000002
           
            lw 		t3,DPCR(t4)
#            sw 		zero,DICR(t4)
            ori		t3,0x800
            sw 		t3,DPCR(t4)

            sw 		t2,GPU1(t4)
            sw 		a0,D2_MADR(t4)
            sw 		zero,D2_BCR(t4)
            li 		t1,0x01000401
            sw 		t1,D2_CHCR(t4)
           
            jr 		t9



