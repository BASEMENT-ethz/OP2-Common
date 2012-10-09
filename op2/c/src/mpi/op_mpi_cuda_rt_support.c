/*
 * Open source copyright declaration based on BSD open source template:
 * http://www.opensource.org/licenses/bsd-license.php
 *
 * This file is part of the OP2 distribution.
 *
 * Copyright (c) 2011, Mike Giles and others. Please see the AUTHORS file in
 * the main source directory for a full list of copyright holders.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of Mike Giles may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Mike Giles ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Mike Giles BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// This file implements the MPI+CUDA-specific run-time support functions
//

//
// header files
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <math_constants.h>

#include <op_lib_c.h>
#include <op_lib_core.h>
#include <op_rt_support.h>
#include <op_cuda_rt_support.h>

#include <op_lib_mpi.h>
#include <op_util.h>

//
//export lists on the device
//

int** export_exec_list_d;
int** export_nonexec_list_d;

void op_exchange_halo (op_arg* arg)
{
//  printf("CALLING THE CUDA FUNCTION\n");
//  fflush(0);

  op_dat dat = arg->dat;

  if((arg->argtype == OP_ARG_DAT) &&
    (arg->acc == OP_READ || arg->acc == OP_RW ) &&
      (dat->dirtybit == 1)) {

//    printf("Exchanging Halo of data array %10s\n",dat->name);
    halo_list imp_exec_list = OP_import_exec_list[dat->set->index];
    halo_list imp_nonexec_list = OP_import_nonexec_list[dat->set->index];

    halo_list exp_exec_list = OP_export_exec_list[dat->set->index];
    halo_list exp_nonexec_list = OP_export_nonexec_list[dat->set->index];

    //-------first exchange exec elements related to this data array--------

    //sanity checks
    if(compare_sets(imp_exec_list->set,dat->set) == 0)
    {
      printf("Error: Import list and set mismatch\n");
      MPI_Abort(OP_MPI_WORLD, 2);
    }
    if(compare_sets(exp_exec_list->set,dat->set) == 0)
    {
      printf("Error: Export list and set mismatch\n");
      MPI_Abort(OP_MPI_WORLD, 2);
    }

    gather_data_to_buffer(*arg, exp_exec_list, exp_nonexec_list);

    cutilSafeCall (cudaMemcpy (OP_mpi_buffer_list[dat->index]-> buf_exec,
      arg->dat->buffer_d, exp_exec_list->size*arg->dat->size, cudaMemcpyDeviceToHost));

    cutilSafeCall (cudaMemcpy (OP_mpi_buffer_list[dat->index]-> buf_nonexec,
      arg->dat->buffer_d+exp_exec_list->size*arg->dat->size,
        exp_nonexec_list->size*arg->dat->size,
        cudaMemcpyDeviceToHost));

    cutilSafeCall (cudaThreadSynchronize ());

    for(int i=0; i<exp_exec_list->ranks_size; i++) {
      MPI_Isend(&OP_mpi_buffer_list[dat->index]->
          buf_exec[exp_exec_list->disps[i]*dat->size],
          dat->size*exp_exec_list->sizes[i],
          MPI_CHAR, exp_exec_list->ranks[i],
          dat->index, OP_MPI_WORLD,
          &OP_mpi_buffer_list[dat->index]->
          s_req[OP_mpi_buffer_list[dat->index]->s_num_req++]);
    }


    int init = dat->set->size*dat->size;
    for(int i=0; i < imp_exec_list->ranks_size; i++) {
      MPI_Irecv(&(OP_dat_list[dat->index]->
            data[init+imp_exec_list->disps[i]*dat->size]),
          dat->size*imp_exec_list->sizes[i],
          MPI_CHAR, imp_exec_list->ranks[i],
          dat->index, OP_MPI_WORLD,
          &OP_mpi_buffer_list[dat->index]->
          r_req[OP_mpi_buffer_list[dat->index]->r_num_req++]);
    }


    //-----second exchange nonexec elements related to this data array------
    //sanity checks
    if(compare_sets(imp_nonexec_list->set,dat->set) == 0) {
      printf("Error: Non-Import list and set mismatch");
      MPI_Abort(OP_MPI_WORLD, 2);
    }
    if(compare_sets(exp_nonexec_list->set,dat->set)==0) {
      printf("Error: Non-Export list and set mismatch");
      MPI_Abort(OP_MPI_WORLD, 2);
    }

    for(int i=0; i<exp_nonexec_list->ranks_size; i++) {
      MPI_Isend(&OP_mpi_buffer_list[dat->index]->
          buf_nonexec[exp_nonexec_list->disps[i]*dat->size],
          dat->size*exp_nonexec_list->sizes[i],
          MPI_CHAR, exp_nonexec_list->ranks[i],
          dat->index, OP_MPI_WORLD,
          &OP_mpi_buffer_list[dat->index]->
          s_req[OP_mpi_buffer_list[dat->index]->s_num_req++]);
    }

    int nonexec_init = (dat->set->size+imp_exec_list->size)*dat->size;
    for(int i=0; i<imp_nonexec_list->ranks_size; i++) {
      MPI_Irecv(&(OP_dat_list[dat->index]->
            data[nonexec_init+imp_nonexec_list->disps[i]*dat->size]),
          dat->size*imp_nonexec_list->sizes[i],
          MPI_CHAR, imp_nonexec_list->ranks[i],
          dat->index, OP_MPI_WORLD,
          &OP_mpi_buffer_list[dat->index]->
          r_req[OP_mpi_buffer_list[dat->index]->r_num_req++]);
    }

    //clear dirty bit
    dat->dirtybit = 0;
    arg->sent = 1;
  }
}

void op_wait_all(op_arg* arg)
{
  if(arg->argtype == OP_ARG_DAT && arg->sent == 1)
  {
    op_dat dat = arg->dat;
    MPI_Waitall(OP_mpi_buffer_list[dat->index]->s_num_req,
      OP_mpi_buffer_list[dat->index]->s_req,
      MPI_STATUSES_IGNORE );
    MPI_Waitall(OP_mpi_buffer_list[dat->index]->r_num_req,
      OP_mpi_buffer_list[dat->index]->r_req,
      MPI_STATUSES_IGNORE );
    OP_mpi_buffer_list[dat->index]->s_num_req = 0;
    OP_mpi_buffer_list[dat->index]->r_num_req = 0;

    int init = dat->set->size*dat->size;
    cutilSafeCall( cudaMemcpy( dat->data_d + init, dat->data + init,
          OP_import_exec_list[dat->set->index]->size*arg->dat->size,
          cudaMemcpyHostToDevice ) );

    int nonexec_init = (dat->set->size+OP_import_exec_list[dat->set->index]->size)*dat->size;
    cutilSafeCall( cudaMemcpy( dat->data_d + nonexec_init, dat->data + nonexec_init,
          OP_import_nonexec_list[dat->set->index]->size*arg->dat->size,
          cudaMemcpyHostToDevice ) );

    cutilSafeCall(cudaThreadSynchronize ());
  }
}

void op_partition(const char* lib_name, const char* lib_routine,
  op_set prime_set, op_map prime_map, op_dat coords )
{
  partition(lib_name, lib_routine, prime_set, prime_map, coords );

  for(int s = 0; s<OP_set_index; s++)
  {
    op_set set=OP_set_list[s];
    for(int d=0; d<OP_dat_index; d++) { //for each data array
      op_dat dat=OP_dat_list[d];

      if(dat->set->index == set->index)
          op_mv_halo_device(set, dat);
    }
  }

  op_mv_halo_list_device();

}




// SEQ added for HYDRA....
void op_exchange_halo_seq(op_arg* arg)
{
  int rank;
  op_dat dat = arg->dat;

  MPI_Comm_rank (MPI_COMM_WORLD, &rank);

  if(arg->sent == 1) {
    printf("Error: Halo exchange already in flight for dat %s\n", dat->name);
    fflush(stdout);
    MPI_Abort(OP_MPI_WORLD, 2);
  }

  if(arg->dat != NULL && //(arg->idx != -1) &&
     (arg->acc == OP_READ || arg->acc == OP_RW /* good for debug || arg->acc == OP_INC*/) &&
     (dat->dirtybit == 1))
  {
//    printf("Rank = %d: exchanging Halo of data array %s\n", rank, dat->name);
//    fflush (stdout);
    halo_list imp_exec_list = OP_import_exec_list[dat->set->index];
    halo_list imp_nonexec_list = OP_import_nonexec_list[dat->set->index];

    halo_list exp_exec_list = OP_export_exec_list[dat->set->index];
    halo_list exp_nonexec_list = OP_export_nonexec_list[dat->set->index];

    //-------first exchange exec elements related to this data array--------

    //sanity checks
    if(compare_sets(imp_exec_list->set,dat->set) == 0)
    {
      printf("Error: Import list and set mismatch\n");
      MPI_Abort(OP_MPI_WORLD, 2);
    }
    if(compare_sets(exp_exec_list->set,dat->set) == 0)
    {
      printf("Error: Export list and set mismatch\n");
      MPI_Abort(OP_MPI_WORLD, 2);
    }

    int set_elem_index;
    for(int i=0; i<exp_exec_list->ranks_size; i++) {
      for(int j = 0; j < exp_exec_list->sizes[i]; j++)
      {
        set_elem_index = exp_exec_list->list[exp_exec_list->disps[i]+j];        
//        printf ("first memcpy, size = %d\n", dat->size);
        memcpy(&OP_mpi_buffer_list[dat->index]->
            buf_exec[exp_exec_list->disps[i]*dat->size+j*dat->size],
            (void *)&dat->data[dat->size*(set_elem_index)],dat->size);
      }
//      printf("export from %d to %d data %10s, number of elements of size %d | sending:\n ",
//                my_rank, exp_exec_list->ranks[i], dat->name,exp_exec_list->sizes[i]);
//        printf ("first mpisend\n");
      MPI_Isend(&OP_mpi_buffer_list[dat->index]->
          buf_exec[exp_exec_list->disps[i]*dat->size],
          dat->size*exp_exec_list->sizes[i],
          MPI_CHAR, exp_exec_list->ranks[i],
          dat->index, OP_MPI_WORLD,
          &OP_mpi_buffer_list[dat->index]->
          s_req[OP_mpi_buffer_list[dat->index]->s_num_req++]);
    }


    int init = dat->set->size*dat->size;
    for(int i=0; i < imp_exec_list->ranks_size; i++) {
//      printf("import on to %d from %d data %10s, number of elements of size %d | recieving:\n ",
//            my_rank, imp_exec_list.ranks[i], dat.name, imp_exec_list.sizes[i]);
//        printf ("first recv\n");
      MPI_Irecv(&(OP_dat_list[dat->index]->
            data[init+imp_exec_list->disps[i]*dat->size]),
          dat->size*imp_exec_list->sizes[i],
          MPI_CHAR, imp_exec_list->ranks[i],
          dat->index, OP_MPI_WORLD,
          &OP_mpi_buffer_list[dat->index]->
          r_req[OP_mpi_buffer_list[dat->index]->r_num_req++]);
    }

//    printf("Rank = %d: during exchanging Halo of data array %s\n", rank, dat->name);
//    fflush (stdout);

    //-----second exchange nonexec elements related to this data array------
    //sanity checks
    if(compare_sets(imp_nonexec_list->set,dat->set) == 0)
    {
      printf("Error: Non-Import list and set mismatch");
      MPI_Abort(OP_MPI_WORLD, 2);
    }
    if(compare_sets(exp_nonexec_list->set,dat->set)==0)
    {
      printf("Error: Non-Export list and set mismatch");
      MPI_Abort(OP_MPI_WORLD, 2);
    }

//    printf("Rank = %d: second exchange for %s\n", rank, dat->name);
//    fflush (stdout);

    for(int i=0; i<exp_nonexec_list->ranks_size; i++) {
      for(int j = 0; j < exp_nonexec_list->sizes[i]; j++)
      {
//        int rank;
        set_elem_index = exp_nonexec_list->list[exp_nonexec_list->disps[i]+j];
//        if (myrank == 1)
//        printf ("rank = %d, first memcpy, size = %d\n", rank, dat->size);
//        printf ("second memcpy, dat->dim = %d, dat->size = %d, j = %d, exp_nonexec_list->disps[i] = %d, dat->index = %d\n",
//          dat->dim, dat->size, j, exp_nonexec_list->disps[i], dat->index);
        memcpy(&OP_mpi_buffer_list[dat->index]->
            buf_nonexec[exp_nonexec_list->disps[i]*dat->size+j*dat->size],
            (void *)&dat->data[dat->size*(set_elem_index)],dat->size);
      }
//        printf ("second send\n");
      MPI_Isend(&OP_mpi_buffer_list[dat->index]->
          buf_nonexec[exp_nonexec_list->disps[i]*dat->size],
          dat->size*exp_nonexec_list->sizes[i],
          MPI_CHAR, exp_nonexec_list->ranks[i],
          dat->index, OP_MPI_WORLD,
          &OP_mpi_buffer_list[dat->index]->
          s_req[OP_mpi_buffer_list[dat->index]->s_num_req++]);
    }

    int nonexec_init = (dat->set->size+imp_exec_list->size)*dat->size;
    for(int i=0; i<imp_nonexec_list->ranks_size; i++) {
//        printf ("second recv\n");
      MPI_Irecv(&(OP_dat_list[dat->index]->
            data[nonexec_init+imp_nonexec_list->disps[i]*dat->size]),
          dat->size*imp_nonexec_list->sizes[i],
          MPI_CHAR, imp_nonexec_list->ranks[i],
          dat->index, OP_MPI_WORLD,
          &OP_mpi_buffer_list[dat->index]->
          r_req[OP_mpi_buffer_list[dat->index]->r_num_req++]);
    }
    //clear dirty bit
    dat->dirtybit = 0;
    arg->sent = 1;

//    printf("Rank = %d: finished exchanging Halo of data array %s\n", rank, dat->name);
//    fflush (stdout);

  }
}

