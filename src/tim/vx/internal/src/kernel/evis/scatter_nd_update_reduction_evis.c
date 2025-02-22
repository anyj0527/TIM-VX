/****************************************************************************
*
*    Copyright (c) 2019 Vivante Corporation
*
*    Permission is hereby granted, free of charge, to any person obtaining a
*    copy of this software and associated documentation files (the "Software"),
*    to deal in the Software without restriction, including without limitation
*    the rights to use, copy, modify, merge, publish, distribute, sublicense,
*    and/or sell copies of the Software, and to permit persons to whom the
*    Software is furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vsi_nn_types.h"
#include "vsi_nn_tensor.h"
#include "vsi_nn_graph.h"
#include "vsi_nn_log.h"
#include "vsi_nn_prv.h"
#include "vsi_nn_tensor_util.h"
#include "vsi_nn_error.h"
#include "utils/vsi_nn_util.h"
#include "kernel/vsi_nn_kernel.h"

__BEGIN_DECLS

typedef enum
{
    NONE = 0,
    Add,
    Mul,
    Max,
    Min
} vsi_scatter_nd_update_type_e;

/*
 * Define kernel meta.
 */
#define KERNEL_SOURCE_1    "scatter_nd_update_reduction"
#define KERNEL_SOURCE_2    "scatter_nd_update_reduction_conv"

#define HASH_SCATTER_ND_UPDATE_KEY(_in0_type, _in2_type, _out_type, _stage, _op_type) \
    ((_in0_type << 24) | (_in2_type << 16) | (_out_type << 8) | (_stage << 4) | (_op_type))

