/*
 * PlayStation GPU-console library functions.
 */
 
#ifndef __ASM_PS_LIBPSX_H 
#define __ASM_PS_LIBPSX_H 

void InitGPU( int );
void cls( void );
void mem2vram( void *, int, int, int );
void LoadFont( void );
void *set_tPage( void * );
void *print_txt( char *, void * );
void SendList( void * );
void print_hex( int, int, int );
void print2( int, int, char );
// waits for DMA & GPU to be idle
void gpu_dma_gpu_idle( void );
// waits for GPU's DMA idle
void gpu_dma_idle( void );


void xprintf (char *, ...);
int OpenEvent( unsigned int, int, int, void * );
int CloseEvent( int );
int EnableEvent( int);
int DisableEvent( int );
void exit_critic( void );
int SetRCnt( int, unsigned short, int );
void _96_init( void );
void _96_remove( void );

void AddQueue(int, int *);

void CDInstallInt( void );
int LoadCDSec( int, int, void *);
int SeekCD( char *);
int LoadCD( int, void *, int );
int SysError( int, int );

int int_on( int );
int int_off( int );

#endif
