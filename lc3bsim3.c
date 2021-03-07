/*
    Name 1: Your full name
    UTEID 1: Your UT EID
*/

/***************************************************************/
/*                                                             */
/*   LC-3b Simulator                                           */
/*                                                             */
/*   EE 460N                                                   */
/*   The University of Texas at Austin                         */
/*                                                             */
/***************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/***************************************************************/
/*                                                             */
/* Files:  ucode        Microprogram file                      */
/*         isaprogram   LC-3b machine language program file    */
/*                                                             */
/***************************************************************/

/***************************************************************/
/* These are the functions you'll have to write.               */
/***************************************************************/

void eval_micro_sequencer();
void cycle_memory();
void eval_bus_drivers();
void drive_bus();
void latch_datapath_values();

/***************************************************************/
/* A couple of useful definitions.                             */
/***************************************************************/
#define FALSE 0
#define TRUE 1

/***************************************************************/
/* Use this to avoid overflowing 16 bits on the bus.           */
/***************************************************************/
#define Low16bits(x) ((x)&0xFFFF)

/***************************************************************/
/* Definition of the control store layout.                     */
/***************************************************************/
#define CONTROL_STORE_ROWS 64
#define INITIAL_STATE_NUMBER 18

/***************************************************************/
/* Definition of bit order in control store word.              */
/***************************************************************/
enum CS_BITS
{
    IRD,
    COND1,
    COND0,
    J5,
    J4,
    J3,
    J2,
    J1,
    J0,
    LD_MAR,
    LD_MDR,
    LD_IR,
    LD_BEN,
    LD_REG,
    LD_CC,
    LD_PC,
    GATE_PC,
    GATE_MDR,
    GATE_ALU,
    GATE_MARMUX,
    GATE_SHF,
    PCMUX1,
    PCMUX0,
    DRMUX,
    SR1MUX,
    ADDR1MUX,
    ADDR2MUX1,
    ADDR2MUX0,
    MARMUX,
    ALUK1,
    ALUK0,
    MIO_EN,
    R_W,
    DATA_SIZE,
    LSHF1,
    CONTROL_STORE_BITS
} CS_BITS;

/***************************************************************/
/* Functions to get at the control bits.                       */
/***************************************************************/
int GetIRD(int *x) { return (x[IRD]); }
int GetCOND(int *x) { return ((x[COND1] << 1) + x[COND0]); }
int GetJ(int *x) { return ((x[J5] << 5) + (x[J4] << 4) +
                           (x[J3] << 3) + (x[J2] << 2) +
                           (x[J1] << 1) + x[J0]); }
int GetLD_MAR(int *x) { return (x[LD_MAR]); }
int GetLD_MDR(int *x) { return (x[LD_MDR]); }
int GetLD_IR(int *x) { return (x[LD_IR]); }
int GetLD_BEN(int *x) { return (x[LD_BEN]); }
int GetLD_REG(int *x) { return (x[LD_REG]); }
int GetLD_CC(int *x) { return (x[LD_CC]); }
int GetLD_PC(int *x) { return (x[LD_PC]); }
int GetGATE_PC(int *x) { return (x[GATE_PC]); }
int GetGATE_MDR(int *x) { return (x[GATE_MDR]); }
int GetGATE_ALU(int *x) { return (x[GATE_ALU]); }
int GetGATE_MARMUX(int *x) { return (x[GATE_MARMUX]); }
int GetGATE_SHF(int *x) { return (x[GATE_SHF]); }
int GetPCMUX(int *x) { return ((x[PCMUX1] << 1) + x[PCMUX0]); }
int GetDRMUX(int *x) { return (x[DRMUX]); }
int GetSR1MUX(int *x) { return (x[SR1MUX]); }
int GetADDR1MUX(int *x) { return (x[ADDR1MUX]); }
int GetADDR2MUX(int *x) { return ((x[ADDR2MUX1] << 1) + x[ADDR2MUX0]); }
int GetMARMUX(int *x) { return (x[MARMUX]); }
int GetALUK(int *x) { return ((x[ALUK1] << 1) + x[ALUK0]); }
int GetMIO_EN(int *x) { return (x[MIO_EN]); }
int GetR_W(int *x) { return (x[R_W]); }
int GetDATA_SIZE(int *x) { return (x[DATA_SIZE]); }
int GetLSHF1(int *x) { return (x[LSHF1]); }

