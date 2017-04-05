.origin 0
.entrypoint START

#include "PRU_stereoCapture.hp"

#define GPIO1 0x4804c000
#define GPIO_CLEARDATAOUT 0x190
#define GPIO_SETDATAOUT 0x194

#define SHARED 0x10000

#define MASK0 0x000000ff
#define PMASK 1024
// Memory location of command:
//#define CMD 0x80001000
#define CMD 0x00010000
// Memory location where to store the frame:
//#define IMGRAM 0x80001008
#define IMGRAM 0x00010004
// Number of lines per frame. Should be 120. Lets just do 30 for now:
#define LINES 30
// Define the number of bytes to read per line, divided by 4. So, for 160 pixels, this is 320 bytes,
// and dividing by 4 gives 80
#define BPLD4 80

// *** LED routines, so that LED USR0 can be used for some simple debugging
// *** Affects: r2, r3
.macro LED_OFF
		MOV r2, 1<<21
    MOV r3, GPIO1 | GPIO_CLEARDATAOUT
    SBBO r2, r3, 0, 4
.endm

.macro LED_ON
		MOV r2, 1<<21
    MOV r3, GPIO1 | GPIO_SETDATAOUT
    SBBO r2, r3, 0, 4
.endm

.macro UDEL
UDEL:
		MOV r14, 1
UDEL1:
		SUB r14, r14, 1
		QBNE UDEL1, r14, 0 // loop if we've not finished
.endm

.macro DEL
DEL:
		MOV r1, 50000
DEL1:
		SUB r1, r1, 1
		QBNE DEL1, r1, 0 // loop if we've not finished
.endm

// *** WAITVSYNC: Waits for VSYNC (r31 bit 8) transition to low
// ***            i.e. the frame is about to begin.
// ***            It will wait for any incomplete frame to
// ***            complete first.
// ***            VSYNC  ---¬_________
// ***                       ^
// ***            Affects: none
.macro WAITVSYNC
WAITVSYNC:
		WBS r31.t8	// wait until bit8 is high
		// check it is really high
		MOV r1, 100
WAITVSYNC1:
		MOV r2, 256
		MOV r3, r31
		AND r3, r3, r2
		QBEQ WAITVSYNC, r3, 0 // it went low! so go back to the start
		SUB r1, r1, 1
		QBNE WAITVSYNC1, r1, 0 // loop
		
		// ok, it has remained high for quite a while.
WAITVSYNCL:		
		WBC r31.t8	// wait until bit8 is low
		// check if it is really low
		MOV r1, 100
WAITVSYNCL1:
		MOV r2, 256
		MOV r3, r31
		AND r3, r3, r2
		QBNE WAITVSYNCL, r3, 0 // it went high! so go back to the start
		SUB r1, r1, 1
		QBNE WAITVSYNCL1, r1, 0 // loop
		// ok it has remained low for quite a while.
.endm

// *** WAITLINE: Waits for HREF (r31 bit 9) to go low.
// ***           As soon as it does, we can begin reading in
// ***           the line data on each PCLK rising edge
// ***           HREF  ---¬_________
// ***                     ^
// ***            Affects: none
.macro WAITLINE
		WBC r31.t9	// wait until bit9 is low
.endm

// *** STORELINE: Writes a line to RAM
// ***            Data is read in on rising edge of PCLK (r31 bit 10)
// ***            PCLK __,---¬___,--¬___,--¬___
// ***            Dx   ------¬______,----------
// ***                   ^       ^      ^
// ***            Input: r6 contains the start address to store the line
// ***            Output: r6 points to the location after the line
// ***            Affects: r2, r3, r5, r6, r7
.macro STORELINE
STORELINE:
		MOV r3, MASK0
		MOV r7, BPLD4 // This will be the loop counter to read the entire line of bytes
STORELINELOOP:
		// First byte:
		WBC r31.t10	// Ensure PCLK is actually low
		UDEL
WAITRISE1:
		// Now wait until PCLK rising edge
		MOV r2, r31	// Read in the data
		MOV r4, PMASK
		AND r1, r2, r4
		QBNE WAITRISE1, r1, r4
		UDEL

		MOV r2, r31	// Read in the data
		AND r2, r2, r3 // r2 contains only the single byte now
		LSL r2, r2, 16 // We need to shift this byte
		MOV r5, r2 // Store in r5
		// Second byte:
		WBC r31.t10	// Ensure PCLK is actually low
		UDEL
WAITRISE2:
		// Now wait until PCLK rising edge
		MOV r2, r31	// Read in the data
		MOV r4, PMASK
		AND r1, r2, r4
		QBNE WAITRISE2, r1, r4
		UDEL

		MOV r2, r31	// Read in the data		
		AND r2, r2, r3 // r2 contains only the single byte now
		LSL r2, r2, 24 // We need to shift this byte
		OR r5, r5, r2 // Merge with r5
		// Third byte:
		WBC r31.t10	// Ensure PCLK is actually low
		UDEL
