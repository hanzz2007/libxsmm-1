/******************************************************************************
** Copyright (c) 2015-2019, Intel Corporation                                **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Alexander Heinecke (Intel Corp.)
******************************************************************************/
#include "generator_gemm_common.h"
#include "generator_common.h"
#include "generator_x86_instructions.h"
#include "libxsmm_main.h"

#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(push,target(LIBXSMM_OFFLOAD_TARGET))
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(pop)
#endif

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_init_micro_kernel_config_fullvector( libxsmm_micro_kernel_config*    io_micro_kernel_config,
    const libxsmm_gemm_descriptor* i_xgemm_desc,
    const char*                    i_arch,
    const unsigned int             i_use_masking_a_c ) {
  memset(io_micro_kernel_config, 0, sizeof(*io_micro_kernel_config)); /* avoid warning "maybe used uninitialized" */
  if ( strcmp( i_arch, "wsm" ) == 0 ) {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_SSE3;
    io_micro_kernel_config->vector_reg_count = 16;
    io_micro_kernel_config->use_masking_a_c = i_use_masking_a_c;
    io_micro_kernel_config->vector_name = 'x';
    if ( LIBXSMM_GEMM_PRECISION_F64 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      io_micro_kernel_config->vector_length = 2;
      io_micro_kernel_config->datatype_size = 8;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_MOVAPD;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_MOVUPD;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_MOVDDUP;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_MOVAPD;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_MOVAPD;
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_MOVUPD;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_MOVUPD;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_XORPD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_MULPD;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_ADDPD;
    } else {
      io_micro_kernel_config->vector_length = 4;
      io_micro_kernel_config->datatype_size = 4;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_MOVAPS;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_MOVUPS;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_MOVSS;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_SHUFPS;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_MOVAPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_MOVAPS;
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_MOVUPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_MOVUPS;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_XORPS;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_MULPS;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_ADDPS;
    }
  } else if ( strcmp( i_arch, "snb" ) == 0 ) {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX;
    io_micro_kernel_config->vector_reg_count = 16;
    io_micro_kernel_config->use_masking_a_c = i_use_masking_a_c;
    io_micro_kernel_config->vector_name = 'y';
    if ( LIBXSMM_GEMM_PRECISION_F64 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      io_micro_kernel_config->vector_length = 4;
      io_micro_kernel_config->datatype_size = 8;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPD;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VBROADCASTSD;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPD;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPD;
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VMULPD;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VADDPD;
    } else {
      io_micro_kernel_config->vector_length = 8;
      io_micro_kernel_config->datatype_size = 4;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VBROADCASTSS;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPS;
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPS;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VMULPS;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VADDPS;
    }
  } else if ( strcmp( i_arch, "hsw" ) == 0 ) {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX2;
    io_micro_kernel_config->vector_reg_count = 16;
    io_micro_kernel_config->use_masking_a_c = i_use_masking_a_c;
    io_micro_kernel_config->vector_name = 'y';
    if ( LIBXSMM_GEMM_PRECISION_F64 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      io_micro_kernel_config->vector_length = 4;
      io_micro_kernel_config->datatype_size = 8;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPD;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VBROADCASTSD;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPD;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPD;
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VFMADD231PD;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_UNDEF;
    } else {
      io_micro_kernel_config->vector_length = 8;
      io_micro_kernel_config->datatype_size = 4;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VBROADCASTSS;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPS;
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPS;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VFMADD231PS;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_UNDEF;
    }
  } else if ( (strcmp( i_arch, "knl" ) == 0) ||
      (strcmp( i_arch, "knm" ) == 0) ||
      (strcmp( i_arch, "skx" ) == 0) ||
      (strcmp( i_arch, "clx" ) == 0) ||
      (strcmp( i_arch, "cpx" ) == 0)   ) {
    if ((strcmp( i_arch, "knl" ) == 0)) {
      io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX512_MIC;
    } else if ((strcmp( i_arch, "knm" ) == 0)) {
      io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX512_KNM;
    } else if ((strcmp( i_arch, "skx" ) == 0)) {
      io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX512_CORE;
    } else if ((strcmp( i_arch, "clx" ) == 0)) {
      io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX512_CLX;
    } else if ((strcmp( i_arch, "cpx" ) == 0)) {
      io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX512_CPX;
    } else {
      /* shouldn't happen */
    }
    io_micro_kernel_config->vector_reg_count = 32;
    io_micro_kernel_config->use_masking_a_c = i_use_masking_a_c;
    io_micro_kernel_config->vector_name = 'z';
    if ( LIBXSMM_GEMM_PRECISION_F64 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      io_micro_kernel_config->vector_length = 8;
      io_micro_kernel_config->datatype_size = 8;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPD;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VBROADCASTSD;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPD;
        if ( (i_use_masking_a_c == 0) ) {
          io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPD;
        } else {
          io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVAPD;
        }
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VPXORD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VFMADD231PD;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VADDPD;
    } else if ( LIBXSMM_GEMM_PRECISION_F32 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      io_micro_kernel_config->vector_length = 16;
      io_micro_kernel_config->datatype_size = 4;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VBROADCASTSS;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
        if ( (i_use_masking_a_c == 0) ) {
          io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPS;
        } else {
          io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
        }
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VPXORD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VFMADD231PS;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VADDPS;
    } else if ( LIBXSMM_GEMM_PRECISION_I16 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      /* C is 32bit, so we treat all 3 matrices as 32bit element arrays */
      io_micro_kernel_config->vector_length = 16;
      io_micro_kernel_config->datatype_size = 4;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
        if ( (i_use_masking_a_c == 0) ) {
          io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPS;
        } else {
          io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
        }
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VPXORD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VPADDD;
    } else if ( LIBXSMM_GEMM_PRECISION_BF16 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      /* C is 32bit, so we treat all 3 matrices as 32bit element arrays */
      io_micro_kernel_config->vector_length = 16;
      io_micro_kernel_config->datatype_size = 4;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
        if ( (i_use_masking_a_c == 0) ) {
          io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPS;
        } else {
          io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
        }
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VPXORD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VADDPS;
    } else {
      /* shouldn't happen as we caught this case earlier */
      io_micro_kernel_config->instruction_set = LIBXSMM_X86_GENERIC;
      io_micro_kernel_config->vector_reg_count = 0;
      io_micro_kernel_config->use_masking_a_c = 0;
      io_micro_kernel_config->vector_name = 'a';
      io_micro_kernel_config->vector_length = 0;
      io_micro_kernel_config->datatype_size = 0;
      io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_UNDEF;
    }
  } else {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_GENERIC;
    io_micro_kernel_config->vector_reg_count = 0;
    io_micro_kernel_config->use_masking_a_c = 0;
    io_micro_kernel_config->vector_name = 'a';
    io_micro_kernel_config->vector_length = 0;
    io_micro_kernel_config->datatype_size = 0;
    io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_UNDEF;
  }

  io_micro_kernel_config->prefetch_instruction = LIBXSMM_X86_INSTR_PREFETCHT1;
  io_micro_kernel_config->alu_add_instruction = LIBXSMM_X86_INSTR_ADDQ;
  io_micro_kernel_config->alu_sub_instruction = LIBXSMM_X86_INSTR_SUBQ;
  io_micro_kernel_config->alu_cmp_instruction = LIBXSMM_X86_INSTR_CMPQ;
  io_micro_kernel_config->alu_jmp_instruction = LIBXSMM_X86_INSTR_JL;
  io_micro_kernel_config->alu_mov_instruction = LIBXSMM_X86_INSTR_MOVQ;
}

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_init_micro_kernel_config_halfvector( libxsmm_micro_kernel_config*    io_micro_kernel_config,
    const libxsmm_gemm_descriptor* i_xgemm_desc,
    const char*                    i_arch,
    const unsigned int             i_use_masking_a_c ) {
  if ( strcmp( i_arch, "wsm" ) == 0 ) {
#if !defined(NDEBUG)
    fprintf(stderr, "LIBXSMM WARNING, libxsmm_generator_gemm_init_micro_kernel_config_halfvector, redirecting to scalar, please fix the generation code!!!\n");
#endif
    libxsmm_generator_gemm_init_micro_kernel_config_scalar( io_micro_kernel_config, i_xgemm_desc, i_arch, i_use_masking_a_c );
  } else if ( strcmp( i_arch, "snb" ) == 0 ) {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX;
    io_micro_kernel_config->vector_reg_count = 16;
    io_micro_kernel_config->use_masking_a_c = i_use_masking_a_c;
    io_micro_kernel_config->vector_name = 'x';
    if ( LIBXSMM_GEMM_PRECISION_F64 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      io_micro_kernel_config->vector_length = 2;
      io_micro_kernel_config->datatype_size = 8;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPD;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VMOVDDUP;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPD;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPD;
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VMULPD;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VADDPD;
    } else {
      io_micro_kernel_config->vector_length = 4;
      io_micro_kernel_config->datatype_size = 4;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VBROADCASTSS;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPS;
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPS;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VMULPS;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VADDPS;
    }
  } else if ( strcmp( i_arch, "hsw" ) == 0 ) {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX2;
    io_micro_kernel_config->vector_reg_count = 16;
    io_micro_kernel_config->use_masking_a_c = i_use_masking_a_c;
    io_micro_kernel_config->vector_name = 'x';
    if ( LIBXSMM_GEMM_PRECISION_F64 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      io_micro_kernel_config->vector_length = 2;
      io_micro_kernel_config->datatype_size = 8;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPD;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VMOVDDUP;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPD;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPD;
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPD;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VFMADD231PD;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_UNDEF;
    } else {
      io_micro_kernel_config->vector_length = 4;
      io_micro_kernel_config->datatype_size = 4;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_A & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
      } else {
        io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VBROADCASTSS;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      if ( (LIBXSMM_GEMM_FLAG_ALIGN_C & i_xgemm_desc->flags) != 0 ) {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVAPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVNTPS;
      } else {
        io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
        io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVUPS;
      }
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPS;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VFMADD231PS;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_UNDEF;
    }
  } else if ( (strcmp( i_arch, "knl" ) == 0) ||
      (strcmp( i_arch, "knm" ) == 0) ||
      (strcmp( i_arch, "skx" ) == 0) ||
      (strcmp( i_arch, "clx" ) == 0) ||
      (strcmp( i_arch, "cpx" ) == 0)   ) {
#if !defined(NDEBUG)
    fprintf(stderr, "LIBXSMM WARNING, libxsmm_generator_gemm_init_micro_kernel_config_halfvector, AVX512 redirecting to fullvector!\n");
#endif
    libxsmm_generator_gemm_init_micro_kernel_config_fullvector( io_micro_kernel_config, i_xgemm_desc, i_arch, i_use_masking_a_c );
  } else {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_GENERIC;
    io_micro_kernel_config->vector_reg_count = 0;
    io_micro_kernel_config->use_masking_a_c = 0;
    io_micro_kernel_config->vector_name = 'a';
    io_micro_kernel_config->vector_length = 0;
    io_micro_kernel_config->datatype_size = 0;
    io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_UNDEF;
  }

  io_micro_kernel_config->prefetch_instruction = LIBXSMM_X86_INSTR_PREFETCHT1;
  io_micro_kernel_config->alu_add_instruction = LIBXSMM_X86_INSTR_ADDQ;
  io_micro_kernel_config->alu_sub_instruction = LIBXSMM_X86_INSTR_SUBQ;
  io_micro_kernel_config->alu_cmp_instruction = LIBXSMM_X86_INSTR_CMPQ;
  io_micro_kernel_config->alu_jmp_instruction = LIBXSMM_X86_INSTR_JL;
  io_micro_kernel_config->alu_mov_instruction = LIBXSMM_X86_INSTR_MOVQ;
}

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_init_micro_kernel_config_scalar( libxsmm_micro_kernel_config*    io_micro_kernel_config,
    const libxsmm_gemm_descriptor* i_xgemm_desc,
    const char*                    i_arch,
    const unsigned int             i_use_masking_a_c ) {
  if ( strcmp( i_arch, "wsm" ) == 0 ) {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_SSE3;
    io_micro_kernel_config->vector_reg_count = 16;
    io_micro_kernel_config->use_masking_a_c = i_use_masking_a_c;
    io_micro_kernel_config->vector_name = 'x';
    if ( LIBXSMM_GEMM_PRECISION_F64 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      io_micro_kernel_config->vector_length = 1;
      io_micro_kernel_config->datatype_size = 8;
      io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_MOVSD;
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_MOVSD;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_MOVSD;
      io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_MOVSD;
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_XORPD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_MULSD;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_ADDSD;
    } else {
      io_micro_kernel_config->vector_length = 1;
      io_micro_kernel_config->datatype_size = 4;
      io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_MOVSS;
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_MOVSS;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_MOVSS;
      io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_MOVSS;
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_XORPS;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_MULSS;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_ADDSS;
    }
  } else if ( strcmp( i_arch, "snb" ) == 0 ) {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX;
    io_micro_kernel_config->vector_reg_count = 16;
    io_micro_kernel_config->use_masking_a_c = i_use_masking_a_c;
    io_micro_kernel_config->vector_name = 'x';
    if ( LIBXSMM_GEMM_PRECISION_F64 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      io_micro_kernel_config->vector_length = 1;
      io_micro_kernel_config->datatype_size = 8;
      io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VMULSD;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VADDSD;
    } else {
      io_micro_kernel_config->vector_length = 1;
      io_micro_kernel_config->datatype_size = 4;
      io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPS;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VMULSS;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VADDSS;
    }
  } else if ( strcmp( i_arch, "hsw" ) == 0 ) {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX2;
    io_micro_kernel_config->vector_reg_count = 16;
    io_micro_kernel_config->use_masking_a_c = i_use_masking_a_c;
    io_micro_kernel_config->vector_name = 'x';
    if ( LIBXSMM_GEMM_PRECISION_F64 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      io_micro_kernel_config->vector_length = 1;
      io_micro_kernel_config->datatype_size = 8;
      io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VFMADD231SD;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_UNDEF;
    } else {
      io_micro_kernel_config->vector_length = 1;
      io_micro_kernel_config->datatype_size = 4;
      io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VXORPS;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VFMADD231SS;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_UNDEF;
    }
  } else if ( (strcmp( i_arch, "knl" ) == 0) ||
      (strcmp( i_arch, "knm" ) == 0) ||
      (strcmp( i_arch, "skx" ) == 0) ||
      (strcmp( i_arch, "clx" ) == 0) ||
      (strcmp( i_arch, "cpx" ) == 0)   ) {
    if ((strcmp( i_arch, "knl" ) == 0)) {
      io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX512_MIC;
    } else if ((strcmp( i_arch, "knm" ) == 0)) {
      io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX512_KNM;
    } else if ((strcmp( i_arch, "skx" ) == 0)) {
      io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX512_CORE;
    } else if ((strcmp( i_arch, "clx" ) == 0)) {
      io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX512_CLX;
    } else if ((strcmp( i_arch, "cpx" ) == 0)) {
      io_micro_kernel_config->instruction_set = LIBXSMM_X86_AVX512_CPX;
    } else {
      /* shouldn't happen */
    }
    io_micro_kernel_config->vector_reg_count = 16;
    io_micro_kernel_config->use_masking_a_c = i_use_masking_a_c;
    io_micro_kernel_config->vector_name = 'x';
    if ( LIBXSMM_GEMM_PRECISION_F64 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) {
      io_micro_kernel_config->vector_length = 1;
      io_micro_kernel_config->datatype_size = 8;
      io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVSD;
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VPXORD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VFMADD231SD;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VADDSD;
    } else {
      io_micro_kernel_config->vector_length = 1;
      io_micro_kernel_config->datatype_size = 4;
      io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
      io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_VMOVSS;
      io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_VPXORD;
      io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_VFMADD231SS;
      io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_VADDSS;
    }
  } else {
    io_micro_kernel_config->instruction_set = LIBXSMM_X86_GENERIC;
    io_micro_kernel_config->vector_reg_count = 0;
    io_micro_kernel_config->use_masking_a_c = 0;
    io_micro_kernel_config->vector_name = 'a';
    io_micro_kernel_config->vector_length = 0;
    io_micro_kernel_config->datatype_size = 0;
    io_micro_kernel_config->a_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->b_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->b_shuff_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->c_vmove_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->c_vmove_nts_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->vxor_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->vmul_instruction = LIBXSMM_X86_INSTR_UNDEF;
    io_micro_kernel_config->vadd_instruction = LIBXSMM_X86_INSTR_UNDEF;
  }

  io_micro_kernel_config->prefetch_instruction = LIBXSMM_X86_INSTR_PREFETCHT1;
  io_micro_kernel_config->alu_add_instruction = LIBXSMM_X86_INSTR_ADDQ;
  io_micro_kernel_config->alu_sub_instruction = LIBXSMM_X86_INSTR_SUBQ;
  io_micro_kernel_config->alu_cmp_instruction = LIBXSMM_X86_INSTR_CMPQ;
  io_micro_kernel_config->alu_jmp_instruction = LIBXSMM_X86_INSTR_JL;
  io_micro_kernel_config->alu_mov_instruction = LIBXSMM_X86_INSTR_MOVQ;
}

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_add_flop_counter( libxsmm_generated_code*         io_generated_code,
    const libxsmm_gemm_descriptor* i_xgemm_desc ) {
  if ( io_generated_code->code_type == 0 ) {
    char l_new_code[512];
    const unsigned int l_max_code_length = sizeof(l_new_code) - 1;
    int l_code_length = 0;

    l_code_length = LIBXSMM_SNPRINTF( l_new_code, l_max_code_length, "#ifndef NDEBUG\n" );
    libxsmm_append_code_as_string( io_generated_code, l_new_code, l_code_length );
    l_code_length = LIBXSMM_SNPRINTF( l_new_code, l_max_code_length, "#ifdef _OPENMP\n" );
    libxsmm_append_code_as_string( io_generated_code, l_new_code, l_code_length );
    l_code_length = LIBXSMM_SNPRINTF( l_new_code, l_max_code_length, "#pragma omp atomic\n" );
    libxsmm_append_code_as_string( io_generated_code, l_new_code, l_code_length );
    l_code_length = LIBXSMM_SNPRINTF( l_new_code, l_max_code_length, "#endif\n" );
    libxsmm_append_code_as_string( io_generated_code, l_new_code, l_code_length );
    l_code_length = LIBXSMM_SNPRINTF( l_new_code, l_max_code_length, "libxsmm_num_total_flops += %u;\n", 2u * i_xgemm_desc->m * i_xgemm_desc->n * i_xgemm_desc->k);
    libxsmm_append_code_as_string( io_generated_code, l_new_code, l_code_length );
    l_code_length = LIBXSMM_SNPRINTF( l_new_code, l_max_code_length, "#endif\n" );
    libxsmm_append_code_as_string( io_generated_code, l_new_code, l_code_length );
  }
}

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_header_kloop( libxsmm_generated_code*             io_generated_code,
    libxsmm_loop_label_tracker*        io_loop_label_tracker,
    const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
    const libxsmm_micro_kernel_config* i_micro_kernel_config,
    const unsigned int                 i_m_blocking,
    const unsigned int                 i_k_blocking ) {
  LIBXSMM_UNUSED(i_m_blocking);
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_kloop, 0);
  libxsmm_x86_instruction_register_jump_back_label( io_generated_code, io_loop_label_tracker );
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_kloop, i_k_blocking);
}

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_footer_kloop( libxsmm_generated_code*             io_generated_code,
    libxsmm_loop_label_tracker*        io_loop_label_tracker,
    const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
    const libxsmm_micro_kernel_config* i_micro_kernel_config,
    const libxsmm_gemm_descriptor*     i_xgemm_desc,
    const unsigned int                 i_m_blocking,
    const unsigned int                 i_max_blocked_k,
    const unsigned int                 i_kloop_complete ) {
  LIBXSMM_UNUSED(i_m_blocking);
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_cmp_instruction, i_gp_reg_mapping->gp_reg_kloop, i_max_blocked_k );
  libxsmm_x86_instruction_jump_back_to_label( io_generated_code, i_micro_kernel_config->alu_jmp_instruction, io_loop_label_tracker );
  if ( i_kloop_complete != 0 ) {
    int l_b_offset = 0;
    if ( (i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_TRANS_B) > 0 ) {
      l_b_offset = i_xgemm_desc->ldb * i_xgemm_desc->k * i_micro_kernel_config->datatype_size;
    } else {
      l_b_offset = i_xgemm_desc->k * i_micro_kernel_config->datatype_size;
    }

    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_sub_instruction,
        i_gp_reg_mapping->gp_reg_b, l_b_offset );
  }
}

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_header_reduceloop( libxsmm_generated_code*             io_generated_code,
    libxsmm_loop_label_tracker*        io_loop_label_tracker,
    const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
    const libxsmm_micro_kernel_config* i_micro_kernel_config ) {
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_reduce_loop, 0);
  libxsmm_x86_instruction_register_jump_back_label( io_generated_code, io_loop_label_tracker );
}

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_footer_reduceloop( libxsmm_generated_code*             io_generated_code,
    libxsmm_loop_label_tracker*        io_loop_label_tracker,
    const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
    const libxsmm_micro_kernel_config* i_micro_kernel_config,
    const libxsmm_gemm_descriptor*     i_xgemm_desc) {
  LIBXSMM_UNUSED(i_xgemm_desc);
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_reduce_loop, 1);
  libxsmm_x86_instruction_alu_reg( io_generated_code, i_micro_kernel_config->alu_cmp_instruction, i_gp_reg_mapping->gp_reg_reduce_count, i_gp_reg_mapping->gp_reg_reduce_loop);
  libxsmm_x86_instruction_jump_back_to_label( io_generated_code, i_micro_kernel_config->alu_jmp_instruction, io_loop_label_tracker );
}


