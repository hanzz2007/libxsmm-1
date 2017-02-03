/******************************************************************************
** Copyright (c) 2016-2017, Intel Corporation                                **
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
/* Rajkishore Barik, Alexander Heinecke (Intel Corp.)
******************************************************************************/

int imgifm1, img, ofm1, ifm1, oj, ij, ii, kj, ki, ifm2, ofm2, kh, kw, ifm1ofm1, ifh, ofh;

/* computing first logical thread */
const int ltid = tid - start_thread;
/* number of tasks that could be run in parallel */
const int work = handle->desc.N * handle->blocksifm;
/* compute chunck size */
const int chunksize = (work % handle->desc.threads == 0) ? (work / handle->desc.threads) : ((work / handle->desc.threads) + 1);
/* compute thr_begin and thr_end */
const int thr_begin = (ltid * chunksize < work) ? (ltid * chunksize) : work;
const int thr_end = ((ltid + 1) * chunksize < work) ? ((ltid + 1) * chunksize) : work;

/* number of tasks that could be run in parallel */
const int transpose_work = handle->blocksofm * handle->blocksifm;
/* compute chunck size */
const int transpose_chunksize = (transpose_work % handle->desc.threads == 0) ? (transpose_work / handle->desc.threads) : ((transpose_work / handle->desc.threads) + 1);
/* compute thr_begin and thr_end */
const int transpose_thr_begin = (ltid * transpose_chunksize < transpose_work) ? (ltid * transpose_chunksize) : transpose_work;
const int transpose_thr_end = ((ltid + 1) * transpose_chunksize < transpose_work) ? ((ltid + 1) * transpose_chunksize) : transpose_work;

