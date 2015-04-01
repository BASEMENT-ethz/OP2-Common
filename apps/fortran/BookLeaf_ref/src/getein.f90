
!Crown Copyright 2014 AWE.
!
! This file is part of Bookleaf.
!
! Bookleaf is free software: you can redistribute it and/or modify it under
! the terms of the GNU General Public License as published by the
! Free Software Foundation, either version 3 of the License, or (at your option)
! any later version.
!
! Bookleaf is distributed in the hope that it will be useful, but
! WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
! FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
! details.
!
! You should have received a copy of the GNU General Public License along with
! Bookleaf. If not, see http://www.gnu.org/licenses/.

MODULE getein_mod

  IMPLICIT NONE

  PUBLIC :: getein

CONTAINS

  SUBROUTINE getein(dt,d_ein_out,d_elfx,d_elfy,d_elu,d_elv)

    USE kinds_mod,    ONLY: ink,rlk
    USE pointers_mod, ONLY: elmass,ein
    USE reals_mod,    ONLY: zerocut
    USE timing_mod,   ONLY: bookleaf_times
    USE TYPH_util_mod,ONLY: get_time
    USE OP2_Fortran_Reference
    use, intrinsic :: ISO_C_BINDING
    USE op2_bookleaf, ONLY: m_el2node,m_el2el,s_elements, &
&                           d_elmass,d_ein
    USE getein_kernels

    ! Argument list
    REAL(KIND=rlk),                      INTENT(IN)  :: dt
    type(op_dat),INTENT(INOUT) :: d_ein_out,d_elfx,d_elfy,d_elu,d_elv

    ! Local
    INTEGER(KIND=ink)                                :: iel
    REAL(KIND=rlk)                                   :: w1,t0,t1

    ! Timer
    t0=get_time()

    !# Missing code here that can't be merged
    ! FdS internal energy update
    call op_par_loop_8(getein_update,s_elements, &
&           op_arg_dat(d_elfx,   -1,OP_ID,4,'real(8)',OP_READ), &
&           op_arg_dat(d_elfy,   -1,OP_ID,4,'real(8)',OP_READ), &
&           op_arg_dat(d_elu,    -1,OP_ID,4,'real(8)',OP_READ), &
&           op_arg_dat(d_elv,    -1,OP_ID,4,'real(8)',OP_READ), &
&           op_arg_dat(d_elmass, -1,OP_ID,1,'real(8)',OP_READ), &
&           op_arg_dat(d_ein,    -1,OP_ID,1,'real(8)',OP_READ), &
&           op_arg_dat(d_ein_out,-1,OP_ID,1,'real(8)',OP_WRITE),&
&           op_arg_gbl(dt,1,'real(8)',OP_READ))

    !# Missing code here that can't be merged

    ! Timing data
    t1=get_time()
    t1=t1-t0
    bookleaf_times%time_in_getein=bookleaf_times%time_in_getein+t1

  END SUBROUTINE getein

END MODULE getein_mod