LIBXSMM_API_INTERN
void libxsmm_generator_gemm_header_nloop( libxsmm_generated_code*             io_generated_code,
    libxsmm_loop_label_tracker*        io_loop_label_tracker,
    const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
    const libxsmm_micro_kernel_config* i_micro_kernel_config,
    const unsigned int                 i_n_blocking) {
  libxsmm_x86_instruction_register_jump_back_label( io_generated_code, io_loop_label_tracker );
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_nloop, i_n_blocking );
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_mov_instruction, i_gp_reg_mapping->gp_reg_mloop, 0 );
}


LIBXSMM_API_INTERN
void libxsmm_generator_gemm_footer_nloop( libxsmm_generated_code*             io_generated_code,
    libxsmm_loop_label_tracker*        io_loop_label_tracker,
    const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
    const libxsmm_micro_kernel_config* i_micro_kernel_config,
    const libxsmm_gemm_descriptor*     i_xgemm_desc,
    const unsigned int                 i_n_blocking,
    const unsigned int                 i_n_done ) {
  if ( LIBXSMM_GEMM_PRECISION_BF16 == LIBXSMM_GETENUM_OUT( i_xgemm_desc->datatype ) ) {
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_c,
        (i_n_blocking*(i_xgemm_desc->ldc)*(i_micro_kernel_config->datatype_size/2)) - ((i_xgemm_desc->m)*(i_micro_kernel_config->datatype_size/2)) );
  } else {
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_c,
        (i_n_blocking*(i_xgemm_desc->ldc)*(i_micro_kernel_config->datatype_size)) - ((i_xgemm_desc->m)*(i_micro_kernel_config->datatype_size)) );
  }