element_output_type *const out = ((element_output_type*)handle->grad_output->data) + (handle->desc.pad_h_out * handle->ofwp + handle->desc.pad_w_out) * handle->ofmblock;
LIBXSMM_VLA_DECL(5, element_output_type, del_out, out, handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock);
LIBXSMM_VLA_DECL(5, element_input_type, del_input, (element_input_type*)handle->grad_input->data, handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
LIBXSMM_VLA_DECL(6, element_filter_type, wt, (element_filter_type*)handle->reg_filter->data, handle->blocksifm, handle->desc.R, handle->desc.S, handle->ifmblock, handle->ofmblock);
LIBXSMM_VLA_DECL(6, element_filter_type, tr_wt, (element_filter_type*)handle->scratch1, handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock);

/* avoid warning by using the xconv.sconv sequence to get some fn. ptr. to act as source of the type-cast */
libxsmm_convfunction jitted_conv_bp_no_pf = (libxsmm_convfunction)handle->code_bwd[0].xconv.sconv;
#if defined(LIBXSMM_CONV_NO_PREFETCH)
libxsmm_convfunction jitted_conv_bp_peeled_no_pf = (libxsmm_convfunction)handle->code_bwd[2].xconv.sconv;
#else
libxsmm_convfunction jitted_conv_bp_pf = (libxsmm_convfunction)handle->code_bwd[1].xconv.sconv;
libxsmm_convfunction jitted_conv_bp_peeled_noweight_pf = (libxsmm_convfunction)handle->code_bwd[3].xconv.sconv;
#endif

/*element_input_type *l_input;*/
/*element_filter_type *l_wt;*/
/*element_output_type* l_output;*/

#if defined(INPUT_PADDING)
/* Variables and initializations related to padding */
int iii;
#if defined(__AVX512F__)
element_input_type *prefetch_ptr;
#endif
const int padded_h = handle->ifhp + 2 * handle->desc.pad_h;
const int padded_w = handle->ifwp + 2 * handle->desc.pad_w;
#if defined(__AVX512F__) || defined(__AVX__)
element_input_type (* __restrict input_ptr);
element_input_type (* __restrict copy_ptr);
const int big_block_size = padded_w * handle->ifmblock;
const int block_size = handle->ifwp * handle->ifmblock;
#endif
LIBXSMM_VLA_DECL(3, element_input_type, input_buffer, ((element_input_type*)handle->scratch5) + ltid * padded_h * padded_w * handle->ifmblock, padded_w, handle->ifmblock);
LIBXSMM_VLA_DECL(3, element_input_type, input_to_use, input_buffer, padded_w, handle->ifmblock);
const size_t small_block_size = handle->ifwp * handle->ifmblock * libxsmm_dnn_typesize(handle->datatype) * 8;
#if defined(__AVX512F__) || defined(__AVX__)
copy_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(3, input_buffer, handle->desc.pad_h, handle->desc.pad_w, 0, padded_w, handle->ifmblock);
#endif
memset(&LIBXSMM_VLA_ACCESS(3, input_buffer, 0, 0, 0, padded_w, handle->ifmblock), 0,
       padded_w * padded_h * handle->ifmblock * sizeof(element_input_type));
/* Based on the input datatype select the right intrinsics */
#ifdef INPUT_F32

#if defined(__AVX512F__)
#define LOAD(x)             _mm512_load_ps(x)
#define STORE(x,y)          _mm512_store_ps(x,y)
#endif

#if defined(__AVX__)
#define LOAD_256(x)         _mm256_load_ps(x)
#define STORE_256(x,y)      _mm256_store_ps(x,y)
#endif
#define CHUNK_SIZE          16

#endif

#ifdef INPUT_I16

#if defined(__AVX512F__)
#define LOAD(x)             _mm512_load_si512 (x)
#define STORE(x,y)          _mm512_store_si512(x,y)
#endif

#if defined(__AVX__)
#define LOAD_256(x)         _mm256_load_si256((__m256i const *)x)
#define STORE_256(x,y)      _mm256_store_si256((__m256i*)x,y)
#endif
#define CHUNK_SIZE          32

#endif

#ifdef INPUT_I8

#if defined(__AVX512F__)
#define LOAD(x)             _mm512_load_si512 (x)
#define STORE(x,y)          _mm512_store_si512(x,y)
#endif

#if defined(__AVX__)
#define LOAD_256(x)         _mm256_load_si256((__m256i const *)x)
#define STORE_256(x,y)      _mm256_store_si256((__m256i*)x,y)
#endif
#define CHUNK_SIZE          64

#endif

#else
LIBXSMM_VLA_DECL(5, element_input_type, input_to_use, del_input, handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
#endif

kh = handle->desc.R;
kw = handle->desc.S;

#if defined(INPUT_PADDING)
ifh=handle->ifhp + 2 * handle->desc.pad_h;
#else
ifh=handle->ifhp;
#endif
ofh=handle->ofh;

/* lazy barrier init */
libxsmm_barrier_init(handle->barrier, ltid);

for (ifm1ofm1 = transpose_thr_begin; ifm1ofm1 < transpose_thr_end; ++ifm1ofm1) {
  ofm1 = ifm1ofm1/handle->blocksifm;
  ifm1 = ifm1ofm1%handle->blocksifm;
  for (kj=0; kj < kh; ++kj) {
    for (ki=0; ki < kw; ++ki) {
      /* TODO: enable this later */
      /*transpose<VLEN,VLEN>(&wt[ofm1][ifm1][kj][ki][0][0],&tr_wt[ofm1][ifm1][kj][ki][0][0]);*/
      for (ofm2 = 0; ofm2 < handle->ofmblock; ++ofm2) {
        for (ifm2 = 0; ifm2 < handle->ifmblock; ++ifm2) {
          LIBXSMM_VLA_ACCESS(6, tr_wt, ofm1, ifm1, kj, ki, ofm2, ifm2, handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock) =
            LIBXSMM_VLA_ACCESS(6, wt, ofm1, ifm1, kj, ki, ifm2, ofm2, handle->blocksifm, handle->desc.R, handle->desc.S, handle->ifmblock, handle->ofmblock);
        }
      }
    }
  }
}
libxsmm_barrier_wait(handle->barrier, ltid);

#if defined(INPUT_PADDING)

#define LIBXSMM_JITTED_CONV_BP_PF(del_input, i_img, i_ifm1, i_ij, i_ii, i_ifm2, \
                                  tr_wt, w_ofm1, w_ifm1, w_kj, w_ki, w_ofm2, w_ifm2, \
                                  del_out, o_img, o_ofm1, o_oj, o_oi, o_ofm2, \
                                  pf_del_input, pi_img, pi_ifm1, pi_ij, pi_ii, pi_ifm2, \
                                  pf_tr_wt, pw_ofm1, pw_ifm1, pw_kj, pw_ki, pw_ofm2, pw_ifm2, \
                                  pf_del_out, po_img, po_ofm1, po_oj, po_oi, po_ofm2) \
                    jitted_conv_bp_pf(  \
                        &LIBXSMM_VLA_ACCESS(3, del_input, (i_ij), (i_ii), (i_ifm2), padded_w, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(6, tr_wt, (w_ofm1), (w_ifm1), (w_kj), (w_ki), (w_ofm2), (w_ifm2), handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(5, del_out, (o_img), (o_ofm1), (o_oj), (o_oi), (o_ofm2), handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock), \
                        &LIBXSMM_VLA_ACCESS(3, pf_del_input, (pi_ij), (pi_ii), (pi_ifm2), padded_w, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(6, pf_tr_wt, (pw_ofm1), (pw_ifm1), (pw_kj), (pw_ki), (pw_ofm2), (pw_ifm2), handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(5, pf_del_out, po_img, po_ofm1, po_oj, po_oi, po_ofm2, handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock) \
                       )

#define LIBXSMM_JITTED_CONV_BP_NO_PF(del_input, i_img, i_ifm1, i_ij, i_ii, i_ifm2, \
                                  tr_wt, w_ofm1, w_ifm1, w_kj, w_ki, w_ofm2, w_ifm2, \
                                  del_out, o_img, o_ofm1, o_oj, o_oi, o_ofm2) \
                    jitted_conv_bp_no_pf(  \
                        &LIBXSMM_VLA_ACCESS(3, del_input, (i_ij), (i_ii), (i_ifm2), padded_w, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(6, tr_wt, (w_ofm1), (w_ifm1), (w_kj), (w_ki), (w_ofm2), (w_ifm2), handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(5, del_out, (o_img), (o_ofm1), (o_oj), (o_oi), (o_ofm2), handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock), \
                        NULL, \
                        NULL, \
                        NULL \
                       )
#define LIBXSMM_JITTED_CONV_BP_PEELED_NOWEIGHT_PF(del_input, i_img, i_ifm1, i_ij, i_ii, i_ifm2, \
                                  tr_wt, w_ofm1, w_ifm1, w_kj, w_ki, w_ofm2, w_ifm2, \
                                  del_out, o_img, o_ofm1, o_oj, o_oi, o_ofm2, \
                                  pf_del_input, pi_img, pi_ifm1, pi_ij, pi_ii, pi_ifm2, \
                                  pf_del_out, po_img, po_ofm1, po_oj, po_oi, po_ofm2) \
                    jitted_conv_bp_peeled_noweight_pf(  \
                        &LIBXSMM_VLA_ACCESS(3, del_input, (i_ij), (i_ii), (i_ifm2), padded_w, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(6, tr_wt, (w_ofm1), (w_ifm1), (w_kj), (w_ki), (w_ofm2), (w_ifm2), handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(5, del_out, (o_img), (o_ofm1), (o_oj), (o_oi), (o_ofm2), handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock), \
                        &LIBXSMM_VLA_ACCESS(3, pf_del_input, (pi_ij), (pi_ii), (pi_ifm2), padded_w, handle->ifmblock), \
                        NULL,   \
                        &LIBXSMM_VLA_ACCESS(5, pf_del_out, (po_img), (po_ofm1), (po_oj), (po_oi), (po_ofm2), handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock) \
                       )
#define LIBXSMM_JITTED_CONV_BP_PEELED_NO_PF(del_input, i_img, i_ifm1, i_ij, i_ii, i_ifm2, \
                                  tr_wt, w_ofm1, w_ifm1, w_kj, w_ki, w_ofm2, w_ifm2, \
                                  del_out, o_img, o_ofm1, o_oj, o_oi, o_ofm2) \
                    jitted_conv_bp_peeled_no_pf(  \
                        &LIBXSMM_VLA_ACCESS(3, del_input, (i_ij), (i_ii), (i_ifm2), padded_w, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(6, tr_wt, (w_ofm1), (w_ifm1), (w_kj), (w_ki), (w_ofm2), (w_ifm2), handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(5, del_out, (o_img), (o_ofm1), (o_oj), (o_oi), (o_ofm2), handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock), \
                        NULL, \
                        NULL, \
                        NULL \
                       )

#else

#define LIBXSMM_JITTED_CONV_BP_PF(del_input, i_img, i_ifm1, i_ij, i_ii, i_ifm2, \
                                  tr_wt, w_ofm1, w_ifm1, w_kj, w_ki, w_ofm2, w_ifm2, \
                                  del_out, o_img, o_ofm1, o_oj, o_oi, o_ofm2, \
                                  pf_del_input, pi_img, pi_ifm1, pi_ij, pi_ii, pi_ifm2, \
                                  pf_tr_wt, pw_ofm1, pw_ifm1, pw_kj, pw_ki, pw_ofm2, pw_ifm2, \
                                  pf_del_out, po_img, po_ofm1, po_oj, po_oi, po_ofm2) \
                    jitted_conv_bp_pf(  \
                        &LIBXSMM_VLA_ACCESS(5, del_input, (i_img), (i_ifm1), (i_ij), (i_ii), (i_ifm2), handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(6, tr_wt, (w_ofm1), (w_ifm1), (w_kj), (w_ki), (w_ofm2), (w_ifm2), handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(5, del_out, (o_img), (o_ofm1), (o_oj), (o_oi), (o_ofm2), handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock), \
                        &LIBXSMM_VLA_ACCESS(5, pf_del_input, (pi_img), (pi_ifm1), (pi_ij), (pi_ii), (pi_ifm2), handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(6, pf_tr_wt, (pw_ofm1), (pw_ifm1), (pw_kj), (pw_ki), (pw_ofm2), (pw_ifm2), handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(5, pf_del_out, po_img, po_ofm1, po_oj, po_oi, po_ofm2, handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock) \
                      )

#define LIBXSMM_JITTED_CONV_BP_NO_PF(del_input, i_img, i_ifm1, i_ij, i_ii, i_ifm2, \
                                    tr_wt, w_ofm1, w_ifm1, w_kj, w_ki, w_ofm2, w_ifm2, \
                                    del_out, o_img, o_ofm1, o_oj, o_oi, o_ofm2) \
                    jitted_conv_bp_no_pf(  \
                        &LIBXSMM_VLA_ACCESS(5, del_input, (i_img), (i_ifm1), (i_ij), (i_ii), (i_ifm2), handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(6, tr_wt, (w_ofm1), (w_ifm1), (w_kj), (w_ki), (w_ofm2), (w_ifm2), handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock), \
                        &LIBXSMM_VLA_ACCESS(5, del_out, (o_img), (o_ofm1), (o_oj), (o_oi), (o_ofm2), handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock), \
                        NULL, \
                        NULL, \
                        NULL \
                      )
#define LIBXSMM_JITTED_CONV_BP_PEELED_NOWEIGHT_PF(del_input, i_img, i_ifm1, i_ij, i_ii, i_ifm2, \
                                                  tr_wt, w_ofm1, w_ifm1, w_kj, w_ki, w_ofm2, w_ifm2, \
                                                  del_out, o_img, o_ofm1, o_oj, o_oi, o_ofm2, \
                                                  pf_del_input, pi_img, pi_ifm1, pi_ij, pi_ii, pi_ifm2, \
                                                  pf_del_out, po_img, po_ofm1, po_oj, po_oi, po_ofm2) \
                  jitted_conv_bp_peeled_noweight_pf(  \
                      &LIBXSMM_VLA_ACCESS(5, del_input, (i_img), (i_ifm1), (i_ij), (i_ii), (i_ifm2), handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock), \
                      &LIBXSMM_VLA_ACCESS(6, tr_wt, (w_ofm1), (w_ifm1), (w_kj), (w_ki), (w_ofm2), (w_ifm2), handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock), \
                      &LIBXSMM_VLA_ACCESS(5, del_out, (o_img), (o_ofm1), (o_oj), (o_oi), (o_ofm2), handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock), \
                      &LIBXSMM_VLA_ACCESS(5, pf_del_input, (pi_img), (pi_ifm1), (pi_ij), (pi_ii), (pi_ifm2), handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock), \
                      NULL,   \
                      &LIBXSMM_VLA_ACCESS(5, pf_del_out, (po_img), (po_ofm1), (po_oj), (po_oi), (po_ofm2), handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock) \
                    )
#define LIBXSMM_JITTED_CONV_BP_PEELED_NO_PF(del_input, i_img, i_ifm1, i_ij, i_ii, i_ifm2, \
                                            tr_wt, w_ofm1, w_ifm1, w_kj, w_ki, w_ofm2, w_ifm2, \
                                            del_out, o_img, o_ofm1, o_oj, o_oi, o_ofm2) \
                  jitted_conv_bp_peeled_no_pf(  \
                      &LIBXSMM_VLA_ACCESS(5, del_input, (i_img), (i_ifm1), (i_ij), (i_ii), (i_ifm2), handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock), \
                      &LIBXSMM_VLA_ACCESS(6, tr_wt, (w_ofm1), (w_ifm1), (w_kj), (w_ki), (w_ofm2), (w_ifm2), handle->blocksifm, handle->desc.R, handle->desc.S, handle->ofmblock, handle->ifmblock), \
                      &LIBXSMM_VLA_ACCESS(5, del_out, (o_img), (o_ofm1), (o_oj), (o_oi), (o_ofm2), handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock), \
                      NULL, \
                      NULL, \
                      NULL \
                    )
#endif

if ( libxsmm_target_archid == LIBXSMM_X86_AVX512_MIC  ||
     libxsmm_target_archid == LIBXSMM_X86_AVX512_CORE ) {
  /* special casing for ifh < 2*kh scenario where the loop peeling does not work */
  if (ifh <= 2*kh) {
    for (imgifm1 = thr_begin; imgifm1 < thr_end; ++imgifm1) {
      img = imgifm1/handle->blocksifm;
      ifm1 = imgifm1%handle->blocksifm;

#if defined(INPUT_PADDING)

#if defined(__AVX512F__)
      input_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, 0, 0, 0,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);

      if (ifm1+1 != handle->blocksifm) {
        /* Prefetch next ifm, same image */
        prefetch_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1+1, 0, 0, 0, handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
      } else {
        /* Prefetch ifm 0 from next image */
        prefetch_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(5, del_input, img+1, 0, 0, 0, 0, handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
      }
#endif

      if (small_block_size % 512 == 0) {
        for (oj = 0; oj < handle->ifhp; oj++) {
#if defined(__AVX512F__)
          for (ij = 0; ij < block_size; ij += CHUNK_SIZE) {
            STORE(&copy_ptr[ij+oj*big_block_size], LOAD(&input_ptr[ij+oj*block_size]));
            _mm_prefetch((const char*)&prefetch_ptr[ij+oj*block_size], _MM_HINT_T1);
          }
#else
          for (ij = 0; ij < handle->ifwp; ij++) {
            for (iii = 0; iii < handle->ifmblock; iii++) {
              LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock)
              = LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
            }
          }
#endif
        }
      } else {
        for (oj = 0; oj < handle->ifhp; oj++) {
          for (ij = 0; ij < handle->ifwp; ij++) {
            for (iii = 0; iii < handle->ifmblock; iii++) {
               LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock)
               = LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
            }
#if defined(__AVX512F__)
            _mm_prefetch((const char*)&prefetch_ptr[ij*handle->ifmblock +oj*block_size], _MM_HINT_T1);
#endif
          }
        }
      }
#endif

      for (ofm1 = 0; ofm1 < handle->blocksofm; ++ofm1) {
        for (ij= 0 ; ij < ifh; ++ij) {
          for (kj=0; kj < kh; ++kj) {
            oj = ij - kh + kj + 1;
            if (oj >= 0 && oj < ofh) {
              LIBXSMM_JITTED_CONV_BP_NO_PF(
                input_to_use, img, ifm1, ij, 0, 0,
                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                del_out, img, ofm1, oj, 0, 0);
            }
          }
        }
      }

#if defined(INPUT_PADDING)
      if (small_block_size % 512 == 0) {
        for (oj = 0; oj < handle->ifhp; oj++) {
#if defined(__AVX512F__)
          for (ij = 0; ij < block_size; ij += CHUNK_SIZE) {
            STORE(&input_ptr[ij+oj*block_size], LOAD(&copy_ptr[ij+oj*big_block_size]));
          }
#else
          for (ij = 0; ij < handle->ifwp; ij++) {
            for (iii = 0; iii < handle->ifmblock; iii++) {
              LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock)
              = LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock);
            }
          }
#endif
        }
      } else {
        for (oj = 0; oj < handle->ifhp; oj++) {
          for (ij = 0; ij < handle->ifwp; ij++) {
            for (iii = 0; iii < handle->ifmblock; iii++) {
              LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock)
              = LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock);
            }
          }
        }
      }
#endif

#if !defined(INPUT_PADDING)
#include "libxsmm_dnn_zero_rim_st_input_custom.tpl.c"
#endif
    }
  } else {
    for (imgifm1 = thr_begin; imgifm1 < thr_end; ++imgifm1) {
      img = imgifm1/handle->blocksifm;
      ifm1 = imgifm1%handle->blocksifm;

#if defined(INPUT_PADDING)

#if defined(__AVX512F__)
      input_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, 0, 0, 0,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
      if (ifm1+1 != handle->blocksifm) {
        /* Prefetch next ifm, same image */
        prefetch_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1+1, 0, 0, 0, handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
      } else {
        /* Prefetch ifm 0 from next image */
        prefetch_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(5, del_input, img+1, 0, 0, 0, 0, handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
      }
#endif

      if (small_block_size % 512 == 0) {
        for (oj = 0; oj < handle->ifhp; oj++) {
#if defined(__AVX512F__)
          for (ij = 0; ij < block_size; ij += CHUNK_SIZE) {
            _mm512_store_ps(&copy_ptr[ij+oj*big_block_size], _mm512_load_ps(&input_ptr[ij+oj*block_size]));
            _mm_prefetch((const char*)&prefetch_ptr[ij+oj*block_size], _MM_HINT_T1);
          }
#else
          for (ij = 0; ij < handle->ifwp; ij++) {
            for (iii = 0; iii < handle->ifmblock; iii++) {
              LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock)
              = LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
            }
          }
#endif
        }
      } else {
        for (oj = 0; oj < handle->ifhp; oj++) {
          for (ij = 0; ij < handle->ifwp; ij++) {
            for (iii = 0; iii < handle->ifmblock; iii++) {
              LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock)
              = LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
            }
#if defined(__AVX512F__)
            _mm_prefetch((const char*)&prefetch_ptr[ij*handle->ifmblock +oj*block_size], _MM_HINT_T1);
#endif
          }
        }
      }
#endif

      for (ofm1 = 0; ofm1 < handle->blocksofm; ++ofm1) {
#if !defined(LIBXSMM_CONV_NO_PREFETCH)
        /* NON PEELED PROLOGUE VERSION */
        if ((kh == 3) && (kw == 3)) { /* 3x3 convolution */

          /* Unroll 1 */
          /* prefetch ij=2; kj=0 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+2][0][0]), &(tr_wt[ofm1][ifm1][kh-0-1][0][0][0]), &(del_out[img][ofm1][ij+2-kh+0+1][0][0]));*/
          ij = 0; kj=2, oj=ij-kh+kj+1;
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+2, 0, 0,
                                tr_wt, ofm1, ifm1, kh-0-1, 0, 0, 0,
                                del_out, img, ofm1, ij+2-kh+0+1, 0, 0
                        );

          /* Unroll 2 */
          ij = 1; kj=1, oj=ij-kh+kj+1;
          /* prefetch ij=2; kj=1 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+1][0][0]), &(tr_wt[ofm1][ifm1][kh-1-1][0][0][0]), &(del_out[img][ofm1][ij+1-kh+1+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+1, 0, 0,
                                tr_wt, ofm1, ifm1, kh-1-1, 0, 0, 0,
                                del_out, img, ofm1, ij+1-kh+1+1, 0, 0
                        );

          /* Unroll 3 */
          ij = 1; kj=2, oj=ij-kh+kj+1;
          /* prefetch for ij = 2 and kj=2 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+1][0][0]), &(tr_wt[ofm1][ifm1][kh-2-1][0][0][0]), &(del_out[img][ofm1][ij+1-kh+2+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+1, 0, 0,
                                tr_wt, ofm1, ifm1, kh-2-1, 0, 0, 0,
                                del_out, img, ofm1, ij+1-kh+2+1, 0, 0
                        );
        } else if ((kh == 5) && (kw == 5)) { /* kh=5 */
          /* Unroll 1 */
          ij = 0; kj=4; oj=ij-kh+kj+1;
          /* prefetch for ij= 2 kj=4 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+2][0][0]), &(tr_wt[ofm1][ifm1][kh-4-1][0][0][0]), &(del_out[img][ofm1][ij+2-kh+4+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+2, 0, 0,
                                tr_wt, ofm1, ifm1, kh-4-1, 0, 0, 0,
                                del_out, img, ofm1, ij+2-kh+4+1, 0, 0
                        );

          /* Unroll 2 */
          ij = 1; kj=3; oj=ij-kh+kj+1;
          /* prefetch for ij= 3 kj=1 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+2][0][0]), &(tr_wt[ofm1][ifm1][kh-1-1][0][0][0]), &(del_out[img][ofm1][ij+2-kh+1+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+2, 0, 0,
                                tr_wt, ofm1, ifm1, kh-1-1, 0, 0, 0,
                                del_out, img, ofm1, ij+2-kh+1+1, 0, 0
                        );

          /* Unroll 3 */
          ij = 1; kj=4; oj=ij-kh+kj+1;
          /* prefetch for ij= 3 kj=2 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+2][0][0]), &(tr_wt[ofm1][ifm1][kh-2-1][0][0][0]), &(del_out[img][ofm1][ij+2-kh+2+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+2, 0, 0,
                                tr_wt, ofm1, ifm1, kh-2-1, 0, 0, 0,
                                del_out, img, ofm1, ij+2-kh+2+1, 0, 0
                        );

          /* Unroll 4 */
          ij = 2; kj=2; oj=ij-kh+kj+1;
          /* prefetch for ij= 3 kj=3 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+1][0][0]), &(tr_wt[ofm1][ifm1][kh-3-1][0][0][0]), &(del_out[img][ofm1][ij+1-kh+3+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+1, 0, 0,
                                tr_wt, ofm1, ifm1, kh-3-1, 0, 0, 0,
                                del_out, img, ofm1, ij+1-kh+3+1, 0, 0
                        );

          /* Unroll 5 */
          /* prefetch for ij= 3 kj=4 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+1][0][0]), &(tr_wt[ofm1][ifm1][kh-4-1][0][0][0]), &(del_out[img][ofm1][ij+1-kh+4+1][0][0]));*/
          ij = 2; kj=3; oj=ij-kh+kj+1;
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+1, 0, 0,
                                tr_wt, ofm1, ifm1, kh-4-1, 0, 0, 0,
                                del_out, img, ofm1, ij+1-kh+4+1, 0, 0
                        );

          /* Unroll 6 */
          ij = 2; kj=4; oj=ij-kh+kj+1;
          /* prefetch for ij= 4 kj=0 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+2][0][0]), &(tr_wt[ofm1][ifm1][kh-0-1][0][0][0]), &(del_out[img][ofm1][ij+2-kh+0+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+2, 0, 0,
                                tr_wt, ofm1, ifm1, kh-0-1, 0, 0, 0,
                                del_out, img, ofm1, ij+2-kh+0+1, 0, 0
                        );


          /* Unroll 7 */
          ij = 3; kj=1; oj=ij-kh+kj+1;
          /* prefetch for ij= 4 kj=1 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+1][0][0]), &(tr_wt[ofm1][ifm1][kh-1-1][0][0][0]), &(del_out[img][ofm1][ij+1-kh+1+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+1, 0, 0,
                                tr_wt, ofm1, ifm1, kh-1-1, 0, 0, 0,
                                del_out, img, ofm1, ij+1-kh+1+1, 0, 0
                        );

          /* Unroll 8 */
          ij = 3; kj=2; oj=ij-kh+kj+1;
          /* prefetch for ij= 4 kj=2 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+1][0][0]), &(tr_wt[ofm1][ifm1][kh-2-1][0][0][0]), &(del_out[img][ofm1][ij+1-kh+2+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+1, 0, 0,
                                tr_wt, ofm1, ifm1, kh-2-1, 0, 0, 0,
                                del_out, img, ofm1, ij+1-kh+2+1, 0, 0
                        );

          /* Unroll 9 */
          ij = 3; kj=3; oj=ij-kh+kj+1;
          /* prefetch for ij= 4 kj=3 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+1][0][0]), &(tr_wt[ofm1][ifm1][kh-3-1][0][0][0]), &(del_out[img][ofm1][ij+1-kh+3+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+1, 0, 0,
                                tr_wt, ofm1, ifm1, kh-3-1, 0, 0, 0,
                                del_out, img, ofm1, ij+1-kh+3+1, 0, 0
                        );

          /* Unroll 10 */
          ij = 3; kj=4; oj=ij-kh+kj+1;
          /* prefetch for ij= 4 kj=4 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+1][0][0]), &(tr_wt[ofm1][ifm1][kh-4-1][0][0][0]), &(del_out[img][ofm1][ij+1-kh+4+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ij+1, 0, 0,
                                tr_wt, ofm1, ifm1, kh-4-1, 0, 0, 0,
                                del_out, img, ofm1, ij+1-kh+4+1, 0, 0
                        );
        } else {
          for (ij=0; ij < kh-1; ij++) {
            for (kj=0; kj < kh; kj++) {
              oj = ij - kh + kj + 1;
              if (oj >=0) {
                LIBXSMM_JITTED_CONV_BP_NO_PF(
                                         input_to_use, img, ifm1, ij, 0, 0,
                                         tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                         del_out, img, ofm1, oj, 0, 0);
              }
            }
          }
        }

        /* PEELED INNERMOST VERSION */
        for (ij=kh-1; ij < ifh-kh+1; ij++) {
          /*jitted_conv_bp_peeled_noweight_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ij+1][0][0]), NULL, &(del_out[img][ofm1][ij+1-kh+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PEELED_NOWEIGHT_PF(
                    input_to_use, img, ifm1, ij, 0, 0,
                    tr_wt, ofm1, ifm1, kh-1, 0, 0, 0,
                    del_out, img, ofm1, ij-kh+1, 0, 0,
                    input_to_use, img, ifm1, ij+1, 0, 0,
                    del_out, img, ofm1, ij+1-kh+1, 0, 0);
        }


        /* NON PEELED EPILOGUE VERSION */
        if ((kh==3) && (kw==3)) {
          if ( (ofm1+1 == handle->blocksofm) &&  (ifm1+1 == handle->blocksifm) ) { /* prefetch next img, kj=2, ij=0 */
            ij=ifh-2; kj=0; oj=ij-kh+kj+1; /* ifh-2-3+0+1 = ifh-4 */
            /* prefetch ij=0 and kj =2 */
            /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img+1][0][0][0][0]), &(tr_wt[0][0][kh-2-1][0][0][0]), &(del_out[img+1][0][-kh+2+1][0][0]));*/
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img+1, 0, 0, 0, 0,
                                tr_wt, 0, 0, kh-2-1, 0, 0, 0,
                                del_out, img+1, 0, -kh+2+1, 0, 0
                        );

            ij=ifh-2; kj=1; oj=ij-kh+kj+1; /* ifh-2-3+1+1 = ifh-3 */
            /* prefetch ij=1 and kj =1 */
            /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img+1][0][1][0][0]), &(tr_wt[0][0][kh-1-1][0][0][0]), &(del_out[img+1][0][1-kh+1+1][0][0]));*/
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img+1, 0, 0, 0, 0,
                                tr_wt, 0, 0, kh-1-1, 0, 0, 0,
                                del_out, img+1, 0, 1-kh+1+1, 0, 0
                        );

            ij=ifh-1; kj=0; oj=ij-kh+kj+1; /* ifh-1-3+0+1 = ifh-3 */
            /* prefetch ij=1, kj=2 */
            /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img+1][0][1][0][0]), &(tr_wt[0][0][kh-2-1][0][0][0]), &(del_out[img+1][0][1-kh+2+1][0][0]));*/
