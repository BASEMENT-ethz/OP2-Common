//
// auto-generated by op2.py
//

//user function
#include "spMV.h"

// host stub function
void op_par_loop_spMV(char const *name, op_set set,
  op_arg arg0,
  op_arg arg4,
  op_arg arg5){

  int nargs = 9;
  op_arg args[9];

  arg0.idx = 0;
  args[0] = arg0;
  for ( int v=1; v<4; v++ ){
    args[0 + v] = op_arg_dat(arg0.dat, v, arg0.map, 1, "double", OP_INC);
  }

  args[4] = arg4;
  arg5.idx = 0;
  args[5] = arg5;
  for ( int v=1; v<4; v++ ){
    args[5 + v] = op_arg_dat(arg5.dat, v, arg5.map, 1, "double", OP_READ);
  }


  // initialise timers
  double cpu_t1, cpu_t2, wall_t1, wall_t2;
  op_timing_realloc(3);
  op_timers_core(&cpu_t1, &wall_t1);

  int  ninds   = 2;
  int  inds[9] = {0,0,0,0,-1,1,1,1,1};

  if (OP_diags>2) {
    printf(" kernel routine with indirection: spMV\n");
  }

  // get plan
  #ifdef OP_PART_SIZE_3
    int part_size = OP_PART_SIZE_3;
  #else
    int part_size = OP_part_size;
  #endif

  int set_size = op_mpi_halo_exchanges(set, nargs, args);

  if (set->size >0) {

    op_plan *Plan = op_plan_get(name,set,part_size,nargs,args,ninds,inds);

    // execute plan
    int block_offset = 0;
    for ( int col=0; col<Plan->ncolors; col++ ){
      if (col==Plan->ncolors_core) {
        op_mpi_wait_all(nargs, args);
      }
      int nblocks = Plan->ncolblk[col];

      #pragma omp parallel for
      for ( int blockIdx=0; blockIdx<nblocks; blockIdx++ ){
        int blockId  = Plan->blkmap[blockIdx + block_offset];
        int nelem    = Plan->nelems[blockId];
        int offset_b = Plan->offset[blockId];
        for ( int n=offset_b; n<offset_b+nelem; n++ ){
          int map0idx = arg0.map_data[n * arg0.map->dim + 0];
          int map1idx = arg0.map_data[n * arg0.map->dim + 1];
          int map2idx = arg0.map_data[n * arg0.map->dim + 2];
          int map3idx = arg0.map_data[n * arg0.map->dim + 3];

          double* arg0_vec[] = {
             &((double*)arg0.data)[1 * map0idx],
             &((double*)arg0.data)[1 * map1idx],
             &((double*)arg0.data)[1 * map2idx],
             &((double*)arg0.data)[1 * map3idx]};
          double* arg5_vec[] = {
             &((double*)arg5.data)[1 * map0idx],
             &((double*)arg5.data)[1 * map1idx],
             &((double*)arg5.data)[1 * map2idx],
             &((double*)arg5.data)[1 * map3idx]};

          spMV(
            arg0_vec,
            &((double*)arg4.data)[16 * n],
            arg5_vec);
        }
      }

      block_offset += nblocks;
    }
    OP_kernels[3].transfer  += Plan->transfer;
    OP_kernels[3].transfer2 += Plan->transfer2;
  }

  if (set_size == 0 || set_size == set->core_size) {
    op_mpi_wait_all(nargs, args);
  }
  // combine reduction data
  op_mpi_set_dirtybit(nargs, args);

  // update kernel record
  op_timers_core(&cpu_t2, &wall_t2);
  OP_kernels[3].name      = name;
  OP_kernels[3].count    += 1;
  OP_kernels[3].time     += wall_t2 - wall_t1;
}