#if 0
  if ( i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_CL2 ||
      i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2CL2BL2_VIA_C ) {
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_c_prefetch,
        (i_n_blocking*(i_xgemm_desc->ldc)*(i_micro_kernel_config->datatype_size)) - ((i_xgemm_desc->m)*(i_micro_kernel_config->datatype_size)) );
  }
#endif
  if (i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_BATCH_REDUCE) {
    /* handle trans B */
    int l_b_offset = 0;
    if ( (i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_TRANS_B) > 0 ) {
      l_b_offset = i_n_blocking * i_micro_kernel_config->datatype_size;
    } else {
      l_b_offset = i_n_blocking * i_xgemm_desc->ldb * i_micro_kernel_config->datatype_size;
    }

    libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_0 );
    libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_reduce_loop );
    libxsmm_generator_gemm_header_reduceloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config );
    libxsmm_x86_instruction_alu_mem( io_generated_code,
        i_micro_kernel_config->alu_mov_instruction,
        i_gp_reg_mapping->gp_reg_a,
        i_gp_reg_mapping->gp_reg_reduce_loop, 8,
        0,
        i_gp_reg_mapping->gp_reg_help_0,
        0 );
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_sub_instruction,
        i_gp_reg_mapping->gp_reg_help_0, ((i_xgemm_desc->m)*(i_micro_kernel_config->datatype_size)) );
    libxsmm_x86_instruction_alu_mem( io_generated_code,
        i_micro_kernel_config->alu_mov_instruction,
        i_gp_reg_mapping->gp_reg_a,
        i_gp_reg_mapping->gp_reg_reduce_loop, 8,
        0,
        i_gp_reg_mapping->gp_reg_help_0,
        1 );
    libxsmm_x86_instruction_alu_mem( io_generated_code,
        i_micro_kernel_config->alu_mov_instruction,
        i_gp_reg_mapping->gp_reg_b,
        i_gp_reg_mapping->gp_reg_reduce_loop, 8,
        0,
        i_gp_reg_mapping->gp_reg_help_0,
        0 );
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction,
        i_gp_reg_mapping->gp_reg_help_0, l_b_offset );
    libxsmm_x86_instruction_alu_mem( io_generated_code,
        i_micro_kernel_config->alu_mov_instruction,
        i_gp_reg_mapping->gp_reg_b,
        i_gp_reg_mapping->gp_reg_reduce_loop, 8,
        0,
        i_gp_reg_mapping->gp_reg_help_0,
        1 );
    libxsmm_generator_gemm_footer_reduceloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc);
    libxsmm_x86_instruction_pop_reg( io_generated_code, i_gp_reg_mapping->gp_reg_reduce_loop );
    libxsmm_x86_instruction_pop_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_0 );
  } else {
    /* handle trans B */
    int l_b_offset = 0;
    if ( (i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_TRANS_B) > 0 ) {
      l_b_offset = i_n_blocking * i_micro_kernel_config->datatype_size;
    } else {
      l_b_offset = i_n_blocking * i_xgemm_desc->ldb * i_micro_kernel_config->datatype_size;
    }

    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction,
        i_gp_reg_mapping->gp_reg_b, l_b_offset );

    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_sub_instruction,
        i_gp_reg_mapping->gp_reg_a, ((i_xgemm_desc->m)*(i_micro_kernel_config->datatype_size)) );
  }
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_cmp_instruction, i_gp_reg_mapping->gp_reg_nloop, i_n_done );
  libxsmm_x86_instruction_jump_back_to_label( io_generated_code, i_micro_kernel_config->alu_jmp_instruction, io_loop_label_tracker );
}

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_header_mloop( libxsmm_generated_code*             io_generated_code,
    libxsmm_loop_label_tracker*        io_loop_label_tracker,
    const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
    const libxsmm_micro_kernel_config* i_micro_kernel_config,
    const unsigned int                 i_m_blocking ) {
  libxsmm_x86_instruction_register_jump_back_label( io_generated_code, io_loop_label_tracker );
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_mloop, i_m_blocking );
}

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_footer_mloop( libxsmm_generated_code*             io_generated_code,
    libxsmm_loop_label_tracker*        io_loop_label_tracker,
    const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
    const libxsmm_micro_kernel_config* i_micro_kernel_config,
    const libxsmm_gemm_descriptor*     i_xgemm_desc,
    const unsigned int                 i_m_blocking,
    const unsigned int                 i_m_done,
    const unsigned int                 i_k_unrolled ) {
  /* advance C pointer */
  if ( LIBXSMM_GEMM_PRECISION_BF16 == LIBXSMM_GETENUM_OUT( i_xgemm_desc->datatype ) ) {
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction,
        i_gp_reg_mapping->gp_reg_c, i_m_blocking*(i_micro_kernel_config->datatype_size/2) );
  } else {
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction,
        i_gp_reg_mapping->gp_reg_c, i_m_blocking*(i_micro_kernel_config->datatype_size) );
  }

  /* C prefetch */