#if defined(INPUT_PADDING)
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img+1, 0, 0, 0, 0,
                                tr_wt, 0, 0, kh-2-1, 0, 0, 0,
                                del_out, img+1, 0, 1-kh+2+1, 0, 0
                        );

#else
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                      tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                      del_out, img, ofm1, oj, 0, 0,
                                      input_to_use, img+1, 0, 1, 0, 0,
                                      tr_wt, 0, 0, kh-2-1, 0, 0, 0,
                                      del_out, img+1, 0, 1-kh+2+1, 0, 0
                                      );
#endif

          } else {
            if (ofm1+1 == handle->blocksofm) { /* prefecth next ifm1, kj=2, ij = 0 */
              ij=ifh-2; kj=0; oj=ij-kh+kj+1; /* ifh-2-3+0+1 = ifh-4 */
              /* prefetch ij=0 and kj =2 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1+1][0][0][0]), &(tr_wt[0][ifm1+1][kh-2-1][0][0][0]), &(del_out[img][0][-kh+2+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1+1, 0, 0, 0,
                                tr_wt, ofm1, ifm1+1, kh-2-1, 0, 0, 0,
                                del_out, img, 0, -kh+2+1, 0, 0
                        );
              ij=ifh-2; kj=1; oj=ij-kh+kj+1; /* ifh-2-3+1+1 = ifh-3 */
              /* prefetch ij=1 and kj =1 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1+1][1][0][0]), &(tr_wt[0][ifm1+1][kh-1-1][0][0][0]), &(del_out[img][0][1-kh+1+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1+1, 0, 0, 0,
                                tr_wt, ofm1, ifm1+1, kh-1-1, 0, 0, 0,
                                del_out, img, 0, 1-kh+1+1, 0, 0
                        );
              ij=ifh-1; kj=0; oj=ij-kh+kj+1; /* ifh-1-3+0+1 = ifh-3 */
              /* prefetch ij=1, kj=2 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1+1][1][0][0]), &(tr_wt[0][ifm1+1][kh-2-1][0][0][0]), &(del_out[img][0][1-kh+2+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1+1, 0, 0, 0,
                                tr_wt, ofm1, ifm1+1,  kh-2-1, 0, 0, 0,
                                del_out, img, 0, 1-kh+2+1, 0, 0
                        );
            } else {
              ij=ifh-2; kj=0; oj=ij-kh+kj+1; /* ifh-2-3+0+1 = ifh-4 */
              /* prefetch ij=0 and kj =2 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][0][0][0]), &(tr_wt[ofm1+1][ifm1][kh-2-1][0][0][0]), &(del_out[img][ofm1+1][-kh+2+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, 0, 0, 0,
                                tr_wt, ofm1+1, ifm1,  kh-2-1, 0, 0, 0,
                                del_out, img, ofm1+1, -kh+2+1, 0, 0
                        );

              ij=ifh-2; kj=1; oj=ij-kh+kj+1; /* ifh-2-3+1+1 = ifh-3 */
              /* prefetch ij=1 and kj =1 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][1][0][0]), &(tr_wt[ofm1+1][ifm1][kh-1-1][0][0][0]), &(del_out[img][ofm1+1][1-kh+1+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, 1, 0, 0,
                                tr_wt, ofm1+1, ifm1,  kh-1-1, 0, 0, 0,
                                del_out, img, ofm1+1, 1-kh+1+1, 0, 0
                        );

              ij=ifh-1; kj=0; oj=ij-kh+kj+1; /* ifh-1-3+0+1 = ifh-3 */
              /* prefetch ij=1, kj=2 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][1][0][0]), &(tr_wt[ofm1+1][ifm1][kh-2-1][0][0][0]), &(del_out[img][ofm1+1][1-kh+2+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, 1, 0, 0,
                                tr_wt, ofm1+1, ifm1,  kh-2-1, 0, 0, 0,
                                del_out, img, ofm1+1, 1-kh+2+1, 0, 0
                        );
            } /* if (ofm1+1 == nBOfm) */
          } /* if ( (ofm1+1 == nBOfm) &&  (ifm1+1 == nBIfm) ) */
        } else if ((kh==5) && (kw==5))  {  /* kh =5 */
          /* Unroll 1 */
          ij = ifh-4; kj=0; oj=ij-kh+kj+1; /* ifh-8 */
          /* prefetch for ij=ifh-3 kj=1 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ifh-3][0][0]), &(tr_wt[ofm1][ifm1][kh-1-1][0][0][0]), &(del_out[img][ofm1][ifh-3-kh+1+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ifh-3, 0, 0,
                                tr_wt, ofm1, ifm1,  kh-1-1, 0, 0, 0,
                                del_out, img, ofm1, ifh-3-kh+1+1, 0, 0
                        );

          /* Unroll 2 */
          ij = ifh-4; kj=1; oj=ij-kh+kj+1;
          /* prefetch for ij=ifh-3 kj=2 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ifh-3][0][0]), &(tr_wt[ofm1][ifm1][kh-2-1][0][0][0]), &(del_out[img][ofm1][ifh-3-kh+2+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ifh-3, 0, 0,
                                tr_wt, ofm1, ifm1,  kh-2-1, 0, 0, 0,
                                del_out, img, ofm1, ifh-3-kh+2+1, 0, 0
                        );

          /* Unroll 3 */
          ij = ifh-4; kj=2; oj=ij-kh+kj+1;
          /* prefetch for ij=ifh-2 kj=0 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ifh-2][0][0]), &(tr_wt[ofm1][ifm1][kh-0-1][0][0][0]), &(del_out[img][ofm1][ifh-2-kh+0+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ifh-2, 0, 0,
                                tr_wt, ofm1, ifm1,  kh-0-1, 0, 0, 0,
                                del_out, img, ofm1, ifh-2-kh+0+1, 0, 0
                        );

          /* Unroll 4 */
          ij = ifh-4; kj=3; oj=ij-kh+kj+1;
          /* prefetch for ij=ifh-2 kj=1 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ifh-2][0][0]), &(tr_wt[ofm1][ifm1][kh-1-1][0][0][0]), &(del_out[img][ofm1][ifh-2-kh+1+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ifh-2, 0, 0,
                                tr_wt, ofm1, ifm1,  kh-1-1, 0, 0, 0,
                                del_out, img, ofm1, ifh-2-kh+1+1, 0, 0
                        );

          /* Unroll 5 */
          ij = ifh-3; kj=0; oj=ij-kh+kj+1; /* ifh-4 */
          /* prefetch for ij=ifh-1 kj=0 */
          /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][ifh-1][0][0]), &(tr_wt[ofm1][ifm1][kh-0-1][0][0][0]), &(del_out[img][ofm1][ifh-1-kh+0+1][0][0]));*/
          LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, ifh-1, 0, 0,
                                tr_wt, ofm1, ifm1,  kh-0-1, 0, 0, 0,
                                del_out, img, ofm1, ifh-1-kh+0+1, 0, 0
                        );


          if ( (ofm1+1 == handle->blocksofm) &&  (ifm1+1 == handle->blocksifm) ) { /* prefetch next img, kj=4, ij=0 */
            /* Unroll 6 */
            ij = ifh-3; kj=1; oj=ij-kh+kj+1;
            /* prefetch for ij=0 kj=4 */
            /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img+1][0][0][0][0]), &(tr_wt[0][0][kh-4-1][0][0][0]), &(del_out[img+1][0][-kh+4+1][0][0]));*/
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img+1, 0, 0, 0, 0,
                                tr_wt, 0, 0,  kh-4-1, 0, 0, 0,
                                del_out, img+1, 0, -kh+4+1, 0, 0
                        );

            /* Unroll 7 */
            ij = ifh-3; kj=2; oj=ij-kh+kj+1;
            /* prefetch for ij=1 kj=3 */
            /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img+1][0][1][0][0]), &(tr_wt[0][0][kh-3-1][0][0][0]), &(del_out[img+1][0][1-kh+3+1][0][0]));*/

