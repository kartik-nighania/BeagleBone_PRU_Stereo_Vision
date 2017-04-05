#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <cairo/cairo.h>

#include <i2cfunc.h>

#include "PRU_stereoCapture"
#include <pruss_intc_mapping.h>	 
#include "initCamera.h"

#define PRU_NUM 	 1
#define ADDEND1	 	 0x98765400u
#define ADDEND2		 0x12345678u
#define ADDEND3		 0x10210210u

#define DDR_BASEADDR     0x80000000
#define OFFSET_DDR	 0x00001008

#define OFFSET_SHAREDRAM 0
#define PRUSS1_SHARED_DATARAM    4


#define RGB565_MASK_RED 0xF800  
#define RGB565_MASK_GREEN 0x07E0  
#define RGB565_MASK_BLUE 0x001F  

static int LOCAL_exampleInit ( );
static unsigned short LOCAL_examplePassed ( unsigned short pruNum );

static int mem_fd;
static void *ddrMem, *sharedMem;

static int chunk;

static unsigned int *sharedMem_int;

	FILE* outfile;

static unsigned int gram[160][120];
__attribute__((aligned (16)))
unsigned char cairobuff[128000];


cairo_surface_t *cs;
cairo_t *c;

void
init_graphicslib(void)
{
	int stride;
	stride=cairo_format_stride_for_width(CAIRO_FORMAT_RGB16_565, 160);
	printf("stride is %d\n", stride);
	cs=cairo_image_surface_create_for_data(cairobuff, CAIRO_FORMAT_RGB16_565, 160, 120, stride);
	c=cairo_create(cs);
}

int reverse(int x, int bits)
{
    x = ((x & 0x55555555) << 1) | ((x & 0xAAAAAAAA) >> 1);
    x = ((x & 0x33333333) << 2) | ((x & 0xCCCCCCCC) >> 2);
    x = ((x & 0x0F0F0F0F) << 4) | ((x & 0xF0F0F0F0) >> 4);
    x = ((x & 0x00FF00FF) << 8) | ((x & 0xFF00FF00) >> 8);
    x = ((x & 0x0000FFFF) << 16) | ((x & 0xFFFF0000) >> 16);
    return x >> (32 - bits);
}

unsigned short int
rev2(unsigned short int v)
{
	int i;
	unsigned short int q=0;
	unsigned short int m;
	unsigned short int mm;
	for (i=0; i<16; i++)
	{
		m=1;
		m=m<<i;
		mm=0x8000;
		if (m & v)
		{
			q |= (mm>>i);
		}
	}
	return(q);
}

void
ram2cairo(void)
{
	int x, y;
	unsigned int p1, p2, pt;
	unsigned int* bufp;
	
	bufp=(unsigned int*) cairobuff;
	
	for (y=0; y<120; y++)
	{
		for (x=0; x<160; x=x+2)
		{
			p1=gram[x+0][y];
			p2=gram[x+1][y];
			pt=((p1 & 0xffff)<<16) | (p2 & 0xffff);
			bufp[ (x/2) + (80*(y))]=pt;
		}
	}
}

void dumpdata(void)
{
	unsigned short int *DDR_regaddr;
	unsigned char* test;
	int ln;
	int x;
	unsigned char tv;

  unsigned short int rgb565;
  unsigned char rgb24[4];
  unsigned char v1, v2;
  rgb24[3]=0;
	

	
	DDR_regaddr = ddrMem + OFFSET_DDR;
	test=(unsigned char*)&sharedMem_int[OFFSET_SHAREDRAM+1];
	for (ln=0; ln<30; ln++)
	{
		for (x=0; x<160; x++)
		{
			//rgb565=*DDR_regaddr++;
			v2=*test++;
			v1=*test++;
			

			
			
			
			rgb565=((unsigned short int)v1)<<8;
			rgb565=rgb565 | ((unsigned short int)v2);
			
			
			
			gram[x][(chunk*30)+ln]=rgb565;
			
			rgb24[2] = (unsigned char) ((rgb565 & RGB565_MASK_RED) >> 11);     
 			rgb24[1] = (unsigned char) ((rgb565 & RGB565_MASK_GREEN) >> 5);  
 			rgb24[0] = (unsigned char) ((rgb565 & RGB565_MASK_BLUE));
 			rgb24[2] <<= 3;  
 			rgb24[1] <<= 2;  
 			rgb24[0] <<= 3;
 			//printf("%02x,%02x:%04x.", v1, v2, rgb565 );
			
			fwrite(rgb24, 1, 3, outfile);
		}
		//printf("\n");
	}
	
}


