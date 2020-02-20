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

#include "lite/kernels/xpu/conv_compute.h"
#include "lite/backends/xpu/xpu_header_sitter.h"
#include "lite/core/op_registry.h"

namespace paddle {
namespace lite {
namespace kernels {
namespace xpu {

template <>
void Conv2dCompute<PRECISION(kFloat)>::Run() {
  auto& param = this->Param<param_t>();
  auto& ctx = this->ctx_->As<XPUContext>();

  auto& x_dims = param.x->dims();
  auto& w_dims = param.filter->dims();
  int groups = param.groups;
  auto& strides = param.strides;
  auto paddings = *param.paddings;
  auto dilations = *param.dilations;

  int r = xdnn::conv2d_forward_int16<float, float, float, float>(
    ctx.GetRawContext(), /* context */
    x_dims[0], /* num */
    x_dims[1], /* input_c */
    x_dims[2], /* input_h */
    x_dims[3], /* input_w */
    w_dims[0], /* num_filter */
    w_dims[2], /* kernel_h */
    w_dims[3], /* kernel_w */
    strides[0], /* stride_h */
    strides[1], /* stride_w */
    paddings[0], /* pad_h */
    paddings[1], /* pad_w */
    dilations[0], /* dilation_h */
    dilations[1], /* dilation_w */
    groups, /* group */
    param.x->data<float>(), /* bottom */
    param.filter->data<float>(), /* weight */
    param.output->mutable_data<float>(TARGET(kXPU)), /* top */
    nullptr, /* bias */
    nullptr, /* branch */
    xdnn::Activation_t::LINEAR, /* type */
    nullptr, /* max_image_ptr */
    nullptr, /* max_filter_ptr */
    nullptr /* max_result_ptr */);
  CHECK(r == 0);
}

}  // namespace xpu
}  // namespace kernels
}  // namespace lite
}  // namespace paddle

namespace xpu = paddle::lite::kernels::xpu;
using Conv2dFp32 = xpu::Conv2dCompute<PRECISION(kFloat)>;

REGISTER_LITE_KERNEL(conv2d, kXPU, kFloat, kNCHW, Conv2dFp32, def)
    .BindInput("Input", {LiteType::GetTensorTy(TARGET(kXPU))})
    .BindInput("Bias", {LiteType::GetTensorTy(TARGET(kXPU))})
    .BindInput("Filter", {LiteType::GetTensorTy(TARGET(kXPU))})
    .BindOutput("Output", {LiteType::GetTensorTy(TARGET(kXPU))})
    .Finalize();