#if defined(INPUT_PADDING)
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                      tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                      del_out, img, ofm1, oj, 0, 0,
                                      input_to_use, img+1, 0, 0, 0, 0,
                                      tr_wt, 0, 0,  kh-3-1, 0, 0, 0,
                                      del_out,  img+1, 0, 1-kh+3+1, 0, 0
                                      );
#else
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img+1, 0, 1, 0, 0,
                                tr_wt, 0, 0,  kh-3-1, 0, 0, 0,
                                del_out,  img+1, 0, 1-kh+3+1, 0, 0
                        );
#endif

            /* Unroll 8 */
            ij = ifh-2; kj=0; oj=ij-kh+kj+1;
            /* prefetch for ij=1 kj=4 */
            /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img+1][0][1][0][0]), &(tr_wt[0][0][kh-4-1][0][0][0]), &(del_out[img+1][0][1-kh+4+1][0][0]));*/
#if defined(INPUT_PADDING)
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                      tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                      del_out, img, ofm1, oj, 0, 0,
                                      input_to_use, img+1, 0, 0, 0, 0,
                                      tr_wt, 0, 0,  kh-4-1, 0, 0, 0,
                                      del_out,  img+1, 0, 1-kh+4+1, 0, 0
                                      );
#else
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img+1, 0, 1, 0, 0,
                                tr_wt, 0, 0,  kh-4-1, 0, 0, 0,
                                del_out,  img+1, 0, 1-kh+4+1, 0, 0
                        );