/*******************************************************************************
 * MPI Halo Exchange Wait-all Function (to complete the non-blocking comms)
 *******************************************************************************/

void op_wait_all_seq(op_arg* arg)
{
  if(arg->argtype == OP_ARG_DAT && arg->sent == 1)
  {
    op_dat dat = arg->dat;
    MPI_Waitall(OP_mpi_buffer_list[dat->index]->s_num_req,
      OP_mpi_buffer_list[dat->index]->s_req,
      MPI_STATUSES_IGNORE );
    MPI_Waitall(OP_mpi_buffer_list[dat->index]->r_num_req,
      OP_mpi_buffer_list[dat->index]->r_req,
      MPI_STATUSES_IGNORE );
    OP_mpi_buffer_list[dat->index]->s_num_req = 0;
    OP_mpi_buffer_list[dat->index]->r_num_req = 0;
  }

  //arg->sent = 0;
}




/*******************************************************************************
* Monitir/Print the Contents/Original Global Index/Current Index/Rank of an
* element in op_dat
*******************************************************************************/
void op_monitor_dat_mpi(op_dat dat, int original_g_index)
{
  int my_rank, comm_size;
  MPI_Comm_rank(OP_MPI_WORLD, &my_rank);
  MPI_Comm_size(OP_MPI_WORLD, &comm_size);
  
  //check if the element requested is held in local mpi process
  int local_index = linear_search(OP_part_list[dat->set->index]->g_index,
    original_g_index, 0, dat->set->size - 1);
  
  if(local_index >= 0)
  {
    if(strcmp(dat->type,"double") == 0)
    {
      double* value = (double *)xmalloc(sizeof(double)*dat->dim);
      memcpy(value, (void *)(&dat->data[local_index*dat->size]), sizeof(double)*dat->dim);
      printf("op_dat %s element %d located on mpi rank %d at local index: %d value: ",
        dat->name, original_g_index, my_rank, local_index);
      for(int i = 0; i<dat->dim; i++)
        printf("%lf ",value[i]);
      printf("\n");
      free(value);
    }
    else if(strcmp(dat->type,"float") == 0)
    {
      float* value = (float *)xmalloc(sizeof(float)*dat->dim);
      memcpy(value, (void *)(&dat->data[local_index*dat->size]), sizeof(float)*dat->dim);
      printf("op_dat %s element %d located on mpi rank %d at local index: %d value: ",
        dat->name, original_g_index, my_rank, local_index);
      for(int i = 0; i<dat->dim; i++)
        printf("%f ",value[i]);
      printf("\n");
      free(value);
    }
    else if(strcmp(dat->type,"int") == 0)
    {
      int* value = (int *)xmalloc(sizeof(int)*dat->dim);
      memcpy(value, (void *)(&dat->data[local_index*dat->size]), sizeof(int)*dat->dim);
      printf("op_dat %s element %d located on mpi rank %d at local index: %d value: ",
        dat->name, original_g_index, my_rank, local_index);
      for(int i = 0; i<dat->dim; i++)
        printf("%d ",value[i]);
      printf("\n");
      free(value);
    }
    if(strcmp(dat->type,"long") == 0)
    {
      long* value = (long *)xmalloc(sizeof(long)*dat->dim);
      memcpy(value, (void *)(&dat->data[local_index*dat->size]), sizeof(long)*dat->dim);
      printf("op_dat %s element %d located on mpi rank %d at local index: %d value: ",
        dat->name, original_g_index, my_rank, local_index);
      for(int i = 0; i<dat->dim; i++)
        printf("%ld ",value[i]);
      printf("\n");
      free(value);
    }
  }
}