#define HASH_SCATTER_ND_UPDATE_SH_KERNEL_PREPROCESS_NAME(SRC0_TYPE) \
    CVIVANTE_NAMESPACE("evis.scatter_nd_update_reduction_preprocess_"#SRC0_TYPE)

#define HASH_SCATTER_ND_UPDATE_SH_KERNEL_PROCESS_NAME(REDUCTION_TYPE, SRC2_TYPE) \
    CVIVANTE_NAMESPACE("evis.scatter_nd_update_reduction_"#REDUCTION_TYPE"_"#SRC2_TYPE)

#define HASH_SCATTER_ND_UPDATE_SH_KERNEL_CONV_NAME(DST_TYPE) \
    CVIVANTE_NAMESPACE("evis.scatter_nd_update_reduction_conv_"#DST_TYPE)

#define TENSOR_SCATTER_ND_UPDATE_REDUCTION_PREPROCESS_KERNELS(IN0_TYPE, SOURCE) \
    { HASH_SCATTER_ND_UPDATE_KEY(IN0_TYPE, 0, 0, 0, 0), \
        HASH_SCATTER_ND_UPDATE_SH_KERNEL_PREPROCESS_NAME(IN0_TYPE), \
        SOURCE },

#define TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(REDUCTION_TYPE, IN2_TYPE, SOURCE) \
    { HASH_SCATTER_ND_UPDATE_KEY(0, IN2_TYPE, 0, 1, REDUCTION_TYPE), \
        HASH_SCATTER_ND_UPDATE_SH_KERNEL_PROCESS_NAME(REDUCTION_TYPE, IN2_TYPE), \
        SOURCE },

#define TENSOR_SCATTER_ND_UPDATE_REDUCTION_CONV_KERNELS(OUT_TYPE, SOURCE) \
    { HASH_SCATTER_ND_UPDATE_KEY(0, 0, OUT_TYPE, 2, 0), \
        HASH_SCATTER_ND_UPDATE_SH_KERNEL_CONV_NAME(OUT_TYPE), \
        SOURCE },

typedef struct
{
    uint32_t key;
    char * function_name;
    const char * source_name;
} _kernel_map_type;

static const _kernel_map_type scatter_nd_update_reduction_preprocess_map[] =
{
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PREPROCESS_KERNELS(U8,   KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PREPROCESS_KERNELS(I8,   KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PREPROCESS_KERNELS(I16,  KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PREPROCESS_KERNELS(F16,  KERNEL_SOURCE_1)
};

static const _kernel_map_type scatter_nd_update_reduction_process_map[] =
{
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Add, U8,   KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Mul, U8,   KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Max, U8,   KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Min, U8,   KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Add, I8,   KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Mul, I8,   KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Max, I8,   KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Min, I8,   KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Add, I16,  KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Mul, I16,  KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Max, I16,  KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Min, I16,  KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Add, F16,  KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Mul, F16,  KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Max, F16,  KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Min, F16,  KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Add, BF16, KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Mul, BF16, KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Max, BF16, KERNEL_SOURCE_1)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_PROCESS_KERNELS(Min, BF16, KERNEL_SOURCE_1)
};

static const _kernel_map_type scatter_nd_update_reduction_conv_map[] =
{
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_CONV_KERNELS(U8,   KERNEL_SOURCE_2)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_CONV_KERNELS(I8,   KERNEL_SOURCE_2)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_CONV_KERNELS(I16,  KERNEL_SOURCE_2)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_CONV_KERNELS(F16,  KERNEL_SOURCE_2)
    TENSOR_SCATTER_ND_UPDATE_REDUCTION_CONV_KERNELS(BF16, KERNEL_SOURCE_2)
};

/*
 * Kernel params
 */
static vx_param_description_t _scatter_nd_update_preprocess_kernel_param_def[] =
{
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    // Add kererl parameters here
};

static vx_param_description_t _scatter_nd_update_process_kernel_param_def[] =
{
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    // Add kererl parameters here
};

static vx_param_description_t _scatter_nd_update_conv_kernel_param_def[] =
{
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    // Add kererl parameters here
};

#define _SCATTER_ND_UPDATE_PREPROCESS_PARAM_NUM  _cnt_of_array(_scatter_nd_update_preprocess_kernel_param_def)
#define _SCATTER_ND_UPDATE_PROCESS_PARAM_NUM  _cnt_of_array(_scatter_nd_update_process_kernel_param_def)
#define _SCATTER_ND_UPDATE_CONV_PARAM_NUM  _cnt_of_array(_scatter_nd_update_conv_kernel_param_def)

static vsi_status get_scatter_nd_update_tensor_reshape_size
    (
    vsi_nn_tensor_t ** inputs,
    vsi_size_t sizes[VSI_NN_MAX_DIM_NUM],
    uint32_t block_size,
    uint32_t coordDim,
    vsi_size_t strides[VSI_NN_MAX_DIM_NUM],
    int32_t* newDim
    )
{
    vsi_status status = VSI_SUCCESS;
    uint32_t dims_num = inputs[0]->attr.dim_num;
    vsi_size_t *input_size = inputs[0]->attr.size;
    uint32_t i = 0;
    vsi_size_t elementCnt = 1;

#define VSI_NN_MAX_IMAGE_WIDTH  GPU_TENSOR_MAX_WIDTH

    newDim[0] = 0;
    for (i = 0; i < dims_num; ++i)
    {
        elementCnt *= input_size[i];
    }

    for (i = 0; i < VSI_NN_MAX_DIM_NUM; ++i)
    {
        sizes[i] = 1;
    }

    sizes[0] = block_size;
    sizes[1] = elementCnt / block_size;
    newDim[0] = 2;

    if (coordDim == 1 && strides) // index shape
    {
        for (i = 0; i < VSI_NN_MAX_DIM_NUM; i++)
        {
            strides[i] = 0;
        }
    }
    else if (coordDim >= 2 && coordDim <= VSI_NN_MAX_DIM_NUM && strides)
    {
        for (i = 0; i < VSI_NN_MAX_DIM_NUM; i++)
        {
            strides[i] = 0;
        }

        strides[0] = input_size[dims_num - coordDim];
        for (i = 1; i < coordDim - 1; i++)
        {
            strides[i] = strides[i - 1] * input_size[dims_num - coordDim + i];
        }
    }

#undef VSI_NN_MAX_IMAGE_WIDTH

    return status;
} /* _get_EltOP_tensor_reshape_size */

/*
 * Kernel initializer
 */
DEF_KERNEL_INITIALIZER(_scatter_nd_update_preprocess_initializer)
    (
    vsi_nn_kernel_node_t                node,
    const vsi_nn_kernel_node_param_t  * param,
    size_t                              param_size
    )
{
    vsi_status status = VSI_FAILURE;
    gpu_param_t gpu_param = {
        1,
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
        };

    vsi_nn_kernel_tensor_attr_t * attr[1] = { NULL };
    int32_t     width         = 0;
    int32_t     element_size  = 1;
    int32_t     input_zp0     = 0;
    float       input_scale0  = 1;
    int32_t     i             = 0;

    VSI_UNREFERENCED(param_size);

    attr[0] = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)param[0] );
    CHECK_PTR_FAIL_GOTO( attr[0], "Create tensor attr buffer fail.", OnError );

    for (i = 0; i < (int32_t)attr[0]->shape->size; i++)
    {
        element_size *= (int32_t)attr[0]->shape->data[i];
    }
    width = element_size / 8;

    input_zp0     = attr[0]->zero_point;
    input_scale0  = attr[0]->scale;

    if (attr[0]->quant == VSI_NN_KERNEL_QUANT_NONE)
    {
        input_scale0 = 1.0f;
    }

    gpu_param.global_scale[0]  = 1;
    gpu_param.global_scale[1]  = 1;
    gpu_param.global_scale[2]  = 1;
    if (element_size < 8)
    {
        gpu_param.global_size[0]   = element_size;
    }
    else
    {
        gpu_param.global_size[0]   = width;
    }
    gpu_param.global_size[1]   = 1;
    gpu_param.global_size[2]   = 1;

    status = vsi_nn_kernel_gpu_config( node, &gpu_param );
    CHECK_STATUS_FAIL_GOTO(status, OnError);

    {
        gpu_dp_inst_t uniConvert1stUint8SubZpToFp32_4x4 = {{
                0x05050505, // TCfg
                0x04040404, // ASelt
                0x00010000, 0x00030002, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000400, // AccumType, ConstantType, and PostShift
                0xffff0001, 0x00000000, 0xffff0001, 0x00000000,
                0xffff0001, 0x00000000, 0xffff0001, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniConvert2ndU8SubZpToFp32_4x4 = {{
                0x09090909, // TCfg
                0x04040404, // ASelt
                0x00050004, 0x00070006, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000300, // AccumType, ConstantType, and PostShift
                0x00010001, 0x00000000, 0x00010001, 0x00000000,
                0x00010001, 0x00000000, 0x00010001, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        status = vsi_nn_kernel_gpu_add_param( node,
                    "uniConvert1stUint8SubZpToFp32_4x4", &uniConvert1stUint8SubZpToFp32_4x4 );
        status |= vsi_nn_kernel_gpu_add_param( node,
                    "uniConvert2ndU8SubZpToFp32_4x4", &uniConvert2ndU8SubZpToFp32_4x4 );
        status |= vsi_nn_kernel_gpu_add_param( node, "input_scale", &input_scale0 );
        status |= vsi_nn_kernel_gpu_add_param( node, "input_zp", &input_zp0 );
        CHECK_STATUS_FAIL_GOTO(status, OnError);
    }

OnError:
    if (attr[0])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[0] );
        attr[0] = NULL;
    }
    return status;
} /* _scatter_nd_update_preprocess_initializer() */

DEF_KERNEL_INITIALIZER(_scatter_nd_update_process_initializer)
    (
    vsi_nn_kernel_node_t                node,
    const vsi_nn_kernel_node_param_t  * param,
    size_t                              param_size
    )
{
    vsi_status status = VSI_FAILURE;
    gpu_param_t gpu_param = {
        2,
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
        };

    vsi_nn_kernel_tensor_attr_t * attr[3] = { NULL };
    int32_t     block_size = 1;
    int32_t     update_width = 1;
    int32_t     index_num  = 1;
    int32_t     width = 0;
    int32_t     coord_dim  = 0;
    int32_t     strides[VSI_NN_MAX_DIM_NUM] = {0};
    int32_t     coord_strides[8]  = {0};
    int32_t     coord_strides1[4] = {0};
    int32_t     input_zp2     = 0;
    float       input_scale2  = 1;
    int32_t     i = 0;

    VSI_UNREFERENCED(param_size);

    attr[0] = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)param[0] );
    CHECK_PTR_FAIL_GOTO( attr[0], "Create tensor attr buffer fail.", OnError );
    attr[1] = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)param[1] );
    CHECK_PTR_FAIL_GOTO( attr[1], "Create tensor attr buffer fail.", OnError );
    attr[2] = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)param[2] );
    CHECK_PTR_FAIL_GOTO( attr[2], "Create tensor attr buffer fail.", OnError );

    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[4], &strides[0]);
    CHECK_STATUS_FAIL_GOTO(status, OnError );
    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[5], &strides[1]);
    CHECK_STATUS_FAIL_GOTO(status, OnError );
    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[6], &strides[2]);
    CHECK_STATUS_FAIL_GOTO(status, OnError );
    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[7], &strides[3]);
    CHECK_STATUS_FAIL_GOTO(status, OnError );
    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[8], &strides[4]);
    CHECK_STATUS_FAIL_GOTO(status, OnError );
    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[9], &strides[5]);
    CHECK_STATUS_FAIL_GOTO(status, OnError );
    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[10], &strides[6]);
    CHECK_STATUS_FAIL_GOTO(status, OnError );
    status = vsi_nn_kernel_scalar_read_int32((vsi_nn_kernel_scalar_t)param[11], &coord_dim);
    CHECK_STATUS_FAIL_GOTO(status, OnError );

    block_size   = (int32_t)(attr[2]->shape->data[0]);
    update_width = (int32_t)(attr[1]->shape->data[0]);
    index_num    = (int32_t)(attr[0]->shape->data[1]);
    width = block_size;

    input_zp2     = attr[1]->zero_point;
    input_scale2  = attr[1]->scale;

    coord_strides[coord_dim - 1] = 1;
    for (i = 0; i < coord_dim - 1; i++)
    {
        coord_strides[i] = strides[coord_dim - 2 - i];
    }
    memcpy(coord_strides1, coord_strides + 4, 4 * sizeof(int32_t));

    gpu_param.global_scale[0]  = 1;
    gpu_param.global_scale[1]  = 1;
    gpu_param.global_scale[2]  = 1;

    gpu_param.global_size[0]   = width;
    gpu_param.global_size[1]   = index_num;
    gpu_param.global_size[2]   = 1;

    status = vsi_nn_kernel_gpu_config( node, &gpu_param );
    CHECK_STATUS_FAIL_GOTO(status, OnError);

    {
        gpu_dp_inst_t uniConvert1stUint8SubZpToFp32_4x4 = {{
                0x05050505, // TCfg
                0x04040404, // ASelt
                0x00010000, 0x00030002, // ABin
                0x0a0a0a0a, // BSelt
                0x00000000, 0x00000000, // BBin
                0x00000400, // AccumType, ConstantType, and PostShift
                0xffff0001, 0x00000000, 0xffff0001, 0x00000000,
                0xffff0001, 0x00000000, 0xffff0001, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };

        gpu_dp_inst_t uniConvBF16toF32_Part0_2x8 = {{
            0x11111111, // TCfg
            0x01010101, // ASelt
            0x01050004, 0x03070206, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00000001, 0x00000001, 0x00000001, 0x00000001,
            0x00000001, 0x00000001, 0x00000001, 0x00000001 // Constant
        }, GPU_DP_TYPE_16};

        status = vsi_nn_kernel_gpu_add_param( node, "update_width", &update_width );
        status |= vsi_nn_kernel_gpu_add_param( node, "output_width", &block_size );
        status |= vsi_nn_kernel_gpu_add_param( node, "coord_stride", &coord_strides );
        status |= vsi_nn_kernel_gpu_add_param( node, "coord_stride1", &coord_strides1 );
        CHECK_STATUS_FAIL_GOTO(status, OnError);

        if (attr[1]->dtype == U8 || attr[1]->dtype == I8 || attr[1]->dtype == I16)
        {
            status = vsi_nn_kernel_gpu_add_param( node,
                    "uniConvert1stUint8SubZpToFp32_4x4",  &uniConvert1stUint8SubZpToFp32_4x4 );
            status |= vsi_nn_kernel_gpu_add_param( node, "update_scale", &input_scale2 );
            status |= vsi_nn_kernel_gpu_add_param( node, "update_zp", &input_zp2 );
            CHECK_STATUS_FAIL_GOTO(status, OnError );
        }
        else if (attr[1]->dtype == BF16)
        {
            status = vsi_nn_kernel_gpu_add_param( node,
                    "uniConvBF16toF32_Part0_2x8",  &uniConvBF16toF32_Part0_2x8 );
            CHECK_STATUS_FAIL_GOTO(status, OnError );
        }
    }

OnError:
    if (attr[0])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[0] );
        attr[0] = NULL;
    }
    if (attr[1])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[1] );
        attr[1] = NULL;
    }
    if (attr[2])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[2] );
        attr[2] = NULL;
    }
    return status;
} /* _scatter_nd_update_process_initializer() */