#endif

            /* Unroll 9 */
            ij = ifh-2; kj=1; oj=ij-kh+kj+1;
            /* prefetch for ij=2 kj=2 */
            /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img+1][0][2][0][0]), &(tr_wt[0][0][kh-2-1][0][0][0]), &(del_out[img+1][0][2-kh+2+1][0][0]));*/
#if defined(INPUT_PADDING)
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                      tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                      del_out, img, ofm1, oj, 0, 0,
                                      input_to_use, img+1, 0, 0, 0, 0,
                                      tr_wt, 0, 0,  kh-2-1, 0, 0, 0,
                                      del_out,  img+1, 0, 2-kh+2+1, 0, 0
                                      );
#else
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img+1, 0, 2, 0, 0,
                                tr_wt, 0, 0,  kh-2-1, 0, 0, 0,
                                del_out,  img+1, 0, 2-kh+2+1, 0, 0
                        );
#endif

            /* Unroll 10 */
            ij = ifh-1; kj=0; oj=ij-kh+kj+1;
            /* ij=2 kj=3 */
            /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img+1][0][2][0][0]), &(tr_wt[0][0][kh-3-1][0][0][0]), &(del_out[img+1][0][2-kh+3+1][0][0]));*/
#if defined(INPUT_PADDING)
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                      tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                      del_out, img, ofm1, oj, 0, 0,
                                      input_to_use, img+1, 0, 0, 0, 0,
                                      tr_wt, 0, 0,  kh-3-1, 0, 0, 0,
                                      del_out,  img+1, 0, 2-kh+3+1, 0, 0
                                      );
