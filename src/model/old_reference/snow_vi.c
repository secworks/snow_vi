// =====================================================================
//  snow_vi.h
//  ---------
//
//
// Author: Joachim Strömbergon
// Copyright 2024 Assured AB
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following
// disclaimer in the documentation and/or other materials provided
// with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
// CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
// USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// =====================================================================

#include "snow_vi.h"
#include "aes.h"
#include "debug.h"

#define INTERNAL_DEBUG 0

static LFSR A, B;
static u128 r1, r2, r3, z;
static u128 t1, t2;

static const int sigma[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};

static const int INIT_MODE = 1;
static const int WORK_MODE = 0;



static void print_state(void) {

  print_lfsr("lfsr_a",A);
  print_lfsr("lsfr_b",B);
  printf("\n");

  print_128("r1",r1);
  print_128("r2",r2);
  print_128("r3",r3);
  printf("\n");

  print_128("t1",t1);
  print_128("t2",t2);
  printf("\n");

  print_128("z ",z);
  printf("\n");
}


static void clearR123(void) {
    for(int i = 0; i < 4; i++) {
        r1.w[i] = 0;
        r2.w[i] = 0;
        r3.w[i] = 0;
    }
}

static u16 MULx(u16 V, u16 c)  //Multiply with c
{
    if (V & 0x8000)
        return(V << 1) ^ c;
    else
        return (V << 1);
}




static void CalcOutput(void) {
    for(int idx = 0; idx < 4; idx++) {
        z.w[idx] = (t1.w[idx] + r1.w[idx]) ^ r2.w[idx];
    }
}


static void CalcTaps(void) {
    //T1,T2 from LFSR1 and LFSR2
    t1.w[0] = B.w[4];
    t1.w[1] = B.w[5];
    t1.w[2] = B.w[6];
    t1.w[3] = B.w[7];

    t2.w[0] = A.w[4];
    t2.w[1] = A.w[5];
    t2.w[2] = A.w[6];
    t2.w[3] = A.w[7];
}


static void LoadLFSR(const u8 *K, const u8 *IV)
{
    for (int idx = 0; idx < 16; idx++)
    {
        A.b[idx]      = IV[idx];
        A.b[idx + 16] = K[idx];

        B.b[idx]      = 0x00;
        B.b[idx + 16] = K[idx + 16];

    }
    CalcTaps();
}


static void ClockLFSR_Step(void) //the update of the LFSR
{
    u16 u = MULx(A.s[0],0x4a6d) ^ A.s[7] ^ B.s[0];
    u16 v = MULx(B.s[0],0xcc87) ^ B.s[8] ^ A.s[0];

    for(int idx = 0; idx < 15; idx++) {
        A.s[idx] = A.s[idx + 1];
        B.s[idx] = B.s[idx + 1];
    }
    A.s[15] = u;
    B.s[15] = v;
}

static void ClockLFSRMode(int mode){

    for (int idx = 0; idx < 8; idx++) {
        ClockLFSR_Step();
    }
    if (mode == INIT_MODE) {
        for(int idx = 0; idx < 8; idx++) {
            A.s[idx+8] = A.s[idx+8] ^ z.s[idx];
        }
    }
    CalcTaps();
}


static void ClockFSM(void)
{
    u128 next_r1;
    u128 aes_out;

    for (int idx = 0; idx < 4; idx++) {
        next_r1.w[idx] = ((t2.w[idx] ^ r3.w[idx]) + r2.w[idx]);
    }

    AESRound(r2.b, aes_out.b); //The second AES round
    for (int idx = 0; idx < 16; idx++) {
        r3.b[idx] = aes_out.b[idx];
    }

    AESRound(r1.b,aes_out.b);  //The first AES round
    for (int idx = 0; idx < 16; idx++) {
        r2.b[idx] = aes_out.b[idx];
    }

    for (int idx = 0; idx < 16; idx++) {
        r1.b[idx] = next_r1.b[sigma[idx]];
    }

}


void SNOW_Vi_Init(const u8 * key, const u8 * iv) {

    clearR123();
    LoadLFSR(key, iv);
    CalcOutput();

    printf("State after loading:\n");
    print_state();

//    for (i = 0 ; i<16 ; i++)
//    {
//        CalcOutput();
//        ClockFSM();
//        ClockLFSRMode(INIT_MODE);
//        if (i>=14) {
//            for (int idx = 0; idx < 16; idx++) {
//                r1.b[idx] = r1.b[idx] ^ key[idx + ((i-14)<<4)];
//            }
//        }
//        if (INTERNAL_DEBUG){
//            printf("---- State after initialization round %d (of 16) : ----\n", i+1);
//            print_state();
//        }
//    }

}


u128 SNOW_Vi_Keystream(void) {
    CalcOutput();
    ClockFSM();
    ClockLFSRMode(WORK_MODE);

    if (INTERNAL_DEBUG) {
        print_state();
    }
    return z;
}