void op_monitor_map_mpi(op_map map, int original_g_index)
{
  int my_rank, comm_size;
  MPI_Comm_rank(OP_MPI_WORLD, &my_rank);
  MPI_Comm_size(OP_MPI_WORLD, &comm_size);
  
  /* Compute global partition range information for each set*/
  int** part_range = (int **)xmalloc(OP_set_index*sizeof(int*));
  get_part_range(part_range,my_rank,comm_size, MPI_COMM_WORLD);
    
  //check if the element requested is held in local mpi process
  int local_index = linear_search(OP_part_list[map->from->index]->g_index, 
    original_g_index, 0, map->from->size - 1);
  
  if(local_index >= 0)
  {
    int* value_c_l = (int *)xmalloc(sizeof(int)*map->dim);
    memcpy(value_c_l, (void *)(&map->map[local_index*map->dim]), sizeof(int)*map->dim);
      
    int* value_c_g = (int *)xmalloc(sizeof(int)*map->dim);
    for(int i = 0; i<map->dim; i++)
      value_c_g[i] = get_global_index(value_c_l[i], my_rank, part_range[map->to->index],comm_size);    
    
    int* value_o_l = (int *)xmalloc(sizeof(int)*map->dim);
    int* orig_rank = (int *)xmalloc(sizeof(int)*map->dim);
       
    int* value_o_g = (int *)xmalloc(sizeof(int)*map->dim);
    for(int i = 0; i<map->dim; i++)
      value_o_g[i] = OP_part_list[map->to->index]->g_index[value_c_l[i]];
    
    for(int i = 0; i<map->dim; i++)
      orig_rank[i] = get_partition(value_o_g[i], orig_part_range[map->to->index], 
        &value_o_l[i], comm_size);
      
    printf("op_map %s element (from %s to %s) at original global index %d ",
      map->name, map->from->name, map->to->name, original_g_index);
    printf("is now located on mpi rank %d at local index: %d \n",
      my_rank, local_index);
    
    printf("points to current to-set elements : \n");
    for(int i = 0; i<map->dim; i++)
    {
      printf("-> with curr local index: %d curr global index: %d ",value_c_l[i], value_c_g[i]);
      printf("originally located on mpi rank %d, ",orig_rank[i]);
      printf("orig local index: %d orig global index: %d\n",value_o_l[i], value_o_g[i]);
    }
    printf("\n");
    fflush(stdout);
    
    free(value_c_l);
    free(value_c_g);
    free(value_o_l);
    free(value_o_g);
    free(orig_rank);
  }
  
  for(int i = 0; i<OP_set_index; i++)free(part_range[i]);free(part_range);  
}