/***************************************************************/
/* The control store rom.                                      */
/***************************************************************/
int CONTROL_STORE[CONTROL_STORE_ROWS][CONTROL_STORE_BITS];

/***************************************************************/
/* Main memory.                                                */
/***************************************************************/
/* MEMORY[A][0] stores the least significant byte of word at word address A
   MEMORY[A][1] stores the most significant byte of word at word address A 
   There are two write enable signals, one for each byte. WE0 is used for 
   the least significant byte of a word. WE1 is used for the most significant 
   byte of a word. */

#define WORDS_IN_MEM 0x08000
#define MEM_CYCLES 5
int MEMORY[WORDS_IN_MEM][2];

/***************************************************************/

/***************************************************************/

/***************************************************************/
/* LC-3b State info.                                           */
/***************************************************************/
#define LC_3b_REGS 8

int RUN_BIT; /* run bit */
int BUS;     /* value of the bus */

typedef struct System_Latches_Struct
{

    int PC,  /* program counter */
        MDR, /* memory data register */
        MAR, /* memory address register */
        IR,  /* instruction register */
        N,   /* n condition bit */
        Z,   /* z condition bit */
        P,   /* p condition bit */
        BEN; /* ben register */

    int READY; /* ready bit */
               /* The ready bit is also latched as you dont want the memory system to assert it 
     at a bad point in the cycle*/

    int REGS[LC_3b_REGS]; /* register file. */

    int MICROINSTRUCTION[CONTROL_STORE_BITS]; /* The microintruction */

    int STATE_NUMBER; /* Current State Number - Provided for debugging */
} System_Latches;

/* Data Structure for Latch */

System_Latches CURRENT_LATCHES, NEXT_LATCHES;

/***************************************************************/
/* A cycle counter.                                            */
/***************************************************************/
int CYCLE_COUNT;