#if 0
  if ( i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_CL2 ||
      i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2CL2BL2_VIA_C ) {
    libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction,
        i_gp_reg_mapping->gp_reg_c_prefetch, i_m_blocking*(i_micro_kernel_config->datatype_size) );

  }
#endif

  /* B prefetch */
  if ( i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_BL2_VIA_C ||
      i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C ||
      i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C_AHEAD ||
      i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C_JPST) {
    if ( (i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_TRANS_B) == 0 ) {
      libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction,
          i_gp_reg_mapping->gp_reg_b_prefetch, i_m_blocking*(i_micro_kernel_config->datatype_size) );
    }
  }

  if (i_k_unrolled == 0) {
    /* A prefetch */
    if ( i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2 ||
        i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C) {
      if (i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_BATCH_REDUCE) {
        if ( i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2 ) {
          libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_0 );
          libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_reduce_loop );
          libxsmm_generator_gemm_header_reduceloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config );
          libxsmm_x86_instruction_alu_mem( io_generated_code,
              i_micro_kernel_config->alu_mov_instruction,
              i_gp_reg_mapping->gp_reg_a_prefetch,
              i_gp_reg_mapping->gp_reg_reduce_loop, 8,
              0,
              i_gp_reg_mapping->gp_reg_help_0,
              0 );
          libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_sub_instruction, i_gp_reg_mapping->gp_reg_help_0,
              ((i_xgemm_desc->k) * (i_micro_kernel_config->datatype_size) * (i_xgemm_desc->lda) ) -
              (i_m_blocking * (i_micro_kernel_config->datatype_size)) );
          libxsmm_x86_instruction_alu_mem( io_generated_code,
              i_micro_kernel_config->alu_mov_instruction,
              i_gp_reg_mapping->gp_reg_a_prefetch,
              i_gp_reg_mapping->gp_reg_reduce_loop, 8,
              0,
              i_gp_reg_mapping->gp_reg_help_0,
              1 );
          libxsmm_generator_gemm_footer_reduceloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc);
          libxsmm_x86_instruction_pop_reg( io_generated_code, i_gp_reg_mapping->gp_reg_reduce_loop );
          libxsmm_x86_instruction_pop_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_0 );
        }
      } else {
        libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_sub_instruction, i_gp_reg_mapping->gp_reg_a_prefetch,
            ((i_xgemm_desc->k) * (i_micro_kernel_config->datatype_size) * (i_xgemm_desc->lda) ) -
            (i_m_blocking * (i_micro_kernel_config->datatype_size)) );
      }
    }
    /* advance A pointer */
    if (i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_BATCH_REDUCE) {
      libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_0 );
      libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_reduce_loop );
      libxsmm_generator_gemm_header_reduceloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config );
      libxsmm_x86_instruction_alu_mem( io_generated_code,
          i_micro_kernel_config->alu_mov_instruction,
          i_gp_reg_mapping->gp_reg_a,
          i_gp_reg_mapping->gp_reg_reduce_loop, 8,
          0,
          i_gp_reg_mapping->gp_reg_help_0,
          0 );
      libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_sub_instruction, i_gp_reg_mapping->gp_reg_help_0,
          ((i_xgemm_desc->k) * (i_micro_kernel_config->datatype_size) * (i_xgemm_desc->lda) ) - (i_m_blocking * (i_micro_kernel_config->datatype_size)) );
      libxsmm_x86_instruction_alu_mem( io_generated_code,
          i_micro_kernel_config->alu_mov_instruction,
          i_gp_reg_mapping->gp_reg_a,
          i_gp_reg_mapping->gp_reg_reduce_loop, 8,
          0,
          i_gp_reg_mapping->gp_reg_help_0,
          1 );
      libxsmm_generator_gemm_footer_reduceloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc);
      libxsmm_x86_instruction_pop_reg( io_generated_code, i_gp_reg_mapping->gp_reg_reduce_loop );
      libxsmm_x86_instruction_pop_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_0 );
    } else {
      libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_sub_instruction, i_gp_reg_mapping->gp_reg_a,
          ((i_xgemm_desc->k) * (i_micro_kernel_config->datatype_size) * (i_xgemm_desc->lda) ) - (i_m_blocking * (i_micro_kernel_config->datatype_size)) );
    }
  } else {
    /* A prefetch */
    if ( i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2 ||
        i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C) {
      if ( i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_BATCH_REDUCE ) {
        libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_0 );
        libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_reduce_loop );
        libxsmm_generator_gemm_header_reduceloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config );
        libxsmm_x86_instruction_alu_mem( io_generated_code,
            i_micro_kernel_config->alu_mov_instruction,
            i_gp_reg_mapping->gp_reg_a_prefetch,
            i_gp_reg_mapping->gp_reg_reduce_loop, 8,
            0,
            i_gp_reg_mapping->gp_reg_help_0,
            0 );
        libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_help_0,
            (i_m_blocking * (i_micro_kernel_config->datatype_size)) );
        libxsmm_x86_instruction_alu_mem( io_generated_code,
            i_micro_kernel_config->alu_mov_instruction,
            i_gp_reg_mapping->gp_reg_a_prefetch,
            i_gp_reg_mapping->gp_reg_reduce_loop, 8,
            0,
            i_gp_reg_mapping->gp_reg_help_0,
            1 );
        libxsmm_generator_gemm_footer_reduceloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc);
        libxsmm_x86_instruction_pop_reg( io_generated_code, i_gp_reg_mapping->gp_reg_reduce_loop );
        libxsmm_x86_instruction_pop_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_0 );
      } else {
        libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_a_prefetch,
            (i_m_blocking * (i_micro_kernel_config->datatype_size)) );
      }
    }

    if (i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_BATCH_REDUCE) {
      libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_0 );
      libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_reduce_loop );
      libxsmm_generator_gemm_header_reduceloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config );
      libxsmm_x86_instruction_alu_mem( io_generated_code,
          i_micro_kernel_config->alu_mov_instruction,
          i_gp_reg_mapping->gp_reg_a,
          i_gp_reg_mapping->gp_reg_reduce_loop, 8,
          0,
          i_gp_reg_mapping->gp_reg_help_0,
          0 );
      libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_help_0,
          (i_m_blocking * (i_micro_kernel_config->datatype_size)) );
      libxsmm_x86_instruction_alu_mem( io_generated_code,
          i_micro_kernel_config->alu_mov_instruction,
          i_gp_reg_mapping->gp_reg_a,
          i_gp_reg_mapping->gp_reg_reduce_loop, 8,
          0,
          i_gp_reg_mapping->gp_reg_help_0,
          1 );
      libxsmm_generator_gemm_footer_reduceloop( io_generated_code, io_loop_label_tracker, i_gp_reg_mapping, i_micro_kernel_config, i_xgemm_desc);
      libxsmm_x86_instruction_pop_reg( io_generated_code, i_gp_reg_mapping->gp_reg_reduce_loop );
      libxsmm_x86_instruction_pop_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_0 );
    } else {
      /* advance A pointer */
      libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_add_instruction, i_gp_reg_mapping->gp_reg_a,
          (i_m_blocking * (i_micro_kernel_config->datatype_size)) );
    }
  }

  /* loop handling */
  libxsmm_x86_instruction_alu_imm( io_generated_code, i_micro_kernel_config->alu_cmp_instruction, i_gp_reg_mapping->gp_reg_mloop, i_m_done );
  libxsmm_x86_instruction_jump_back_to_label( io_generated_code, i_micro_kernel_config->alu_jmp_instruction, io_loop_label_tracker );
}

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_load_C( libxsmm_generated_code*             io_generated_code,
    const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
    const libxsmm_micro_kernel_config* i_micro_kernel_config,
    const libxsmm_gemm_descriptor*     i_xgemm_desc,
    const unsigned int                 i_m_blocking,
    const unsigned int                 i_n_blocking ) {
  unsigned int l_m_blocking, l_vec_reg_acc_start;
  /* register blocking counter in n */
  unsigned int l_n = 0;
  /* register blocking counter in m */
  unsigned int l_m = 0;

  assert(0 < i_micro_kernel_config->vector_length);
  /* deriving register blocking from kernel config */
  l_m_blocking = i_m_blocking / i_micro_kernel_config->vector_length;
  /* start register of accumulator */
  l_vec_reg_acc_start = i_micro_kernel_config->vector_reg_count - (i_n_blocking * l_m_blocking);

#if !defined(NDEBUG)
  /* Do some test if it is possible to generate the requested code.
     This is not done in release mode and therefore bad
     things might happen.... HUAAH */
  if (i_micro_kernel_config->instruction_set == LIBXSMM_X86_SSE3 ||
      i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX  ||
      i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX2 ) {
    if ( (i_n_blocking > 3) || (i_n_blocking < 1) || (i_m_blocking < 1) ) {
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_REG_BLOCK );
      return;
    }
  } else if ( i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_MIC  ||
      ( (i_micro_kernel_config->instruction_set >= LIBXSMM_X86_AVX512_CORE) && (i_m_blocking == i_micro_kernel_config->vector_length) ) ) {
    if ( (i_n_blocking > 30) || (i_n_blocking < 1) || (i_m_blocking != i_micro_kernel_config->vector_length) ) {
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_REG_BLOCK );
      return;
    }
  } else if ( i_micro_kernel_config->instruction_set >= LIBXSMM_X86_AVX512_CORE ) {
    if ( (i_n_blocking > 6) || (i_n_blocking < 1) || (i_m_blocking < 1) ) {
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_REG_BLOCK );
      return;
    }
  } else {}
  if ( i_m_blocking % i_micro_kernel_config->vector_length != 0 ) {
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_M_BLOCK );
    return;
  }