#else
            LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img+1, 0, 2, 0, 0,
                                tr_wt, 0, 0,  kh-3-1, 0, 0, 0,
                                del_out,  img+1, 0, 2-kh+3+1, 0, 0
                        );
#endif
          } else {
            if (ofm1+1 == handle->blocksofm) { /* prefecth next ifm1,  kj=4, ij=0 */
              /* Unroll 6 */
              ij = ifh-3; kj=1; oj=ij-kh+kj+1;
              /* prefetch for ij=0 kj=4 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1+1][0][0][0]), &(tr_wt[0][ifm1+1][kh-4-1][0][0][0]), &(del_out[img][0][-kh+4+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1+1, 0, 0, 0,
                                tr_wt, 0, ifm1+1,  kh-4-1, 0, 0, 0,
                                del_out, img, 0, 0-kh+4+1, 0, 0
                        );

              /* Unroll 7 */
              ij = ifh-3; kj=2; oj=ij-kh+kj+1;
              /* prefetch for ij=1 kj=3 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1+1][1][0][0]), &(tr_wt[0][ifm1+1][kh-3-1][0][0][0]), &(del_out[img][0][1-kh+3+1][0][0]));*/
#if defined(INPUT_PADDING)
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                        tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                        del_out, img, ofm1, oj, 0, 0,
                                        input_to_use, img, ifm1+1, 0, 0, 0,
                                        tr_wt, 0, ifm1+1,  kh-3-1, 0, 0, 0,
                                        del_out, img, 0, 1-kh+3+1, 0, 0
                                        );
#else
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1+1, 1, 0, 0,
                                tr_wt, 0, ifm1+1,  kh-3-1, 0, 0, 0,
                                del_out, img, 0, 1-kh+3+1, 0, 0
                        );
