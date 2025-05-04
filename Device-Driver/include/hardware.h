/*
 * hardware.h
 *
 * Defines the interface to the hardware.  You must write routines
 * to handle TransmitInterrupt() and ReceiveInterrupt() 
 */

#ifndef _hardware_h
#define _hardware_h

/*
 * The terminal numbers in the system range from 0 to (NUM_TERMINALS-1).
 */
#define NUM_TERMINALS 4   /* Number of hardware ttys in the system */


/*
 * ----------------------------------------------------------------------
 * IMPORTANT:
 * You *must* write routines as part of your terminal driver to handle
 * these two interrupts.  The procedure names must be "TransmitInterrupt"
 * and "ReceiveInterrupt" as shown below:
 * ----------------------------------------------------------------------
 */

/*
 * TransmitInterrupt is called once for each character written to the
 * data register after the character has been written to the screen
 * of the terminal.
 */
void TransmitInterrupt(int term);

/*
 * ReceiveInterrupt is called once for every character typed on the
 * keyboard of the terminal after the character has been placed in the
 * data register 
 */
void ReceiveInterrupt(int term);


/*
 * ----------------------------------------------------------------------
 * Routines provided by the hardware, for you to use:
 * ----------------------------------------------------------------------
 */

/*
 * Place character c in the device output data register for the
 * terminal and start the hardware going to place it on the screen 
 */
void WriteDataRegister(int term, char c);

/* 
 * Read the current contents of the device input data register for
 * terminal term
 */
char ReadDataRegister(int term);

/*
 * This routine must be called once before calling any of the other
 * hardware routines on terminal term. Returns 0 if everything 
 * went OK, or -1 otherwise. 
 */
int InitHardware (int term);

#endif /* _hardware_h */