#endif /*!defined(NDEBUG)*/

  /* load C accumulator */
  if (0 == (LIBXSMM_GEMM_FLAG_BETA_0 & i_xgemm_desc->flags)) { /* Beta=1 */
    if ( ( (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CORE) || (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_KNM) ||
           (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CLX) || (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CPX) ) &&
         ( (LIBXSMM_GEMM_PRECISION_I16 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) && (LIBXSMM_GEMM_PRECISION_F32 == LIBXSMM_GETENUM_OUT( i_xgemm_desc->datatype ) ) ) ) {
      /* we add when scaling during conversion to FP32 */
      for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
        for ( l_m = 0; l_m < l_m_blocking; l_m++ ) {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
              i_micro_kernel_config->instruction_set,
              i_micro_kernel_config->vxor_instruction,
              i_micro_kernel_config->vector_name,
              l_vec_reg_acc_start + l_m + (l_m_blocking * l_n),
              l_vec_reg_acc_start + l_m + (l_m_blocking * l_n),
              l_vec_reg_acc_start + l_m + (l_m_blocking * l_n) );
        }
      }
    } else if ( ( (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CORE) || (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_KNM) ||
                  (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CLX) || (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CPX) ) &&
                ( (LIBXSMM_GEMM_PRECISION_BF16 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) && (LIBXSMM_GEMM_PRECISION_BF16 == LIBXSMM_GETENUM_OUT( i_xgemm_desc->datatype ) ) ) ) {
      /* we add when scaling during conversion to FP32 */
      for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
        for ( l_m = 0; l_m < l_m_blocking; l_m++ ) {
          /* load 16 bit values into ymm */
          libxsmm_x86_instruction_vec_move( io_generated_code,
              i_micro_kernel_config->instruction_set,
              i_micro_kernel_config->c_vmove_instruction,
              i_gp_reg_mapping->gp_reg_c,
              LIBXSMM_X86_GP_REG_UNDEF, 0,
              ((l_n * i_xgemm_desc->ldc) + (l_m * (i_micro_kernel_config->vector_length))) * (i_micro_kernel_config->datatype_size/2),
              'y',
              0, i_micro_kernel_config->use_masking_a_c, 1, 0 );

          /* convert 16 bit values into 32 bit (integer convert) */
          libxsmm_x86_instruction_vec_compute_convert( io_generated_code,
              i_micro_kernel_config->instruction_set,
              LIBXSMM_X86_INSTR_VPMOVSXWD,
              i_micro_kernel_config->vector_name,
              0, LIBXSMM_X86_VEC_REG_UNDEF,
              l_vec_reg_acc_start + l_m + (l_m_blocking * l_n),
              LIBXSMM_X86_VEC_REG_UNDEF);

          /* shift 16 bits to the left to generate valid FP32 numbers */
          libxsmm_x86_instruction_vec_shuffle_reg(io_generated_code,
              i_micro_kernel_config->instruction_set,
              LIBXSMM_X86_INSTR_VPSLLD,
              i_micro_kernel_config->vector_name,
              l_vec_reg_acc_start + l_m + (l_m_blocking * l_n),
              l_vec_reg_acc_start + l_m + (l_m_blocking * l_n),
              LIBXSMM_X86_VEC_REG_UNDEF,
              16);
        }
      }
    } else {
      /* adding to C, so let's load C */
      for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
        for ( l_m = 0; l_m < l_m_blocking; l_m++ ) {
          libxsmm_x86_instruction_vec_move( io_generated_code,
              i_micro_kernel_config->instruction_set,
              i_micro_kernel_config->c_vmove_instruction,
              i_gp_reg_mapping->gp_reg_c,
              LIBXSMM_X86_GP_REG_UNDEF, 0,
              ((l_n * i_xgemm_desc->ldc) + (l_m * (i_micro_kernel_config->vector_length))) * (i_micro_kernel_config->datatype_size),
              i_micro_kernel_config->vector_name,
              l_vec_reg_acc_start + l_m + (l_m_blocking * l_n), i_micro_kernel_config->use_masking_a_c, 1, 0 );
        }
#if 0
        if ( i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_CL2 ||
            i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2CL2BL2_VIA_C )  {
          for (l_m = 0; l_m < l_m_blocking; l_m += l_m++ ) {
            libxsmm_x86_instruction_prefetch( io_generated_code,
                i_micro_kernel_config->prefetch_instruction,
                i_gp_reg_mapping->gp_reg_c_prefetch,
                LIBXSMM_X86_GP_REG_UNDEF, 0,
                ((l_n * i_xgemm_desc->ldc) + (l_m * (i_micro_kernel_config->vector_length))) * (i_micro_kernel_config->datatype_size));
          }
        }
#endif
      }
    }
  } else {
    /* overwriting C, so let's xout accumulator */
    for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
      for ( l_m = 0; l_m < l_m_blocking; l_m++ ) {
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
            i_micro_kernel_config->instruction_set,
            i_micro_kernel_config->vxor_instruction,
            i_micro_kernel_config->vector_name,
            l_vec_reg_acc_start + l_m + (l_m_blocking * l_n),
            l_vec_reg_acc_start + l_m + (l_m_blocking * l_n),
            l_vec_reg_acc_start + l_m + (l_m_blocking * l_n) );
      }