#endif

              /* Unroll 8 */
              ij = ifh-2; kj=0; oj=ij-kh+kj+1;
              /* prefetch for ij=1 kj=4 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1+1][1][0][0]), &(tr_wt[0][ifm1+1][kh-4-1][0][0][0]), &(del_out[img][0][1-kh+4+1][0][0]));*/
#if defined(INPUT_PADDING)
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                        tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                        del_out, img, ofm1, oj, 0, 0,
                                        input_to_use, img, ifm1+1, 0, 0, 0,
                                        tr_wt, 0, ifm1+1,  kh-4-1, 0, 0, 0,
                                        del_out, img, 0, 1-kh+4+1, 0, 0
                                        );
#else
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1+1, 1, 0, 0,
                                tr_wt, 0, ifm1+1,  kh-4-1, 0, 0, 0,
                                del_out, img, 0, 1-kh+4+1, 0, 0
                        );
#endif

              /* Unroll 9 */
              ij = ifh-2; kj=1; oj=ij-kh+kj+1;
              /* prefetch for ij=2 kj=2 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1+1][2][0][0]), &(tr_wt[0][ifm1+1][kh-2-1][0][0][0]), &(del_out[img][0][2-kh+2+1][0][0]));*/
#if defined(INPUT_PADDING)
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                        tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                        del_out, img, ofm1, oj, 0, 0,
                                        input_to_use, img, ifm1+1, 0, 0, 0,
                                        tr_wt, 0, ifm1+1,  kh-2-1, 0, 0, 0,
                                        del_out, img, 0, 2-kh+2+1, 0, 0
                                        );
#else
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1+1, 2, 0, 0,
                                tr_wt, 0, ifm1+1,  kh-2-1, 0, 0, 0,
                                del_out, img, 0, 2-kh+2+1, 0, 0
                        );
#endif

              /* Unroll 10 */
              ij = ifh-1; kj=0; oj=ij-kh+kj+1;
              /* ij=2 kj=3 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1+1][2][0][0]), &(tr_wt[0][ifm1+1][kh-3-1][0][0][0]), &(del_out[img][0][2-kh+3+1][0][0]));*/
#if defined(INPUT_PADDING)
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                        tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                        del_out, img, ofm1, oj, 0, 0,
                                        input_to_use, img, ifm1+1, 0, 0, 0,
                                        tr_wt, 0, ifm1+1,  kh-3-1, 0, 0, 0,
                                        del_out, img, 0, 2-kh+3+1, 0, 0
                                        );
#else
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1+1, 2, 0, 0,
                                tr_wt, 0, ifm1+1,  kh-3-1, 0, 0, 0,
                                del_out, img, 0, 2-kh+3+1, 0, 0
                        );
#endif
            } else {
              /* Unroll 6 */
              ij = ifh-3; kj=1; oj=ij-kh+kj+1;
              /* prefetch for ij=0 kj=4 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][0][0][0]), &(tr_wt[ofm1+1][ifm1][kh-4-1][0][0][0]), &(del_out[img][ofm1+1][-kh+4+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, 0, 0, 0,
                                tr_wt, ofm1+1, ifm1,  kh-4-1, 0, 0, 0,
                                del_out, img, ofm1+1, -kh+4+1, 0, 0
                        );

              /* Unroll 7 */
              ij = ifh-3; kj=2; oj=ij-kh+kj+1;
              /* prefetch for ij=1 kj=3 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][1][0][0]), &(tr_wt[ofm1+1][ifm1][kh-3-1][0][0][0]), &(del_out[img][ofm1+1][1-kh+3+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, 1, 0, 0,
                                tr_wt, ofm1+1, ifm1,  kh-3-1, 0, 0, 0,
                                del_out, img, ofm1+1, 1-kh+3+1, 0, 0
                        );

              /* Unroll 8 */
              ij = ifh-2; kj=0; oj=ij-kh+kj+1;
              /* prefetch for ij=1 kj=4 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][1][0][0]), &(tr_wt[ofm1+1][ifm1][kh-4-1][0][0][0]), &(del_out[img][ofm1+1][1-kh+4+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, 1, 0, 0,
                                tr_wt, ofm1+1, ifm1,  kh-4-1, 0, 0, 0,
                                del_out, img, ofm1+1, 1-kh+4+1, 0, 0
                        );

              /* Unroll 9 */
              ij = ifh-2; kj=1; oj=ij-kh+kj+1;
              /* prefetch for ij=2 kj=2 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][2][0][0]), &(tr_wt[ofm1+1][ifm1][kh-2-1][0][0][0]), &(del_out[img][ofm1+1][2-kh+2+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, 2, 0, 0,
                                tr_wt, ofm1+1, ifm1,  kh-2-1, 0, 0, 0,
                                del_out, img, ofm1+1, 2-kh+2+1, 0, 0
                        );

              /* Unroll 10 */
              ij = ifh-1; kj=0; oj=ij-kh+kj+1;
              /* ij=2 kj=3 */
              /*jitted_conv_bp_pf(l_input, l_wt, l_output, &(del_input[img][ifm1][2][0][0]), &(tr_wt[ofm1+1][ifm1][kh-3-1][0][0][0]), &(del_out[img][ofm1+1][2-kh+3+1][0][0]));*/
              LIBXSMM_JITTED_CONV_BP_PF(input_to_use, img, ifm1, ij, 0, 0,
                                tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                del_out, img, ofm1, oj, 0, 0,
                                input_to_use, img, ifm1, 2, 0, 0,
                                tr_wt, ofm1+1, ifm1,  kh-3-1, 0, 0, 0,
                                del_out, img, ofm1+1, 2-kh+3+1, 0, 0
                        );
            }
          }
        } else {
          for (ij=ifh-kh +1; ij < ifh; ij++) {
            for (kj=0; kj < kh; kj++) {
              oj = ij - kh + kj + 1;
              if (oj < ofh) {
                LIBXSMM_JITTED_CONV_BP_NO_PF(
                  input_to_use, img, ifm1, ij, 0, 0,
                  tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                  del_out, img, ofm1, oj, 0, 0);
              }
            }
          }
        }
