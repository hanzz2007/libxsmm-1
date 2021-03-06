/******************************************************************************
** Copyright (c) 2017-2018, Intel Corporation                                **
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
/* Sasikanth Avancha, Dhiraj Kalamkar (Intel Corp.)
******************************************************************************/


#pragma once
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.hpp"
#include "check.hpp"
#include "FusedConvBNImpl.hpp"
#include "libxsmm.h"

#define VLEN 16

#define CHKERR_LIBXSMM_DNN(A) if ( A != LIBXSMM_DNN_SUCCESS )\
{\
  fprintf(stdout, "%s, %s\n", gp->node_name.c_str(), libxsmm_dnn_get_error(A) );\
  fflush(stdout);\
}

#define CHKERR_LIBXSMM_DNN_CREATE(t, A) if ( A != LIBXSMM_DNN_SUCCESS )\
{\
  fprintf(stdout, "Creating tensor %s in %s, %s\n", t, gp->node_name.c_str(), libxsmm_dnn_get_error(A) );\
  fflush(stdout);\
}

#define CHKERR_LIBXSMM_DNN_LINK(t, A) if ( A != LIBXSMM_DNN_SUCCESS )\
{\
  fprintf(stdout, "Linking tensor %s in %s, %s\n", t, gp->node_name.c_str(), libxsmm_dnn_get_error(A) );\
  fflush(stdout);\
}

#define CHKERR_LIBXSMM_DNN_BIND(t, A) if ( A != LIBXSMM_DNN_SUCCESS )\
{\
  fprintf(stdout, "Binding tensor %s in %s, %s\n", t, gp->node_name.c_str(), libxsmm_dnn_get_error(A) );\
  fflush(stdout);\
}

class FusedConvBNXSMM : public FusedConvBNImpl
{
  protected:
    FusedConvBNImpl *gp_;
    libxsmm_dnn_conv_desc conv_desc;
    libxsmm_dnn_fusedbatchnorm_desc fusedbn_desc_train;
    libxsmm_dnn_fusedbatchnorm_desc fusedbn_desc_test;
    libxsmm_dnn_layer* libxsmm_handle_conv = NULL;
    libxsmm_dnn_fusedbatchnorm* libxsmm_handle_bn_train = NULL;
    libxsmm_dnn_fusedbatchnorm* libxsmm_handle_bn_test = NULL;
    libxsmm_dnn_tensor* libxsmm_input = NULL;
    libxsmm_dnn_tensor* libxsmm_input_bntrain = NULL;
    libxsmm_dnn_tensor* libxsmm_input_add_bntrain = NULL;
    libxsmm_dnn_tensor* libxsmm_input_bntest = NULL;
    libxsmm_dnn_tensor* libxsmm_input_add_bntest = NULL;
    libxsmm_dnn_tensor* libxsmm_middle = NULL;
    libxsmm_dnn_tensor* libxsmm_output_bntrain = NULL;
    libxsmm_dnn_tensor* libxsmm_output_bntest = NULL;
    libxsmm_dnn_tensor* libxsmm_filter = NULL;
    libxsmm_dnn_tensor* libxsmm_checkpoint_filter = NULL;
    libxsmm_dnn_tensor* libxsmm_checkpoint_history_filter = NULL;
    libxsmm_dnn_tensor* libxsmm_temp = NULL;
    libxsmm_dnn_tensor* libxsmm_delinput = NULL;
    libxsmm_dnn_tensor* libxsmm_delinput_add = NULL;
    libxsmm_dnn_tensor* libxsmm_deloutput = NULL;
    libxsmm_dnn_tensor* libxsmm_delmiddle_bn = NULL;
    libxsmm_dnn_tensor* libxsmm_delmiddle_conv = NULL;
    libxsmm_dnn_tensor* libxsmm_delfilter = NULL;
    libxsmm_dnn_tensor* libxsmm_expectval_train = NULL;
    libxsmm_dnn_tensor* libxsmm_stddev_train = NULL;
    libxsmm_dnn_tensor* libxsmm_expectval_test = NULL;
    libxsmm_dnn_tensor* libxsmm_stddev_test = NULL;
    libxsmm_dnn_tensor* libxsmm_variance_train = NULL;
    libxsmm_dnn_tensor* libxsmm_variance_test = NULL;
    libxsmm_dnn_tensor* libxsmm_gamma_train = NULL;
    libxsmm_dnn_tensor* libxsmm_gamma_test = NULL;
    libxsmm_dnn_tensor* libxsmm_beta_train = NULL;
    libxsmm_dnn_tensor* libxsmm_beta_test = NULL;
    libxsmm_dnn_tensor* libxsmm_delgamma = NULL;
    libxsmm_dnn_tensor* libxsmm_delbeta = NULL;
    libxsmm_dnn_tensor_datalayout* libxsmm_layout;
    libxsmm_dnn_err_t status;

    FusedConvBNImplParams *cp;
    bool updated_scratch=false;
    void *bexpect=NULL, *bstddev=NULL, *bvariance=NULL, *gexp_test=NULL, *gvar_test=NULL;
    void *scratch=NULL;

  public:
    FusedConvBNXSMM(FusedConvBNImplParams *gp, int engine);
    virtual ~FusedConvBNXSMM(void) {}
    void forwardPropagate(vector<TensorBuf*>& inp, TensorBuf* weightp, TensorBuf *hweightp, TensorBuf* midp, TensorBuf* gammap, TensorBuf* betap, TensorBuf* gmeanp, TensorBuf* gvarp, TensorBuf* outp, int tid);
    void backPropagate(TensorBuf* deloutp, TensorBuf* weightp, TensorBuf* delgammap, TensorBuf* delbetap, TensorBuf* delmidp, vector<TensorBuf *>& delinp, int tid);
    void weightUpdate(TensorBuf*, TensorBuf*, TensorBuf*, int tid);
    void dumpBuffer(TensorBuf *wt, void* temp);
};