#if 0
      if ( i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_CL2 ||
          i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2CL2BL2_VIA_C )  {
        for (l_m = 0; l_m < l_m_blocking; l_m += l_m++ ) {
          libxsmm_x86_instruction_prefetch( io_generated_code,
              i_micro_kernel_config->prefetch_instruction,
              i_gp_reg_mapping->gp_reg_c_prefetch,
              LIBXSMM_X86_GP_REG_UNDEF, 0,
              ((l_n * i_xgemm_desc->ldc) + (l_m * (i_micro_kernel_config->vector_length))) * (i_micro_kernel_config->datatype_size));
        }
      }
#endif
    }
  }
}

LIBXSMM_API_INTERN
void libxsmm_generator_gemm_store_C( libxsmm_generated_code*             io_generated_code,
    const libxsmm_gp_reg_mapping*      i_gp_reg_mapping,
    const libxsmm_micro_kernel_config* i_micro_kernel_config,
    const libxsmm_gemm_descriptor*     i_xgemm_desc,
    const unsigned int                 i_m_blocking,
    const unsigned int                 i_n_blocking )
{
  /* deriving register blocking from kernel config */
  unsigned int l_m_blocking = i_m_blocking/i_micro_kernel_config->vector_length;
  /* register blocking counter in n */
  unsigned int l_n = 0;
  /* register blocking counter in m */
  unsigned int l_m = 0;
  /* start register of accumulator */
  unsigned int l_vec_reg_acc_start = i_micro_kernel_config->vector_reg_count - (i_n_blocking * l_m_blocking);
  /* select store instruction */
  unsigned int l_vstore = (LIBXSMM_GEMM_FLAG_ALIGN_C_NTS_HINT == (LIBXSMM_GEMM_FLAG_ALIGN_C_NTS_HINT & i_xgemm_desc->flags)) ? i_micro_kernel_config->c_vmove_nts_instruction : i_micro_kernel_config->c_vmove_instruction;

  /* @TODO fix this test */
#if !defined(NDEBUG)
  if (i_micro_kernel_config->instruction_set == LIBXSMM_X86_SSE3 ||
      i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX  ||
      i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX2 ) {
    if ( (i_n_blocking > 3) || (i_n_blocking < 1) || (i_m_blocking < 1) ) {
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_REG_BLOCK );
      return;
    }
  } else if ( (i_micro_kernel_config->instruction_set >= LIBXSMM_X86_AVX512 && i_micro_kernel_config->instruction_set < LIBXSMM_X86_AVX512_CORE)  ||
      ( (i_micro_kernel_config->instruction_set >= LIBXSMM_X86_AVX512_CORE) && (i_m_blocking == i_micro_kernel_config->vector_length) ) ) {
    if ( (i_n_blocking > 30) || (i_n_blocking < 1) || (i_m_blocking != i_micro_kernel_config->vector_length) ) {
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_REG_BLOCK );
      return;
    }
  } else if ( i_micro_kernel_config->instruction_set >= LIBXSMM_X86_AVX512_CORE ) {
    if ( (i_n_blocking > 6) || (i_n_blocking < 1) || (i_m_blocking < 1) ) {
      LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_REG_BLOCK );
      return;
    }
  } else {}
  if ( i_m_blocking % i_micro_kernel_config->vector_length != 0 ) {
    LIBXSMM_HANDLE_ERROR( io_generated_code, LIBXSMM_ERR_M_BLOCK );
    return;
  }
