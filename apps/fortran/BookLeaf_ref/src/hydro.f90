
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

SUBROUTINE hydro()

  USE kinds_mod,    ONLY: ink,rlk
  USE integers_mod, ONLY: nel,nnod,nstep,idtel,nshape
  USE reals_mod,    ONLY: time,time_end,dt_initial
  USE getdt_mod,    ONLY: getdt
  USE lagstep_mod,  ONLY: lagstep
  USE paradef_mod,  ONLY: MProcW
  USE timing_mod,   ONLY: bookleaf_times
  USE TYPH_util_mod,ONLY: get_time
  USE pointers_mod, ONLY: rho,elmass,elvol,ielmat,ein,pre,csqrd,      &
&                           ndx,ndy,elx,ely,ndu,ndv,ielnod

  USE op2_bookleaf

  IMPLICIT NONE

  ! Local
  REAL(KIND=rlk)    :: dt,t0,t1,t2,grind

  ! Timer
  bookleaf_times%time_hydro=get_time()

  ! initialise
  nstep=0_ink
  dt=dt_initial

  l1:DO
    t0=get_time()
    ! increment step
    nstep=nstep+1_ink
    ! calculate timestep
    IF (nstep.GT.1_ink) CALL getdt(dt)
    !# Missing code here that can't be merged
    ! update time
    time=time+dt
    !# Code here that can't be taken out
    ! lagrangian step
    CALL lagstep(dt)
    !# Missing code here that can't be merged
    t1=get_time()
    grind=(t1-t0)*1.0e6_rlk/nel
    IF (MProcW) THEN
!      WRITE(6,'(" step=",i7,"  el=",i7,"  dt=",1pe13.6,"  time=",'      &
!&      //'1pe13.6,"  grind=",1pe8.1)') nstep,idtel,dt,time,grind
      WRITE(6,'(" step=",i7,"  el=",i7,"  dt=",1pe13.6,"  time=",'      &
&      //'1pe13.6)') nstep,idtel,dt,time

    ENDIF
    ! IO Timing data
    t2=get_time()
    t2=t2-t1
    bookleaf_times%time_step_io=bookleaf_times%time_step_io+t2
    ! test for end of calculation
    IF (time.GE.time_end) EXIT l1
    !# test for resources
  ENDDO l1

END SUBROUTINE hydro