#else /* NO_PREFETCH */
        for (ij=0; ij < kh-1; ++ij) {
          for (kj=0; kj < kh; ++kj) {
            oj = ij - kh + kj + 1;
            if (oj >=0) {
              LIBXSMM_JITTED_CONV_BP_NO_PF(input_to_use, img, ifm1, ij, 0, 0,
                                    tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                    del_out, img, ofm1, oj, 0, 0);
            }
          }
        }
        for (ij=kh-1; ij < ifh-kh +1 ; ++ij) {
          oj = ij - kh + 1;
          LIBXSMM_JITTED_CONV_BP_PEELED_NO_PF(input_to_use, img, ifm1, ij, 0, 0,
                                       tr_wt,ofm1, ifm1, kh-1, 0, 0, 0,
                                       del_out, img, ofm1, oj, 0, 0);
        }
        for (ij=ifh-kh +1 ; ij < ifh; ++ij) {
          for (kj=0; kj < kh; ++kj) {
            oj = ij - kh + kj + 1;
            if (oj < ofh) {
              LIBXSMM_JITTED_CONV_BP_NO_PF(input_to_use, img, ifm1, ij, 0, 0,
                                    tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
                                    del_out, img, ofm1, oj, 0, 0);
            }
          }
        }
#endif
      }
#if defined(INPUT_PADDING)
      /* Write back input buffer */
      if (small_block_size % 512 == 0) {
        for (oj = 0; oj < handle->ifhp; oj++) {
#if defined(__AVX512F__)
          for (ij = 0; ij < block_size; ij += CHUNK_SIZE) {
            STORE(&input_ptr[ij+oj*block_size], LOAD(&copy_ptr[ij+oj*big_block_size]));
          }
#else
          for (ij = 0; ij < handle->ifwp; ij++) {
            for (iii = 0; iii < handle->ifmblock; iii++) {
              LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock)
              = LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock);
            }
          }
#endif
        }
      } else {
        for (oj = 0; oj < handle->ifhp; oj++) {
          for (ij = 0; ij < handle->ifwp; ij++) {
            for (iii = 0; iii < handle->ifmblock; iii++) {
              LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock)
              = LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock);
            }
          }
        }
      }
#endif

#if !defined(INPUT_PADDING)
#include "libxsmm_dnn_zero_rim_st_input_custom.tpl.c"
#endif
    }
  }
} else if ( libxsmm_target_archid == LIBXSMM_X86_AVX2 ) {
  for (imgifm1 = thr_begin; imgifm1 < thr_end; ++imgifm1) {
    img = imgifm1/handle->blocksifm;
    ifm1 = imgifm1%handle->blocksifm;

#if defined(INPUT_PADDING)

#if defined(__AVX__)
    input_ptr = (element_input_type*)&LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, 0, 0, 0,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
#endif

    if (small_block_size % 256 == 0) {
      for (oj = 0; oj < handle->ifhp; oj++) {
#if defined(__AVX__)
        for (ij = 0; ij < block_size; ij += CHUNK_SIZE/2) {
          STORE_256(&copy_ptr[ij+oj*big_block_size], LOAD_256(&input_ptr[ij+oj*block_size]));
        }
#else
        for (ij = 0; ij < handle->ifwp; ij++) {
          for (iii = 0; iii < handle->ifmblock; iii++) {
            LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock)
            = LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
          }
        }
#endif
      }
    } else {
      for (oj = 0; oj < handle->ifhp; oj++) {
        for (ij = 0; ij < handle->ifwp; ij++) {
          for (iii = 0; iii < handle->ifmblock; iii++) {
            LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock)
            = LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
          }
        }
      }
    }
#endif

    for (ofm1 = 0; ofm1 < handle->blocksofm; ++ofm1) {
      for (ij= 0 ; ij < ifh; ++ij) {
        for (kj=0; kj < kh; ++kj) {
          oj = ij - kh + kj + 1;
          if (oj >= 0 && oj < ofh) {
            LIBXSMM_JITTED_CONV_BP_NO_PF(
              input_to_use, img, ifm1, ij, 0, 0,
              tr_wt, ofm1, ifm1, kh-kj-1, 0, 0, 0,
              del_out, img, ofm1, oj, 0, 0);
          }
        }
      }
    }

#if defined(INPUT_PADDING)
    /* Write back input buffer */
    if (small_block_size % 256 == 0) {
      for (oj = 0; oj < handle->ifhp; oj++) {
#if defined(__AVX__)
        for (ij = 0; ij < block_size; ij += CHUNK_SIZE/2) {
          STORE_256(&input_ptr[ij+oj*block_size], LOAD_256(&copy_ptr[ij+oj*big_block_size]));
        }
#else
        for (ij = 0; ij < handle->ifwp; ij++) {
          for (iii = 0; iii < handle->ifmblock; iii++) {
            LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock)
            = LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock);
          }
        }
#endif
      }
    } else {
      for (oj = 0; oj < handle->ifhp; oj++) {
        for (ij = 0; ij < handle->ifwp; ij++) {
          for (iii = 0; iii < handle->ifmblock; iii++) {
            LIBXSMM_VLA_ACCESS(5, del_input, img, ifm1, oj, ij, iii,handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock)
            = LIBXSMM_VLA_ACCESS(3, input_buffer, oj + handle->desc.pad_h, ij + handle->desc.pad_w, iii, padded_w, handle->ifmblock);
          }
        }
      }
    }
#endif

#include "libxsmm_dnn_zero_rim_st_input_custom.tpl.c"
  }
/* should never happen, this is just an additional check */
} else {
  status = LIBXSMM_DNN_ERR_UNSUPPORTED_ARCH;
}

#undef LIBXSMM_JITTED_CONV_BP_PF
#undef LIBXSMM_JITTED_CONV_BP_NO_PF
#undef LIBXSMM_JITTED_CONV_BP_PEELED_NOWEIGHT_PF
#undef LIBXSMM_JITTED_CONV_BP_PEELED_NO_PF

#if defined(INPUT_PADDING)
#undef LOAD
#undef LOAD_256
#undef STORE
#undef STORE_256
#undef CHUNK_SIZE
#endif
