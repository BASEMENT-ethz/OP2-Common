
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

SUBROUTINE init_memory()

  USE kinds_mod,   ONLY: ink
  USE integers_mod,ONLY: nshape,nel1,nnod1
  USE logicals_mod,ONLY: zsp
  USE error_mod,   ONLY: halt
  USE paradef_mod, ONLY: ielsort1
  USE pointers_mod,ONLY: ielreg,ielmat,ielnod,rho,qq,csqrd,pre,ein,cnwt, &
&                        elmass,elvol,ndu,ndv,a1,a2,a3,b1,b2,b3,ndx,ndy, &
&                        indtype,ielel,cnmass,elx,ely,qx,qy,spmass,ielsd, &
&                        ndmass,ndarea
  USE scratch_mod, ONLY: rscratch21,rscratch22,rscratch23,rscratch24,    &
&                        rscratch25,rscratch26,rscratch27,rscratch11,    &
&                        rscratch12,rscratch13,rscratch14,rscratch15

  IMPLICIT NONE

  ! Local
  INTEGER(KIND=ink) :: ierr,isz

  ALLOCATE(ielreg(1:nel1),ielmat(1:nel1),ielnod(nshape,1:nel1),rho(1:nel1), &
&          qq(1:nel1),csqrd(1:nel1),pre(1:nel1),ein(1:nel1),elmass(1:nel1), &
&          elvol(1:nel1),ndu(1:nnod1),ndv(1:nnod1),a1(1:nel1),a2(1:nel1),   &
&          a3(1:nel1),b1(1:nel1),b2(1:nel1),b3(1:nel1),cnwt(nshape,1:nel1), &
&          ndx(1:nnod1),ndy(1:nnod1),indtype(1:nnod1),ielel(nshape,1:nel1), &
&          cnmass(nshape,1:nel1),elx(nshape,1:nel1),ely(nshape,1:nel1),     &
&          qx(nshape,1:nel1),qy(nshape,1:nel1),ielsort1(nel1),              &
&          ielsd(nshape,1:nel1),ndmass(1:nnod1),ndarea(1:nnod1),STAT=ierr)
  IF (ierr.NE.0_ink) CALL halt("ERROR: failed to allocate memory",0)
  isz=MAX(nel1,nnod1)
  ALLOCATE(rscratch11(1:isz),rscratch12(1:isz),rscratch13(1:isz),       &
&          rscratch14(1:isz),rscratch15(1:isz),rscratch21(nshape,1:isz),&
&          rscratch22(nshape,1:isz),rscratch23(nshape,1:isz),           &
&          rscratch24(nshape,1:isz),rscratch25(nshape,1:isz),           &
&          rscratch26(nshape,1:isz),rscratch27(nshape,1:isz),STAT=ierr)
  IF (ierr.NE.0_ink) CALL halt("ERROR: failed to allocate memory",0)
  IF (zsp) THEN
    ALLOCATE(spmass(nshape,1:nel1),STAT=ierr)
    IF (ierr.NE.0_ink) CALL halt("ERROR: failed to allocate memory",0)
  ENDIF

END SUBROUTINE init_memory

SUBROUTINE init()

  USE kinds_mod,    ONLY: ink,rlk
  USE integers_mod, ONLY: nshape,nel,nnod,nel1
  USE logicals_mod, ONLY: zsp
  USE reals_mod,    ONLY: time,time_start,mat_rho,mat_ein
  USE pointers_mod, ONLY: ielmat,rho,ein,elmass,elvol,qq,qx,qy,pre,     &
&                         csqrd,ndx,ndy,elx,ely,ielel,ielnod,ielsd,cnwt,&
&                         cnmass,spmass,indtype,ndu
  USE geometry_mod, ONLY: getgeom,getgeom2
  USE scratch_mod,  ONLY: rscratch21
  USE getpc_mod,    ONLY: getpc
  USE utilities_mod,ONLY: getconn,getsconn
  USE op2_bookleaf
  USE init_kernels
  USE common_kernels,ONLY: set_zero1,set_zero4
  USE parameters_mod,ONLY: LI
  USE op2_constants

  IMPLICIT NONE

  ! Local
  INTEGER(KIND=ink)                       :: iel,imat,ii,jj,j1,j2
  INTEGER(KIND=ink),DIMENSION(0:nshape-1) :: nodes
  REAL(KIND=rlk)                          :: x1,x2,x3,x4,y1,y2,y3,y4,w1,&