WAITRISE3:
		// Now wait until PCLK rising edge
		MOV r2, r31	// Read in the data
		MOV r4, PMASK
		AND r1, r2, r4
		QBNE WAITRISE3, r1, r4
		UDEL

		MOV r2, r31	// Read in the data		
		AND r2, r2, r3 // r2 contains only the single byte now
		OR r5, r5, r2 // Merge with r5; no need to shift this time
		// Fourth byte:
		WBC r31.t10	// Ensure PCLK is actually low
		UDEL
WAITRISE4:
		// Now wait until PCLK rising edge
		MOV r2, r31	// Read in the data
		MOV r4, PMASK
		AND r1, r2, r4
		QBNE WAITRISE4, r1, r4
		UDEL

		MOV r2, r31	// Read in the data		
		AND r2, r2, r3 // r2 contains only the single byte now
		LSL r2, r2, 8 // We need to shift this byte
		OR r5, r5, r2 // Merge with r5
		// ok, now r5 contains two pixels i.e. 4 bytes.
		// we can now write it to RAM (address in r6)
		SBBO r5, r6, 0, 4 // Put contents of r5 into the address at r6
		ADD r6, r6, 4 // increment DDR address by 4 bytes
		// Check to see if we still need to read more pixels in this line
		SUB r7, r7, 1
		QBNE STORELINELOOP, r7, 0 // loop if we've not finished
		// we're almost done with this line. Wait until HREF (r31 bit 9) is high, then the line is done
		WBS r31.t9	// wait until bit9 is high
.endm


START:

    // Enable OCP master port
    LBCO      r0, CONST_PRUCFG, 4, 4
    CLR     r0, r0, 4         // Clear SYSCFG[STANDBY_INIT] to enable OCP master port
    SBCO      r0, CONST_PRUCFG, 4, 4

    // Configure the programmable pointer register for PRU0 by setting c28_pointer[15:0]
    // field to 0x0100.  This will make C28 point to 0x00010000 (PRU shared RAM).
    MOV     r0, 0x00000100
    MOV       r1, CTPPR_0
    ST32      r0, r1

    // Configure the programmable pointer register for PRU0 by setting c31_pointer[15:0]
    // field to 0x0010.  This will make C31 point to 0x80001000 (DDR memory).
    MOV     r0, 0x00100000
    MOV       r1, CTPPR_1
    ST32      r0, r1

    //Load values from external DDR Memory into Registers R0/R1/R2
    LBCO      r0, CONST_DDR, 0, 12

    //Store values from read from the DDR memory into PRU shared RAM
    //SBCO      r0, CONST_PRUSHAREDRAM, 0, 12


		LED_OFF
		
		// r10 counts down from 12 to zero, not used currently
		// r9 counts up from 0 to 12
		
		MOV r13, 0
		MOV r10, 12	// Total of 12 chunks
		MOV r9, 0
CAM:
		SET r30.t11	// disable the data bus
		MOV r6, IMGRAM	// address to store the image
		MOV r6, 0x00010000 // PRU shared RAM
		ADD r6, r6, 4
		MOV r11, CMD
		MOV r8, LINES	// r8 is lines counter
		// wait for command
CMDLOOP:
		DEL
		LBCO r12, CONST_PRUSHAREDRAM, 0, 4
		QBEQ CMDLOOP, r12, 99 // loop until we get an instruction
		QBEQ CMDLOOP, r12, 100 // loop until we get an instruction
		LED_ON
		QBEQ CAPTURE0, r12, 0 // 0=begin capture 0
		JMP CHUNKX
		//JMP CAPTURE0

		
		
		
CAPTURE0:
		MOV r13, 0
		MOV r9, 0
		MOV r10, 12	// Total of 12 chunks
		CLR r30.t11	// enable data bus
		WAITVSYNC
		JMP LINELOOP
CHUNKX:
		CLR r30.t11	// enable data bus
//		MOV r13, r9
		MOV r13, r12
		WAITVSYNC
CHUNK1:
		MOV r8, LINES
CHUNK2:
		WBC r31.t9	// wait for HSYNC to go low
// check if it is really low
		UDEL
		MOV r3, r31
		MOV r4, 512
		AND r3, r3, r4
		QBNE CHUNK2, r3, 0
		
		
		WBS r31.t9	// wait for end of line
		SUB r8, r8, 1
		QBNE CHUNK2, r8, 0 // loop until chunk is complete
		SUB r13, r13, 1
		QBNE CHUNK1, r13, 0 // do the next chunk
		MOV r8, LINES


LINELOOP:
		WAITLINE
		STORELINE
		SUB r8, r8, 1 // decrement lines counter
		QBNE LINELOOP, r8, 0 // loop if we've not finished
		SUB r10, r10, 1 // a chunk is complete
		ADD r9, r9, 1
		
		MOV r1, 100
		MOV r2, CMD
		SBCO r1, CONST_PRUSHAREDRAM, 0, 4 // Put contents of r1 into shared RAM
		ADD r13, r13, 1
		JMP CAM // finished, wait for next command		
		
	  //LED_ON




EXIT:
    // Send notification to Host for program completion
    MOV       r31.b0, PRU1_ARM_INTERRUPT+16

    // Halt the processor
    HALT

ERR:
	LED_ON
	JMP ERR