/***************************************************************/
/*                                                             */
/* Procedure : help                                            */
/*                                                             */
/* Purpose   : Print out a list of commands.                   */
/*                                                             */
/***************************************************************/
void help()
{
    printf("----------------LC-3bSIM Help-------------------------\n");
    printf("go               -  run program to completion       \n");
    printf("run n            -  execute program for n cycles    \n");
    printf("mdump low high   -  dump memory from low to high    \n");
    printf("rdump            -  dump the register & bus values  \n");
    printf("?                -  display this help menu          \n");
    printf("quit             -  exit the program                \n\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : cycle                                           */
/*                                                             */
/* Purpose   : Execute a cycle                                 */
/*                                                             */
/***************************************************************/
void cycle()
{

    eval_micro_sequencer();
    cycle_memory();
    eval_bus_drivers();
    drive_bus();
    latch_datapath_values();

    CURRENT_LATCHES = NEXT_LATCHES;

    CYCLE_COUNT++;
}

/***************************************************************/
/*                                                             */
/* Procedure : run n                                           */
/*                                                             */
/* Purpose   : Simulate the LC-3b for n cycles.                 */
/*                                                             */
/***************************************************************/
void run(int num_cycles)
{
    int i;

    if (RUN_BIT == FALSE)
    {
        printf("Can't simulate, Simulator is halted\n\n");
        return;
    }

    printf("Simulating for %d cycles...\n\n", num_cycles);
    for (i = 0; i < num_cycles; i++)
    {
        if (CURRENT_LATCHES.PC == 0x0000)
        {
            RUN_BIT = FALSE;
            printf("Simulator halted\n\n");
            break;
        }
        cycle();
    }
}

/***************************************************************/
/*                                                             */
/* Procedure : go                                              */
/*                                                             */
/* Purpose   : Simulate the LC-3b until HALTed.                 */
/*                                                             */
/***************************************************************/
void go()
{
    if (RUN_BIT == FALSE)
    {
        printf("Can't simulate, Simulator is halted\n\n");
        return;
    }

    printf("Simulating...\n\n");
    while (CURRENT_LATCHES.PC != 0x0000)
        cycle();
    RUN_BIT = FALSE;
    printf("Simulator halted\n\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : mdump                                           */
/*                                                             */
/* Purpose   : Dump a word-aligned region of memory to the     */
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void mdump(FILE *dumpsim_file, int start, int stop)
{
    int address; /* this is a byte address */

    printf("\nMemory content [0x%.4x..0x%.4x] :\n", start, stop);
    printf("-------------------------------------\n");
    for (address = (start >> 1); address <= (stop >> 1); address++)
        printf("  0x%.4x (%d) : 0x%.2x%.2x\n", address << 1, address << 1, MEMORY[address][1], MEMORY[address][0]);
    printf("\n");

    /* dump the memory contents into the dumpsim file */
    fprintf(dumpsim_file, "\nMemory content [0x%.4x..0x%.4x] :\n", start, stop);
    fprintf(dumpsim_file, "-------------------------------------\n");
    for (address = (start >> 1); address <= (stop >> 1); address++)
        fprintf(dumpsim_file, " 0x%.4x (%d) : 0x%.2x%.2x\n", address << 1, address << 1, MEMORY[address][1], MEMORY[address][0]);
    fprintf(dumpsim_file, "\n");
    fflush(dumpsim_file);
}

/***************************************************************/
/*                                                             */
/* Procedure : rdump                                           */
/*                                                             */
/* Purpose   : Dump current register and bus values to the     */
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void rdump(FILE *dumpsim_file)
{
    int k;

    printf("\nCurrent register/bus values :\n");
    printf("-------------------------------------\n");
    printf("Cycle Count  : %d\n", CYCLE_COUNT);
    printf("PC           : 0x%.4x\n", CURRENT_LATCHES.PC);
    printf("IR           : 0x%.4x\n", CURRENT_LATCHES.IR);
    printf("STATE_NUMBER : 0x%.4x\n\n", CURRENT_LATCHES.STATE_NUMBER);
    printf("BUS          : 0x%.4x\n", BUS);
    printf("MDR          : 0x%.4x\n", CURRENT_LATCHES.MDR);
    printf("MAR          : 0x%.4x\n", CURRENT_LATCHES.MAR);
    printf("CCs: N = %d  Z = %d  P = %d\n", CURRENT_LATCHES.N, CURRENT_LATCHES.Z, CURRENT_LATCHES.P);
    printf("Registers:\n");
    for (k = 0; k < LC_3b_REGS; k++)
        printf("%d: 0x%.4x\n", k, CURRENT_LATCHES.REGS[k]);
    printf("\n");

    /* dump the state information into the dumpsim file */
    fprintf(dumpsim_file, "\nCurrent register/bus values :\n");
    fprintf(dumpsim_file, "-------------------------------------\n");
    fprintf(dumpsim_file, "Cycle Count  : %d\n", CYCLE_COUNT);
    fprintf(dumpsim_file, "PC           : 0x%.4x\n", CURRENT_LATCHES.PC);
    fprintf(dumpsim_file, "IR           : 0x%.4x\n", CURRENT_LATCHES.IR);
    fprintf(dumpsim_file, "STATE_NUMBER : 0x%.4x\n\n", CURRENT_LATCHES.STATE_NUMBER);
    fprintf(dumpsim_file, "BUS          : 0x%.4x\n", BUS);
    fprintf(dumpsim_file, "MDR          : 0x%.4x\n", CURRENT_LATCHES.MDR);
    fprintf(dumpsim_file, "MAR          : 0x%.4x\n", CURRENT_LATCHES.MAR);
    fprintf(dumpsim_file, "CCs: N = %d  Z = %d  P = %d\n", CURRENT_LATCHES.N, CURRENT_LATCHES.Z, CURRENT_LATCHES.P);
    fprintf(dumpsim_file, "Registers:\n");
    for (k = 0; k < LC_3b_REGS; k++)
        fprintf(dumpsim_file, "%d: 0x%.4x\n", k, CURRENT_LATCHES.REGS[k]);
    fprintf(dumpsim_file, "\n");
    fflush(dumpsim_file);
}

/***************************************************************/
/*                                                             */
/* Procedure : get_command                                     */
/*                                                             */
/* Purpose   : Read a command from standard input.             */
/*                                                             */
/***************************************************************/
void get_command(FILE *dumpsim_file)
{
    char buffer[20];
    int start, stop, cycles;

    printf("LC-3b-SIM> ");

    scanf("%s", buffer);
    printf("\n");

    switch (buffer[0])
    {
    case 'G':
    case 'g':
        go();
        break;

    case 'M':
    case 'm':
        scanf("%i %i", &start, &stop);
        mdump(dumpsim_file, start, stop);
        break;

    case '?':
        help();
        break;
    case 'Q':
    case 'q':
        printf("Bye.\n");
        exit(0);

    case 'R':
    case 'r':
        if (buffer[1] == 'd' || buffer[1] == 'D')
            rdump(dumpsim_file);
        else
        {
            scanf("%d", &cycles);
            run(cycles);
        }
        break;

    default:
        printf("Invalid Command\n");
        break;
    }
}

/***************************************************************/
/*                                                             */
/* Procedure : init_control_store                              */
/*                                                             */
/* Purpose   : Load microprogram into control store ROM        */
/*                                                             */
/***************************************************************/
void init_control_store(char *ucode_filename)
{
    FILE *ucode;
    int i, j, index;
    char line[200];

    printf("Loading Control Store from file: %s\n", ucode_filename);

    /* Open the micro-code file. */
    if ((ucode = fopen(ucode_filename, "r")) == NULL)
    {
        printf("Error: Can't open micro-code file %s\n", ucode_filename);
        exit(-1);
    }

    /* Read a line for each row in the control store. */
    for (i = 0; i < CONTROL_STORE_ROWS; i++)
    {
        if (fscanf(ucode, "%[^\n]\n", line) == EOF)
        {
            printf("Error: Too few lines (%d) in micro-code file: %s\n",
                   i, ucode_filename);
            exit(-1);
        }

        /* Put in bits one at a time. */
        index = 0;

        for (j = 0; j < CONTROL_STORE_BITS; j++)
        {
            /* Needs to find enough bits in line. */
            if (line[index] == '\0')
            {
                printf("Error: Too few control bits in micro-code file: %s\nLine: %d\n",
                       ucode_filename, i);
                exit(-1);
            }
            if (line[index] != '0' && line[index] != '1')
            {
                printf("Error: Unknown value in micro-code file: %s\nLine: %d, Bit: %d\n",
                       ucode_filename, i, j);
                exit(-1);
            }

            /* Set the bit in the Control Store. */
            CONTROL_STORE[i][j] = (line[index] == '0') ? 0 : 1;
            index++;
        }

        /* Warn about extra bits in line. */
        if (line[index] != '\0')
            printf("Warning: Extra bit(s) in control store file %s. Line: %d\n",
                   ucode_filename, i);
    }
    printf("\n");
}

/************************************************************/
/*                                                          */
/* Procedure : init_memory                                  */
/*                                                          */
/* Purpose   : Zero out the memory array                    */
/*                                                          */
/************************************************************/
void init_memory()
{
    int i;

    for (i = 0; i < WORDS_IN_MEM; i++)
    {
        MEMORY[i][0] = 0;
        MEMORY[i][1] = 0;
    }
}

/**************************************************************/
/*                                                            */
/* Procedure : load_program                                   */
/*                                                            */
/* Purpose   : Load program and service routines into mem.    */
/*                                                            */
/**************************************************************/
void load_program(char *program_filename)
{
    FILE *prog;
    int ii, word, program_base;

    /* Open program file. */
    prog = fopen(program_filename, "r");
    if (prog == NULL)
    {
        printf("Error: Can't open program file %s\n", program_filename);
        exit(-1);
    }

    /* Read in the program. */
    if (fscanf(prog, "%x\n", &word) != EOF)
        program_base = word >> 1;
    else
    {
        printf("Error: Program file is empty\n");
        exit(-1);
    }

    ii = 0;
    while (fscanf(prog, "%x\n", &word) != EOF)
    {
        /* Make sure it fits. */
        if (program_base + ii >= WORDS_IN_MEM)
        {
            printf("Error: Program file %s is too long to fit in memory. %x\n",
                   program_filename, ii);
            exit(-1);
        }

        /* Write the word to memory array. */
        MEMORY[program_base + ii][0] = word & 0x00FF;
        MEMORY[program_base + ii][1] = (word >> 8) & 0x00FF;
        ii++;
    }

    if (CURRENT_LATCHES.PC == 0)
        CURRENT_LATCHES.PC = (program_base << 1);

    printf("Read %d words from program into memory.\n\n", ii);
}

/***************************************************************/
/*                                                             */
/* Procedure : initialize                                      */
/*                                                             */
/* Purpose   : Load microprogram and machine language program  */
/*             and set up initial state of the machine.        */
/*                                                             */
/***************************************************************/
void initialize(char *ucode_filename, char *program_filename, int num_prog_files)
{
    int i;
    init_control_store(ucode_filename);

    init_memory();
    for (i = 0; i < num_prog_files; i++)
    {
        load_program(program_filename);
        while (*program_filename++ != '\0')
            ;
    }
    CURRENT_LATCHES.Z = 1;
    CURRENT_LATCHES.STATE_NUMBER = INITIAL_STATE_NUMBER;
    memcpy(CURRENT_LATCHES.MICROINSTRUCTION, CONTROL_STORE[INITIAL_STATE_NUMBER], sizeof(int) * CONTROL_STORE_BITS);

    NEXT_LATCHES = CURRENT_LATCHES;

    RUN_BIT = TRUE;
}

/***************************************************************/
/*                                                             */
/* Procedure : main                                            */
/*                                                             */
/***************************************************************/
int main(int argc, char *argv[])
{
    FILE *dumpsim_file;

    /* Error Checking */
    if (argc < 3)
    {
        printf("Error: usage: %s <micro_code_file> <program_file_1> <program_file_2> ...\n",
               argv[0]);
        exit(1);
    }

    printf("LC-3b Simulator\n\n");

    initialize(argv[1], argv[2], argc - 2);

    if ((dumpsim_file = fopen("dumpsim", "w")) == NULL)
    {
        printf("Error: Can't open dumpsim file\n");
        exit(-1);
    }

    while (1)
        get_command(dumpsim_file);
}

/***************************************************************/
/* Do not modify the above code.
   You are allowed to use the following global variables in your
   code. These are defined above.

   CONTROL_STORE
   MEMORY
   BUS

   CURRENT_LATCHES
   NEXT_LATCHES

   You may define your own local/global variables and functions.
   You may use the functions to get at the control bits defined
   above.

   Begin your code here 	  			       */
/***************************************************************/
uint16_t memory_data = 0;
uint16_t memory_access_cycle_counter = 0;
uint16_t current_bus_value = 0;

uint8_t get_opcode()
{
    return (((CURRENT_LATCHES.IR & 0xF000) >> 12) & (0x000F));
}

uint16_t adder_muxes()
{
    uint16_t result = 0;
    switch (GetADDR1MUX(CURRENT_LATCHES.MICROINSTRUCTION))
    {
    case 1:
        if (GetSR1MUX(CURRENT_LATCHES.MICROINSTRUCTION))
        {
            result += CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6];
        }
        break;
    case 0:
        result += CURRENT_LATCHES.PC;
        break;
    }

    switch (GetADDR2MUX(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        int shift = GetLSHF1(CURRENT_LATCHES.MICROINSTRUCTION);
    case 1:
        if (((0x0020 & CURRENT_LATCHES.IR) >> 5))
        {
            result += (((-1) * (CURRENT_LATCHES.IR & 0x003F)) << shift);
        }
        else
        {
            result += ((CURRENT_LATCHES.IR & 0x003F) << shift);
        }
        break;
    case 2:
        if (((0x0100 & CURRENT_LATCHES.IR) >> 8))
        {
            result += (((-1) * (CURRENT_LATCHES.IR & 0x01FF)) << shift);
        }
        else
        {
            result += ((CURRENT_LATCHES.IR & 0x01FF) << shift);
        }
        break;
    case 3:
        if (((0x0400 & CURRENT_LATCHES.IR) >> 10))
        {
            result += (((-1) * (CURRENT_LATCHES.IR & 0x07FF)) << shift);
        }
        else
        {
            result += ((CURRENT_LATCHES.IR & 0x07FF) << shift);
        }
        break;
    }
}
void eval_micro_sequencer()
{
    /* 
   * Evaluate the address of the next state according to the 
   * micro sequencer logic. Latch the next microinstruction.
   */
    //If the IRD bit is high, we know to load a new opcode!
    if (GetIRD(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        NEXT_LATCHES.STATE_NUMBER = 0x0000 + get_opcode();
    }
    else
    {
        //Tells us whether we go to the next state or stay in this state until a condition is met
        switch (GetCOND(CURRENT_LATCHES.MICROINSTRUCTION))
        {
        case 0:
            //unconditional
            NEXT_LATCHES.STATE_NUMBER = GetJ(CURRENT_LATCHES.MICROINSTRUCTION);
            break;
        case 1:
            //compare the ready bit
            NEXT_LATCHES.STATE_NUMBER = GetJ(CURRENT_LATCHES.MICROINSTRUCTION) ^ (CURRENT_LATCHES.READY << 1);
            break;
        case 2:
            // compare the branch enable bit
            NEXT_LATCHES.STATE_NUMBER = GetJ(CURRENT_LATCHES.MICROINSTRUCTION) ^ (CURRENT_LATCHES.BEN << 2);
            break;
        case 3:
            //compare the priority bit
            NEXT_LATCHES.STATE_NUMBER = GetJ(CURRENT_LATCHES.MICROINSTRUCTION) ^ ((CURRENT_LATCHES.IR & 0x0800) >> 11);
            break;
        }
    }
    //Get the microinstruction from the control store of the state number
    *NEXT_LATCHES.MICROINSTRUCTION = CONTROL_STORE[NEXT_LATCHES.STATE_NUMBER];
}

void cycle_memory()
{
    /* 
   * This function emulates memory and the WE logic. 
   * Keep track of which cycle of MEMEN we are dealing with.  
   * If fourth, we need to latch Ready bit at the end of 
   * cycle to prepare microsequencer for the fifth cycle.  
   */
    //Reset the Ready Bit
    NEXT_LATCHES.READY = 0;
    //If we are suppose to be accessing memory
    if (GetMIO_EN(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        //Increment the memory access cycle counter
        memory_access_cycle_counter++;
        //If we have reached cycle 4
        if (memory_access_cycle_counter == 4)
        {
            switch (GetR_W(CURRENT_LATCHES.MICROINSTRUCTION))
            {
            case 0: //Reading From Memory
                memory_data = (MEMORY[CURRENT_LATCHES.MAR >> 1][1] << 8) + (MEMORY[CURRENT_LATCHES.MAR >> 1][0]);
                break;
            case 1: // Writing To Memory
                //If we are suppose to write a full word
                if (GetDATA_SIZE(CURRENT_LATCHES.MICROINSTRUCTION))
                {
                    MEMORY[CURRENT_LATCHES.MAR >> 1][0] = (CURRENT_LATCHES.MDR & 0x00FF);
                    MEMORY[CURRENT_LATCHES.MAR >> 1][1] = (CURRENT_LATCHES.MDR & 0xFF00);
                }
                //Else, we are suppose to write a byte
                else
                {
                    //Which memory location to access
                    if (CURRENT_LATCHES.MAR % 2)
                    {
                        MEMORY[CURRENT_LATCHES.MAR >> 1][0] = ((CURRENT_LATCHES.MDR >> ((CURRENT_LATCHES.MAR & 0x0001) << 3)) & 0x00FF);
                    }
                    else
                    {
                        MEMORY[CURRENT_LATCHES.MAR >> 1][1] = ((CURRENT_LATCHES.MDR >> ((CURRENT_LATCHES.MAR & 0x0001) << 3)) & 0x00FF);
                    }
                }
                break;
            }
            //Set the Memory Ready Bit
            NEXT_LATCHES.READY = 1;
            memory_access_cycle_counter = 0;
        }
    }
}
uint16_t mar_mux_bus_driver = 0;
uint16_t pc_bus_driver = 0;
uint16_t alu_bus_driver = 0;
uint16_t shift_bus_driver = 0;
uint16_t mdr_bus_driver = 0;

uint16_t mar_gate_value()
{
    if (GetMARMUX(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        return adders_result();
    }
    return current_bus_value = ((CURRENT_LATCHES.IR & 0x007F) << 1);
}

uint16_t pc_gate_value()
{
    return CURRENT_LATCHES.PC;
}

uint16_t alu_gate_value()
{
    switch (GetALUK(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        int steering_bit = ((CURRENT_LATCHES.IR & 0x0020) >> 5);
        int sign = ((0x0010 & CURRENT_LATCHES.IR) >> 4);
    case 0: // ADD
        if (steering_bit)
        {
            if (sign)
            {
                return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6] + (((-1) * (CURRENT_LATCHES.IR & 0x003F)));
            }
            else
            {
                return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6] + (CURRENT_LATCHES.IR & 0x003F);
            }
        }
        else
        {
            return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6] + CURRENT_LATCHES.REGS[CURRENT_LATCHES.IR & 0x0007];
        }
        break;
    case 1: // AND
        if (steering_bit)
        {
            if (sign)
            {
                return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6] & (((-1) * (CURRENT_LATCHES.IR & 0x003F)));
            }
            else
            {
                return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6] & (CURRENT_LATCHES.IR & 0x003F);
            }
        }
        else
        {
            return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6] & CURRENT_LATCHES.REGS[CURRENT_LATCHES.IR & 0x0007];
        }
        break;
    case 2: // XOR
        if (steering_bit)
        {
            if (sign)
            {
                return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6] ^ (((-1) * (CURRENT_LATCHES.IR & 0x003F)));
            }
            else
            {
                return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6] ^ (CURRENT_LATCHES.IR & 0x003F);
            }
        }
        else
        {
            return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6] ^ CURRENT_LATCHES.REGS[CURRENT_LATCHES.IR & 0x0007];
        }
        break;
    case 3:
        if (GetSR1MUX(CURRENT_LATCHES.MICROINSTRUCTION))
        {
            return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & (0x01C0)) >> 6];
        }
        else
        {
            return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & (0x0E00)) >> 9];
        }
        break;
    }
}