DEF_KERNEL_INITIALIZER(_scatter_nd_update_conv_initializer)
    (
    vsi_nn_kernel_node_t                node,
    const vsi_nn_kernel_node_param_t  * param,
    size_t                              param_size
    )
{
    vsi_status status = VSI_FAILURE;
    gpu_param_t gpu_param = {
        1,
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
        };

    vsi_nn_kernel_tensor_attr_t * attr[1] = { NULL };
    int32_t     width         = 0;
    int32_t     element_size  = 1;
    int32_t     i             = 0;
    float       output_zp     = 0;
    float       output_scale  = 1.0f;

    VSI_UNREFERENCED(param_size);

    attr[0] = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)param[2] );
    CHECK_PTR_FAIL_GOTO( attr[0], "Create tensor attr buffer fail.", OnError );

    output_zp     = (float)attr[0]->zero_point;
    output_scale  = (float)1.0 / attr[0]->scale;

    for (i = 0; i < (int32_t)attr[0]->shape->size; i++)
    {
        element_size *= (int32_t)attr[0]->shape->data[i];
    }
    width = element_size / 8;

    gpu_param.global_scale[0]  = 1;
    gpu_param.global_scale[1]  = 1;
    gpu_param.global_scale[2]  = 1;
    if (element_size < 8)
    {
        gpu_param.global_size[0]   = element_size;
    }
    else
    {
        gpu_param.global_size[0]   = width;
    }
    gpu_param.global_size[1]   = 1;
    gpu_param.global_size[2]   = 1;

    status = vsi_nn_kernel_gpu_config( node, &gpu_param );
    CHECK_STATUS_FAIL_GOTO(status, OnError);

    {
        gpu_dp_inst_t uniConvertInt32toUint8_2x8 = {{
            0x33333333, // TCfg
            0x11110000, // ASelt
            0x03020100, 0x03020100, // ABin
            0x00000000, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00002400, // AccumType, ConstantType, and PostShift
            0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniExtractHalf8_2x8 = {{
            0x11111111, // TCfg
            0x11110000, // ASelt
            0x06040200, 0x06040200, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000100, // AccumType, ConstantType, and PostShift
            0x00003c00, 0x00003c00, 0x00003c00, 0x00003c00,
            0x00003c00, 0x00003c00, 0x00003c00, 0x00003c00 // Constant
        }, GPU_DP_TYPE_16 };
        gpu_dp_inst_t uniExtractOddData_2x8 = {{
            0x11111111, // TCfg
            0x11110000, // ASelt
            0x07050301, 0x07050301, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00000001, 0x00000001, 0x00000001, 0x00000001,
            0x00000001, 0x00000001, 0x00000001, 0x00000001 // Constant
        }, GPU_DP_TYPE_16};

        status = vsi_nn_kernel_gpu_add_param( node,
                    "uniConvertInt32toUint8_2x8", &uniConvertInt32toUint8_2x8 );
        status |= vsi_nn_kernel_gpu_add_param( node,
                    "uniExtractHalf8_2x8", &uniExtractHalf8_2x8 );
        status |= vsi_nn_kernel_gpu_add_param( node,
                    "uniExtractOddData_2x8", &uniExtractOddData_2x8 );
        status |= vsi_nn_kernel_gpu_add_param( node, "output_zp", &output_zp );
        status |= vsi_nn_kernel_gpu_add_param( node, "output_scale", &output_scale );
        CHECK_STATUS_FAIL_GOTO(status, OnError);
    }

OnError:
    if (attr[0])
    {
        vsi_nn_kernel_tensor_attr_release( &attr[0] );
        attr[0] = NULL;
    }
    return status;
} /* _scatter_nd_update_conv_initializer() */

static vsi_status _query_kernel
    (
    vsi_nn_tensor_t* const* const inputs,
    vsi_nn_tensor_t* const* const outputs,
    vsi_nn_kernel_t* kernel_preprocess,
    vsi_nn_kernel_t* kernel_process,
    vsi_nn_kernel_t* kernel_conv,
    int32_t reduction_flg
    )
{
    vsi_status status = VSI_SUCCESS;
    vsi_nn_kernel_dtype_e input0_dtype = U8;
    vsi_nn_kernel_dtype_e input2_dtype = F16;
    vsi_nn_kernel_dtype_e output_dtype = U8;
    uint32_t key = 0;
    size_t i = 0;

    input0_dtype = vsi_nn_kernel_map_dtype( inputs[0]->attr.dtype.vx_type );
    input2_dtype = vsi_nn_kernel_map_dtype( inputs[2]->attr.dtype.vx_type );
    output_dtype = vsi_nn_kernel_map_dtype( outputs[0]->attr.dtype.vx_type );

    key = HASH_SCATTER_ND_UPDATE_KEY(input0_dtype, 0, 0, 0, 0);

    for ( i = 0; i < _cnt_of_array(scatter_nd_update_reduction_preprocess_map); i ++ )
    {
        if ( scatter_nd_update_reduction_preprocess_map[i].key == key )
        {
            break;
        }
    }

    if ( i < _cnt_of_array(scatter_nd_update_reduction_preprocess_map) )
    {
        snprintf( kernel_preprocess->info.name, VX_MAX_KERNEL_NAME, "%s",
                        scatter_nd_update_reduction_preprocess_map[i].function_name );
        kernel_preprocess->info.parameters = _scatter_nd_update_preprocess_kernel_param_def;
        kernel_preprocess->info.numParams = _SCATTER_ND_UPDATE_PREPROCESS_PARAM_NUM;
        kernel_preprocess->info.initialize = _scatter_nd_update_preprocess_initializer;

        vsi_nn_kernel_add_source( kernel_preprocess, VSI_NN_GPU_SOURCE_FMT_CODE, 2,
                "vsi_nn_kernel_header",
                scatter_nd_update_reduction_preprocess_map[i].source_name );
        vsi_nn_kernel_add_source( kernel_preprocess, VSI_NN_GPU_SOURCE_FMT_EXECUTABLE, 1,
                scatter_nd_update_reduction_preprocess_map[i].source_name );
    }
    else
    {
        status = VSI_FAILURE;
    }

    key = HASH_SCATTER_ND_UPDATE_KEY( 0, input2_dtype, 0, 1, reduction_flg);

    for ( i = 0; i < _cnt_of_array(scatter_nd_update_reduction_process_map); i ++ )
    {
        if ( scatter_nd_update_reduction_process_map[i].key == key )
        {
            break;
        }
    }
    if ( i < _cnt_of_array(scatter_nd_update_reduction_process_map) )
    {
        snprintf( kernel_process->info.name, VX_MAX_KERNEL_NAME, "%s",
                        scatter_nd_update_reduction_process_map[i].function_name );
        kernel_process->info.parameters = _scatter_nd_update_process_kernel_param_def;
        kernel_process->info.numParams = _SCATTER_ND_UPDATE_PROCESS_PARAM_NUM;
        kernel_process->info.initialize = _scatter_nd_update_process_initializer;

        vsi_nn_kernel_add_source( kernel_process, VSI_NN_GPU_SOURCE_FMT_CODE, 2,
                "vsi_nn_kernel_header",
                scatter_nd_update_reduction_process_map[i].source_name );
        vsi_nn_kernel_add_source( kernel_process, VSI_NN_GPU_SOURCE_FMT_EXECUTABLE, 1,
                scatter_nd_update_reduction_process_map[i].source_name );
    }
    else
    {
        status |= VSI_FAILURE;
    }

    key = HASH_SCATTER_ND_UPDATE_KEY( 0, 0, output_dtype, 2, 0);

    for ( i = 0; i < _cnt_of_array(scatter_nd_update_reduction_conv_map); i ++ )
    {
        if ( scatter_nd_update_reduction_conv_map[i].key == key )
        {
            break;
        }
    }
    if ( i < _cnt_of_array(scatter_nd_update_reduction_conv_map) )
    {
        snprintf( kernel_conv->info.name, VX_MAX_KERNEL_NAME, "%s",
                        scatter_nd_update_reduction_conv_map[i].function_name );
        kernel_conv->info.parameters = _scatter_nd_update_conv_kernel_param_def;
        kernel_conv->info.numParams = _SCATTER_ND_UPDATE_CONV_PARAM_NUM;
        kernel_conv->info.initialize = _scatter_nd_update_conv_initializer;

        vsi_nn_kernel_add_source( kernel_conv, VSI_NN_GPU_SOURCE_FMT_CODE, 2,
                "vsi_nn_kernel_header",
                scatter_nd_update_reduction_conv_map[i].source_name );
        vsi_nn_kernel_add_source( kernel_conv, VSI_NN_GPU_SOURCE_FMT_EXECUTABLE, 1,
                scatter_nd_update_reduction_conv_map[i].source_name );
    }
    else
    {
        status |= VSI_FAILURE;
    }

    return status;
} /* _query_kernel() */

static vsi_nn_kernel_node_t _setup
    (
    vsi_nn_graph_t              * graph,
    vsi_nn_tensor_t            ** inputs,
    size_t                        input_num,
    vsi_nn_tensor_t            ** outputs,
    size_t                        output_num,
    const vsi_nn_kernel_param_t * params,
    vsi_nn_kernel_t             * kernel
    )
{
    vsi_status status = VSI_FAILURE;
    vsi_nn_kernel_node_t node = NULL;
    vsi_size_t  shapes[3][VSI_NN_MAX_DIM_NUM] = {{0}};
    vsi_size_t  strides[VSI_NN_MAX_DIM_NUM] = {0};
    int32_t block_size  = vsi_nn_kernel_param_get_int32( params, "block_size" );
    int32_t coord_dim   = vsi_nn_kernel_param_get_int32( params, "coord_dim" );
    int32_t reduction  = vsi_nn_kernel_param_get_int32( params, "reduction" );
    int32_t rs_in_dim = 0, rs_idx_dim = 0, rs_out_dim = 0;
    int32_t i = 0;
    vsi_nn_tensor_t * tensors[2] = { NULL };
    vsi_nn_kernel_t * ikernels[2] = { NULL };

    VSI_UNREFERENCED(input_num);
    VSI_UNREFERENCED(output_num);

    status = get_scatter_nd_update_tensor_reshape_size(&inputs[1], shapes[0], coord_dim, 0,
                                                    NULL, &rs_idx_dim);
    status |= get_scatter_nd_update_tensor_reshape_size(&inputs[2], shapes[1], block_size, 0,
                                                    NULL, &rs_in_dim);
    status |= get_scatter_nd_update_tensor_reshape_size(&outputs[0], shapes[2], block_size, coord_dim,
                                                    strides, &rs_out_dim);
    CHECK_STATUS_FAIL_GOTO( status, final );

    {
        vsi_nn_tensor_attr_t attr;
        vsi_nn_kernel_node_t preprocess_node = NULL;
        vsi_nn_kernel_node_t process_node = NULL;
        vsi_nn_kernel_node_param_t preprocess_params[_SCATTER_ND_UPDATE_PREPROCESS_PARAM_NUM] = { NULL };
        vsi_nn_kernel_node_param_t process_params[_SCATTER_ND_UPDATE_PROCESS_PARAM_NUM] = { NULL };
        vsi_nn_kernel_node_param_t conv_params[_SCATTER_ND_UPDATE_CONV_PARAM_NUM] = { NULL };
        int32_t width = 1;
        int32_t res = 0;

        ikernels[0] = vsi_nn_kernel_create( VSI_NN_KERNEL_TYPE_EVIS );
        ikernels[0]->unique_id = kernel->unique_id;
        ikernels[1] = vsi_nn_kernel_create( VSI_NN_KERNEL_TYPE_EVIS );
        ikernels[1]->unique_id = kernel->unique_id;

        memset( &attr, 0, sizeof(vsi_nn_tensor_attr_t) );
        attr.dtype = outputs[0]->attr.dtype;
        attr.dtype.vx_type = VSI_NN_TYPE_FLOAT32;
        attr.dtype.qnt_type = VSI_NN_QNT_TYPE_NONE;
        attr.is_const = FALSE;
        attr.vtl = TRUE;

        for (i = 0; i < rs_out_dim; i++)
        {
            attr.size[i] = shapes[2][i];
            width *= (int32_t)shapes[2][i];
        }
        attr.dim_num = rs_out_dim;

        res = width % 8;
        width = (width >> 3) << 3;

        tensors[0] = vsi_nn_CreateTensor( graph, &attr );  // ref'
        attr.size[0] = 1;
        attr.size[1] = 1;
        attr.dim_num = rs_out_dim;
        tensors[1] = vsi_nn_CreateTensor( graph, &attr );  // link_buffer0

        status = _query_kernel( inputs, outputs, ikernels[0], ikernels[1], kernel, reduction);
        if ( VSI_SUCCESS == status)
        {
            // convert ref to float
            preprocess_node = vsi_nn_kernel_create_node( graph, ikernels[0] );
            if (preprocess_node)
            {
                uint32_t index = 0;
                /* Pass parameters to node. */
                preprocess_params[index++] = vsi_nn_kernel_tensor_reshape( inputs[0]->t,  shapes[2], rs_out_dim );
                preprocess_params[index++] = (vsi_nn_kernel_node_param_t)tensors[0]->t;
                preprocess_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &width );
                preprocess_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &res );
                status = vsi_nn_kernel_node_pass_param( preprocess_node, preprocess_params,
                            _SCATTER_ND_UPDATE_PREPROCESS_PARAM_NUM );
                CHECK_STATUS(status);
                vsi_nn_kernel_tensor_release( &preprocess_params[0] );
                vsi_nn_kernel_scalar_release( &preprocess_params[2] );
                vsi_nn_kernel_scalar_release( &preprocess_params[3] );
            }

            // update
            process_node = vsi_nn_kernel_create_node( graph, ikernels[1] );
            if (process_node)
            {
                uint32_t index = 0;
                /* Pass parameters to node. */
                process_params[index++] = vsi_nn_kernel_tensor_reshape( inputs[1]->t,  shapes[0], rs_idx_dim );
                process_params[index++] = vsi_nn_kernel_tensor_reshape( inputs[2]->t,  shapes[1], rs_in_dim );
                process_params[index++] = (vsi_nn_kernel_node_param_t)tensors[0]->t;
                process_params[index++] = (vsi_nn_kernel_node_param_t)tensors[1]->t;
                process_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &strides[0] );
                process_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &strides[1] );
                process_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &strides[2] );
                process_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &strides[3] );
                process_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &strides[4] );
                process_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &strides[5] );
                process_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &strides[6] );
                process_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &coord_dim );
                status = vsi_nn_kernel_node_pass_param( process_node, process_params,
                                _SCATTER_ND_UPDATE_PROCESS_PARAM_NUM );
                CHECK_STATUS(status);
                vsi_nn_kernel_tensor_release( &process_params[0] );
                vsi_nn_kernel_tensor_release( &process_params[1] );
                vsi_nn_kernel_scalar_release( &process_params[4] );
                vsi_nn_kernel_scalar_release( &process_params[5] );
                vsi_nn_kernel_scalar_release( &process_params[6] );
                vsi_nn_kernel_scalar_release( &process_params[7] );
                vsi_nn_kernel_scalar_release( &process_params[8] );
                vsi_nn_kernel_scalar_release( &process_params[9] );
                vsi_nn_kernel_scalar_release( &process_params[10] );
                vsi_nn_kernel_scalar_release( &process_params[11] );
            }

            // convert float to output
            node = vsi_nn_kernel_create_node( graph, kernel );
            if ( node )
            {
                uint32_t index = 0;
                /* Pass parameters to node. */
                conv_params[index++] = (vsi_nn_kernel_node_param_t)tensors[0]->t;
                conv_params[index++] = (vsi_nn_kernel_node_param_t)tensors[1]->t;
                conv_params[index++] = vsi_nn_kernel_tensor_reshape( outputs[0]->t, shapes[2], rs_out_dim );
                conv_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &width );
                conv_params[index++] = vsi_nn_kernel_scalar_create( graph, I32, &res );
                status = vsi_nn_kernel_node_pass_param( node, conv_params, _SCATTER_ND_UPDATE_CONV_PARAM_NUM );
                CHECK_STATUS(status);
                vsi_nn_kernel_tensor_release( &conv_params[2] );
                vsi_nn_kernel_scalar_release( &conv_params[3] );
                vsi_nn_kernel_scalar_release( &conv_params[4] );
            }
        }

        if (preprocess_node) {vsi_nn_kernel_node_release( &preprocess_node );}
        if (process_node) {vsi_nn_kernel_node_release( &process_node );}
    }

final:
    if (ikernels[0])
    {
        vsi_nn_kernel_release(&ikernels[0]);
    }
    if (ikernels[1])
    {
        vsi_nn_kernel_release(&ikernels[1]);
    }
    vsi_safe_release_tensor(tensors[0]);
    vsi_safe_release_tensor(tensors[1]);

    return node;
} /* _setup() */

__END_DECLS

REGISTER_BACKEND_EVIS( scatter_nd_update_reduction, _setup )