&                                            w2,w3,w4


  ! initialise connectivity
  ielel(1:,1:nel1)=getconn(nel1,nshape,ielnod(1:,1:nel1))
  ielsd(1:,1:nel1)=getsconn(nel1,nshape,ielel(1:,1:nel1))


  ! initialise node type
  DO iel=1,nel1
    nodes(0:nshape-1)=ielnod(1:nshape,iel)
    IF (COUNT(indtype(nodes).LT.0_ink).EQ.3_ink) THEN
      l1:DO ii=0,nshape-1
        IF (indtype(nodes(ii)).GT.0_ink) EXIT l1
      ENDDO l1
      ii=MOD(ii+2_ink,nshape)
      jj=nodes(ii)
      IF (jj.LE.nnod) THEN
        j1=nodes(MOD(ii+1_ink,nshape))
        j2=nodes(MOD(ii+3_ink,nshape))
        IF (((indtype(j1).EQ.-2_ink).AND.(indtype(j2).EQ.-1_ink)).OR.     &
&           ((indtype(j2).EQ.-2_ink).AND.(indtype(j1).EQ.-1_ink))) THEN
          indtype(jj)=-3_ink
        ENDIF
      ENDIF
    ENDIF
  ENDDO

  !Now everything is declared hopefully, we can pass it on to OP2
  call op2_bookleaf_declare
  call bookleaf_op2_init_const

  ! initialise time
  time=time_start

  ! initialise geometry
  CALL getgeom2(d_ndx,d_ndy,d_elx,d_ely)

  ! initialise density, energy and mass
  call op_par_loop_9(init_dem,s_elements, &
&                    op_arg_dat(d_ielmat,-1,OP_ID,   1,'integer(4)',OP_READ), &
&                    op_arg_gbl(mat_rho,LI,'real(8)',OP_READ), &
&                    op_arg_gbl(mat_ein,LI,'real(8)',OP_READ), &
&                    op_arg_dat(d_rho,   -1,OP_ID,   1,'real(8)',OP_WRITE), &
&                    op_arg_dat(d_ein,   -1,OP_ID,   1,'real(8)',OP_WRITE), &
&                    op_arg_dat(d_elmass,-1,OP_ID,   1,'real(8)',OP_READ), &
&                    op_arg_dat(d_elvol, -1,OP_ID,   1,'real(8)',OP_READ), &
&                    op_arg_dat(d_cnmass,-1,OP_ID,   4,'real(8)',OP_WRITE), &
&                    op_arg_dat(d_cnwt,  -1,OP_ID,   4,'real(8)',OP_READ))



  ! initialise subzonal pressure mass
  IF (zsp) THEN
      call op_par_loop_4(init_subz_pm,s_elements, &
&                    op_arg_dat(d_elx, -1,OP_ID,   4,'real(8)',OP_READ), &
&                    op_arg_dat(d_ely, -1,OP_ID,   4,'real(8)',OP_READ), &
&                    op_arg_dat(d_rho, -1,OP_ID,   1,'real(8)',OP_READ), &
&                    op_arg_dat(d_spmass,-1,OP_ID, 4,'real(8)',OP_WRITE))
  ENDIF

  ! initialise pressure and sound speed
  CALL getpc(d_rho,d_ein,d_pre,d_csqrd)

  ! initialise artifical viscosity
  call op_par_loop_1(set_zero1,s_elements, &
&                    op_arg_dat(d_qq,-1,OP_ID,1,'real(8)',OP_WRITE))
  call op_par_loop_1(set_zero4,s_elements, &
&                    op_arg_dat(d_qx,-1,OP_ID,4,'real(8)',OP_WRITE))
  call op_par_loop_1(set_zero4,s_elements, &
&                    op_arg_dat(d_qy,-1,OP_ID,4,'real(8)',OP_WRITE))


END SUBROUTINE init

