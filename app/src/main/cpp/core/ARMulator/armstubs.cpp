/* armstubs.cpp — stub implementations of ARMulator functions not needed
   for the BeebEm embedded use case (no OS environment, no XScale, no iWMMXt,
   ARM32 mode only so Emulate26 is never called). */

#include "armdefs.h"

/* Forward declarations matching C++ linkage used by armemu.cpp/armsupp.cpp */
extern ARMword ARMul_Emulate26(ARMul_State *);
extern ARMword read_cp15_reg(unsigned, unsigned, unsigned);

/* iwmmxt.h declares ARMul_HandleIwmmxt */
extern int ARMul_HandleIwmmxt(ARMul_State *, ARMword);

/* ------------------------------------------------------------------ */
/* ARM32 mode: Emulate26 is never reached via arminit.cpp DoProg path */
ARMword ARMul_Emulate26(ARMul_State *)
{
    return 0;
}

/* CP15 register read — no MMU/cache emulation needed */
ARMword read_cp15_reg(unsigned /*crn*/, unsigned /*opcode_2*/, unsigned /*crm*/)
{
    return 0;
}

/* Debugger callback — return instruction unchanged */
ARMword ARMul_Debug(ARMul_State *, ARMword /*pc*/, ARMword instr)
{
    return instr;
}

/* Co-processor initialisation */
unsigned ARMul_CoProInit(ARMul_State *state)
{
    (void)state;
    return TRUE;
}

void ARMul_CoProExit(ARMul_State *state)
{
    (void)state;
}

/* XScale-specific stubs */
void XScale_check_memacc(ARMul_State *state, ARMword *address, int store)
{
    (void)state; (void)address; (void)store;
}

void XScale_set_fsr_far(ARMul_State *state, ARMword fsr, ARMword far_addr)
{
    (void)state; (void)fsr; (void)far_addr;
}

int XScale_debug_moe(ARMul_State *state, int moe)
{
    (void)state; (void)moe;
    return 0;
}

/* iWMMXt stub */
int ARMul_HandleIwmmxt(ARMul_State *state, ARMword instr)
{
    (void)state; (void)instr;
    return 0;
}

/* OS environment stubs */
unsigned ARMul_OSInit(ARMul_State *state)
{
    (void)state;
    return TRUE;
}

void ARMul_OSExit(ARMul_State *state)
{
    (void)state;
}

ARMword ARMul_OSLastErrorP(ARMul_State *state)
{
    (void)state;
    return 0;
}

unsigned ARMul_OSException(ARMul_State *state, ARMword vector, ARMword pc)
{
    (void)state; (void)vector; (void)pc;
    return 0;
}
