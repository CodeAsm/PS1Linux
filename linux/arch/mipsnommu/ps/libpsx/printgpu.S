#---------- print text ----------

#include <asm/regdef.h>
#include <asm/ps/gpu.h>

#define	CLUT_ID(page, x, y)	(( page & 0xf ) << 2 )  | (( page & 0x10 ) << 10 ) | ( x >> 4 ) | ( y << 6 )
#define	TAG( adr, size ) 	.word	adr - 0x80000000 + ( size << 24 )
#define	END_TAG( size )		.word	0xffffff + ( size << 24 )

#define  GPU_PRIM_COMMAND	0
#define  GPU_PRIM_POLYGON	1
#define  GPU_PRIM_LINE		 2
#define  GPU_PRIM_SPRITE 	3
#define  GPU_PRIM_TRANSFER 4
#define  GPU_PRIM_ENVIRON  7

			.global	cls
cls:	subu	sp,sp,4
			sw		ra,(sp)
			la		a0,back
			jal	 SendList
			lw		ra,(sp)
			addu	sp,sp,4
			jr		ra

prim_cls:	END_TAG( 3 )
  		.byte	0x0, 0x0, 0x0, 0x02			# rgb
			.short	0, 0										# x, y  
			.short	320, 512								# w, h
	  		
back:
      TAG( back2, 3 )
      .word 0x02000000
      .word 0x000003c0
      .word 0x010000fe


back2:   END_TAG( 8 )   	# Primitive of gouroud 4 point polygon
      .word  0x38000000    # Type + bgr    top left
			.short 0,0
      .word  0x00000000   # bgr           top right
			.short 0,255
      .word  0x00000000    # bgr           bottom left
			.short 319,0
      .word  0x00000000    # bgr           bottom right
			.short 319,255


			.global	LoadFont

LoadFont:   subu	sp,sp,4
			sw		 ra,(sp)
			
			la 		a0,font+64      # transfer image data to VRAM
      li 		a1,320          # x = 320, y = 0
    	li 		a2,64+(48<<16)	# w = 64, h = 48
      li 		a3,64*48/2			# anzahl
      jal 	 mem2vram

      la 		a0,font+20      # transfer clut data to VRAM
      
#			lh		v0,(a0)					# transparent or not
#			or		v0,v0,0x8000
#			sh		v0,(a0)
			
      li 		a1,320+(384<<16) # x = 320, y = 384
      li 		a2,16+(1<<16)		# w = 16, h = 1
      li 		a3,8
      jal 	 mem2vram

	lw		ra,(sp)
	addu	sp,sp,4
	jr		ra
	
			.global	set_tPage

#---------- print text -----------------------------------------
# a0: void *pimitive

set_tPage:	addu	v0,a0,8
			sw		v0,(a0)
			li		v1,1
			sb		v1,3(a0)
			li		v1,0xE1000005               	# tPage for font
			sw		v1,4(a0)
			jr		ra
			

#---------- print text -----------------------------------------
# a0: char *text
# a1: *primitive
			
			.global	print_txt

print_txt:	subu	sp,sp,12
			sw		s0,(sp)
			sw		s1,4(sp)
			sw		ra,8(sp)

			move	s0,a0					# txt-pointer
			move	a0,a1					# prim-tab
			li		a1,0					 # x
			li		a2,0					 # y

print_inf1:	lbu		a3,(s0)						# Zeichen
			addu	s0,s0,1
			beqz	a3,print_end
			xor	 v0,a3,0x80
			bnez	v0,no_control
			lbu	 a1,(s0)					 # new x
			lbu	 a2,1(s0)					# new y
			sll	 a1,a1,3					 # x * 8
			addu	t0,a2,a2					# y * 12
			addu	t0,t0,a2
			sll	 a2,t0,2
			addu	s0,s0,2
			j		 print_inf1

no_control: 		jal		print
			move	a0,v0			// next primitive address
			addu	a1,a1,8						# x += 8
			j		print_inf1

print_end:  
			li		t0,0x04ffffff
			sw		t0,-20(a0)	// end of primitives list

			lw		s0,(sp)
			lw		s1,4(sp)
			lw		ra,8(sp)

			addu		sp,sp,12
			jr		ra

#---------- print -----------------------
// creates primitive at prim-address
# a0: prim-address, 20 bytes per character
# a1: x
# a2: y
# a3: char
// Output:
//	v0 -- address of the next primitive	

print:
			addu	v0,a0,5*4				# prim needs 4+1 words
      sw		v0,(a0)					# link to next prim
      li		t0,4
      sb		t0,3(a0)         # prim - size

// set font color
      li		t0,0x64808080
      sw		t0,4(a0)
// set x y position
      sh		a1,8(a0)				# x
      sh		a2,10(a0)       # y
// set char decomposition (u,v)
      and	 t0,a3,0x1f
      sll	 t0,t0,3				 # * 8
      sb		t0,12(a0)			 # u
      srl	 t0,a3,5
      addu	t1,t0,t0				# * 2
      addu	t1,t1,t0				# * 3
      sll	 t0,t1,2	 			# * 12
      sb		t0,13(a0)	 		# v
// set clut id      
      li		t0,CLUT_ID( 21, 0, 128 )
      sh		t0,14(a0)
// set font size
      li		t0,0x000c0008 	# w, h
      sw		t0,16(a0)

      jr		ra

			.global print2
#---------- print -----------------------
# a0: x
# a1: y
# a2: char

print2:     subu	sp,sp,4
			sw		ra,(sp)
			la		t0,pp1
			
      sh		a0,8(t0)				# x
      sh		a1,10(t0)       # y
      and	 t1,a2,0x1f
      sll	 t1,t1,3				 # * 8
      sb		t1,12(t0)			 # u
      srl	 t1,a2,5
      addu	t2,t1,t1				# * 2
      addu	t2,t2,t1				# * 3
      sll	 t1,t2,2	 			# * 12
      sb		t1,13(t0)	 		# v
         
	la		a0,print_prim
	jal	 SendList
			
 		li 	 a0,1000
			
ww:	 subu 	a0,a0,1
			bnez 	a0,ww
	
			lw		ra,(sp)
			addu	sp,sp,4
      jr		ra


			.global	print_prim
			
print_prim:	TAG( pp1, 1 )
			.word	0xE1000005              	# tPage for font
pp1:	END_TAG( 4 )
    	.byte	0x80, 0x80, 0x80, 0x64		# rgb, font color
			.short	0, 0						 # x, y
			.byte	 0, 0						// u, v - char decomposition
			.short	CLUT_ID( 21, 0, 128 )
			.short	8, 12  					# w, h
    		

			.balign	4
font:
#include "font/font.S"
			.balign	2048
the_end:
			
