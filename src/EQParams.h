// =============================================================================
// EQParams.h
// -----------------------------------------------------------------------------
// Shared parameter struct for MCColorEqualizer.
// Used by MCColorEqualizer.cpp, CudaKernel.cu, and MetalKernel.mm.
//
// Layout: only int and float (4-byte aligned), no padding issues across
// CUDA, Metal, and host compilers.
// =============================================================================

#ifndef EQ_PARAMS_H
#define EQ_PARAMS_H

typedef struct {
    int   inputCS;       // 0=ACES AP1, 1=DWG, 2=AWG3, 3=AWG4
    int   spaceType;     // 8=RGB Spherical, 11=OKLCH

    float hueMaster;     // 0.0–2.0, default 1.0
    float hueVals[10];   // -1.0–1.0, default 0.0

    float satMaster;     // 0.0–2.0, default 1.0
    float satVals[10];   // 0.0–2.0, default 1.0

    float lumMaster;     // 0.0–2.0, default 1.0
    float lumVals[10];   // 0.0–2.0, default 1.0
} EQParams;

#endif // EQ_PARAMS_H
