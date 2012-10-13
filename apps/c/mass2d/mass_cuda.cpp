#include <stdio.h>
#include <stdlib.h>
#include <set>
#include <string>
#include <vector>

#include "op_lib_cpp.h"
#include "op_lib_mat.h"

#include "types.h"

void op_par_loop_mass(const char *, op_set, op_arg, op_arg);

void op_par_loop_rhs(const char *, op_set, op_arg, op_arg, op_arg);

void read_from_triangle_files(std::string basename,
                              int *nnode,
                              int *nele,
                              int **p_elem_node,
                              ValueType **p_xn)
{
  FILE *f;

  f = fopen((basename + ".node").c_str(), "r");
  int dim, tmp, ttmp;

  fscanf(f, "%d %d %d %d", nnode, &dim, &tmp, &ttmp);

  if ( dim != 3 ) {
    fprintf(stderr, "Unknown triangle format\n");
    exit(-1);
  }
  *p_xn = (ValueType *)malloc(*nnode * sizeof(ValueType) * 2);

  ValueType dummy;
  for ( int i = 0; i < *nnode; i++ ) {
    fscanf(f, "%d %g %g %g", &tmp, (*p_xn) + 2*i, (*p_xn) + 2*i + 1, &dummy);
  }
  fclose(f);
  f = fopen((basename + ".ele").c_str(), "r");
  fscanf(f, "%d %d %d", nele, &dim, &tmp);
  if ( dim != 3 ) {
    fprintf(stderr, "Unknown ele format\n");
    exit(-1);
  }
  *p_elem_node = (int *)malloc(*nele * sizeof(int) * 3);
  for ( int i = 0; i < *nele; i++ ) {
    fscanf(f, "%d %d %d %d %d", &tmp,
        (*p_elem_node) + 3*i,
        (*p_elem_node) + 3*i + 1,
        (*p_elem_node) + 3*i + 2,
        &ttmp);
    // correct for fortran numbering
    (*p_elem_node)[3*i]--;
    (*p_elem_node)[3*i+1]--;
    (*p_elem_node)[3*i+2]--;
  }
  fclose(f);
}

int main(int argc, char **argv)
{
  int *p_elem_node;
  ValueType *p_xn, *p_b, *p_f, *p_x;
  ValueType val;

  op_init(argc, argv, 5);

  int nnode = 4;
  int nele = 2;
  if ( argc > 1 ) {
    read_from_triangle_files(argv[1], &nnode, &nele, &p_elem_node, &p_xn);
  } else {
    p_elem_node = (int *)malloc(3 * nele * sizeof(int));
    p_elem_node[0] = 0;
    p_elem_node[1] = 1;
    p_elem_node[2] = 3;
    p_elem_node[3] = 2;
    p_elem_node[4] = 3;
    p_elem_node[5] = 1;


    p_xn = (ValueType *)malloc(2 * nnode * sizeof(ValueType));
    p_xn[0] = 0.0f;
    p_xn[1] = 0.0f;
    p_xn[2] = 2.0f;
    p_xn[3] = 0.0f;
    p_xn[4] = 1.0f;
    p_xn[5] = 1.0f;
    p_xn[6] = 0.0f;
    p_xn[7] = 1.5f;

    p_f = (ValueType*)malloc(nnode * sizeof(ValueType));
    p_f[0] = 1.0f;
    p_f[1] = 2.0f;
    p_f[2] = 3.0f;
    p_f[3] = 4.0f;

    p_b = (ValueType*)malloc(nnode * sizeof(ValueType));
    memset(p_b, 0, nnode * sizeof(ValueType));

    p_x = (ValueType*)malloc(nnode * sizeof(ValueType));
    memset(p_x, 0, nnode * sizeof(ValueType));
  }
  op_set nodes = op_decl_set(nnode, "nodes");
  op_set elements = op_decl_set(nele, "elements");
  op_map elem_node = op_decl_map(elements, nodes, 3, p_elem_node, "elem_node");

  op_sparsity sparsity = op_decl_sparsity(elem_node, elem_node, "sparsity");
  op_mat mat = op_decl_mat(sparsity, 1, VALUESTR, sizeof(ValueType), "mat");
  op_dat xn = op_decl_dat(nodes, 2, VALUESTR, p_xn, "xn");

  // Dat for the field initial condition
  op_dat f = op_decl_dat(nodes, 1, VALUESTR, p_f, "f");

  // Dat for the RHS vector
  op_dat b = op_decl_dat(nodes, 1, VALUESTR, p_b, "b");

  // Dat for solution
  op_dat x = op_decl_dat(nodes, 1, VALUESTR, p_x, "x");

  op_par_loop_mass("mass", elements,
                   op_arg_mat(mat, -3, elem_node, -3, elem_node, 1, VALUESTR, OP_INC),
                   op_arg_dat(xn, -3, elem_node, 2, VALUESTR, OP_READ));

  op_par_loop_rhs("rhs", elements,
                  op_arg_dat(b, -3, elem_node, 1, VALUESTR, OP_INC),
                  op_arg_dat(xn, -3, elem_node, 2, VALUESTR, OP_READ),
                  op_arg_dat(f, -3, elem_node, 1, VALUESTR, OP_READ));

  op_solve(mat, b, x);

  // Check result
  int failed = 0;
  op_fetch_data(x);
  for (int i=0; i<nnode; ++i) {
    double delta = fabs((double)p_x[i] - (double)p_f[i]);
    if (delta > TOLERANCE) {
      failed = 1;
      printf("Failed: delta = %18.16f for node %d.\n", delta, i);
    }
  }

  op_exit();
  return failed;
}