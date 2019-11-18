// Copyright (c) 2019 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include "lite/backends/cuda/blas.h"
#include "lite/core/context.h"
#include "lite/core/kernel.h"
#include "lite/core/types.h"
#include "lite/operators/op_params.h"

namespace paddle {
namespace lite {
namespace kernels {
namespace cuda {

class SearchAlignedMatMulCompute
    : public KernelLite<TARGET(kCUDA), PRECISION(kFloat)> {
 public:
  using param_t = operators::MatMulParam;

  void PrepareForRun() override {
    auto& param = this->Param<param_t>();
    int seq_num = param.X->lod()[0].size() - 1;
    cudaMalloc(reinterpret_cast<void**>(&A_dev_), 3 * seq_num * sizeof(float*));
    CHECK(A_dev_);
    A_host_ = static_cast<float**>(malloc(3 * seq_num * sizeof(float*)));
    CHECK(A_host_);
  }

  void Run() override {
    auto& param = this->Param<param_t>();
    CHECK(ctx_) << "running context should be set first";
    auto& ctx = this->ctx_->template As<CUDAContext>();
    CHECK(ctx.cublas_fp32()) << "blas should init first";
    auto& blas = *ctx.cublas_fp32();
    auto cuda_stream = ctx.exec_stream();

    auto x = param.X;
    auto y = param.Y;
    auto out = param.Out;
    bool x_transpose = param.transpose_X;
    bool y_transpose = param.transpose_Y;
    float alpha = param.alpha;
    const auto& x_dims = x->dims();
    const auto& y_dims = y->dims();
    const auto& x_lod = x->lod();
    const auto& y_lod = y->lod();
    const auto& x_lod_0 = x_lod[0];
    const auto& y_lod_0 = y_lod[0];
    int seq_num = x_lod_0.size() - 1;
    int x_inner_size = x_dims[1];
    int y_inner_size = y_dims[1];
    int x_batch_size = x_lod_0[1];
    int y_batch_size = y_lod_0[1];
    int M = x_transpose ? x_inner_size : x_batch_size;
    int N = y_transpose ? y_batch_size : y_inner_size;
    int X_K = x_transpose ? x_batch_size : x_inner_size;
    int Y_K = y_transpose ? y_inner_size : y_batch_size;
    CHECK_EQ(X_K, Y_K) << "K of Input(X) and Input(Y) is not equal";
    int K = X_K;

    auto x_data = x->data<float>();
    auto y_data = y->data<float>();
    auto out_data = out->mutable_data<float>(TARGET(kCUDA));
    auto x_stride = x_batch_size * x_inner_size;
    auto y_stride = y_batch_size * y_inner_size;
    auto out_stride = M * N;
    int lda = x_transpose ? M : K;
    int ldb = y_transpose ? K : N;
    int ldc = N;
    for (int seq = 0; seq < seq_num; seq++) {
      A_host_[seq] = static_cast<float*>(x_data) + seq * x_stride;
      A_host_[seq + seq_num] = static_cast<float*>(y_data) + seq * y_stride;
      A_host_[seq + seq_num * 2] = out_data + seq * out_stride;
    }
    cudaMemcpyAsync(A_dev_,
                    A_host_,
                    3 * seq_num * sizeof(float*),
                    cudaMemcpyDefault,
                    cuda_stream);

    cublasOperation_t transa = x_transpose ? CUBLAS_OP_T : CUBLAS_OP_N;
    cublasOperation_t transb = y_transpose ? CUBLAS_OP_T : CUBLAS_OP_N;
    float beta = 0.f;
    blas.batched_sgemm(transa,
                       transb,
                       M,
                       N,
                       K,
                       &alpha,
                       static_cast<const float**>(A_dev_),
                       lda,
                       static_cast<const float**>(A_dev_ + seq_num),
                       ldb,
                       &beta,
                       static_cast<const float**>(A_dev_ + seq_num * 2),
                       ldc,
                       seq_num);
  }

  ~SearchAlignedMatMulCompute() {
    if (A_dev_ != nullptr) {
      cudaFree(A_dev_);
    }
    if (A_host_ != nullptr) {
      free(A_host_);
    }
  }

 private:
  float** A_dev_{nullptr};
  float** A_host_{nullptr};
};

}  // namespace cuda
}  // namespace kernels
}  // namespace lite
}  // namespace paddle