SUBROUTINE init_comm()

  USE kinds_mod,    ONLY: ink
  USE integers_mod, ONLY: nel1
  USE paradef_mod,  ONLY: e_loc_glob,ielsort1
  USE utilities_mod,ONLY: sort
  USE error_mod,    ONLY: halt

  IMPLICIT NONE

  ielsort1(1:nel1)=sort(e_loc_glob(1:nel1))
  if (ielsort1(1).eq.-HUGE(1_ink)) then
    call halt("ERROR: sort failed for ielsort1",0)
  endif

END SUBROUTINE init_comm

SUBROUTINE init_defaults()

  USE kinds_mod,   ONLY: rlk,lok,ink
  USE strings_mod, ONLY: sfile
  USE integers_mod,ONLY: eos_type,max_seg,max_subseg
  USE reals_mod,   ONLY: time_start,time_end,dt_initial,dt_g,dt_min,    &
&                        dt_max,cfl_sf,div_sf,ccut,zcut,zerocut,pcut,   &
&                        eos_param,dencut,accut,cq1,cq2,kappaall,       &
&                        kappareg,pmeritall,pmeritreg
  USE logicals_mod,ONLY: zdtnotreg,zmidlength

  IMPLICIT NONE

  ! file defaults
  sfile='control'
  ! time defaults
  time_start=0.0_rlk
  time_end  =1.0_rlk
  dt_initial=1.0e-5_rlk
  dt_g      =1.02_rlk
  dt_min    =1.0e-8_rlk
  dt_max    =1.0e-1_rlk
  cfl_sf    =0.5_rlk
  div_sf    =0.25_rlk
  ! dt options
  zdtnotreg(:) =.FALSE._lok
  zmidlength(:)=.FALSE._lok
  ! cutoffs
  zcut=1.0e-8_rlk
  ccut=1.0e-6_rlk
  zerocut=1.0e-40_rlk
  pcut=1.0e-8_rlk
  dencut=1.0e-6_rlk
  accut=1.0e-6_rlk
  ! eos
  eos_type(:)=1_ink
  eos_param(1,:)=1.4_rlk
  eos_param(2:,:)=0.0_rlk
  ! artificial viscosity
  cq1=0.5_rlk
  cq2=0.75_rlk
  ! hourglass control
  kappaall=0.0_rlk
  kappareg(:)=0.0_rlk
  pmeritall=0.0_rlk
  pmeritreg(:)=0.0_rlk
  ! meshgen
  max_seg=50_ink
  max_subseg=5_ink

END SUBROUTINE init_defaults

SUBROUTINE init_parallel()

  USE kinds_mod,     ONLY: ink,lok
  USE paradef_mod,   ONLY: rankW,MProcW,NProcW,CommS,CommW,zparallel
  USE TYPH_util_mod, ONLY: TYPH_Init,TYPH_Get_Size,TYPH_Get_Rank,set_comm

  IMPLICIT NONE

  ! Local
  INTEGER(KIND=ink) :: ierr

  ierr=TYPH_Init()
  ierr=TYPH_Get_Size(NProcW)
  zparallel=.FALSE._lok
  IF (NProcW.GT.1_ink) zparallel=.TRUE._lok
  ierr=TYPH_Get_Rank(RankW)
  MProcW=.FALSE._lok
  IF (RankW.EQ.0_ink) MProcW=.TRUE._lok
  ierr=set_comm(CommW)
  ierr=set_comm(CommS)

END SUBROUTINE init_parallel

SUBROUTINE init_parameters()

  USE kinds_mod,   ONLY: rlk,ink
  USE reals_mod,   ONLY: kappaall,kappareg,pmeritall,pmeritreg
  USE logicals_mod,ONLY: zhg,zsp
  USE integers_mod,ONLY: nreg,nshape

  IMPLICIT NONE

  ! hourglass filter
  kappareg=MERGE(kappareg,kappaall,kappareg.GT.0.0_rlk)
  zhg=ANY(kappareg(1:nreg).GT.0.0_rlk)
  ! subzonal pressures
  pmeritreg=MERGE(pmeritreg,pmeritall,pmeritreg.GT.0.0_rlk)
  zsp=ANY(pmeritreg(1:nreg).GT.0.0_rlk)
  ! geometry
  nshape=4_ink

END SUBROUTINE init_parameters