#endif

  /* in case of IGEMM just do some potential conversion to FP */
  /* let convert the int32 accumulator into a FP32 values */
  if ( ( (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CORE) || (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_KNM) ||
       (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CLX) || (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CPX) ) &&
       ( (LIBXSMM_GEMM_PRECISION_I16 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) && (LIBXSMM_GEMM_PRECISION_F32 == LIBXSMM_GETENUM_OUT( i_xgemm_desc->datatype ) ) ) ) {
    /* load address of scaling factor from stack */
    libxsmm_x86_instruction_alu_mem( io_generated_code,
        i_micro_kernel_config->alu_mov_instruction,
        LIBXSMM_X86_GP_REG_RSP,
        LIBXSMM_X86_GP_REG_UNDEF, 0,
        48,
        i_gp_reg_mapping->gp_reg_help_1,
        0 );
    /* broadcast scaling factor into a vector register */
    libxsmm_x86_instruction_vec_move( io_generated_code,
        i_micro_kernel_config->instruction_set,
        LIBXSMM_X86_INSTR_VBROADCASTSS,
        i_gp_reg_mapping->gp_reg_help_1,
        LIBXSMM_X86_GP_REG_UNDEF, 0,
        0,
        i_micro_kernel_config->vector_name, 0,
        0, 1, 0 );
    /* loop over the accumulator, convert and scale */
    for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
      for ( l_m = 0; l_m < l_m_blocking; l_m++ ) {
        /* convert current accumulator register into FP32 */
        libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
            i_micro_kernel_config->instruction_set,
            LIBXSMM_X86_INSTR_VCVTDQ2PS,
            i_micro_kernel_config->vector_name,
            l_vec_reg_acc_start + l_m + (l_m_blocking * l_n),
            l_vec_reg_acc_start + l_m + (l_m_blocking * l_n),
            LIBXSMM_X86_VEC_REG_UNDEF );
        /* scale it */
        if (0 == (LIBXSMM_GEMM_FLAG_BETA_0 & i_xgemm_desc->flags)) { /* Beta=1 */
          libxsmm_x86_instruction_vec_compute_mem( io_generated_code,
              i_micro_kernel_config->instruction_set,
              LIBXSMM_X86_INSTR_VFMADD213PS,
              0,
              i_gp_reg_mapping->gp_reg_c,
              LIBXSMM_X86_GP_REG_UNDEF,
              0,
              ((l_n * i_xgemm_desc->ldc) + (l_m * (i_micro_kernel_config->vector_length))) * (i_micro_kernel_config->datatype_size),
              i_micro_kernel_config->vector_name,
              0,
              l_vec_reg_acc_start + l_m + (l_m_blocking * l_n));
        } else {
          libxsmm_x86_instruction_vec_compute_reg( io_generated_code,
              i_micro_kernel_config->instruction_set,
              LIBXSMM_X86_INSTR_VMULPS,
              i_micro_kernel_config->vector_name,
              0,
              l_vec_reg_acc_start + l_m + (l_m_blocking * l_n),
              l_vec_reg_acc_start + l_m + (l_m_blocking * l_n) );
        }
      }
    }
    /* storing C accumulator */
    for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
      for ( l_m = 0; l_m < l_m_blocking; l_m++ ) {
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            l_vstore,
            i_gp_reg_mapping->gp_reg_c,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            ((l_n * i_xgemm_desc->ldc) + (l_m * (i_micro_kernel_config->vector_length))) * (i_micro_kernel_config->datatype_size),
            i_micro_kernel_config->vector_name,
            l_vec_reg_acc_start + l_m + (l_m_blocking * l_n), i_micro_kernel_config->use_masking_a_c, 0, 1 );
      }
      if ( i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_BL2_VIA_C ||
           i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C ||
           i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C_AHEAD ||
           i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C_JPST)  {
        if ( (i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_TRANS_B) == 0 ) {
          /* determining how many prefetches we need in M direction as we just need one prefetch per cache line */
          unsigned int l_m_advance = 64 / ((i_micro_kernel_config->vector_length) * (i_micro_kernel_config->datatype_size)); /* 64: hardcoded cache line length */

          for (l_m = 0; l_m < l_m_blocking; l_m += l_m_advance ) {
            libxsmm_x86_instruction_prefetch( io_generated_code,
                i_micro_kernel_config->prefetch_instruction,
                i_gp_reg_mapping->gp_reg_b_prefetch,
                LIBXSMM_X86_GP_REG_UNDEF, 0,
                ((l_n * i_xgemm_desc->ldc) + (l_m * (i_micro_kernel_config->vector_length))) * (i_micro_kernel_config->datatype_size));
          }
        }
      }
    }
  } else if ( ( (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CORE) || (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_KNM) ||
                (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CLX)  || (i_micro_kernel_config->instruction_set == LIBXSMM_X86_AVX512_CPX) ) &&
              ( (LIBXSMM_GEMM_PRECISION_BF16 == LIBXSMM_GETENUM_INP( i_xgemm_desc->datatype ) ) && (LIBXSMM_GEMM_PRECISION_BF16 == LIBXSMM_GETENUM_OUT( i_xgemm_desc->datatype ) ) ) ) {
#if 0
    /* push 0x7f800000 on the stack, naninf masking */
    libxsmm_x86_instruction_alu_imm( io_generated_code, LIBXSMM_X86_INSTR_MOVQ, i_gp_reg_mapping->gp_reg_help_5, 0x7f800000);
    libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_5 );

    /* push 0x00010000 on the stack, fixup masking */
    libxsmm_x86_instruction_alu_imm( io_generated_code, LIBXSMM_X86_INSTR_MOVQ, i_gp_reg_mapping->gp_reg_help_5, 0x00010000);
    libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_5 );

    /* push 0x00007fff on the stack, rneadd */
    libxsmm_x86_instruction_alu_imm( io_generated_code, LIBXSMM_X86_INSTR_MOVQ, i_gp_reg_mapping->gp_reg_help_5, 0x00007fff);
    libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_5 );

    /* push 0x00000001 on the stack, fixup */
    libxsmm_x86_instruction_alu_imm( io_generated_code, LIBXSMM_X86_INSTR_MOVQ, i_gp_reg_mapping->gp_reg_help_5, 0x00000001);
    libxsmm_x86_instruction_push_reg( io_generated_code, i_gp_reg_mapping->gp_reg_help_5 );
