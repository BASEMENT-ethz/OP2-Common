//
// auto-generated by op2.m on 25-Mar-2012 09:54:14
//

// header

#include "op_lib_cpp.h"

#include "op_cuda_rt_support.h"
#include "op_cuda_reduction.h"
// global constants

#ifndef MAX_CONST_SIZE
#define MAX_CONST_SIZE 128
#endif

__constant__ double alpha;

void op_decl_const_char(int dim, char const *type,
            int size, char *dat, char const *name){
  cutilSafeCall(cudaMemcpyToSymbol(name, dat, dim*size));
}

// user kernel files

#include "res_kernel.cu"
#include "update_kernel.cu"