int main (void)
{
    unsigned int ret;
    int i;
    void *DDR_paramaddr;
    void *DDR_ackaddr;
    int fin;
    char fname_new[255];
    
    tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;
    
    delay_ms(500);
    printf("Starting up the camera..\n");
    cam_init();
    delay_ms(500);
    
    printf("\nINFO: Starting %s example.\r\n", "PRU_memAccess_DDR_PRUsharedRAM");
    /* Initialize the PRU */
    prussdrv_init ();		
    

    /* Open PRU Interrupt */
    ret = prussdrv_open(PRU_EVTOUT_1);
    if (ret)
    {
        printf("prussdrv_open open failed\n");
        return (ret);
    }
    
    /* Get the interrupt initialized */
    prussdrv_pruintc_init(&pruss_intc_initdata);
    
    init_graphicslib();
    chunk=0;
    // Open file
    outfile=fopen("img.raw", "wb");


    /* Initialize example */
    printf("\tINFO: Initializing example.\r\n");
    LOCAL_exampleInit(PRU_NUM);
    
    //printf("before:\n");
    //dumpdata();
    
    	
    
    /* Execute example on PRU */
    printf("\tINFO: Executing example.\r\n");
    
    DDR_paramaddr = ddrMem + OFFSET_DDR - 8;
    DDR_ackaddr = ddrMem + OFFSET_DDR - 4;
    //*(unsigned long*) DDR_ackaddr = 0;
    //*(unsigned long*) DDR_paramaddr = 99;
    
    sharedMem_int[OFFSET_SHAREDRAM]=99; // param
    // Execute program
    prussdrv_exec_program (PRU_NUM, "./prucode_cam.bin");
    //sleep(1);
    for (i=0; i<4; i++)
		{
			printf("Executing %d. \n", i);
			sharedMem_int[OFFSET_SHAREDRAM]=(unsigned int)(i); // param
			
		
			printf("Waiting for ack (curr=%d). \n", sharedMem_int[OFFSET_SHAREDRAM]);
			fin=0;
			do
			{
				if ( sharedMem_int[OFFSET_SHAREDRAM] == 100 )
				{
					// we have received the ack!
					dumpdata(); // Store to file
					sharedMem_int[OFFSET_SHAREDRAM] = 99;
					fin=1;
					printf("Ack\n");
				}
			} while(!fin);
			chunk++;
		}
			
    	//prussdrv_pru_wait_event (PRU_EVTOUT_1);
    	printf("Done\n");
    	//prussdrv_pru_clear_event (PRU1_ARM_INTERRUPT);

 		   	

		fclose(outfile);
ram2cairo(); // insert video ram into the cairo buffer
				if (CAIRO_HAS_PNG_FUNCTIONS)
				{
					sprintf(fname_new, "img.png");
					cairo_surface_write_to_png(cs, fname_new);
				}
    
    
    //******************
    void *DDR_regaddr1;
    DDR_regaddr1 = ddrMem + OFFSET_DDR;
		printf("value is %08x\n", *((unsigned long*)DDR_regaddr1));
		printf("next value is %08x\n", *( ((unsigned long*)DDR_regaddr1+1) ));

    
    /* Disable PRU and close memory mapping*/
    prussdrv_pru_disable(PRU_NUM); 
    prussdrv_exit ();
    munmap(ddrMem, 0x0FFFFFFF);
    close(mem_fd);

    return(0);
}

static int LOCAL_exampleInit (  )
{
    void *DDR_regaddr1, *DDR_regaddr2, *DDR_regaddr3;	
    
    prussdrv_map_prumem(PRUSS1_SHARED_DATARAM, &sharedMem);
    sharedMem_int = (unsigned int*) sharedMem;

    /* open the device */
    mem_fd = open("/dev/mem", O_RDWR);
    if (mem_fd < 0) {
        printf("Failed to open /dev/mem (%s)\n", strerror(errno));
        return -1;
    }	

    /* map the DDR memory */
    ddrMem = mmap(0, 0x0FFFFFFF, PROT_WRITE | PROT_READ, MAP_SHARED, mem_fd, DDR_BASEADDR);
    if (ddrMem == NULL) {
        printf("Failed to map the device (%s)\n", strerror(errno));
        close(mem_fd);
        return -1;
    }
    
    /* Store Addends in DDR memory location */
    DDR_regaddr1 = ddrMem + OFFSET_DDR;
    DDR_regaddr2 = ddrMem + OFFSET_DDR + 0x00000004;
    DDR_regaddr3 = ddrMem + OFFSET_DDR + 0x00000008;

    *(unsigned long*) DDR_regaddr1 = ADDEND1;
    *(unsigned long*) DDR_regaddr2 = ADDEND2;
    *(unsigned long*) DDR_regaddr3 = ADDEND3;

    return(0);
}

static unsigned short LOCAL_examplePassed ( unsigned short pruNum )
{
    unsigned int result_0, result_1, result_2;

     /* Allocate Shared PRU memory. */
    prussdrv_map_prumem(PRUSS0_SHARED_DATARAM, &sharedMem);
    sharedMem_int = (unsigned int*) sharedMem;

    result_0 = sharedMem_int[OFFSET_SHAREDRAM];
    result_1 = sharedMem_int[OFFSET_SHAREDRAM + 1];
    result_2 = sharedMem_int[OFFSET_SHAREDRAM + 2];

    return ((result_0 == ADDEND1) & (result_1 ==  ADDEND2) & (result_2 ==  ADDEND3)) ;

}