uint16_t shift_gate_value()
{
    int right_shift = ((CURRENT_LATCHES.IR & 0x0010) >> 4);
    int arithmetical = ((CURRENT_LATCHES.IR & 0x0020) >> 5);
    int shift = (CURRENT_LATCHES.IR & 0x000F);
    if (right_shift)
    {
        if (arithmetical)
        {
            int sign = ((0x8000 & CURRENT_LATCHES.IR) >> 15);
            if (sign)
            {
                    return (((-1) * CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6]) >> shift;
            }
            else
            {
                    return ((CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6]) >> shift;
            }
        }
        else
        {
            return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6] >> shift;
        }
    }
    else
    {
        return CURRENT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x01C0) >> 6] << shift;
    }
}
uint16_t mdr_gate_value()
{
    if (GetDATA_SIZE(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        return CURRENT_LATCHES.MDR;
    }
    else
    {
        int sign = ((0x0080 & CURRENT_LATCHES.IR) >> 7);
        if (sign)
        {
                return (-1)*(CURRENT_LATCHES.MDR >> 8*(CURRENT_LATCHES.MAR % 2))&0x00FF);
        }
        else
        {
                 return (CURRENT_LATCHES.MDR >> 8*(CURRENT_LATCHES.MAR % 2))&0x00FF);
        }
    }
}