#endif

    /* storing downconverted and rounded C accumulator */
    for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
      for ( l_m = 0; l_m < l_m_blocking; l_m++ ) {
        unsigned int reg_X = l_vec_reg_acc_start + l_m + (l_m_blocking * l_n);

        libxsmm_x86_instruction_vec_shuffle_reg(io_generated_code,
            i_micro_kernel_config->instruction_set,
            LIBXSMM_X86_INSTR_VPSRAD,
            i_micro_kernel_config->vector_name,
            reg_X,
            reg_X,
            LIBXSMM_X86_VEC_REG_UNDEF,
            16);

        libxsmm_x86_instruction_vec_compute_convert( io_generated_code,
            i_micro_kernel_config->instruction_set,
            LIBXSMM_X86_INSTR_VPMOVDW,
            i_micro_kernel_config->vector_name,
            reg_X, LIBXSMM_X86_VEC_REG_UNDEF,
            0,
            LIBXSMM_X86_VEC_REG_UNDEF);

        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            l_vstore,
            i_gp_reg_mapping->gp_reg_c,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            ((l_n * i_xgemm_desc->ldc) + (l_m * (i_micro_kernel_config->vector_length))) * (i_micro_kernel_config->datatype_size/2),
            'y',
            0, i_micro_kernel_config->use_masking_a_c, 0, 1 );
      }

      if ( i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_BL2_VIA_C ||
           i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C ||
           i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C_AHEAD ||
           i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C_JPST)  {
        if ( (i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_TRANS_B) == 0 ) {
          /* determining how many prefetches we need in M direction as we just need one prefetch per cache line */
          unsigned int l_m_advance = 64 / ((i_micro_kernel_config->vector_length) * (i_micro_kernel_config->datatype_size/2)); /* 64: hardcoded cache line length */

          for (l_m = 0; l_m < l_m_blocking; l_m += l_m_advance ) {
            libxsmm_x86_instruction_prefetch( io_generated_code,
                i_micro_kernel_config->prefetch_instruction,
                i_gp_reg_mapping->gp_reg_b_prefetch,
                LIBXSMM_X86_GP_REG_UNDEF, 0,
                ((l_n * i_xgemm_desc->ldc) + (l_m * (i_micro_kernel_config->vector_length))) * (i_micro_kernel_config->datatype_size/2));
          }
        }
      }
    }
  } else {
    /* storing C accumulator */
    for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
      for ( l_m = 0; l_m < l_m_blocking; l_m++ ) {
        libxsmm_x86_instruction_vec_move( io_generated_code,
            i_micro_kernel_config->instruction_set,
            l_vstore,
            i_gp_reg_mapping->gp_reg_c,
            LIBXSMM_X86_GP_REG_UNDEF, 0,
            ((l_n * i_xgemm_desc->ldc) + (l_m * (i_micro_kernel_config->vector_length))) * (i_micro_kernel_config->datatype_size),
            i_micro_kernel_config->vector_name,
            l_vec_reg_acc_start + l_m + (l_m_blocking * l_n), i_micro_kernel_config->use_masking_a_c, 0, 1 );
      }

      if ( i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_BL2_VIA_C ||
           i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C ||
           i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C_AHEAD ||
           i_xgemm_desc->prefetch == LIBXSMM_GEMM_PREFETCH_AL2BL2_VIA_C_JPST)  {
        if ( (i_xgemm_desc->flags & LIBXSMM_GEMM_FLAG_TRANS_B) == 0 ) {
          /* determining how many prefetches we need in M direction as we just need one prefetch per cache line */
          unsigned int l_m_advance = 64 / ((i_micro_kernel_config->vector_length) * (i_micro_kernel_config->datatype_size)); /* 64: hardcoded cache line length */

          for (l_m = 0; l_m < l_m_blocking; l_m += l_m_advance ) {
            libxsmm_x86_instruction_prefetch( io_generated_code,
                i_micro_kernel_config->prefetch_instruction,
                i_gp_reg_mapping->gp_reg_b_prefetch,
                LIBXSMM_X86_GP_REG_UNDEF, 0,
                ((l_n * i_xgemm_desc->ldc) + (l_m * (i_micro_kernel_config->vector_length))) * (i_micro_kernel_config->datatype_size));
          }
        }
      }
    }
  }
}