void eval_bus_drivers()
{

    /* 
   * Datapath routine emulating operations before driving the bus.
   * Evaluate the input of tristate drivers 
   *             Gate_MARMUX,
   *		 Gate_PC,
   *		 Gate_ALU,
   *		 Gate_SHF,
   *		 Gate_MDR.
   */

    mar_mux_bus_driver = mar_gate_value();
    pc_bus_driver = pc_gate_value();
    alu_bus_driver = alu_gate_value();
    shift_bus_driver = shift_gate_value();
    mdr_bus_driver = mdr_gate_value();
}

void drive_bus()
{

    /* 
   * Datapath routine for driving the bus from one of the 5 possible 
   * tristate drivers. 
   */
    if (GetGATE_MARMUX(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        BUS = mar_mux_bus_driver;
    }
    if (GetGATE_PC(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        BUS = pc_bus_driver;
    }
    if (GetGATE_ALU(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        BUS = alu_bus_driver;
    }
    if (GetGATE_SHF(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        BUS = shift_bus_driver;
    }
    if (GetGATE_MDR(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        BUS = mdr_bus_driver;
    }
}

void latch_datapath_values()
{

    /* 
   * Datapath routine for computing all functions that need to latch
   * values in the data path at the end of this cycle.  Some values
   * require sourcing the bus; therefore, this routine has to come 
   * after drive_bus.
   */
    if (GetLD_MAR(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        NEXT_LATCHES.MAR = BUS;
    }
    if (GetLD_MDR(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        if (GetMIO_EN(CURRENT_LATCHES.MICROINSTRUCTION))
        {
            NEXT_LATCHES.MDR = memory_data;
        }
        else
        {
            if (GetDATA_SIZE(CURRENT_LATCHES.MICROINSTRUCTION))
            {
                NEXT_LATCHES.MDR = BUS;
            }
            else
            {
                NEXT_LATCHES.MDR = BUS << (8 * (CURRENT_LATCHES.MAR & 0x0001));
            }
        }
    }
    if (GetLD_IR(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        NEXT_LATCHES.IR = BUS;
    }
    if (GetLD_BEN(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        NEXT_LATCHES.BEN = (CURRENT_LATCHES.N && ((CURRENT_LATCHES.IR & 0x0800) >> 10)) || (CURRENT_LATCHES.Z && ((CURRENT_LATCHES.IR & 0x0400) >> 9)) || (CURRENT_LATCHES.P && ((CURRENT_LATCHES.IR & 0x0200) >> 8));
    }
    if (GetLD_REG(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        if (GetDRMUX(CURRENT_LATCHES.MICROINSTRUCTION))
        {
            NEXT_LATCHES.REGS[7] = BUS;
        }
        else
        {
            NEXT_LATCHES.REGS[(CURRENT_LATCHES.IR & 0x0E00) >> 9] = BUS;
        }
    }
    if (GetLD_CC(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        if (BUS < 0)
        {
            NEXT_LATCHES.N = 1;
            NEXT_LATCHES.Z = 0;
            NEXT_LATCHES.P = 0;
        }
        if (BUS == 0)
        {
            NEXT_LATCHES.N = 0;
            NEXT_LATCHES.Z = 1;
            NEXT_LATCHES.P = 0;
        }
        if (BUS > 0)
        {
            NEXT_LATCHES.N = 0;
            NEXT_LATCHES.Z = 0;
            NEXT_LATCHES.P = 1;
        }
    }
    if (GetLD_PC(CURRENT_LATCHES.MICROINSTRUCTION))
    {
        switch (GetPCMUX(CURRENT_LATCHES.MICROINSTRUCTION))
        {
        case 0:
            NEXT_LATCHES.PC = CURRENT_LATCHES.PC;
            break;
        case 1:
            NEXT_LATCHES.PC = BUS;
            break;
        case 2:
            NEXT_LATCHES.PC = adders_result();
            break;
        }
    }
}
