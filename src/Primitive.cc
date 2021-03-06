/*
 Authors 
 Martin Schlather, schlather@math.uni-mannheim.de

 Definition of correlation functions and derivatives (spectral measures, 
 tbm operators)

 Copyright (C) 2001 -- 2003 Martin Schlather
 Copyright (C) 2004 -- 2004 Yindeng Jiang & Martin Schlather
 Copyright (C) 2005 -- 2015 Martin Schlather
 Copyright (C) 2015-2017 Olga Moreva (bivariate models) & Martin Schlather
 Copyright (C) 2018 -- 2018 Martin Schlather
 

Note:
 * Never use the below functions directly, but only by the functions indicated 
   in RFsimu.h, since there is no error check (e.g. initialization of RANDOM)
 * VARIANCE, SCALE are not used here 
 * definitions for the random coin method can be found in MPPFcts.cc
 * definitions for genuinely anisotropic or nondomain models are in
   SophisticatedModel.cc; hyper models also in Hypermodel.cc


This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.  
*/



#include <Rmath.h>
#include <R_ext/Lapack.h>
#include <R_ext/Linpack.h>
#include <Rmath.h>
#include "RF.h"
#include "primitive.h"
#include "Operator.h"
#include "Coordinate_systems.h"

#define LOG2 M_LN2

#define i11 0
#define i21 1
#define i22 2

#define epsilon 1e-15


#define GOLDENR 0.61803399
#define GOLDENC (1.0 -GOLDENR)
#define GOLDENTOL 1e-6

#define GOLDENSHIFT2(a, b, c) (a) = (b); (b) = (c);
#define GOLDENSHIFT3(a, b, c, d) (a) = (b); (b) = (c); (c) = (d);

double BesselUpperB[Nothing + 1] =
{80, 80, 80, // circulant
 80, RF_INF, // TBM
 80, 80,    // direct & sequ
 RF_NA, RF_NA, RF_NA,  // GMRF, ave, nugget
 RF_NA, RF_NA, RF_NA,  // exotic
 RF_INF   // Nothing
};


double interpolate(double y, double *stuetz, int nstuetz, int origin,
		 double lambda, int delta)
{
  int index,minindex,maxindex,i;
  double weights,sum,diff,a;
  index  = origin + (int) y;
  minindex = index - delta; if (minindex<0) minindex=0;
  maxindex = index + 1 + delta; if (maxindex>nstuetz) maxindex=nstuetz;
  weights = sum = 0.0;
  for (i=minindex;i<maxindex;i++) {    
    diff = y + (double) (index-i);
    a    = EXP( -lambda * diff * diff);
    weights += a;
    sum += a * stuetz[i];  // or  a/stuetz[i]
  }
  return (double) (weights/sum); // and then   sum/weights       
}




/* BCW */
#define BCW_ALPHA 0
#define BCW_BETA 1
#define BCW_C 2
#define BCW_EPS 1e-7
#define BCW_TAYLOR_ZETA \
  (- LOG2 * (1.0 + 0.5 * zetalog2 * (1.0 + ONETHIRD * zetalog2)))
#define BCW_CAUCHY (POW(1.0 + POW(*x, alpha), zeta) - 1.0)
void bcw(double *x, cov_model *cov, double *v){
  double alpha = P0(BCW_ALPHA), beta=P0(BCW_BETA),
    zeta = beta / alpha,
    absZeta = FABS(zeta);
  if (absZeta > BCW_EPS) 
    *v = BCW_CAUCHY / (1.0 - POW(2.0, zeta));
  else {
    double dewijs = LOG(1.0 + POW(*x, alpha)),
      zetadewijs = zeta * dewijs,
      zetalog2 = zeta * LOG2
      ;
    if (FABS(zetadewijs) > BCW_EPS)
      *v = BCW_CAUCHY / (zeta * BCW_TAYLOR_ZETA);
    else {
      *v = dewijs * (1.0 + 0.5 * zetadewijs * (1.0 + ONETHIRD *zetadewijs))
	/ BCW_TAYLOR_ZETA;
    }
  }
  if (!PisNULL(BCW_C)) *v += P0(BCW_C);
}


void Dbcw(double *x, cov_model *cov, double *v){
  double ha,
    alpha = P0(BCW_ALPHA), 
    beta=P0(BCW_BETA),
    zeta=beta / alpha,
    absZeta = FABS(zeta),
    y=*x;
  
  if (y ==0.0) {
    *v = ((alpha > 1.0) ? 0.0 : (alpha < 1.0) ? -INFTY : alpha); 
  } else {
    ha = POW(y, alpha - 1.0);
    *v = alpha * ha * POW(1.0 + ha * y, zeta - 1.0);
  }
  
  if (absZeta > BCW_EPS) *v *= zeta / (1.0 - POW(2.0, zeta));
  else {
    double zetalog2 = zeta * LOG2;
    *v /= BCW_TAYLOR_ZETA;
  }
}


void DDbcw(double *x, cov_model *cov, double *v){
  double ha,
    alpha = P0(BCW_ALPHA), 
    beta=P0(BCW_BETA), 
    zeta = beta / alpha,
    absZeta = FABS(zeta),
     y=*x;

  if (y == 0.0) {
    *v = ((alpha==2.0) ? -beta * (1.0 - beta) : INFTY); 
  } else {
    ha = POW(y, alpha);
    *v = -alpha * ha / (y * y) * (1.0 - alpha + (1.0 - beta) * ha)
      * POW(1.0 + ha, beta / alpha - 2.0);
  }
  if (absZeta > BCW_EPS) *v *= zeta / (1.0 - POW(2.0, zeta));
  else {
    double zetalog2 = zeta * LOG2;
    *v /= BCW_TAYLOR_ZETA;
  }
}


void Inversebcw(double *x, cov_model *cov, double *v) {  
  double 
    alpha = P0(BCW_ALPHA), 
    beta=P0(BCW_BETA),
    zeta = beta / alpha,
    y = *x;
  if (y == 0.0) {
    *v = beta < 0.0 ? RF_INF : 0.0; 
    return;
  }
  if (!PisNULL(BCW_C)) y = P0(BCW_C) - y;
  if (zeta != 0.0)
    *v = POW(POW(y * (POW(2.0, zeta) - 1.0) + 1.0, 1.0/zeta) - 1.0,
	       1.0/alpha); 
  else 
    *v =  POW(EXP(y * LOG2) - 1.0, 1.0 / alpha);   
}

int checkbcw(cov_model *cov) {
  double
    alpha = P0(BCW_ALPHA), 
    beta=P0(BCW_BETA);
  if (cov->tsdim > 2)
    cov->pref[CircEmbedCutoff] = cov->pref[CircEmbedIntrinsic] = PREF_NONE;
  cov->logspeed = beta > 0 ? RF_INF : beta < 0.0 ? 0 : alpha * INVLOG2;
  return NOERROR;
}

void rangebcw(cov_model *cov, range_type *range) {
  bool tcf = isTcf(cov->typus) || cov->isoown == SPHERICAL_ISOTROPIC,
    posdef = isPosDef(cov->typus) && PisNULL(BCW_C); // tricky programming
  range->min[BCW_ALPHA] = 0.0;
  range->max[BCW_ALPHA] = tcf ? 1.0 : 2.0;
  range->pmin[BCW_ALPHA] = 0.05;
  range->pmax[BCW_ALPHA] = range->max[BCW_ALPHA];
  range->openmin[BCW_ALPHA] = true;
  range->openmax[BCW_ALPHA] = false;

  range->min[BCW_BETA] = RF_NEGINF;
  range->max[BCW_BETA] = posdef ? 0.0 : 2.0;
  range->pmin[BCW_BETA] = -10.0;
  range->pmax[BCW_BETA] = 2.0;
  range->openmin[BCW_BETA] = true;
  range->openmax[BCW_BETA] = posdef;

  range->min[BCW_C] = 0;
  range->max[BCW_C] = RF_INF;
  range->pmin[BCW_C] = 0;
  range->pmax[BCW_C] = 1000;
  range->openmin[BCW_C] = false;
  range->openmax[BCW_C] = true;
}


void coinitbcw(cov_model *cov, localinfotype *li) {
  double
    beta=P0(BCW_BETA);
  if (beta < 0) coinitgenCauchy(cov, li);
  else {
    li->instances = 0;
  }
}
void ieinitbcw(cov_model *cov, localinfotype *li) {
  if (P0(BCW_BETA) < 0) ieinitgenCauchy(cov, li);
  else {
    ieinitBrownian(cov, li); // to do: can nicht passen!!
    // todo: nachrechnen, dass OK!
  }
}



#define LOW_BESSELJ 1e-20
#define LOW_BESSELK 1e-20
#define BESSEL_NU 0
void Bessel(double *x, cov_model *cov, double *v){

  static double nuOld=RF_INF;
  static double gamma;
  double nu = P0(BESSEL_NU), 
    y=*x;
  if  (y <= LOW_BESSELJ) {*v = 1.0; return;}
  if (y == RF_INF)  {*v = 0.0; return;} // also for cosine i.e. nu=-1/2
  if (nuOld!=nu) {
    nuOld=nu;
    gamma = gammafn(nuOld+1.0);
  }
  *v = gamma  * POW(2.0 / y, nuOld) * bessel_j(y, nuOld);
}
int initBessel(cov_model *cov, gen_storage 
	       VARIABLE_IS_NOT_USED *s) {
  ASSERT_GAUSS_METHOD(SpectralTBM);
  return NOERROR;
}
int checkBessel(cov_model *cov) {
  // Whenever TBM3Bessel exists, add further check against too small nu! 
  double nu = P0(BESSEL_NU);
  int i;
  double dim = (2.0 * P0(BESSEL_NU) + 2.0);

  for (i=0; i<= Nothing; i++)
    cov->pref[i] *= (ISNAN(nu) || nu < BesselUpperB[i]);
  if (cov->tsdim>2) cov->pref[SpectralTBM] = PREF_NONE; // do to d > 2 !
  cov->maxdim = (ISNAN(dim) || dim >= INFDIM) ? INFDIM : (int) dim;

  return NOERROR;
}
void spectralBessel(cov_model *cov, gen_storage *S, double *e) { 
  spectral_storage *s = &(S->Sspectral);
  double 
    nu =  P0(BESSEL_NU);
/* see Yaglom ! */
  // nu==0.0 ? 1.0 : // not allowed anymore;
	// other wise densityBessel (to define) will not work
  if (nu >= 0.0) {
    E12(s, cov->tsdim, nu > 0 ? SQRT(1.0 - POW(UNIFORM_RANDOM, 1 / nu)) : 1, e);
  } else {
    double A;
    assert(cov->tsdim == 1);
    if (nu == -0.5) A = 1.0;
    else { // siehe private/bessel.pdf, p. 6, Remarks
      // http://homepage.tudelft.nl/11r49/documents/wi4006/bessel.pdf
      while (true) {
	A = 1.0 - POW(UNIFORM_RANDOM, 1.0 / ( P0(BESSEL_NU) + 0.5));
	if (UNIFORM_RANDOM <= POW(1 + A, nu - 0.5)) break;
      }
    }
    E1(s, A, e);
  }
}
void rangeBessel(cov_model *cov, range_type *range){
  range->min[BESSEL_NU] = 0.5 * ((double) cov->tsdim - 2.0);
  range->max[BESSEL_NU] = RF_INF;
  range->pmin[BESSEL_NU] = 0.0001 + range->min[BESSEL_NU];
  range->pmax[BESSEL_NU] = range->pmin[BESSEL_NU] + 10.0;
  range->openmin[BESSEL_NU] = false;
  range->openmax[BESSEL_NU] = true;
}


/* Cauchy models */
#define CAUCHY_GAMMA 0
void Cauchy(double *x, cov_model *cov, double *v){
  double gamma = P0(CAUCHY_GAMMA);
  *v = POW(1.0 + *x * *x, -gamma);
}
void logCauchy(double *x, cov_model *cov, double *v, double *Sign){
  double gamma = P0(CAUCHY_GAMMA);
  *v = LOG(1.0 + *x * *x) * -gamma;
  *Sign = 1.0;
}
void TBM2Cauchy(double *x, cov_model *cov, double *v){
  double gamma = P0(CAUCHY_GAMMA), y2, lpy2;
  y2 = *x * *x; 
  lpy2 = 1.0 + y2;
  switch ((int) (gamma * 2.0 + 0.001)) {// ueber check sichergestellt
  case 1 : *v = 1.0 / lpy2; break;
  case 3 : *v = (1.0 - y2)/ (lpy2 * lpy2); break;
  case 5 : *v = (1.0-y2*(2.0+0.333333333333333333333*y2))/(lpy2*lpy2*lpy2);
    break;
  case 7 : lpy2 *= lpy2; 
    *v = (1.0- y2*(3.0+y2*(1.0+0.2*y2)))/(lpy2 * lpy2);
    break;
  default :
    ERR("TBM2 for cauchy only possible for alpha=0.5 + k; k=0, 1, 2, 3 ");
  }
}
void DCauchy(double *x, cov_model *cov, double *v){
  double y=*x, gamma = P0(CAUCHY_GAMMA);
  *v = (-2.0 * gamma * y) * POW(1.0 + y * y, -gamma - 1.0);
}
void DDCauchy(double *x, cov_model *cov, double *v){
  double ha = *x * *x, gamma = P0(CAUCHY_GAMMA);
  *v = 2.0 * gamma * ((2.0 * gamma + 1.0) * ha - 1.0) * 
    POW(1.0 + ha, -gamma - 2.0);
}
void InverseCauchy(double*x, cov_model *cov, double *v){
  double
    gamma = P0(CAUCHY_GAMMA);
  if (*x == 0.0) *v = RF_INF;
  else *v = SQRT(POW(*x, -1.0 / gamma) - 1.0);
}
int checkCauchy(cov_model VARIABLE_IS_NOT_USED  *cov){
  return NOERROR;
}
void rangeCauchy(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[CAUCHY_GAMMA] = 0.0;
  range->max[CAUCHY_GAMMA] = RF_INF;
  range->pmin[CAUCHY_GAMMA] = 0.09;
  range->pmax[CAUCHY_GAMMA] = 10.0;
  range->openmin[CAUCHY_GAMMA] = true;
  range->openmax[CAUCHY_GAMMA] = true;
}
void coinitCauchy(cov_model VARIABLE_IS_NOT_USED *cov, localinfotype *li) {
  li->instances = 1;
  li->value[0] = 1.0; //  q[CUTOFF_A] 
  li->msg[0] = MSGLOCAL_JUSTTRY;
}
void DrawMixCauchy(cov_model VARIABLE_IS_NOT_USED *cov, double *random) { //better GR 3.381.4 ?? !!!!
  // split into parts <1 and >1 ?>
  *random = -LOG(1.0 -UNIFORM_RANDOM);
}
double LogMixDensCauchy(double VARIABLE_IS_NOT_USED *x, double logV, cov_model *cov) {
  double gamma = P0(CAUCHY_GAMMA);
  // da dort 1/w vorgezogen 
  return 0.5 * (gamma - 1.0) * logV - 0.5 * lgammafn(gamma);
}


/** another Cauchy model */
#define CTBM_ALPHA 0
#define CTBM_BETA 1
#define CTBM_GAMMA 2
void Cauchytbm(double *x, cov_model *cov, double *v){
  double ha, 
    alpha = P0(CTBM_ALPHA), 
    beta = P0(CTBM_BETA), 
    gamma = P0(CTBM_GAMMA),
    y=*x;
  if (y==0) {
    *v = 1.0;
  } else {
    ha = POW(y, alpha);
    *v = (1.0 + (1.0 - beta / gamma) * ha) * POW(1.0 + ha, -beta / alpha - 1.0);
  }
}

void DCauchytbm(double *x, cov_model *cov, double *v){
  double y= *x, ha, 
    alpha = P0(CTBM_ALPHA), 
    beta = P0(CTBM_BETA),
    gamma = P0(CTBM_GAMMA);
  if (y == 0.0) *v = 0.0; // WRONG VALUE, but multiplied 
  else {                                  // by zero anyway
    ha = POW(y, alpha - 1.0);
    *v = beta *  ha * (-1.0 - alpha / gamma  + ha * y * (beta / gamma - 1.0)) *
      POW(1.0 + ha * y, -beta /alpha - 2.0);
  }
}


void rangeCauchytbm(cov_model *cov, range_type *range){
  range->min[CTBM_ALPHA] = 0.0;
  range->max[CTBM_ALPHA] = 2.0;
  range->pmin[CTBM_ALPHA] = 0.05;
  range->pmax[CTBM_ALPHA] = 2.0;
  range->openmin[CTBM_ALPHA] = true;
  range->openmax[CTBM_ALPHA] = false;

  range->min[CTBM_BETA] = 0.0;
  range->max[CTBM_BETA] = RF_INF;
  range->pmin[CTBM_BETA] = 0.05;
  range->pmax[CTBM_BETA] = 10.0;
  range->openmin[CTBM_BETA] = true;
  range->openmax[CTBM_BETA] = true;
 
  range->min[CTBM_GAMMA] = (double) cov->tsdim;
  range->max[CTBM_GAMMA] = RF_INF;
  range->pmin[CTBM_GAMMA] = range->min[CTBM_GAMMA];
  range->pmax[CTBM_GAMMA] = range->pmin[CTBM_GAMMA] + 10.0;
  range->openmin[CTBM_GAMMA] = false;
  range->openmax[CTBM_GAMMA] = true;

}



/* circular model */
void circular(double *x, cov_model VARIABLE_IS_NOT_USED  *cov, double *v) {
  double y = *x;
  *v = (y >= 1.0) ? 0.0 
    : 1.0 - (2.0 * (y * SQRT(1.0 - y * y) + ASIN(y))) * INVPI;
}
void Dcircular(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){
  double y = *x * *x;
  *v = (y >= 1.0) ? 0.0 : -4 * INVPI * SQRT(1.0 - y);
}
int structCircSph(cov_model *cov, cov_model **newmodel, int dim) { 
  ASSERT_NEWMODEL_NOT_NULL;

  switch (cov->role) {
  case ROLE_POISSON_GAUSS :
    {
      addModel(newmodel, BALL, cov);      
      addModel(newmodel, DOLLAR);
      addModelKappa(*newmodel, DSCALE, SCALESPHERICAL);
      kdefault((*newmodel)->kappasub[DSCALE], SPHERIC_SPACEDIM,
	       (double) cov->tsdim);
      kdefault((*newmodel)->kappasub[DSCALE], SPHERIC_BALLDIM, (double) dim);
     } 
    break;
  case ROLE_POISSON : 
    return addUnifModel(cov, 1.0, newmodel); // to do: felix
  case ROLE_MAXSTABLE : 
    return addUnifModel(cov, 1.0, newmodel);
  default:
    BUG;
  }
  return NOERROR;
}

int structcircular(cov_model *cov, cov_model **newmodel) {
  return structCircSph(cov, newmodel, 2);
}


// spatially constant (matrix); 
#define CONSTANT_M 0

void kappaconstant(int i, cov_model VARIABLE_IS_NOT_USED *cov, int *nr, int *nc) {
  *nr = *nc = i ==  CONSTANT_M ? 0 : -1;
}

void constant(double VARIABLE_IS_NOT_USED *x, cov_model *cov, double *v){
  int 
    vdimSq = cov->vdim[0] * cov->vdim[1];
  MEMCOPY(v, P(CONSTANT_M), vdimSq * sizeof(double));
}

void nonstatconstant(double VARIABLE_IS_NOT_USED *x, double VARIABLE_IS_NOT_USED *y, cov_model *cov, double *v){
  int 
    vdimSq = cov->vdim[0] * cov->vdim[1];
  MEMCOPY(v, P(CONSTANT_M), vdimSq * sizeof(double));
}

int checkconstant(cov_model *cov) {
  int info, err; // anzahl listen elemente

 
  if (GLOBAL.internal.warn_constant) {
    GLOBAL.internal.warn_constant = false;
    warning("NOTE that the definition of 'RMconstant' has changed. Maybe  'RMfixcov' is the model your are looking for");
  }
 
  cov->vdim[0] = cov->vdim[1] = cov->nrow[CONSTANT_M];
  if (cov->vdim[0] != cov->vdim[1])  return ERROR_MATRIX_SQUARE;

  if (cov->typus == VariogramType) { // ACHTUNG wirklich genau VariogramType
    SERR("strange call");
  }

  
  if (cov->q != NULL) {
    return (int) cov->q[0];
  } else QALLOC(1);

  cov->q[0] = NOERROR;

  cov->monotone = COMPLETELY_MON;
  cov->ptwise_definite = pt_posdef;
   
  // check whether positive eigenvalue  
  long vdimSq = cov->nrow[CONSTANT_M] * cov->ncol[CONSTANT_M];
  EXTRA_STORAGE;
  ALLOC_EXTRA(dummy, vdimSq);
  MEMCOPY(dummy, P(CONSTANT_M), vdimSq * sizeof(double));    
  F77_CALL(dpotrf)("Upper", cov->nrow + CONSTANT_M, dummy,
		     cov->ncol + CONSTANT_M, &info); // cholesky
      
  if (info != NOERROR) {
    if (isPosDef(cov)) return cov->q[0] = ERROR_MATRIX_POSDEF;
    cov->monotone = MONOTONE;
    cov->ptwise_definite = pt_indef;
  }
    

  cov->matrix_indep_of_x = true;
  cov->mpp.maxheights[0] = RF_NA;
  err = checkkappas(cov);

 
  return err;
}


void rangeconstant(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){

  range->min[CONSTANT_M] = RF_NEGINF;
  range->max[CONSTANT_M] = RF_INF;
  range->pmin[CONSTANT_M] = -1e10;
  range->pmax[CONSTANT_M] = 1e10;
  range->openmin[CONSTANT_M] = true;
  range->openmax[CONSTANT_M] = true;

 }





/* Covariate models */

int cutidx(double Idx, int len) {
  int idx = (int) ROUND(Idx);
  if (idx < 0) idx = 0;
  if (idx >= len) idx = len - 1;
  return idx;
}


#define GET_LOC_COVARIATE \
  assert(cov->Scovariate != NULL);					\
 location_type **local = P0INT(COVARIATE_RAW) || PisNULL(COVARIATE_X)	\
    ? PLoc(cov) : cov->Scovariate->loc;					\
  assert(local != NULL);						\
  location_type *loc =  LocLoc(local);					\
  assert(loc != NULL)
 

int get_index(double *x, cov_model *cov) {
  GET_LOC_COVARIATE;
  
  int d, idx, i,
    nr = 0,
    cummul = 1.0,  
    ntot = loc->totalpoints,
    tsdim = cov->xdimprev;
  double X[2];

  //printf("grid = %d\n", loc->grid);

  if (loc->grid) {
    for (d = 0; d<tsdim; d++) {
      int len = loc->xgr[d][XLENGTH];
      double
	step = loc->xgr[d][XSTEP];                
      if (d > 1 || !isAnySpherical(cov->isoown)) {
	idx = cutidx((x[d] - loc->xgr[d][XSTART]) / step, len);
      } else {
	if (d == 0) { // to do: report technique?
	  double full, half, y[2];
	  int idx2,
	    dim = 2;
	  for (i=0; i<dim; i++) y[i] = loc->xgr[i][XSTART];
	  if (isSpherical(cov->isoown)) {
	    full = M_2_PI;
	    half = M_PI;
	    if (GLOBAL.coords.polar_coord) NotProgrammedYet("");
	  } else if (isEarth(cov->isoown)) {
	    full = 360.0;
	    half = 180.0;
	  } else BUG;
	  statmod2(y, full, half, X);
	  
	  idx = cutidx((x[0] - X[0]) / step, len);
	  double X0 = X[0] + full * (2.0 * (double) (x[0] > X[0]) - 1);
	  idx2 = cutidx((x[0] - X0) / step, len);
	  if (FABS(x[0] - (X0 + idx2 * step)) <
	      FABS(x[0] - (X[0] + idx * step))) idx = idx2;
	} else { 
	  assert(d==1);
	  idx = cutidx((x[d] - X[1]) / step, len);
	}
      }      
      nr += cummul * idx;

      //  printf("nr = %d\n", nr);

      cummul *= len;  
    }           
  } else { // to do: effizienterer Zugriff ueber Kaestchen eines Gitters, 
    // in dem die jeweiligen Punkte gesammelt werden. Dann muessen nur
    // 3^d Kaestchen durchsucht werden.
    
    cov_model *next = cov->sub[0];
    double distance,
      mindist = RF_INF,
      *given = loc->x;
    for (i=0; i<ntot; i++, given+=tsdim) {
      NONSTATCOV(x, given, next, &distance);
      if (distance < mindist) {
	mindist = distance;
	nr = i;
      }
    }
  }
  return nr;
}


void kappa_covariate(int i, cov_model VARIABLE_IS_NOT_USED *cov,
		     int *nr, int *nc){
    *nc = *nr = i <= COVARIATE_X  || i == COVARIATE_FACTOR ? 0 
      : i <= COVARIATE_FACTOR ? 1 
      : -1;
}

void covariate(double *x, cov_model *cov, double *v){
  // ACHTUNG!! FALLS NAs verdeckt das sind, i.e. COVARIATE_ADDNA = TRUE:
  // HIER WIRD GETRICKST: vdim[0]=1 fuer Kovarianz, das hier nur 
  //                      vim[0] abgefragt wird und de facto univariat ist
  // und vdim[1]=Anzahl der covariaten fuer matrix berechnung, da
  //     fctnintern das produkt betrachtet und somit die dimensionen der
  //     design matrix reflektiert.
  // Fuer COVARIATE_ADDNA = FALSE haben wir ganz normals Verhalten
  GET_LOC_COVARIATE;

  double 
    *p = LP(COVARIATE_C);
  bool addna = cov->q[1];
  assert(cov->vdim[!addna] == 1);
  int  nr, 
    vdim = cov->vdim[addna],
    ntot = loc->totalpoints;
  
  if (cov->role == ROLE_COV) {
    *v = 0.0;
    //for (int i=0; i<vdim; v[i++] = 0.0);
    return;
  }
  
  if (P0INT(COVARIATE_RAW)) {
    // should happen only in a matrix context!
    nr = loc->i_row;
    if (nr * vdim >= LNROW(COVARIATE_C) * LNCOL(COVARIATE_C))
      ERR("illegal access -- 'raw' should be FALSE");
  } else { 
    // interpolate: here nearest neighbour/voronoi
    nr = get_index(x, cov);
  }
   
  if (cov->q[0] == 0) {
    if (GLOBAL.general.vdim_close_together) {
      p += nr * vdim;
      for (int i=0; i<vdim; i++, p++) v[i] = *p;
    } else {
      p += nr;
      //   PMI(cov);
      //    printf("%d %d %d\n", nr, vdim, ntot);
      for (int i=0; i<vdim; i++, p+=ntot) v[i] = *p;
    }  
  } else {
    if (GLOBAL.general.vdim_close_together) {
      double dummy = 0.0;
      p += nr * vdim;      
      for (int i=0; i<vdim; i++, p++) dummy += *p * P(COVARIATE_FACTOR)[i];
      *v = dummy;
   } else {
      p += nr;
      //   PMI(cov);
      //    printf("%d %d %d\n", nr, vdim, ntot);
      for (int i=0; i<vdim; i++, p+=ntot) v[i] = *p * P(COVARIATE_FACTOR)[i];
    }
  }
  //printf("%d %d\n", (int) v[0], (int) v[1]);
}


int check_fix_covariate(cov_model *cov,  location_type ***local){
  int err,
    store = GLOBAL.general.set;
  GLOBAL.general.set = 0;
  bool
    globalXT = PisNULL(COVARIATE_X);
  coord_sys_enum ccs = GLOBAL.coords.coord_system;
  cov_model *next = cov->sub[0];

  if (cov->Scovariate != NULL &&
      cov->Scovariate->matrix_err != MATRIX_NOT_CHECK_YET && 
      cov->Scovariate->matrix_err != NOERROR)
    return cov->Scovariate->matrix_err;

   if ((ccs == cartesian && cov->isoown != CARTESIAN_COORD) ||
      (ccs == earth && cov->isoown != EARTH_COORDS) ||
      (ccs == sphere && cov->isoown != SPHERICAL_COORDS))
    SERR2("'%s' not the global coordinate system ('%s')",
	  ISONAMES[cov->isoown], COORD_SYS_NAMES[GLOBAL.coords.coord_system]);

  kdefault(cov, COVARIATE_RAW, globalXT && cov->calling != NULL && 
	   cov->calling->nr == MIXEDEFFECT); 
 
  if (P0INT(COVARIATE_RAW)) {
     cov_model *prev = cov->calling;
    assert(prev != NULL);    
    if (!globalXT || (prev != NULL && isDollar(prev) && !hasVarOnly(prev))) {
      //      PMI(cov->calling); printf("%d %d\n", !globalXT, PisNULL(COVARIATE_X));
	SERR("if 'raw' then none of {'x', 'T', 'Aniso', 'proj', 'scale'} may be given");
    }
     assert(cov->ownloc == NULL);  
     if (cov->Scovariate == NULL) NEW_STORAGE(covariate); 
    *local = PLoc(cov);
  } else if (globalXT) {
    if (cov->Scovariate == NULL) NEW_STORAGE(covariate); 
    *local = PLoc(cov);
  } else { // neither raw nor globalXT
    bool doset = cov->Scovariate == NULL;
    if (!doset) {
      location_type *loc = LocLoc(cov->Scovariate->loc);
      assert(loc != NULL);
      doset = Loc(cov)->spatialdim != loc->spatialdim || 
	Loc(cov)->xdimOZ != loc->xdimOZ;
    }
    if (doset) {
      NEW_STORAGE(covariate);
      covariate_storage *S = cov->Scovariate;
      GLOBAL.general.set = store;
      S->loc = loc_set(PVEC(COVARIATE_X), false);
      assert(S->loc != NULL);
      
      if (S->loc[0]->timespacedim != cov->tsdim) 
	SERR1("number of columns of '%s' do not equal the dimension",
	      KNAME(COVARIATE_X));
      GLOBAL.general.set = 0;
    }
    *local = cov->Scovariate->loc;
  } // neither raw nor globalXT

  if (next == NULL) {
    addModel(cov, 0, TRAFO);
    next = cov->sub[0];
    kdefault(next, TRAFO_ISO, IsotropicOf(cov->isoprev));
  }
   
  if ((err = CHECK(next, cov->tsdim,  cov->xdimown, ShapeType,
		   KERNEL, cov->isoown,
		   1, cov->role)) != NOERROR) return err;

  return NOERROR;
}



int checkcovariate(cov_model *cov){
 assert(cov != NULL);
  int store = GLOBAL.general.set;
  GLOBAL.general.set = 0;
  int len,
    vdim = -1, 
    err = NOERROR;
  location_type **local = NULL;
  cov_model *prev = cov->calling;
  bool addna; 
  // kdefault(cov, COVARIATE_ADDNA, addna);
 
  if (cov->q == NULL) {
    addna = (!PisNULL(COVARIATE_ADDNA) && P0INT(COVARIATE_ADDNA)) || 
      (!PisNULL(COVARIATE_FACTOR) &&
       (ISNA(P0(COVARIATE_FACTOR)) || ISNAN(P0(COVARIATE_FACTOR))));
    QALLOC(2);
    // cov->q[0] == 0 iff vdim vectors are returned
    cov->q[1] = addna;
  } else addna = (bool) cov->q[1];


  //  printf("%d %d ; %d %d\n", cov->q == NULL ? -1 : (bool) cov->q[1] ,
  //	 addna, !PisNULL(COVARIATE_ADDNA), !PisNULL(COVARIATE_FACTOR));

  if ((err = check_fix_covariate(cov, &local)) != NOERROR) goto ErrorHandling;
  assert(local != NULL);
 
  len = local[0]->len;
  //S = cov->Scovariate;  assert(S != NULL);
  if (len <= 0) BUG;
  for (GLOBAL.general.set=0;  GLOBAL.general.set<len;  GLOBAL.general.set++) {
    int
      ndata = LNROW(COVARIATE_C) * LNCOL(COVARIATE_C),
      ntot = LocLoc(local)->totalpoints;
    if (GLOBAL.general.set == 0) {
      vdim = ndata/ntot;
    }

    if (ntot <= 0) GERR("no locations given");
    if (vdim * ntot != ndata)
      GERR3("Number of data (%d) not a multiple of the number of locations (%d x %d)", ndata, ntot, vdim);
  }
  assert(vdim > 0);
 
  if (PisNULL(COVARIATE_FACTOR)) {
    while (prev != NULL && prev->nr != LIKELIHOOD_CALL &&
	   prev->nr != LINEARPART_CALL) {
      prev = prev->nr == PLUS ? prev->calling : NULL;	
    }
  }
    
  if (addna) {
    if (!isTrend(cov->typus))
      GERR2("'%s' can be true only if '%s' is used as a trend",
	    KNAME(COVARIATE_ADDNA), NAME(cov));
    if (prev == NULL)  {
      GERR1("%s is used with NAs outside a trend definition.", NAME(cov));
    }
    if (PisNULL(COVARIATE_FACTOR)) PALLOC(COVARIATE_FACTOR, vdim, 1);
    for (int i=0; i<vdim; i++) P(COVARIATE_FACTOR)[i] = RF_NAN;
  }

  cov->q[0] = ((bool) cov->q[1]) || PisNULL(COVARIATE_FACTOR) ? 0 : vdim;
 
  cov->vdim[!addna] = 1; 
  cov->vdim[addna] = cov->q[0] == 0.0 ? vdim : 1;
  assert( cov->vdim[0] > 0 && cov->vdim[1] >0);
       
  if (cov->role == ROLE_COV && cov->vdim[0] != 1)
    GERR1("'%s' used in a wrong context", NAME(cov));

  if ((err = checkkappas(cov, false)) != NOERROR ||
      PisNULL(COVARIATE_C) ||
      PisNULL(COVARIATE_X) ||
      PisNULL(COVARIATE_RAW)
     ) goto ErrorHandling;
   
  cov->mpp.maxheights[0] = RF_NA;
  EXTRA_STORAGE;
  
 ErrorHandling:
  GLOBAL.general.set = store;

  //  if (err == NOERROR) APMI(cov->calling->calling);
  PFREE(COVARIATE_ADDNA);

  return err;
      
}

void rangecovariate(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  rangefix(cov, range);

  range->min[COVARIATE_ADDNA] = 0;
  range->max[COVARIATE_ADDNA] = 1;
  range->pmin[COVARIATE_ADDNA] = 0;
  range->pmax[COVARIATE_ADDNA] = 1;
  range->openmin[COVARIATE_ADDNA] = false;
  range->openmax[COVARIATE_ADDNA] = false;

  range->min[COVARIATE_FACTOR] = RF_NEGINF;
  range->max[COVARIATE_FACTOR] = RF_INF;
  range->pmin[COVARIATE_FACTOR] = -1e10;
  range->pmax[COVARIATE_FACTOR] = 1e10;
  range->openmin[COVARIATE_FACTOR] = true;
  range->openmax[COVARIATE_FACTOR] = true;
}





void kappa_fix(int i, cov_model VARIABLE_IS_NOT_USED *cov,
		     int *nr, int *nc){
    *nc = *nr = i <= FIXCOV_X ? 0 
      : i <= FIXCOV_RAW ? 1 
      : -1;
}


void fix(double *x, double *y, cov_model *cov, double *v){
  GET_LOC_COVARIATE;
   int nrx, nry, 
     ntot = loc->totalpoints,
    vdim = cov->vdim[0];
  double
    *p = LP(FIXCOV_M);

  assert(cov->vdim[0] == cov->vdim[1]);
  if (P0INT(FIXCOV_RAW)) {
   // should happen only in a matrix context!
    nrx = loc->i_row;
    nry = loc->i_col;
    if (nrx * vdim >= LNROW(FIXCOV_M) ||
	nry * vdim >= LNCOL(FIXCOV_M))
      ERR("illegal access -- 'raw' should be FALSE");
  } else {    
    nrx = get_index(x, cov);
    nry = get_index(y, cov);
  }

  // printf("nrx = %d %d; %f %f \n", nrx, nry, *x, *y);
  int i, j, k, 
    ntotvdim = ntot * vdim;
  if (GLOBAL.general.vdim_close_together) {
    p += (ntotvdim * nry + nrx) * vdim;
    for (k=i=0; i<vdim; i++, p += ntotvdim) {
      double *q = p;
      for (j=0; j<vdim; j++, q++) v[k++] = *q;
    }
  } else {
    int ntotSqvdim = ntotvdim * ntot;
    p += ntotvdim * nry + nrx;
    for (k=i=0; i<vdim; i++, p += ntotSqvdim) {
      double *q = p;
      for (j=0; j<vdim; j++, q+=ntot) v[k++] = *q;
    }
  }
}


void fixStat(double *x, cov_model *cov, double *v){
  if (!P0INT(FIXCOV_RAW)) ERR2("'%s=FALSE' where '%s=TRUE' is expected.",  
			      KNAME(FIXCOV_RAW), KNAME(FIXCOV_RAW));
  fix(x, NULL, cov, v);
}

int checkfix(cov_model *cov){
  assert(cov != NULL);
  int store = GLOBAL.general.set;
  GLOBAL.general.set = 0;
  int 
    vdim = -1, 
    vdimSq = -1,
    err = NOERROR;
  location_type **local = NULL;

 
  if ((err = check_fix_covariate(cov, &local)) != NOERROR) goto ErrorHandling;
  if (!P0INT(FIXCOV_RAW) && (cov->domown != KERNEL || cov->isoown != SYMMETRIC))
    SERR2("Model only allowed within positive definite kernels, not within positive definite functions. (Got %s, %s.)",  DOMAIN_NAMES[cov->domown], ISONAMES[cov->isoown]);

  assert(local != NULL);
  covariate_storage *S;
  int len;
  len = local[0]->len;
  S = cov->Scovariate;
  assert(S != NULL);
  if (len <= 0) BUG;
  for (GLOBAL.general.set=0;  GLOBAL.general.set<len;  GLOBAL.general.set++) {
    int
      ndata = LNROW(FIXCOV_M) * LNCOL(FIXCOV_M),
      ntot = LocLoc(local)->totalpoints;

    if (GLOBAL.general.set == 0) {
      vdim = (int) SQRT((double) ndata / (double) (ntot * ntot));
      vdimSq = vdim * vdim;
    }

    if (ntot <= 0) GERR("no locations given");
    if (cov->nrow[FIXCOV_M] != cov->ncol[FIXCOV_M])
      GERR("square matrix expected");
    if (vdimSq * ntot * ntot != ndata)
      GERR3("number of data (%d) not a multiple of the number of locations (%d x %d)^2", ndata, ntot, vdim);      
  }
  assert(vdim > 0);
  cov->vdim[0] = cov->vdim[1] = vdim;

  if ((err = checkkappas(cov)) != NOERROR) goto ErrorHandling;
     
  if (vdim == 1 && len == 1) {
    GLOBAL.general.set = 0;
    double *c = LP(FIXCOV_M);   
    int i,
      ntot = LocLoc(local)->totalpoints;
    for (i=0; i<ntot; i++) if (c[i] != 0.0) break;
    cov->ptwise_definite = pt_zero;
    if (i<ntot) {
      if (c[i] > 0.0) {
	cov->ptwise_definite = pt_posdef;
        for (i++; i<ntot; i++)
	  if (c[i] < 0.0) {
	    cov->ptwise_definite = pt_indef;
	    break;
	  }
      } else { // < 0.0
	cov->ptwise_definite = pt_negdef;
        for (i++; i<ntot; i++)
	  if (c[i] > 0.0) {
	    cov->ptwise_definite = pt_indef;
	    break;
	  }
      }
    }
  } else cov->ptwise_definite = pt_unknown; // to do ?! alle Matrizen ueberpruefen...

     
  if (S->matrix_err == MATRIX_NOT_CHECK_YET) {
    S->matrix_err = NOERROR;
    for (GLOBAL.general.set=0; GLOBAL.general.set<len; GLOBAL.general.set++){
      int info, j, k,
	nrow = LNROW(FIXCOV_M),
	ncol = LNCOL(FIXCOV_M),
	total = nrow * ncol * sizeof(double);
      double
	*C =  LP(FIXCOV_M);
      if (nrow != ncol || nrow == 0) {
	S->matrix_err = err = ERROR_MATRIX_SQUARE; 
	goto ErrorHandling;
      }    
      
      if (nrow < 3000) {
	double *dummy = (double*) MALLOC(total);
	MEMCOPY(dummy, C, total);
	F77_CALL(dpofa)(dummy, &nrow, &ncol, &info); // cholesky
	FREE(dummy);
	if (info != 0) {
	  S->matrix_err = err = ERROR_MATRIX_POSDEF;
	  goto ErrorHandling;
	}
      } else {
	if (len==1) 
	  PRINTF("covariance matrix is large, hence not verified to be positive definite.");
	else PRINTF("covariance matrix of %d-th set is large, hence not verified to be positive definite.", GLOBAL.general.set);
      }
      
      for (k=0; k<nrow; k++) {	
	for (j=k+1; j<ncol; j++) {
	  if (C[k + nrow * j] != C[j + nrow * k]) 
	    GERR("matrix not symmteric");
	}
      }
    }
    if (err != NOERROR) goto ErrorHandling;
  }
      
      
  cov->matrix_indep_of_x = true;
  cov->mpp.maxheights[0] = RF_NA;
  EXTRA_STORAGE;
  
 ErrorHandling: 
  GLOBAL.general.set = store;

  return err;
}




void rangefix(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[FIXCOV_M] = RF_NEGINF;
  range->max[FIXCOV_M] = RF_INF;
  range->pmin[FIXCOV_M] = -1e10;
  range->pmax[FIXCOV_M] = 1e10;
  range->openmin[FIXCOV_M] = true;
  range->openmax[FIXCOV_M] = true;

  range->min[FIXCOV_X] = RF_NA;
  range->max[FIXCOV_X] = RF_NA;
  range->pmin[FIXCOV_X] = RF_NA;
  range->pmax[FIXCOV_X] = RF_NA;
  range->openmin[FIXCOV_X] = true;
  range->openmax[FIXCOV_X] = true;

  range->min[FIXCOV_RAW] = 0;
  range->max[FIXCOV_RAW] = 1;
  range->pmin[FIXCOV_RAW] = 0;
  range->pmax[FIXCOV_RAW] = 1;
  range->openmin[FIXCOV_RAW] = false;
  range->openmax[FIXCOV_RAW] = false;
}




/* coxgauss, cmp with nsst1 !! */
// see Gneiting.cc

/* cubic */
void cubic(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) {
  double y=*x, y2=y * y;
  *v = (y >= 1.0) ? 0.0
    : (1.0 + (((0.75 * y2 - 3.5) * y2 + 8.75) * y - 7) * y2);
}
void Dcubic(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) { 
  double y=*x, y2=y * y;
  *v = (y >= 1.0) ? 0.0 : y * (-14.0 + y * (26.25 + y2 * (-17.5 + 5.25 * y2)));
}

/* cutoff */
// see operator.cc

/* dagum */
#define DAGUM_BETA 0
#define DAGUM_GAMMA 1
#define DAGUM_BETAGAMMA 2
void dagum(double *x, cov_model *cov, double *v){
  double gamma = P0(DAGUM_GAMMA), 
    beta=P0(DAGUM_BETA);
  *v = 1.0 - POW((1 + POW(*x, -beta)), -gamma/beta);
}
void Inversedagum(double *x, cov_model *cov, double *v){ 
  double gamma = P0(DAGUM_GAMMA), 
    beta=P0(DAGUM_BETA);
    *v = *x == 0.0 ? RF_INF 
      : POW(POW(1.0 - *x, - beta / gamma ) - 1.0, 1.0 / beta);
} 
void Ddagum(double *x, cov_model *cov, double *v){
  double y=*x, xd, 
    gamma = P0(DAGUM_GAMMA), 
    beta=P0(DAGUM_BETA);
  xd = POW(y, -beta);
  *v = -gamma * xd / y * POW(1 + xd, -gamma/ beta -1);
}

int checkdagum(cov_model *cov){
  if (PisNULL(DAGUM_GAMMA) || PisNULL(DAGUM_BETA))
    SERR("parameters are not given all");
  double
    gamma = P0(DAGUM_GAMMA), 
    beta = P0(DAGUM_BETA);
  kdefault(cov, DAGUM_BETAGAMMA, beta/gamma);
  FIRST_INIT(initdagum);
 

  cov->monotone =  beta >= gamma ? MONOTONE 
    : gamma <= 1.0 ? COMPLETELY_MON
    : gamma <= 2.0 ? NORMAL_MIXTURE : MISMATCH;
    
  return NOERROR;
}

int initdagum(cov_model *cov, gen_storage *stor) {  
  double
    gamma = P0(DAGUM_GAMMA), 
    beta = P0(DAGUM_BETA),
    betagamma = P0(DAGUM_BETAGAMMA);
  
  if (stor->check) {
    bool tcf = isTcf(cov->typus) || cov->isoown == SPHERICAL_ISOTROPIC;
    bool isna_gamma = tcf && ISNA(gamma);
    if (isna_gamma) {
      if (cov->q == NULL) QALLOC(1); // just as a flag
    } else {
      P(DAGUM_BETAGAMMA)[0] = 1.0;
      // dummy value just to be in the range !
    }
  } else {     
    bool isna_gamma = cov->q != NULL;
    if (isna_gamma) P(DAGUM_GAMMA)[0] = beta / betagamma;
  }
  return NOERROR;
}

sortsofparam paramtype_dagum(int k, int VARIABLE_IS_NOT_USED row, int VARIABLE_IS_NOT_USED col) {
  return k==DAGUM_GAMMA ? IGNOREPARAM : ANYPARAM;
}
 

void rangedagum(cov_model *cov, range_type *range){
  bool tcf = isTcf(cov->typus) || cov->isoown == SPHERICAL_ISOTROPIC;
  range->min[DAGUM_BETA] = 0.0;
  range->max[DAGUM_BETA] = 1.0;
  range->pmin[DAGUM_BETA] = 0.01;
  range->pmax[DAGUM_BETA] = 1.0;
  range->openmin[DAGUM_BETA] = true;
  range->openmax[DAGUM_BETA] = false;

  range->min[DAGUM_GAMMA] = 0.0;
  range->max[DAGUM_GAMMA] = tcf ? 1.0 : 2.0;
  range->pmin[DAGUM_GAMMA] = 0.01;
  range->pmax[DAGUM_GAMMA] = range->max[DAGUM_GAMMA];
  range->openmin[DAGUM_GAMMA] = true;
  range->openmax[DAGUM_GAMMA] = tcf;

  range->min[DAGUM_BETAGAMMA] = 0.0;
  range->max[DAGUM_BETAGAMMA] = tcf ? 1.0 : RF_INF;
  range->pmin[DAGUM_BETAGAMMA] = 0.01;
  range->pmax[DAGUM_BETAGAMMA] = tcf ? 1.0 : 20.0;
  range->openmin[DAGUM_BETAGAMMA] = true;
  range->openmax[DAGUM_BETAGAMMA] = true;
}


/*  damped cosine -- derivative of e xponential:*/
#define DC_LAMBDA 0
void dampedcosine(double *x, cov_model *cov, double *v){
  double y = *x, lambda = P0(DC_LAMBDA);
  *v = (y == RF_INF) ? 0.0 : EXP(-y * lambda) * COS(y);
}
void logdampedcosine(double *x, cov_model *cov, double *v, double *Sign){
  double 
    y = *x, 
    lambda = P0(DC_LAMBDA);
  if (y==RF_INF) {
    *v = RF_NEGINF;
    *Sign = 0.0;
  } else {
    double cosy=COS(y);
    *v = -y * lambda + LOG(FABS(cosy));
    *Sign = cosy > 0.0 ? 1.0 : cosy < 0.0 ? -1.0 : 0.0;
  }
}
void Inversedampedcosine(double *x, cov_model *cov, double *v){ 
  Inverseexponential(x, cov, v);
} 
void Ddampedcosine(double *x, cov_model *cov, double *v){
  double y = *x, lambda = P0(DC_LAMBDA);
  *v = - EXP(-lambda*y) * (lambda * COS(y) + SIN(y));
}
int checkdampedcosine(cov_model *cov){
   cov->maxdim = ISNAN(P0(DC_LAMBDA)) 
     ? INFDIM : (int) (PIHALF / ATAN(1.0 / P0(DC_LAMBDA)));
  return NOERROR;
}
void rangedampedcosine(cov_model *cov, range_type *range){
  range->min[DC_LAMBDA]  =    
    cov->tsdim==1 ? 0 :
    cov->tsdim==2 ? 1 :
    cov->tsdim==3 ? 1.7320508075688771932 : 
    1.0 / TAN(PIHALF / cov->tsdim); 
  range->max[DC_LAMBDA] = RF_INF;
  range->pmin[DC_LAMBDA] = range->min[DC_LAMBDA] +  1e-10;
  range->pmax[DC_LAMBDA] = 10;
  range->openmin[DC_LAMBDA] = false;
  range->openmax[DC_LAMBDA] = true;
}


/* De Wijsian */
#define DEW_ALPHA 0 // for both dewijsian models
#define DEW_D 1
void dewijsian(double *x, cov_model *cov, double *v){
  double alpha = P0(DEW_ALPHA);
  *v = -LOG(1.0 + POW(*x, alpha));
}
void Ddewijsian(double *x, cov_model *cov, double *v){
  double alpha = P0(DEW_ALPHA),
    p  = POW(*x, alpha - 1.0) ;
  *v = - alpha * p / (1.0 + *x * p);
}
void DDdewijsian(double *x, cov_model *cov, double *v){
  double alpha = P0(DEW_ALPHA),
    p = POW(*x, alpha - 2.0),
    ha = p * *x * *x,
    haP1 = ha + 1.0;
  *v = alpha * p * (1.0 - alpha + ha) / (haP1 * haP1);
}

void D3dewijsian(double *x, cov_model *cov, double *v){
  double alpha = P0(DEW_ALPHA),
    p = POW(*x, alpha - 3.0),
    ha = p * *x * *x * *x,
    haP1 = ha + 1.0;
  *v = alpha * p * (alpha*alpha*(ha - 1) +3*alpha*(ha +1 ) -2*(ha +1)*(ha +1)  ) / (haP1 * haP1 * haP1);
}

void D4dewijsian(double *x, cov_model *cov, double *v){
  double alpha = P0(DEW_ALPHA),
    p = POW(*x, alpha - 4.0),
    ha = p * *x * *x * *x* *x,
    haP1 = ha + 1.0;
  *v = -alpha * p * (alpha*alpha*alpha*(ha*ha - 4*ha+ 1) +6*alpha*alpha*(ha*ha - 1) +11*alpha*(ha +1)*(ha +1) - 6*(ha +1)*(ha +1)*(ha +1)  ) / (haP1 * haP1 * haP1* haP1);
}

void Inversedewijsian(double *x, cov_model *cov, double *v){ 
  double alpha = P0(DEW_ALPHA);
  *v = POW(EXP(*x) - 1.0, 1.0 / alpha);    
} 
int checkdewijsian(cov_model *cov){
  double alpha = P0(DEW_ALPHA);
  cov->logspeed = alpha;
  return NOERROR;
}

void rangedewijsian(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[DEW_ALPHA] = 0.0;
  range->max[DEW_ALPHA] = 2.0;
  range->pmin[DEW_ALPHA] = UNIT_EPSILON;
  range->pmax[DEW_ALPHA] = 2.0;
  range->openmin[DEW_ALPHA] = true;
  range->openmax[DEW_ALPHA] = false; 
}

void coinitdewijsian(cov_model *cov, localinfotype *li) {
    double
            thres[3] = {0.5, 1.0, 1.8},
            alpha=P0(DEW_ALPHA);

    if (alpha <= thres[0]) {
        li->instances = 2;
        li->value[0] = 0.5;
        li->value[1] = 1.0;
        li->msg[0] = li->msg[1] = MSGLOCAL_OK;
    } else {
        if (alpha <= thres[1]) {
            li->instances = 1;
            li->value[0] = 1.0; //  q[CUTOFF_A]
            li->msg[0] =MSGLOCAL_OK;
        } else {
            if (alpha <= thres[2]) {
                li->instances = 1;
                li->value[0] = CUTOFF_THIRD_CONDITION ; //  q[CUTOFF_A]        
                li->msg[0] = MSGLOCAL_OK;
            }
            else {
                //TO DO: this is copied from Cauchy model and must be understood and changed
                li->instances = 1;
                li->value[0] = CUTOFF_THIRD_CONDITION ; //  q[CUTOFF_A]
                li->msg[0] = MSGLOCAL_JUSTTRY;
            }
        }
    }
  //  printf("\n I am in  coinitdewijsian, alpha = %f, livalue[0] = %f\n", alpha, li->value[0]);
}

/* De Wijsian B */
#define DEW_RANGE 1
void DeWijsian(double *x, cov_model *cov, double *v){
  double alpha = P0(DEW_ALPHA),
    range = P0(DEW_RANGE);
  *v = (*x >= range) ? 0.0 : 1.0 -
    LOG(1.0 + POW(*x, alpha)) / LOG(1.0 + POW(range, alpha));
}

void InverseDeWijsian(double *x, cov_model *cov, double *v){ 
  double alpha = P0(DEW_ALPHA),
    range = P0(DEW_RANGE);
  *v = *x >= 1.0 ? 0.0 
    : POW(POW(1.0 + POW(range, alpha),  1.0 - *x) - 1.0, 1.0 / alpha);
} 

void rangeDeWijsian(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[DEW_ALPHA] = 0.0;
  range->max[DEW_ALPHA] = 1.0;
  range->pmin[DEW_ALPHA] = UNIT_EPSILON;
  range->pmax[DEW_ALPHA] = 1.0;
  range->openmin[DEW_ALPHA] = true;
  range->openmax[DEW_ALPHA] = false; 

  range->min[DEW_RANGE] = 0.0;
  range->max[DEW_RANGE] = RF_INF;
  range->pmin[DEW_RANGE] = UNIT_EPSILON;
  range->pmax[DEW_RANGE] = 1000;
  range->openmin[DEW_RANGE] = true;
  range->openmax[DEW_RANGE] = true; 
}


/* exponential model */
void exponential(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){
   *v = EXP(- *x);
   // printf("x=%f %f\n", *x, *v);
}
void logexponential(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v, double *Sign){
  *v = - *x;
  *Sign = 1.0;
 }
void TBM2exponential(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) 
{
  double y = *x;
  *v = (y==0.0) ?  1.0 : 1.0 - PIHALF * y * RU_I0mL0(y);
}
void Dexponential(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){
  *v = - EXP(- *x);
}
void DDexponential(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){
 *v = EXP(-*x);
}
void Inverseexponential(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){
  *v = (*x == 0.0) ? RF_INF : -LOG(*x);
}

void nonstatLogInvExp(double *x, cov_model *cov,
		      double *left, double *right){
  double 
    z = *x <= 0.0 ? - *x : 0.0;  
  int d,
    dim = cov->tsdim;
 
  for (d=0; d<dim; d++) {
    left[d] = -z;
    right[d] = z;
  }
}


double densityexponential(double *x, cov_model *cov) {
  // to do: ersetzen durch die Familien 

  // spectral density
  int d,
    dim=cov->tsdim;
  double x2 = 0.0,
    dim12 = 0.5 * ((double) (dim + 1));
  for (d=0; d<dim; d++) x2 += x[d] * x[d];
  
  return gammafn(dim12) * POW(M_PI * (1.0 + x2), -dim12);
}

#define HAS_SPECTRAL_ROLE(cov)\
  cov->role == ROLE_GAUSS && cov->method==SpectralTBM

int initexponential(cov_model *cov, gen_storage *s) {
  int dim = cov->tsdim;
  double D = (double) dim;

  if (HAS_SPECTRAL_ROLE(cov)) {
    spec_properties *cs = &(s->spec);
    if (cov->tsdim <= 2) return NOERROR;
    cs->density = densityexponential;
    return search_metropolis(cov, s);
  }
  
  else if (hasAnyShapeRole(cov)) {
    //Inverseexponential(&GLOBAL.mpp. about_ zero, NULL, &(cov->mpp.refradius));
    //  R *= GLOBAL.mpp.radius_natscale_factor;
    
    /*
    if (cov->mpp.moments >= 1) {
      int xi, d;
      double i[3], dimfak, Rfactor, sum,
	dimHalf = 0.5 * D;
      dimfak = gammafn(D);
      for (xi=1; xi<=2; xi++) {
	R = xi * cov->mpp.refradius;  //
	// http://de.wikipedia.org/wiki/Kugel
	i[xi] = dimfak * 2 * POW(M_PI / (double) (xi*xi), dimHalf) / 
	  gammafn(dimHalf);
	
	if (R < RF_INF) {
	  // Gradstein 3.351 fuer n=d-1
	  
	  //printf("here\n");
	  for (sum = 1.0, factor=1.0, d=1; d<dim; d++) {
	    factor *= R / D;
	    sum += factor;
	    //	printf("%d %f %f %f\n", d, sum, factor, R);
	}
	  sum *= dimfak * EXP(-R);
	  // printf("%d %f %f %f\n", d, sum, factor, R);
	  i[xi] -= sum;
	}
      }
      cov->mpp.mM[1] = cov->mpp.mMplus[1] = i[1];
      if (cov->mpp.moments >= 2) {
	cov->mpp.mM[2] = cov->mpp.mMplus[2] = i[2];
      }
    }
    */
  
    assert(cov->mpp.maxheights[0] == 1.0);
    if (cov->mpp.moments >= 1) {
       cov->mpp.mM[1]= cov->mpp.mMplus[1] = 
	SurfaceSphere(dim - 1, 1.0) * gammafn(D);
    }
  }
  
  else ILLEGAL_ROLE;

  return NOERROR;
}
void do_exp(cov_model VARIABLE_IS_NOT_USED *cov, gen_storage VARIABLE_IS_NOT_USED *s) { 
}

void spectralexponential(cov_model *cov, gen_storage *S, double *e) {
  spectral_storage *s = &(S->Sspectral);
  if (cov->tsdim <= 2) {
    double A = 1.0 - UNIFORM_RANDOM;
    E12(s, cov->tsdim, SQRT(1.0 / (A * A) - 1.0), e);
  } else {
    metropolis(cov, S, e);
  }
}

int checkexponential(cov_model *cov) {
  if (cov->tsdim > 2)
    cov->pref[CircEmbedCutoff] = cov->pref[CircEmbedIntrinsic] = PREF_NONE;
  if (cov->tsdim != 2) cov->pref[Hyperplane] = PREF_NONE;
  return NOERROR;
}

int hyperexponential(double radius, double *center, double *rx,
		     cov_model VARIABLE_IS_NOT_USED *cov, bool simulate, 
		     double ** Hx, double ** Hy, double ** Hr)
{
  // lenx : half the length of the rectangle
  // center   : center of the rectangle
  // simulate=false: estimated number of lines returned;
  // simulate=true: number of simulated lines returned;
  // hx, hy : direction of line
  // hr     : distance of the line from the origin
  // rectangular area where center gives the center 
  //  
  // the function expects scale = 1;
  double lambda, phi, lx, ly, *hx, *hy, *hr;
  long i, p, 
    //
    q=0;
    //   q = RF_NA; assert(false);
  int k,
    err;
  
  assert(cov->tsdim==2);

  // we should be in two dimensions
  // first, we simulate the lines for a rectangle with center (0,0)
  // and half the side length equal to lenx
  lx = rx[0];
  ly = rx[1];
  lambda = TWOPI * radius * 0.5; /* total, integrated, intensity */
  //    0.5 in order to get scale 1
  if (!simulate) return lambda < 999999 ? (int) lambda : 999999 ;
  assert(*Hx==NULL);
  assert(*Hy==NULL);
  assert(*Hr==NULL);
  p = (long) rpois(lambda);
  if ((hx=*Hx=(double *) MALLOC(sizeof(double) * (p + 8 * sizeof(int))))==NULL||
      (hy=*Hy=(double *) MALLOC(sizeof(double) * (p + 8 *sizeof(int))))==NULL||
      (hr=*Hr=(double *) MALLOC(sizeof(double) * (p + 8 * sizeof(int))))==NULL){
    err=ERRORMEMORYALLOCATION; goto ErrorHandling;
  }  
  
  /* creating the lines; some of the lines are not relevant as they
     do not intersect the rectangle investigated --> k!=4
     (it is checked if all the corners of the rectangle are on one 
     side (?) )
  */
   for(i=0; i<p; i++) {
    phi = UNIFORM_RANDOM * TWOPI;
    hx[q] = COS(phi);     hy[q] = SIN(phi);    
    hr[q] = UNIFORM_RANDOM * radius;
    k = (hx[q] * (-lx) + hy[q] * (-ly) < hr[q]) +
      (hx[q] * (-lx) + hy[q] * ly < hr[q]) +
      (hx[q] * lx + hy[q] * (-ly) < hr[q]) +
      (hx[q] * lx + hy[q] * ly < hr[q]);
    if (k!=4) { // line inside rectangle, so stored
      // now the simulated line is shifted into the right position 
      hr[q] += center[0] * hx[q] + center[1] * hy[q]; 
      q++; // set pointer for storing to the next element
    }
  }

  return q;

 ErrorHandling:
  return -err;
}

void coinitExp(cov_model VARIABLE_IS_NOT_USED *cov, localinfotype *li) {
  li->instances = 1;
  li->value[0] = 1.0; //  q[CUTOFF_A] 
  li->msg[0] = MSGLOCAL_OK;
}
void ieinitExp(cov_model VARIABLE_IS_NOT_USED *cov, localinfotype *li) {
  li->instances = 1;
  li->value[0] = 1.0;
  li->msg[0] = MSGLOCAL_OK;
}
void DrawMixExp(cov_model VARIABLE_IS_NOT_USED *cov, double *random) {
  // GR 3.325: int_-infty^infty EXP(x^2/2 - b/x^2) = EXP(-\sqrt b)
  double x = GAUSS_RANDOM(1.0);
  *random = 1.0 / (x * x);
}
double LogMixDensExp(double VARIABLE_IS_NOT_USED *x, double VARIABLE_IS_NOT_USED logV, cov_model VARIABLE_IS_NOT_USED *cov) {
  // todo: check use of mixdens --- likely to programme it now completely differently 
  return 0.0;
}


// Brownian motion 
static double lsfbm_alpha = -1,
  lsfbm_C = RF_NA;
static int lsfbm_d = -1;
void refresh(double *x, cov_model *cov) {   
  if (*x > 1.0) ERR1("the coordinate distance in '%s' must be at most 1.", NAME(cov));
  double alpha = P0(LOCALLY_BROWN_ALPHA);
  int d = cov->tsdim;
  if (alpha != lsfbm_alpha || d != lsfbm_d) {
    lsfbm_d = d;
    lsfbm_alpha = alpha;
    double a2 = 0.5 * alpha,
      d2 = 0.5 * d;
    if (P(LOCALLY_BROWN_C) == NULL) {
      lsfbm_C = EXP(-alpha * LOG2 + lgammafn(a2 + d2) + lgammafn(1.0 - a2) 
		    - lgammafn(d2));
      if (PL >= PL_DETAILSUSER)
	PRINTF("'%s' in '%s' equals %f for '%s'=%f\n", KNAME(LOCALLY_BROWN_C),
	       NICK(cov), lsfbm_C, KNAME(LOCALLY_BROWN_ALPHA), alpha);
    } else {
      lsfbm_C =  P0(LOCALLY_BROWN_C);
    }
  }
}

int checklsfbm(cov_model *cov){
  int err;
  lsfbm_alpha = -1;
  if (P(BROWN_ALPHA) == NULL) ERR("alpha must be given");
  if ((err = checkkappas(cov, false)) != NOERROR) return err;
  double alpha = P0(BROWN_ALPHA);
  cov->logspeed = RF_INF;
  cov->full_derivs = alpha <= 1.0 ? 0 : alpha < 2.0 ? 1 : cov->rese_derivs;
  cov->taylor[0][TaylorPow] = cov->tail[0][TaylorPow] = alpha;
  return NOERROR;
}
void rangelsfbm(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[LOCALLY_BROWN_ALPHA] = 0.0;
  range->max[LOCALLY_BROWN_ALPHA] = 2.0;
  range->pmin[LOCALLY_BROWN_ALPHA] = UNIT_EPSILON;
  range->pmax[LOCALLY_BROWN_ALPHA] = 2.0;
  range->openmin[LOCALLY_BROWN_ALPHA] = true;
  range->openmax[LOCALLY_BROWN_ALPHA] = false;

  range->min[LOCALLY_BROWN_C] = 0.5;
  range->max[LOCALLY_BROWN_C] = RF_INF;
  range->pmin[LOCALLY_BROWN_C] = 1.0;
  range->pmax[LOCALLY_BROWN_C] = 1000;
  range->openmin[LOCALLY_BROWN_C] = true;
  range->openmax[LOCALLY_BROWN_C] = P(LOCALLY_BROWN_ALPHA)==NULL;
}

void lsfbm(double *x, cov_model *cov, double *v) {
  refresh(x, cov);
  *v = lsfbm_C - POW(*x, lsfbm_alpha);
  //  printf("x=%f %f %e %f\n", *x, *v, lsfbm_alpha, lsfbm_C);
}
/* lsfbm: first derivative at t=1 */
void Dlsfbm(double *x, cov_model *cov, double *v) 
{// FALSE VALUE FOR *x==0 and  lsfbm_alpha < 1
  refresh(x, cov);
  *v = (*x != 0.0) ? -lsfbm_alpha * POW(*x, lsfbm_alpha - 1.0)
    : lsfbm_alpha > 1.0 ? 0.0 
    : lsfbm_alpha < 1.0 ? RF_NEGINF
    : -1.0;
}
/* lsfbm: second derivative at t=1 */
void DDlsfbm(double *x, cov_model *cov, double *v)  
{// FALSE VALUE FOR *x==0 and  lsfbm_alpha < 2
  refresh(x, cov);
  *v = (lsfbm_alpha == 1.0) ? 0.0
    : (*x != 0.0) ? -lsfbm_alpha * (lsfbm_alpha - 1.0) * POW(*x, lsfbm_alpha - 2.0)
    : lsfbm_alpha < 1.0 ? RF_INF 
    : lsfbm_alpha < 2.0 ? RF_NEGINF 
    : -2.0;
}
void D3lsfbm(double *x, cov_model *cov, double *v)  
{// FALSE VALUE FOR *x==0 and  lsfbm_alpha < 2
  refresh(x, cov);
  *v = lsfbm_alpha == 1.0 || lsfbm_alpha == 2.0 ? 0.0 
    : (*x != 0.0) ? -lsfbm_alpha * (lsfbm_alpha - 1.0) * (lsfbm_alpha - 2.0) * POW(*x, lsfbm_alpha-3.0)
    : lsfbm_alpha < 1.0 ? RF_NEGINF 
    : RF_INF;
}

void D4lsfbm(double *x, cov_model *cov, double *v)  
{// FALSE VALUE FOR *x==0 and  lsfbm_alpha < 2
  refresh(x, cov);
  *v = lsfbm_alpha == 1.0 || lsfbm_alpha == 2.0 ? 0.0 
    : (*x != 0.0) 
    ? -lsfbm_alpha * (lsfbm_alpha - 1.0) * (lsfbm_alpha - 2.0) * (lsfbm_alpha - 3.0) *
    POW(*x, lsfbm_alpha-4.0)
    : lsfbm_alpha < 1.0 ? RF_INF 
    : RF_NEGINF;
}

void Inverselsfbm(double *x, cov_model *cov, double *v) {
  refresh(x, cov);
  *v = POW(lsfbm_C - *x, 1.0 / lsfbm_alpha);
}


// Brownian motion 
void fractalBrownian(double *x, cov_model *cov, double *v) {
  double alpha = P0(BROWN_ALPHA);
  *v = - POW(*x, alpha);//this is an invalid covariance function!
  // keep definition such that the value at the origin is 0
}


void logfractalBrownian(double *x, cov_model *cov, double *v, double *Sign) {
  double alpha = P0(BROWN_ALPHA);
  *v = LOG(*x) * alpha;//this is an invalid covariance function!
  *Sign = -1.0;
  // keep definition such that the value at the origin is 0
}

/* fractalBrownian: first derivative at t=1 */
void DfractalBrownian(double *x, cov_model *cov, double *v) 
{// FALSE VALUE FOR *x==0 and  alpha < 1
  double alpha = P0(BROWN_ALPHA);
  *v = (*x != 0.0) ? -alpha * POW(*x, alpha - 1.0)
    : alpha > 1.0 ? 0.0 
    : alpha < 1.0 ? RF_NEGINF
    : -1.0;
}
/* fractalBrownian: second derivative at t=1 */
void DDfractalBrownian(double *x, cov_model *cov, double *v)  
{// FALSE VALUE FOR *x==0 and  alpha < 2
  double alpha = P0(BROWN_ALPHA);
  *v = (alpha == 1.0) ? 0.0
    : (*x != 0.0) ? -alpha * (alpha - 1.0) * POW(*x, alpha - 2.0)
    : alpha < 1.0 ? RF_INF 
    : alpha < 2.0 ? RF_NEGINF 
    : -2.0;
}
void D3fractalBrownian(double *x, cov_model *cov, double *v)  
{// FALSE VALUE FOR *x==0 and  alpha < 2
  double alpha = P0(BROWN_ALPHA);
  *v = alpha == 1.0 || alpha == 2.0 ? 0.0 
    : (*x != 0.0) ? -alpha * (alpha - 1.0) * (alpha - 2.0) * POW(*x, alpha-3.0)
    : alpha < 1.0 ? RF_NEGINF 
    : RF_INF;
}

void D4fractalBrownian(double *x, cov_model *cov, double *v)  
{// FALSE VALUE FOR *x==0 and  alpha < 2
  double alpha = P0(BROWN_ALPHA);
  *v = alpha == 1.0 || alpha == 2.0 ? 0.0 
    : (*x != 0.0) ? -alpha * (alpha - 1.0) * (alpha - 2.0) * (alpha - 3.0) *
                     POW(*x, alpha-4.0)
    : alpha < 1.0 ? RF_INF 
    : RF_NEGINF;
}
int checkfractalBrownian(cov_model *cov){
  double alpha = P0(BROWN_ALPHA);
  cov->logspeed = RF_INF;
  cov->full_derivs = alpha <= 1.0 ? 0 : alpha < 2.0 ? 1 : cov->rese_derivs;
  cov->taylor[0][TaylorPow] = cov->tail[0][TaylorPow] = alpha;
  return NOERROR;
}

int initfractalBrownian(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  double alpha = P0(BROWN_ALPHA);
  cov->taylor[0][TaylorPow] = cov->tail[0][TaylorPow] = alpha;  
  return NOERROR;
}

void InversefractalBrownian(double *x, cov_model *cov, double *v) {
  double alpha = P0(BROWN_ALPHA);
  *v = POW(*x, 1.0 / alpha);
}

void rangefractalBrownian(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[BROWN_ALPHA] = 0.0;
  range->max[BROWN_ALPHA] = 2.0;
  range->pmin[BROWN_ALPHA] = UNIT_EPSILON;
  range->pmax[BROWN_ALPHA] = 2.0;
  range->openmin[BROWN_ALPHA] = true;
  range->openmax[BROWN_ALPHA] = false;
}
void ieinitBrownian(cov_model *cov, localinfotype *li) {
  li->instances = 1;
  li->value[0] = (cov->tsdim <= 2)
    ? ((P0(BROWN_ALPHA) <= 1.5) ? 1.0 : 2.0)
    : ((P0(BROWN_ALPHA) <= 1.0) ? 1.0 : 2.0);
  li->msg[0] = cov->tsdim <= 3 ? MSGLOCAL_OK : MSGLOCAL_JUSTTRY;
}



/* FD model */
#define FD_ALPHA 0
void FD(double *x, cov_model *cov, double *v){
  double alpha = P0(FD_ALPHA), y, d, k, skP1;
  static double dold=RF_INF;
  static double kold, sk;
  d = alpha * 0.5;
  y = *x;
  if (y == RF_INF) {*v = 0.0; return;} 
  k = TRUNC(y);
  if (dold!=d || kold > k) {
    sk = 1;
    kold = 0.0;
  }
  // Sign (-1)^k is (kold+d), 16.11.03, checked. 
  for (; kold<k; kold += 1.0) sk =  sk * (kold + d) / (kold + 1.0 - d);
  dold = d;
  kold = k;
  if (k == y) {
    *v = sk;
  } else {
    skP1 = sk * (kold + d) / (kold + 1.0 - d);
    *v = sk + (y - k) * (skP1 - sk);
  }
}
void rangeFD(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[FD_ALPHA] = -1.0;
  range->max[FD_ALPHA] = 1.0;
  range->pmin[FD_ALPHA] = range->min[FD_ALPHA] + UNIT_EPSILON;
  range->pmax[FD_ALPHA] = range->max[FD_ALPHA] - UNIT_EPSILON;
  range->openmin[FD_ALPHA] = false;
  range->openmax[FD_ALPHA] = true;
}



/* fractgauss */
#define FG_ALPHA 0
void fractGauss(double *x, cov_model *cov, double *v){
  double y = *x, alpha = P0(FG_ALPHA);
  *v = (y == 0.0) ? 1.0 :  (y==RF_INF) ? 0.0 : 
    0.5 *(POW(y + 1.0, alpha) - 2.0 * POW(y, alpha) + POW(FABS(y - 1.0),alpha));
}
void rangefractGauss(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[FG_ALPHA] = 0.0;
  range->max[FG_ALPHA] = 2.0;
  range->pmin[FG_ALPHA] = UNIT_EPSILON;
  range->pmax[FG_ALPHA] = 2.0;
  range->openmin[FG_ALPHA] = true;
  range->openmax[FG_ALPHA] = false;
}


/* Gausian model */
void Gauss(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) {
  *v = EXP(- *x * *x);
  //  printf("%f %f\n", *x, *v);
}
void logGauss(double *x, cov_model VARIABLE_IS_NOT_USED  *cov, double *v, double *Sign) {
  *v = - *x * *x;
  *Sign = 1.0;
}
void DGauss(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) {
  double y = *x; 
  *v = -2.0 * y * EXP(- y * y);
}
void DDGauss(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) {
  double y = *x * *x; 
  *v = (4.0 * y - 2.0)* EXP(- y);
}
void D3Gauss(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) {
  double y = *x * *x; 
  *v = *x * (12 - 8 * y) * EXP(- y);
}
void D4Gauss(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) {
  double y = *x * *x; 
  *v = ((16 * y - 48) * y + 12) * EXP(- y);
}
void InverseGauss(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) {
  *v = SQRT(-LOG(*x));
}
void nonstatLogInvGauss(double *x, cov_model VARIABLE_IS_NOT_USED *cov, 
		     double *left, double *right) {
  double 
    z = *x <= 0.0 ? SQRT(- *x) : 0.0;  
  int d,
    dim = cov->tsdim;
  for (d=0; d<dim; d++) {
    left[d] = -z;
    right[d] = z;
  }
}
double densityGauss(double *x, cov_model *cov) {  
    int d, dim=cov->tsdim;
    double x2=0.0;
    for (d=0; d<dim; d++) x2 += x[d] * x[d];
    return EXP(- 0.25 * x2 - (double) dim * (M_LN2 + M_LN_SQRT_PI));
}
int struct_Gauss(cov_model *cov, cov_model **newmodel) {  
  ASSERT_NEWMODEL_NOT_NULL;

  switch (cov->role) {
  case ROLE_POISSON_GAUSS :// optimierte density fuer den Gauss-Fall
    double invscale;
    addModel(newmodel, GAUSS, cov);       
    addModel(newmodel, DOLLAR);
    kdefault(*newmodel, DSCALE, INVSQRTTWO);
    addModel(newmodel, TRUNCSUPPORT);
    InverseGauss(&GLOBAL.mpp.about_zero, cov, &invscale);
    kdefault(*newmodel, TRUNC_RADIUS, invscale);
    break;  
  case ROLE_MAXSTABLE :   // optimierte density fuer den Gauss-Fall
    // crash();
    addModel(newmodel, GAUSS_DISTR, cov); // to
    kdefault(*newmodel, GAUSS_DISTR_MEAN, 0.0);
    kdefault(*newmodel, GAUSS_DISTR_SD, INVSQRTTWO);
    return NOERROR;
  default : ILLEGAL_ROLE_STRUCT;
  }

  return NOERROR;
}

double IntUdeU2_intern(int d, double R, double expMR2) {
  // int_0^R u^{d-1} EXP(-u^2) \D u
  return d == 0 ? (pnorm(R, 0.0, INVSQRTTWO, true, false) - 0.5)  * SQRTPI
    : d == 1 ? 0.5  * (1.0 - expMR2)
    : 0.5 * (expMR2 + (d - 1.0) * IntUdeU2_intern(d - 2, R, expMR2));    
}

double IntUdeU2(int d, double R) {
  // int_0^R u^{d-1} EXP(-u^2) \D u
  return IntUdeU2_intern(d, R, EXP(-R*R));
}

int initGauss(cov_model *cov, gen_storage *s) {

  if (hasNoRole(cov)) return NOERROR;

  if (HAS_SPECTRAL_ROLE(cov)) {

   spec_properties *cs = &(s->spec);
  
    if (cov->tsdim <= 2) return NOERROR;
    cs->density = densityGauss;
    return search_metropolis(cov, s);
  }

  else if (hasAnyShapeRole(cov)) {
    // int_{b(0,R) e^{-a r^2} dr = d b_d int_0^R r^{d-1} e^{-a r^2} dr
    // where a = 2.0 * xi / sigma^2
    // here : 2.0 is a factor so that the mpp function leads to the
    //            gaussian covariance model EXP(-x^2)
    //        xi : 1 : integral ()
    //             2 : integral ()^2

   
   double R = RF_INF;
    int 
      dim = cov->tsdim;
    
    assert(cov->nr == GAUSS);
         
  
    assert(cov->mpp.maxheights[0] = 1.0);
    if (cov->mpp.moments >= 1) {
      cov->mpp.mM[1] = cov->mpp.mMplus[1] = 
	SurfaceSphere(dim - 1, 1.0) * IntUdeU2(dim - 1, R);
      int i;
      for (i=2; i<=cov->mpp.moments; i++) {
	cov->mpp.mM[i] = cov->mpp.mM[1] * POW((double) i, -0.5 * dim);
      }
    }    
    cov->mpp.maxheights[0] = 1.0;
  }

  else ILLEGAL_ROLE;
 
  return NOERROR;

}

void do_Gauss(cov_model VARIABLE_IS_NOT_USED *cov, gen_storage VARIABLE_IS_NOT_USED *s) { 

}

void spectralGauss(cov_model *cov, gen_storage *S, double *e) {   
  spectral_storage *s = &(S->Sspectral);
  if (cov->tsdim <= 2) {
    E12(s, cov->tsdim, 2.0 * SQRT(-LOG(1.0 - UNIFORM_RANDOM)), e);

  } else {
    metropolis(cov, S, e);
  }
}
void DrawMixGauss(cov_model VARIABLE_IS_NOT_USED *cov, double VARIABLE_IS_NOT_USED *random) {
  *random = 1.0;
}
double LogMixDensGauss(double VARIABLE_IS_NOT_USED *x, double VARIABLE_IS_NOT_USED logV, cov_model VARIABLE_IS_NOT_USED *cov) {
  return 0.0;
}

/*
void densGauss(double *x, cov_model *cov, double *v) {
  int 
    factor[MAXMPPDIM+1] = {0, 1 / M_SQRT_PI, INVPI, INVPI / M_SQRT_PI, 
			   INVPI * INVPI},
    dim = cov->tsdim;
    *v = factor[dim] * EXP(- *x * *x);
}
*/

/*
void getMassGauss(double *a, cov_model *cov, double *kappas, double *m) {
  int i, j, k, kold,
    dim = cov->tsdim;
  double val[MAXMPPDIM + 1],
    sqrt2pi = SQRT2 * SQRTPI,
    factor[MAXMPPDIM+1] = {1, 
			   SQRT(2) / M_SQRT_PI, 
			   2 * INVPI,
			   2 * SQRT(2) * INVPI / M_SQRT_PI, 
			   4 * INVPI * INVPI};
  
  val[0] = 1.0;
  for (i=1; i<=dim; i++) 
    val[i] = (2.0 * pnorm(SQRT2 * a[i], 0.0, 1.0, false, false) - 1.0) * M_SQRT_PI;
  for (k=kold=i=0; i<dim; i++) {
    m[k++] = val[i];
    for (j=1; j< kold; j++) m[k++] = val[i] * m[j];
    kold = k;
    pr intf("kold = %d k=%d\n", kold, k);
  }
}
*/

/*
void simuGauss(cov_model *cov, int dim, double *v) {
  int i;
  double 
    dummy;
  *v = 0.0;
  if (dim <= 2) {
    *v = dim == 1 ? FABS(GAUSS_RANDOM(1.0)) : rexp(1.0); 
  } else {
    for (i=0; i<dim; i++) {
      dummy = GAUSS_RANDOM(1.0);
      *v += dummy * dummy;
    }
    *v = SQRT(*v);
  }
}
*/



/* generalised fractal Brownian motion */
void genBrownian(double *x, cov_model *cov, double *v) {
  double 
    alpha = P0(BROWN_ALPHA),
    beta =  P0(BROWN_GEN_BETA),
    delta = beta / alpha;
  *v = 1.0 - POW(POW(*x, alpha) + 1.0, delta);
  //this is an invalid covariance function!
  // keep definition such that the value at the origin is 0
}

void loggenBrownian(double *x, cov_model *cov, double *v, double *Sign) {
  genBrownian(x, cov, v);
  *v = LOG(-*v);
  *Sign = -1.0;
}
void InversegenBrownian(double *x, cov_model *cov, double *v) {
  double 
    alpha = P0(BROWN_ALPHA),
    beta =  P0(BROWN_GEN_BETA),
    delta = beta / alpha;
  *v = POW(POW(*x + 1.0, 1.0/delta) - 1.0, 1.0/alpha); 
}
static bool genfbmmsg=true;
int checkgenBrownian(cov_model *cov){
  if (genfbmmsg) warning("Note that the definition of 'genfbm' has changed. This warning appears only once per session.");
  genfbmmsg = false;

  cov->logspeed = RF_INF;
  return NOERROR;
}
void rangegenBrownian(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[BROWN_ALPHA] = 0.0;
  range->max[BROWN_ALPHA] = 2.0;
  range->pmin[BROWN_ALPHA] = UNIT_EPSILON;
  range->pmax[BROWN_ALPHA] = 2.0 - UNIT_EPSILON;
  range->openmin[BROWN_ALPHA] = true;
  range->openmax[BROWN_ALPHA] = false;

  range->min[BROWN_GEN_BETA] = 0.0;
  range->max[BROWN_GEN_BETA] = 2.0;
  range->pmin[BROWN_GEN_BETA] = UNIT_EPSILON;
  range->pmax[BROWN_GEN_BETA] = 2.0 - UNIT_EPSILON;
  range->openmin[BROWN_GEN_BETA] = true;
  range->openmax[BROWN_GEN_BETA] = false;
}


/* gencauchy */
#define GENC_ALPHA 0
#define GENC_BETA 1
void generalisedCauchy(double *x, cov_model *cov, double *v){
  double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA);
  *v = POW(1.0 + POW(*x, alpha), -beta/alpha);
}
void DgeneralisedCauchy(double *x, cov_model *cov, double *v){
  double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA), ha, y=*x;
  if (y ==0.0) {
    *v = ((alpha > 1.0) ? 0.0 : (alpha < 1.0) ? -INFTY : -beta); 
  } else {
    ha = POW(y, alpha - 1.0);
    *v = -beta * ha * POW(1.0 + ha * y, -beta / alpha - 1.0);
  }
}
void DDgeneralisedCauchy(double *x, cov_model *cov, double *v){
  double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA), ha, y=*x;
  if (y ==0.0) {
    *v = ((alpha==1.0) ? beta * (beta + 1.0) 
	  : (alpha==2.0) ?  -beta 
	  : (alpha < 1) ? INFTY : -INFTY); 
  } else {
    ha = POW(y, alpha);
    *v = beta * ha / (y * y) * (1.0 - alpha + (1.0 + beta) * ha)
      * POW(1.0 + ha, -beta / alpha - 2.0);
  }
}

void D3generalisedCauchy(double *x, cov_model *cov, double *v){
  double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA), ha, y=*x;
  if (y ==0.0) {
    *v = ((alpha==2.0) ? 0  :  (alpha == 1)? -beta*(beta+1)*(beta+2) : (alpha < 1)? -INFTY : INFTY);
  } else {
    ha=POW(y, alpha);
    *v = -beta * ha / (y * y* y) * ( (beta + 1)*(beta + 2)*ha*ha  - (3*beta + alpha + 4)*(alpha - 1)*ha + (alpha-1)*(alpha -2) )
      * POW(1.0 + ha, -beta / alpha - 3.0);
  }
}

void D4generalisedCauchy(double *x, cov_model *cov, double *v){
  double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA), ha, y=*x;
  if (y ==0.0) {
    *v = ((alpha==2.0) ? 3*beta*(beta + 2)  :  (alpha == 1)? beta*(beta+1)*(beta+2)*(beta + 3) : (alpha < 1)? INFTY : -INFTY);
  } else {
    ha=POW(y, alpha);
    *v = beta * ha / (y * y* y *y) * ( -(alpha-1)*(alpha-2)*(alpha-3) -
              (alpha - 1)*(4*alpha*beta + alpha*(alpha + 7) + 6*beta*beta + 22*beta + 18 )*ha*ha +
              (alpha - 1)*(alpha*(4*alpha + 7*beta + 4) - 11*beta - 18)*ha +
                                        (beta+1)*(beta +2 )*(beta + 3)*ha*ha*ha)* 
             POW(1.0 + ha, -beta / alpha - 4.0);
  }
}


void loggeneralisedCauchy(double *x, cov_model *cov, double *v, double *Sign){
  double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA);
  *v = LOG(1.0 + POW(*x, alpha)) *  -beta/alpha;
  *Sign = 1.0;
}
void InversegeneralisedCauchy(double *x, cov_model *cov, double *v) {
  double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA);
  *v =  (*x == 0.0) ? RF_INF : POW(POW(*x, -alpha / beta) - 1.0, 1.0 / alpha);
  // MLE works much better with 0.01 then with 0.05
}
int checkgeneralisedCauchy(cov_model *cov){
  if (cov->tsdim > 2)
    cov->pref[CircEmbedCutoff] = cov->pref[CircEmbedIntrinsic] = PREF_NONE;
  cov->monotone = P0(GENC_ALPHA) <= 1.0 ? COMPLETELY_MON : NORMAL_MIXTURE;
  return NOERROR;
}

void rangegeneralisedCauchy(cov_model *cov, range_type *range) {
  bool tcf = isTcf(cov->typus) || cov->isoown == SPHERICAL_ISOTROPIC;
  range->min[GENC_ALPHA] = 0.0;
  range->max[GENC_ALPHA] = tcf ? 1.0 : 2.0;
  range->pmin[GENC_ALPHA] = 0.05;
  range->pmax[GENC_ALPHA] = range->max[GENC_ALPHA];
  range->openmin[GENC_ALPHA] = true;
  range->openmax[GENC_ALPHA] = false;

  range->min[GENC_BETA] = 0.0;
  range->max[GENC_BETA] = RF_INF;
  range->pmin[GENC_BETA] = 0.05;
  range->pmax[GENC_BETA] = 10.0;
  range->openmin[GENC_BETA] = true;
  range->openmax[GENC_BETA] = true;
 
}

void coinitgenCauchy(cov_model *cov, localinfotype *li) {
  double 
    thres[2] = {0.5, 1.0}, 
    alpha=P0(GENC_ALPHA); 
  if (alpha <= thres[0]) {
    li->instances = 2;
    li->value[0] = 0.5;
    li->value[1] = 1.0;
    li->msg[0] = li->msg[1] = MSGLOCAL_OK;
  } else {
    li->instances = 1;
    li->value[0] = 1.0; //  q[CUTOFF_A] 
    li->msg[0] = (alpha <= thres[1]) ? MSGLOCAL_OK : MSGLOCAL_JUSTTRY;
  } 
}
void ieinitgenCauchy(cov_model *cov, localinfotype *li) {
  li->instances = 1;
  if (P0(GENC_ALPHA) <= 1.0) {
    li->value[0] = 1.0;
    li->msg[0] = MSGLOCAL_OK;
  } else {
    li->value[0] = 1.5;
    li->msg[0] = MSGLOCAL_JUSTTRY;
  }
}


/*---------------------------------*/


// ************************* bivariate Cauchy


void kappa_biCauchy(int i, cov_model VARIABLE_IS_NOT_USED *cov, int *nr, int *nc){
    *nc = 1;
    *nr = i == BICauchyalpha || i ==  BICauchybeta|| i == BICauchyscale ? 3 :  1;
}

int checkbiCauchy(cov_model *cov) {
    if (cov->tsdim > 2)
        cov->pref[CircEmbedCutoff] = cov->pref[CircEmbedIntrinsic] = PREF_NONE;
  return NOERROR;
}


void rangebiCauchy(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){

  range->min[BICauchyalpha] = 0.0;
  range->max[BICauchyalpha] = 1;

  range->pmin[BICauchyalpha] = 0.05;
  range->pmax[BICauchyalpha] = 1;

  range->openmin[BICauchyalpha] = true;
  range->openmax[BICauchyalpha] = true;


  range->min[BICauchybeta] = 0.0;
  range->max[BICauchybeta] = RF_INF;
  range->pmin[BICauchybeta] = 0.05;
  range->pmax[BICauchybeta] = 10.0;
  range->openmin[BICauchybeta] = true;
  range->openmax[BICauchybeta] = true;

  range->min[BICauchyscale] = 0.0;
  range->max[BICauchyscale] = RF_INF;
  range->pmin[BICauchyscale] = 1e-2;
  range->pmax[BICauchyscale] = 100.0;
  range->openmin[BICauchyscale] = true;
  range->openmax[BICauchyscale] = true;

  // to do: check rhos

  range->min[BICauchyrho] = -1;
  range->max[BICauchyrho] = 1;
  range->pmin[BICauchyrho] = -1;
  range->pmax[BICauchyrho] = 1;
  range->openmin[BICauchyrho] = false;
  range->openmax[BICauchyrho] = false;

}



void coinitbiCauchy(cov_model *cov, localinfotype *li) {
    double
            thres = 1,
            *alpha = P(BICauchyalpha);

    if ( ( alpha[0] <= thres ) &&  ( alpha[1] <= thres ) && ( alpha[2] <= thres ) ) {
        li->instances = 1;
        li->value[0] = CUTOFF_THIRD_CONDITION; //  q[CUTOFF_A]
        li->msg[0] =MSGLOCAL_OK;
    }
    else {
        li->instances = 1;
        li->value[0] = CUTOFF_THIRD_CONDITION ; //  q[CUTOFF_A]
        li->msg[0] = MSGLOCAL_JUSTTRY;
    }
}

void biCauchy (double *x, cov_model *cov, double *v) {
    int i;
    double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA),
            y = *x, z;

    assert(BICauchyalpha == GENC_ALPHA);
    assert(BICauchybeta == GENC_BETA);

     for (i=0; i<3; i++) {
        z = y/P(BICauchyscale)[i];
        P(GENC_ALPHA)[0] = P(BICauchyalpha)[i];
        P(GENC_BETA)[0] = P(BICauchybeta)[i];
        generalisedCauchy(&z, cov, v + i);
    }
     P(BICauchyalpha)[0] = alpha;
     P(BICauchybeta)[0] = beta;
     v[3] = v[2];
     v[1] *= P0(BICauchyrho);
     v[2] = v[1];
}



void DbiCauchy(double *x, cov_model *cov, double *v) {
    int i;
    double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA),
            y = *x, z;

    assert(BICauchyalpha == GENC_ALPHA);
    assert(BICauchybeta == GENC_BETA);

     for (i=0; i<3; i++) {
        z = y/P(BICauchyscale)[i];
        P(GENC_ALPHA)[0] = P(BICauchyalpha)[i];
        P(GENC_BETA)[0] = P(BICauchybeta)[i];
        DgeneralisedCauchy(&z, cov, v + i);
        v[i] /=P(BICauchyscale)[i];
    }
    P(BICauchyalpha)[0] = alpha;
    P(BICauchybeta)[0] = beta;
    v[3] = v[2];
    v[1] *= P0(BICauchyrho);
    v[2] = v[1];
}

void DDbiCauchy(double *x, cov_model *cov, double *v) {
    int i;
    double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA),
            y = *x, z;

    assert(BICauchyalpha == GENC_ALPHA);
    assert(BICauchybeta == GENC_BETA);

     for (i=0; i<3; i++) {
        z = y/P(BICauchyscale)[i];
        P(GENC_ALPHA)[0] = P(BICauchyalpha)[i];
        P(GENC_BETA)[0] = P(BICauchybeta)[i];
        DDgeneralisedCauchy(&z, cov, v + i);
        v[i] /=(P(BICauchyscale)[i]*P(BICauchyscale)[i]);
    }
     P(BICauchyalpha)[0] = alpha;
     P(BICauchybeta)[0] = beta;
     v[3] = v[2];
     v[1] *= P0(BICauchyrho);
     v[2] = v[1];
}

void D3biCauchy(double *x, cov_model *cov, double *v) {
    int i;
    double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA),
            y = *x, z;

    assert(BICauchyalpha == GENC_ALPHA);
    assert(BICauchybeta == GENC_BETA);

     for (i=0; i<3; i++) {
        z = y/P(BICauchyscale)[i];
        P(GENC_ALPHA)[0] = P(BICauchyalpha)[i];
        P(GENC_BETA)[0] = P(BICauchybeta)[i];
        D3generalisedCauchy(&z, cov, v + i);
        v[i] /=(P(BICauchyscale)[i]*P(BICauchyscale)[i]*P(BICauchyscale)[i]);
    }
     P(BICauchyalpha)[0] = alpha;
     P(BICauchybeta)[0] = beta;
     v[3] = v[2];
     v[1] *= P0(BICauchyrho);
     v[2] = v[1];
}

void D4biCauchy(double *x, cov_model *cov, double *v) {
    int i;
    double alpha = P0(GENC_ALPHA), beta=P0(GENC_BETA),
            y = *x, z;

    assert(BICauchyalpha == GENC_ALPHA);
    assert(BICauchybeta == GENC_BETA);

     for (i=0; i<3; i++) {
        z = y/P(BICauchyscale)[i];
        P(GENC_ALPHA)[0] = P(BICauchyalpha)[i];
        P(GENC_BETA)[0] = P(BICauchybeta)[i];
        D4generalisedCauchy(&z, cov, v + i);
        double dummy = P(BICauchyscale)[i]*P(BICauchyscale)[i];
        v[i] /=(dummy*dummy);

    }
     P(BICauchyalpha)[0] = alpha;
     P(BICauchybeta)[0] = beta;
     v[3] = v[2];
     v[1] *= P0(BICauchyrho);
     v[2] = v[1];
}


/* epsC -> generalised Cauchy : leading 1 is now an eps */
#define EPS_ALPHA 0
#define EPS_BETA 1
#define EPS_EPS 2
void epsC(double *x, cov_model *cov, double *v){
  double 
    alpha = P0(EPS_ALPHA), 
    beta=P0(EPS_BETA),
    eps=P0(EPS_EPS);
  *v = POW(eps + POW(*x, alpha), -beta/alpha);
 }
void logepsC(double *x, cov_model *cov, double *v, double *Sign){
  double 
    alpha = P0(EPS_ALPHA),
    beta=P0(EPS_BETA), 
    eps=P0(EPS_EPS);
  *v = LOG(eps + POW(*x, alpha)) * -beta/alpha;
  *Sign = 1.0;
 }
void DepsC(double *x, cov_model *cov, double *v){
  double ha, 
    y=*x,
    alpha = P0(EPS_ALPHA),
    beta=P0(EPS_BETA), 
    eps=P0(EPS_EPS);
  if (y ==0.0) {
    *v = (eps == 0.0) ? -INFTY : 
      ((alpha > 1.0) ? 0.0 : (alpha < 1.0) ? -INFTY : -beta); 
  } else {
    ha = POW(y, alpha - 1.0);
    *v = -beta * ha * POW(eps + ha * y, -beta / alpha - 1.0);
  }
}
void DDepsC(double *x, cov_model *cov, double *v){
  double ha, 
    y=*x,
    alpha = P0(EPS_ALPHA),
    beta=P0(EPS_BETA), 
    eps=P0(EPS_EPS);
  if (y ==0.0) {
    *v = (eps == 0.0) ? INFTY : ((alpha==2.0) ? beta * (beta + 1.0) : INFTY); 
  } else {
    ha = POW(y, alpha);
    *v = beta * ha / (y * y) * ( (1.0 - alpha) * eps + (1.0 + beta) * ha)
      * POW(eps + ha, -beta / alpha - 2.0);
  }
}
void InverseepsC(double *x, cov_model *cov, double *v){
  double 
    alpha = P0(EPS_ALPHA),
    beta=P0(EPS_BETA), 
    eps=P0(EPS_EPS);
  *v = (*x == 0.0) ? RF_INF : POW(POW(*x, -alpha / beta) - eps, 1.0 / alpha);
}
int checkepsC(cov_model *cov){
  double eps=P0(EPS_ALPHA);
  int i, err;
  if (cov->tsdim > 2)
    cov->pref[CircEmbedCutoff] = cov->pref[CircEmbedIntrinsic] = PREF_NONE;
  if ((err = checkkappas(cov, false)) != NOERROR) return err;
  kdefault(cov, EPS_ALPHA, 1.0); 
  kdefault(cov, EPS_BETA, 1.0); 
  kdefault(cov, EPS_EPS, 0.0); 
  if (ISNAN(eps) || eps == 0.0) {
    //  cov->domown=GENERALISEDCOVARIANCE; // later
    for (i=CircEmbed; i<Nothing; i++) cov->pref[i] = PREF_NONE;
  }
  
  return NOERROR;
}

void rangeepsC(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[EPS_ALPHA] = 0.0;
  range->max[EPS_ALPHA] = 2.0;
  range->pmin[EPS_ALPHA] = 0.05;
  range->pmax[EPS_ALPHA] = 2.0;
  range->openmin[EPS_ALPHA] = true;
  range->openmax[EPS_ALPHA] = false;

  range->min[EPS_BETA] = 0.0;
  range->max[EPS_BETA] = RF_INF;
  range->pmin[EPS_BETA] = 0.05;
  range->pmax[EPS_BETA] = 10.0;
  range->openmin[EPS_BETA] = true;
  range->openmax[EPS_BETA] = true;

  range->min[EPS_EPS] = 0.0;
  range->max[EPS_EPS] = RF_INF;
  range->pmin[EPS_EPS] = 0.0;
  range->pmax[EPS_EPS] = 10.0;
  range->openmin[EPS_EPS] = true; // false for generlised covariance
  range->openmax[EPS_EPS] = true;
}


/* gengneiting */
void genGneiting(double *x, cov_model *cov, double *v)
{
  int kk = P0INT(GENGNEITING_K);
  double s,
    mu=P0(GENGNEITING_MU),
    y=*x;
  if (y >= 1.0) {
    *v = 0.0; 
    return;
  }
  s = mu + 2.0 * (double) kk + 0.5;


  switch (kk) {
  case 0:
    *v = 1.0;
    break;
  case 1:
    *v =  1.0 + s*y ;
    break;
  case 2:
    *v = 1.0 + y * (s + y * (s*s-1.0) * ONETHIRD);
    break;
  case 3:
    *v = 1 + y * (s + y * (0.2 * (2.0*s*s - 3 + y * (s*s-4) * s * ONETHIRD)));
    break;
  default : BUG;
  }
  *v *=  POW(1.0 - y, s);
}


// control thanks to http://calc101.com/webMathematica/derivatives.jsp#topdoit

void DgenGneiting(double *x, cov_model *cov, double *v)
{
  int kk = P0INT(GENGNEITING_K);
  double s,
    mu=P0(GENGNEITING_MU), 
    y=*x;
  if (y >= 1.0) {
    *v = 0.0; 
    return;
  }
  s = mu + 2.0 * (double) kk + 0.5;
  
  switch (kk) {
  case 0: 
    *v = s;
    break;
  case 1:
    *v =  y * s  * (s + 1.0);
    break;
  case 2: 
    *v = ONETHIRD * (s * s + 3.0 * s + 2.0) * y * ((s - 1.0) * y + 1.0);
    break;
  case 3: 
    *v = y * (s * (s + 5) + 6) * (y * (s * (s-2) * y + 3 * s - 3) + 3) / 15.0;
    break;
  default : BUG;
  }
  *v *=  -POW(1.0 - y, s - 1.0);
  
}
void DDgenGneiting(double *x, cov_model *cov, double *v){
  int kk = P0INT(GENGNEITING_K);
  double s,
    mu=P0(GENGNEITING_MU), 
    y=*x;
  if (y >= 1.0) {
    *v = 0.0; 
    return;
  }
  s = mu + 2.0 * (double) kk + 0.5;
  switch (kk) {
  case 0: 
    *v = s * (s-1);
    break;
  case 1:
    *v = s  * (s + 1.0) * (s * y - 1) ; 
    break;
  case 2: {
    double s2;
    s2 = s * s;
    *v = ONETHIRD * (s2 + 3.0 * s + 2.0) * ( y * ((s2 - 1) * y - s + 2) - 1);
  }
    break;
  case 3: 
    double s2;
    s2 = s * s;
    *v = (s2  + 5 * s + 6) / 15.0 * 
      (y * (y * ((s2 - 4) * s * y + 6 * s - 3) -3 * s + 6) - 3);
    break;
  default : BUG;
  }
  *v *=  POW(1.0 - y, s - 2.0);
}


int checkgenGneiting(cov_model *cov){
  double mu=P0(GENGNEITING_MU), 
    dim = 2.0 * mu;
  cov->maxdim = (ISNAN(dim) || dim >= INFDIM) ? INFDIM : (int) dim;
  return NOERROR;
}

void rangegenGneiting(cov_model *cov, range_type *range){
  range->min[GENGNEITING_K] = range->pmin[GENGNEITING_K] = 0;
  range->max[GENGNEITING_K] = range->pmax[GENGNEITING_K] = 3; 
  range->openmin[GENGNEITING_K] = false;
  range->openmax[GENGNEITING_K] = false;
  
  range->min[GENGNEITING_MU] = 0.5 * (double) cov->tsdim; 
  range->max[GENGNEITING_MU] =  RF_INF;
  range->pmin[GENGNEITING_MU] = range->min[GENGNEITING_MU];
  range->pmax[GENGNEITING_MU] = range->pmin[GENGNEITING_MU] + 10.0;
  range->openmin[GENGNEITING_MU] = false;
  range->openmax[GENGNEITING_MU] = true;
}

/*
double porcusG(double t, double nu, double mu, double gamma) {
  double t0 = FABS(t);
  if (t0 > mu) return 0.0;
  return POW(t0, nu) * POW(1.0 - t0 / mu, gamma);
}
*/


double biGneitQuot(double t, double* scale, double *gamma) {
  double t0 = FABS(t);
  return POW(1.0 - t0 / scale[0], gamma[0]) *
    POW(1.0 - t0 / scale[3], gamma[3]) * POW(1.0 - t0 / scale[1], -2 * gamma[1]);
}

void biGneitingbasic(cov_model *cov, 
		     double *scale, 
		     double *gamma, 
		     double *cc
		     ){ 
  double 
    Sign, x12, min, det, a, b, c, sum,
    k = (double) (P0INT(GNEITING_K)),
    kP1 = k + (k >= 1),
    Mu = P0(GNEITING_MU),
    nu = Mu + 0.5,
    *sdiag = P(GNEITING_S), // >= 0 
    s12red = P0(GNEITING_SRED), // in [0,1]
    s12 = s12red * (sdiag[0] <= sdiag[1] ? sdiag[0] : sdiag[1]),

    *tildegamma = P(GNEITING_GAMMA), // 11,22,12

    *Cdiag = P(GNEITING_CDIAG),
    *C = P(GNEITING_C),
    rho = P0(GNEITING_RHORED);

  scale[0] = sdiag[0];
  scale[1] = scale[2] = s12; // = scale[2]
  scale[3] = sdiag[1];

  gamma[0] = tildegamma[0];
  gamma[1] =  gamma[2] = tildegamma[1]; //gamma[2]
  gamma[3] = tildegamma[2];

  sum = 0.0;
  if (scale[0] == scale[1]) sum += gamma[0];
  if (scale[0] == scale[3]) sum += gamma[3];
  if (sum > 2.0 * gamma[1]) ERR("values of gamma not valid.");

  min = 1.0;
  a = 2 * gamma[1] - gamma[0] - gamma[3];
  b = - 2 * gamma[1] * (scale[0] + scale[3]) + gamma[0] * (scale[1] + scale[3])
    + gamma[3] * (scale[1] + scale[0]);
  c = 2 * gamma[1] * scale[0] * scale[3] - gamma[0] * scale[1] * scale[3]
    - gamma[3] * scale[1] * scale[0]; 
  det = b * b - 4 * a * c;
  
  if (det >= 0) {
    det = SQRT(det);
    for (Sign=-1.0; Sign<=1.0; Sign+=2.0) {
      x12 = 0.5 / a * (-b + Sign * det);
      if (x12>0 && x12<scale[1]) {
	double dummy = biGneitQuot(x12, scale, gamma);
	if (dummy < min) min = dummy;
      }
    }    
  }

  cc[0] = C[0] = Cdiag[0];
  cc[3] = C[2] = Cdiag[1];
  cc[1] = cc[2] = C[1] = rho * SQRT(C[0] * C[2] * min) *
    POW(scale[1] * scale[1] / (scale[0] * scale[3]), 0.5 * (nu + 1 + 2.0 * k)) *
    EXP(lgammafn(1.0 + gamma[1]) - lgammafn(2.0 + nu + gamma[1] + kP1)
	+ 0.5 * (  lgammafn(2 + nu + gamma[0] + kP1) - lgammafn(1 + gamma[0])
		 + lgammafn(2 + nu + gamma[3] + kP1) - lgammafn(1 + gamma[3]))
	);
}


int initbiGneiting(cov_model *cov, gen_storage *stor) {
  double  
    *rho = P(GNEITING_RHORED),
    *s = P(GNEITING_S),
    *sred = P(GNEITING_SRED),
    *p_gamma = P(GNEITING_GAMMA),
    *c = P(GNEITING_C),
    *cdiag = P(GNEITING_CDIAG);
  bool check = stor->check;
  biwm_storage *S = cov->Sbiwm;
  assert(S != NULL);
  
  if (s == NULL) {
    PALLOC(GNEITING_S, 2, 1);
    s = P(GNEITING_S);
    s[0] = s[1] = 1.0;
  }
  
  if (sred == NULL) {
    PALLOC(GNEITING_SRED, 1, 1);
    sred = P(GNEITING_SRED);
    sred[0] = 1.0;
  }
  
  if (sred[0] == 1.0) {
    double sum =0.0;
    if (s[0] <= s[1]) sum += p_gamma[0];
    if (s[1] <= s[0]) sum += p_gamma[2];
    if (sum > 2.0 * p_gamma[1]) {
      SERR7("if '%s'=1, then %s[2] must be greater than min(%s[1], %s[3]) / 2 if%s[1] and %s[3] differ and %s[1] otherwise.", KNAME(GNEITING_SRED),
	    KNAME(GNEITING_GAMMA), KNAME(GNEITING_GAMMA),
	    KNAME(GNEITING_GAMMA), KNAME(GNEITING_GAMMA), 
	    KNAME(GNEITING_GAMMA), KNAME(GNEITING_GAMMA));
    }
  }

  if  (S->cdiag_given) {
    if (cdiag == NULL) {
      PALLOC(GNEITING_CDIAG, 2, 1);
      cdiag = P(GNEITING_CDIAG);
      cdiag[0] = cdiag[1] = 1.0;
    }
    if (rho == NULL) 
      QERRC2(GNEITING_RHORED, 
	     "'%s' and '%s' must be set at the same time ", GNEITING_CDIAG,
	     GNEITING_RHORED);
    if (check && c != NULL) {
      if (cov->nrow[GNEITING_C] != 3 || cov->ncol[GNEITING_C] != 1)
	QERRC(GNEITING_C, "must be a 3 x 1 vector");
      if (FABS(c[i11] - cdiag[0]) > c[i11] * epsilon || 
	  FABS(c[i22] - cdiag[1]) > c[i22] * epsilon ) {
	QERRC1(GNEITING_C, "does not match '%s'.", GNEITING_CDIAG);
      }
      double savec12 = c[i21];
      biGneitingbasic(cov, S->scale, S->gamma, S->c);
      if (FABS(c[i21] - savec12) > FABS(c[i21]) * epsilon)
 	QERRC1(GNEITING_C, "does not match '%s'.", GNEITING_RHORED);
    } else {
      if (PisNULL(GNEITING_C)) PALLOC(GNEITING_C, 3, 1);
      c = P(GNEITING_C);
      biGneitingbasic(cov, S->scale, S->gamma, S->c);
    }
  } else {
    if (c == NULL) 
      QERRC2(GNEITING_C, "either '%s' or '%s' must be set", 
	    GNEITING_C, GNEITING_RHORED);
    if (!ISNAN(c[i11]) && !ISNAN(c[i22]) && (c[i11] < 0.0 || c[i22] < 0.0))
      QERRC2(GNEITING_C, "variance parameter %s[0], %s[2] must be non-negative",
	     GNEITING_C, GNEITING_C);
    if (PisNULL(GNEITING_CDIAG)) PALLOC(GNEITING_CDIAG, 2, 1);
    if (PisNULL(GNEITING_RHORED)) PALLOC(GNEITING_RHORED, 1, 1);
    cdiag = P(GNEITING_CDIAG);
    rho = P(GNEITING_RHORED);
    cdiag[0] = c[i11];
    cdiag[1] = c[i22];
    double savec1 = c[i21];
    if (savec1 == 0.0)  rho[0] = 0.0; // wichtig falls c[0] oder c[2]=NA
    else {
      rho[0] = 1.0;
      biGneitingbasic(cov, S->scale, S->gamma, S->c);
      rho[0] = savec1 / c[i21];
    }
  }

  biGneitingbasic(cov, S->scale, S->gamma, S->c);
  cov->initialised = true;
  return NOERROR;
}


void kappa_biGneiting(int i, cov_model *cov, int *nr, int *nc){
  *nc = *nr = i < CovList[cov->nr].kappas? 1 : -1;
  if (i==GNEITING_S || i==GNEITING_CDIAG) *nr=2; else
    if (i==GNEITING_GAMMA || i==GNEITING_C) *nr=3 ;
}

void biGneiting(double *x, cov_model *cov, double *v) { 
  double z, 
    mu = P0(GNEITING_MU);
  int i;
  biwm_storage *S = cov->Sbiwm;
  assert(S != NULL);
  // wegen ML aufruf immer neu berechnet
 
  assert(cov->initialised);
   
  for (i=0; i<4; i++) {
    if (i==2) {
      v[2] = v[1];
      continue;
    }
    z = FABS(*x / S->scale[i]);
    P(GENGNEITING_MU)[0] = mu + S->gamma[i] + 1.0;
    genGneiting(&z, cov, v + i);
    v[i] *= S->c[i]; 
  }
  P(GENGNEITING_MU)[0] = mu;
}


void DbiGneiting(double *x, cov_model *cov, double *v){ 
  double z, 
    mu = P0(GENGNEITING_MU);
  int i;
  biwm_storage *S = cov->Sbiwm;
  assert(S != NULL);
  assert(cov->initialised);
 

  for (i=0; i<4; i++) {
    if (i==2) {
      v[2] = v[1];
      continue;
    }
    z = FABS(*x / S->scale[i]);
    P(GENGNEITING_MU)[0] = mu + S->gamma[i] + 1.0;
    DgenGneiting(&z, cov, v + i);
    v[i] *= S->c[i] / S->scale[i];
  }
  P(GENGNEITING_MU)[0] = mu;
}


void DDbiGneiting(double *x, cov_model *cov, double *v){ 
  double z,
    mu = P0(GENGNEITING_MU);
  int i;
 biwm_storage *S = cov->Sbiwm;
  assert(S != NULL);
  assert(cov->initialised);

 
   for (i=0; i<4; i++) {
    if (i==2) {
      v[2] = v[1];
      continue;
    }
    z = FABS(*x / S->scale[i]);
    P(GENGNEITING_MU)[0] = mu + S->gamma[i] + 1.0;
    DDgenGneiting(&z, cov, v + i);
    v[i] *= S->c[i] / (S->scale[i] * S->scale[i]);
  }
  P(GENGNEITING_MU)[0] = mu;
}

int checkbiGneiting(cov_model *cov) { 
  int err;
  gen_storage s;
  gen_NULL(&s);
  s.check = true;
  
  if ((err = checkkappas(cov, false)) != NOERROR) return err;
  if (PisNULL(GNEITING_K)) QERRC(GNEITING_K, "must be given.");
  if (PisNULL(GNEITING_MU)) QERRC(GNEITING_MU, "must be given.");
  if (PisNULL(GNEITING_GAMMA)) QERRC(GNEITING_GAMMA,"must be given.");

  NEW_STORAGE(biwm);
  biwm_storage *S = cov->Sbiwm;
  S->cdiag_given = !PisNULL(GNEITING_CDIAG) || !PisNULL(GNEITING_RHORED);
 
  if ((err=initbiGneiting(cov, &s)) != NOERROR) return err;

  int dim = 2.0 * P0(GENGNEITING_MU);
  cov->maxdim = (ISNAN(dim) || dim >= INFDIM) ? INFDIM : (int) dim;
  return NOERROR;
}
  


void rangebiGneiting(cov_model *cov, range_type *range){
 // *n = P0(GNEITING_K], 
  range->min[GNEITING_K] = range->pmin[GNEITING_K] = 0;
  range->max[GNEITING_K] = range->pmax[GNEITING_K] = 3;
  range->openmin[GNEITING_K] = false;
  range->openmax[GNEITING_K] = false;
  
 // *mu = P0(GNEITING_MU], 
  range->min[GNEITING_MU] = 0.5 * (double) cov->tsdim; 
  range->max[GNEITING_MU] =  RF_INF;
  range->pmin[GNEITING_MU] = range->min[GNEITING_MU];
  range->pmax[GNEITING_MU] = range->pmin[GNEITING_MU] + 10.0;
  range->openmin[GNEITING_MU] = false;
  range->openmax[GNEITING_MU] = true;
  
 // *scalediag = P0(GNEITING_S], 
  range->min[GNEITING_S] = 0.0;
  range->max[GNEITING_S] = RF_INF;
  range->pmin[GNEITING_S] = 1e-2;
  range->pmax[GNEITING_S] = 6.0;
  range->openmin[GNEITING_S] = true;
  range->openmax[GNEITING_S] = true;
  
  // *scalered12 = P0(GNEITING_SRED], 
  range->min[GNEITING_SRED] = 0;
  range->max[GNEITING_SRED] = 1;
  range->pmin[GNEITING_SRED] = 0.01;
  range->pmax[GNEITING_SRED] = 1;
  range->openmin[GNEITING_SRED] = true;
  range->openmax[GNEITING_SRED] = false;
    
  //   *gamma = P0(GNEITING_GAMMA], 
  range->min[GNEITING_GAMMA] = 0.0;
  range->max[GNEITING_GAMMA] = RF_INF;
  range->pmin[GNEITING_GAMMA] = 1e-5;
  range->pmax[GNEITING_GAMMA] = 100.0;
  range->openmin[GNEITING_GAMMA] = false;
  range->openmax[GNEITING_GAMMA] = true;
   
  //    *c diag = P0(GNEITING_CDIAG]; 
  range->min[GNEITING_CDIAG] = 0;
  range->max[GNEITING_CDIAG] = RF_INF;
  range->pmin[GNEITING_CDIAG] = 1e-05;
  range->pmax[GNEITING_CDIAG] = 1000;
  range->openmin[GNEITING_CDIAG] = true;
  range->openmax[GNEITING_CDIAG] = true; 
  
 //    *rho = P0(GNEITING_RHORED]; 
  range->min[GNEITING_RHORED] = -1;
  range->max[GNEITING_RHORED] = 1;
  range->pmin[GNEITING_RHORED] = -0.95;
  range->pmax[GNEITING_RHORED] = 0.95;
  range->openmin[GNEITING_RHORED] = false;
  range->openmax[GNEITING_RHORED] = false;    

 //    *rho = P0(GNEITING_C]; 
   range->min[GNEITING_C] = RF_NEGINF;
  range->max[GNEITING_C] = RF_INF;
  range->pmin[GNEITING_C] = -1000;
  range->pmax[GNEITING_C] = 1000;
  range->openmin[GNEITING_C] = true;
  range->openmax[GNEITING_C] = true; 
  
 }


/* Gneiting's functions -- alternative to Gaussian */
// #define Sqrt2TenD47 0.30089650263257344820 /* approcx 0.3 ?? */
#define GNEITING_ORIG 0
#define gneiting_origmu 1.5
#define NumericalScale 0.301187465825
#define kk_gneiting 3
#define mu_gneiting 2.683509
#define s_gneiting 0.2745640815
void Gneiting(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){ 
  double y = *x * cov->q[0];
  genGneiting(&y, cov, v);
}

 
void DGneiting(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){ 
  double y = *x * cov->q[0];
  DgenGneiting(&y, cov, v);
  *v  *= cov->q[0];
}


void DDGneiting(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){ 
  double y = *x * cov->q[0];
  DgenGneiting(&y, cov, v);
  *v  *= cov->q[0] * cov->q[0];
}

//void InverseGneiting(cov_model *cov, int scaling) {return 0.5854160193;}

int checkGneiting(cov_model *cov) { 
  int err;
  kdefault(cov, GNEITING_ORIG, 1);
  if ((err = checkkappas(cov)) != NOERROR) return err;
  bool orig = P0INT(GNEITING_ORIG);
  PFREE(GNEITING_ORIG);

  assert(cov->q == NULL);
  cov->nr = GNEITING_INTERN;
  QALLOC(1);
  
  if (orig) {
    cov->q[0] = NumericalScale;
    kdefault(cov, GENGNEITING_MU, gneiting_origmu);
    kdefault(cov, GENGNEITING_K, kk_gneiting);
  } else {
    cov->q[0] = s_gneiting;
    kdefault(cov, GENGNEITING_MU, mu_gneiting);
    kdefault(cov, GENGNEITING_K, kk_gneiting);
  }
  return checkgenGneiting(cov);
}

void rangeGneiting(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[GNEITING_ORIG] = range->pmin[GNEITING_ORIG] = 0;
  range->max[GNEITING_ORIG] = range->pmax[GNEITING_ORIG] = 1;
  range->openmin[GNEITING_ORIG] = false;
  range->openmax[GNEITING_ORIG] = false;
}



/* hyperbolic */
#define BOLIC_NU 0
#define BOLIC_XI 1
#define BOLIC_DELTA 2
void hyperbolic(double *x, cov_model *cov, double *v){ 
  double Sign;
  loghyperbolic(x, cov, v, &Sign);
  *v = EXP(*v);
}
void loghyperbolic(double *x, cov_model *cov, double *v, double *Sign){ 
  double 
    nu = P0(BOLIC_NU),
    xi=P0(BOLIC_XI), 
    delta=P0(BOLIC_DELTA);
  static double nuOld = RF_INF;
  static double xiOld= RF_INF;
  static double deltaOld = RF_INF;
  static double deltasq;
  static double xidelta;
  static double logconst;
  double 
    y=*x;
  *Sign = 1.0;
  if (y==0.0) { 
    *v = 0.0;
    return;
  } else if (y==RF_INF) {
    *v = RF_NEGINF;
    *Sign = 0.0;
    return;
  }
  if (delta==0) { // whittle matern
    if (nu > 80) warning("extremely imprecise results due to nu>80");
    *v = RU_logWM(y * xi, nu, nu, 0.0);
  } else if (xi==0) { //cauchy   => NU2 < 0 !
    y /= delta;
    /* note change in Sign as NU2<0 */
    *v = LOG(1.0 + y * y) * 0.5 * nu; 
  } else {
    if ((nu!=nuOld) || (xi!=xiOld) || (delta!=deltaOld)) {
    nuOld = nu; 
    xiOld = xi;
    deltaOld = delta;
    deltasq = deltaOld * deltaOld;
    xidelta = xiOld * deltaOld;
    logconst = xidelta - LOG(bessel_k(xidelta, nuOld, 2.0)) 
      - nu * LOG(deltaOld);
    }
    y=SQRT(deltasq + y * y);  
    double xiy = xi * y;
    *v = logconst + nu * LOG(y) + LOG(bessel_k(xiy, nu, 2.0)) - xiy;
  }
}
void Dhyperbolic(double *x, cov_model *cov, double *v)
{ 
  double 
    nu = P0(BOLIC_NU), 
    xi=P0(BOLIC_XI), 
    delta=P0(BOLIC_DELTA);
  static double nuOld = RF_INF;
  static double xiOld= RF_INF;
  static double deltaOld = RF_INF;
  static double deltasq;
  static double xidelta;
  static double logconst;
  double s,xi_s, logs, 
    y = *x;
  if (y==0.0) { 
    *v = 1.0;
    return;
  }
  if (delta==0) { // whittle matern
    *v = xi * RU_DWM(y * xi, nu, 0.0);
    *v *= xi;
  } else if (xi==0) { //cauchy
    double ha;
    y /= delta;
    ha = y * y;
    /* note change in Sign as NU2<0 */
    *v = nu * FABS(y) * POW(1.0 + ha, 0.5 * nu - 1.0) / delta;
  } else {
    if ((nu!=nu) || (xi!=xi) || (delta!=delta)) {
      nuOld = nu; 
      xiOld= xi;
      deltaOld = delta;
      deltasq = deltaOld * deltaOld;
      xidelta = xiOld * deltaOld;
      logconst = xidelta - LOG(bessel_k(xidelta, nuOld, 2.0)) 
	- nu * LOG(deltaOld);
    }
    s=SQRT(deltasq + y * y);
    xi_s = xi * s;
    logs = LOG(s);  
    *v = - y * xi * EXP(logconst + (nu-1.0) * logs 
		   + LOG(bessel_k(xi_s, nu-1.0, 2.0)) - xi_s);
  }
}
int checkhyperbolic(cov_model *cov){
  double 
    nu = P0(BOLIC_NU),
    xi=P0(BOLIC_XI),
    delta=P0(BOLIC_DELTA);
  char msg[255];
  int i;
  for (i=0; i<= Nothing; i++)
    cov->pref[i] *= (ISNAN(nu) || nu < BesselUpperB[i]);
  if (nu>0) {
    if ((delta<0) || (xi<=0)) {
      SPRINTF(msg, "xi>0 and delta>=0 if nu>0. Got nu=%f and delta=%f for xi=%f", nu, delta, xi);
      SERR(msg);
    }
  } else if (nu<0) {
    if ((delta<=0) || (xi<0)) {
      SPRINTF(msg, "xi>=0 and delta>0 if nu<0. Got nu=%f and delta=%f for xi=%f", nu, delta, xi);
      SERR(msg);
    }
  } else { // nu==0.0
    if ((delta<=0) || (xi<=0)) {
      SPRINTF(msg, "xi>0 and delta>0 if nu=0. Got nu=%f and delta=%f for xi=%f", nu, delta, xi);
      SERR(msg);
    }
  }
  return NOERROR;
}
void rangehyperbolic(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[BOLIC_NU] = RF_NEGINF;
  range->max[BOLIC_NU] = RF_INF;
  range->pmin[BOLIC_NU] = -20.0;
  range->pmax[BOLIC_NU] = 20.0;
  range->openmin[BOLIC_NU] = true;
  range->openmax[BOLIC_NU] = true;

  int i;
  for (i=1; i<=2; i++) { 
    range->min[i] = 0.0;
    range->max[i] = RF_INF;
    range->pmin[i] = 0.000001;
    range->pmax[i] = 10.0;
    range->openmin[i] = false;
    range->openmax[i] = true;
  }
}


/* iaco cesare model */
#define CES_NU 0
#define CES_LAMBDA 1
#define CES_DELTA 2
void IacoCesare(double *x, cov_model *cov, double *v){
    double
      nu = P0(CES_NU), 
      lambda=P0(CES_LAMBDA), 
      delta=P0(CES_DELTA);
    *v = POW(1.0 + POW(x[0], nu) + POW(x[1], lambda), - delta); 
}
void rangeIacoCesare(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[CES_NU] = 0.0;
  range->max[CES_NU] = 2.0;
  range->pmin[CES_NU] = 0.0;
  range->pmax[CES_NU] = 2.0;
  range->openmin[CES_NU] = true;
  range->openmax[CES_NU] = false;

  range->min[CES_LAMBDA] = 0.0;
  range->max[CES_LAMBDA] = 2.0;
  range->pmin[CES_LAMBDA] = 0.0;
  range->pmax[CES_LAMBDA] = 2.0;
  range->openmin[CES_LAMBDA] = true;
  range->openmax[CES_LAMBDA] = false;
 
  range->min[CES_DELTA] = 0.0;
  range->max[CES_DELTA] = RF_INF;
  range->pmin[CES_DELTA] = 0.0;
  range->pmax[CES_DELTA] = 10.0;
  range->openmin[CES_DELTA] = true;
  range->openmax[CES_DELTA] = true;
}



/* Kolmogorov model */
void Kolmogorov(double *x, cov_model *cov, double *v){
#define fourthird 1.33333333333333333333333333333333333
#define onethird 0.333333333333333333333333333333333
  int d, i, j,
    dim = cov->tsdim,
    dimP1 = dim + 1,
    dimsq = dim * dim;
  double
    rM23, r23,
    r2 =0.0;

  assert(dim == cov->vdim[0] && dim == cov->vdim[1]);

  for (d=0; d<dimsq; v[d++] = 0.0);
  for (d=0; d<dim; d++) r2 += x[d] * x[d];
  if (r2 == 0.0) return;

  rM23 = onethird / r2; // r^-2 /3

  for (d=0; d<dimsq; d+=dimP1) v[d] = fourthird;
  for (d=i= 0; i<dim ; i++) {
    for(j=0; j<dim; j++) {
      v[d++] -= rM23 * x[i] * x[j];
    }
  }

  r23 = -POW(r2, onethird);  // ! -
  for (d=0; d<dimsq; v[d++] *= r23);

}


int checkKolmogorov(cov_model *cov) { 
   if (cov->xdimown != 3) SERR1("dim (%d) != 3", cov->xdimown);
 if (cov->tsdim != cov->xdimprev || cov->tsdim != cov->xdimown) 
    return ERRORDIM;
  return NOERROR;
}
 

/* local-global distinguisher */
#define LGD_ALPHA 0
#define LGD_BETA 1
void lgd1(double *x, cov_model *cov, double *v) {
  double y = *x, alpha = P0(LGD_ALPHA), beta=P0(LGD_BETA);
  *v = (y == 0.0) ? 1.0 : (y < 1.0) 
    ? 1.0 - beta / (alpha + beta) * POW(y, alpha)
    : alpha / (alpha + beta) * POW(y,  -beta);
}
void Inverselgd1(double *x, cov_model *cov, double *v) {
  double alpha = P0(LGD_ALPHA), beta=P0(LGD_BETA);
  ERR("scle of lgd1 not programmed yet"); 
   // 19 next line?!
  // 1.0 / .... fehlt auch
  *v = (19 * alpha < beta)
     ? EXP(LOG(1.0 - *x * (alpha + beta) / beta) / alpha)
     : EXP(LOG(*x * (alpha + beta) / alpha) / beta);
}
void Dlgd1(double *x, cov_model *cov, double *v){
  double y=*x, pp, alpha = P0(LGD_ALPHA), beta=P0(LGD_BETA);
  if (y == 0.0) {
    *v = 0.0;// falscher Wert, aber sonst NAN-Fehler#
    return;
  }
  pp = ( (y < 1.0) ? alpha : -beta ) - 1.0;
  *v = - alpha * beta / (alpha + beta) * EXP(pp * y);
}
int checklgd1(cov_model *cov){
  double dim = 2.0 * (1.5 - P0(LGD_ALPHA));
  cov->maxdim = (ISNAN(dim) || dim >= 2.0) ? 2 : (int) dim;
  return NOERROR;
}
void rangelgd1(cov_model *cov, range_type *range) {
  range->min[LGD_ALPHA] = 0.0;
  range->max[LGD_ALPHA] = (cov->tsdim==2) ? 0.5 : 1.0;
  range->pmin[LGD_ALPHA] = 0.01;
  range->pmax[LGD_ALPHA] = range->max[LGD_ALPHA];
  range->openmin[LGD_ALPHA] = true;
  range->openmax[LGD_ALPHA] = false;

  range->min[LGD_BETA] = 0.0;
  range->max[LGD_BETA] = RF_INF;
  range->pmin[LGD_BETA] = 0.01;
  range->pmax[LGD_BETA] = 20.0;
  range->openmin[LGD_BETA] = true;
  range->openmax[LGD_BETA] = true;
 
}


/* mastein */
// see Hypermodel.cc


#define GET_NU_GEN(NU) (PisNULL(WM_NOTINV) || P0INT(WM_NOTINV) ? NU : 1.0 / NU)
#define GET_NU GET_NU_GEN(P0(WM_NU))
  
double logNonStWM(double *x, double *y, cov_model *cov, double factor){
  cov_model *nu = cov->kappasub[WM_NU];
  int 
    dim = cov->tsdim;
  double nux, nuy, v,
    norm=0.0;
  for (int d=0; d<dim; d++) {
    double delta = x[d] - y[d];
    norm += delta * delta;
  }
  norm = SQRT(norm);
  
  if (nu == NULL) {
    nux = nuy = GET_NU;
  } else {
    FCTN(x, nu, &nux);
    FCTN(y, nu, &nuy);
    if (nux <= 0.0 || nuy <= 0.0)
      ERR1("'%s' is not a positive function", KNAME(WM_NU));
    nux = GET_NU_GEN(nux);
    nuy = GET_NU_GEN(nuy);
  }

  v = RU_logWM(norm, nux, nuy, factor);
  assert(!ISNA(v));
  return v;
}



double ScaleWM(double nu){
  // it is working reasonably well if nu is in [0.001,100]
  // happy to get any better suggestion!!

  static int nstuetz = 19;
  static double stuetz[]=
  {1.41062516176753e-14, 4.41556861847671e-12, 2.22633601732610e-06, 
   1.58113643548649e-03, 4.22181082102606e-02, 2.25024764696152e-01,
   5.70478218148777e-01, 1.03102017706644e+00, 1.57836638352906e+00,
   2.21866372852304e+00, 2.99573229151620e+00, 3.99852231863082e+00,
   5.36837527567695e+00, 7.30561120838150e+00, 1.00809957038601e+01,
   1.40580075785156e+01, 1.97332533513488e+01, 2.78005149402352e+01,
   3.92400265713477e+01};
  static int stuetzorigin = 11;
  
  return interpolate(LOG(nu) * INVLOG2, stuetz, nstuetz, stuetzorigin, 
		     1.5, 5);
}

int checkWM(cov_model *cov) { 
  cov_model *nusub = cov->kappasub[WM_NU];
  static double
    spectrallimit=0.17,
    spectralbest=0.4;
  int i, err;
  bool isna_nu;
 
  if ((err = checkkappas(cov, false)) != NOERROR) return err;

  //  printf("%d %s %s\n", nusub == NULL, DOMAIN_NAMES[cov->domown],
  //	 ISONAMES[cov->isoown]);

  if (nusub != NULL && !isRandom(nusub)) {    
    int dim = cov->tsdim;
    if (cov->domown != KERNEL || cov->isoown != SYMMETRIC) 
      //return ERRORFAILED;
      SERR2("kernel needed. Got %s, %s.",
	    DOMAIN_NAMES[cov->domown], ISONAMES[cov->isoown]);
    ASSERT_CARTESIAN;
    if ((err = CHECK(nusub, dim, dim, ShapeType, XONLY, CARTESIAN_COORD,
		     SCALAR, cov->role // ROLE_COV changed 20.7.14 wg spectral
		     )) != NOERROR) 
      return err;
    if (nusub->tsdim != dim) return ERRORWRONGDIM;
    cov->monotone = NORMAL_MIXTURE;

     // no setbackard !!
    return NOERROR;    
  }

  if (cov->domown != XONLY || !isAnyIsotropic(cov->isoown)) 
    // return ERRORFAILED;
    SERR2("isotropic function needed. Got %s, %s.",
	    DOMAIN_NAMES[cov->domown], ISONAMES[cov->isoown]);

  if (PisNULL(WM_NU)) QERRC(0, "parameter unset"); 
  //  kdefault(cov, WM_NOTINV, true);
  double nu = GET_NU;
  isna_nu = ISNAN(nu);
  
  for (i=0; i<= Nothing; i++) cov->pref[i] *= isna_nu || nu < BesselUpperB[i];
  if (nu<spectralbest) {
    cov->pref[SpectralTBM] = (nu < spectrallimit) ? PREF_NONE : 3;
  } 
  if (cov->tsdim > 2) 
    cov->pref[CircEmbedCutoff] = cov->pref[CircEmbedIntrinsic] = PREF_NONE;
  if (nu > 2.5)  cov->pref[CircEmbed] = 2;

  cov->full_derivs = isna_nu ? -1 : (int) (nu - 1.0);
  cov->monotone = nu <= 0.5 ? COMPLETELY_MON : NORMAL_MIXTURE;

  return NOERROR;
}

void rangeWM(cov_model *cov, range_type *range){
  bool tcf = isTcf(cov->typus) || cov->isoown == SPHERICAL_ISOTROPIC;
  if (tcf) {    
    if (PisNULL(WM_NOTINV) || P0INT(WM_NOTINV)) {
      range->min[WM_NU] = 0.0;
      range->max[WM_NU] = 0.5;
      range->pmin[WM_NU] = 1e-1;
      range->pmax[WM_NU] = 0.5;
      range->openmin[WM_NU] = true;
      range->openmax[WM_NU] = false;
    } else {
      range->min[WM_NU] = 2.0;
      range->max[WM_NU] = RF_INF;
      range->pmin[WM_NU] = 2.0;
      range->pmax[WM_NU] = 10.0;
      range->openmin[WM_NU] = false;
      range->openmax[WM_NU] = true;
    }
  } else { // not tcf
    range->min[WM_NU] = 0.0;
    range->max[WM_NU] = RF_INF;
    range->pmin[WM_NU] = 1e-1;
    range->pmax[WM_NU] = 10.0;
    range->openmin[WM_NU] = true;
    range->openmax[WM_NU] = false;
  }
 
  range->min[WM_NOTINV] = 0.0;
  range->max[WM_NOTINV] = 1.0;
  range->pmin[WM_NOTINV] = 0.0;
  range->pmax[WM_NOTINV] = 1.0;
  range->openmin[WM_NOTINV] = false;
  range->openmax[WM_NOTINV] = false;
}


void coinitWM(cov_model *cov, localinfotype *li) {
  // cutoff_A
  double thres[2] = {0.25, 0.5},
    nu = GET_NU;
  if (nu <= thres[0]) {
    li->instances = 2;
    li->value[0] = 0.5;
    li->value[1] = 1.0;
    li->msg[0] = li->msg[1] = MSGLOCAL_OK;
  } else {
    li->instances = 1;
    li->value[0] = 1.0; //  q[CUTOFF_A] 
    li->msg[0] = (nu <= thres[1]) ? MSGLOCAL_OK : MSGLOCAL_JUSTTRY;
  } 
}

void ieinitWM(cov_model *cov, localinfotype *li) {
  double nu = GET_NU;
  // intrinsic_rawr
  li->instances = 1;
  if (nu <= 0.5) {
    li->value[0] = 1.0;
    li->msg[0] = MSGLOCAL_OK;
  } else {
    li->value[0] = 1.5;
    li->msg[0] = MSGLOCAL_JUSTTRY;
  }
}

double densityWM(double *x, cov_model *cov, double factor) {
  double x2,
    nu = GET_NU,
    powfactor = 1.0;
  int d,
    dim =  cov->tsdim;
  if (nu > 50)
    warning("nu>50 in density of matern class numerically instable. The results cannot be trusted.");
  if (factor == 0.0) factor = 1.0; else factor *= SQRT(nu);
  x2 = x[0] * x[0];
  for (d=1; d<dim; d++) {
    x2 += x[d] * x[d];
    powfactor *= factor;
  }
  x2 /= factor * factor;
  x2 += 1.0;
  
  return powfactor * EXP(lgammafn(nu + 0.5 * (double) dim)
			 - lgammafn(nu)
			 - (double) dim * M_LN_SQRT_PI
			 - (nu + 0.5 * (double) dim) * LOG(x2));
}


void Matern(double *x, cov_model *cov, double *v) {
  *v = RU_WM(*x, GET_NU, SQRT2);
}

void logMatern(double *x, cov_model *cov, double *v, double *Sign) { 
  double nu = GET_NU;
  *v = RU_logWM(*x, nu, nu, SQRT2);
  *Sign = 1.0;
}

void NonStMatern(double *x, double *y, cov_model *cov, double *v){ 
  *v = EXP(logNonStWM(x, y, cov, SQRT2));
}

void logNonStMatern(double *x, double *y, cov_model *cov, double *v, 
		     double *Sign){ 
  *Sign = 1.0;
  *v = logNonStWM(x, y, cov, SQRT2);
}

void DMatern(double *x, cov_model *cov, double *v) {
  *v =RU_DWM(*x, GET_NU, SQRT2);
} 

void DDMatern(double *x, cov_model *cov, double *v) {
  *v=RU_DDWM(*x, GET_NU, SQRT2);
} 

void D3Matern(double *x, cov_model *cov, double *v) {
  *v=RU_D3WM(*x, GET_NU, SQRT2);
} 

void D4Matern(double *x, cov_model *cov, double *v) {
  *v=RU_D4WM(*x, GET_NU, SQRT2);
} 

void InverseMatern(double *x, cov_model *cov, double *v) {
  double
    nu = GET_NU;
  *v =  *x == 0.05 ? SQRT2 * SQRT(nu) /  ScaleWM(nu) : RF_NA;
}

int checkMatern(cov_model *cov) { 
  kdefault(cov, WM_NOTINV, true);
  return checkWM(cov);
}

double densityMatern(double *x, cov_model *cov) {
  return densityWM(x, cov, SQRT2);
}

int initMatern(cov_model *cov, gen_storage *s) {
  if (HAS_SPECTRAL_ROLE(cov)) {
    spec_properties *cs = &(s->spec);
    if (cov->tsdim <= 2) return NOERROR;
    cs->density = densityMatern;
    return search_metropolis(cov, s);
  }

  else ILLEGAL_ROLE;

}

void spectralMatern(cov_model *cov, gen_storage *S, double *e) { 
  spectral_storage *s = &(S->Sspectral);
  /* see Yaglom ! */
  if (cov->tsdim <= 2) {
    double nu = GET_NU;
    E12(s, cov->tsdim, 
	SQRT( 2.0 * nu * (POW(1.0 - UNIFORM_RANDOM, -1.0 / nu) - 1.0) ), e);
  } else {
    metropolis(cov, S, e);
  }
}



// Brownian motion 
#define OESTING_BETA 0
void oesting(double *x, cov_model *cov, double *v) {
  // klein beta interagiert mit 'define beta Rf_beta' in Rmath
  double Beta = P0(OESTING_BETA),
    x2 = *x * *x;
  *v = - x2 * POW(1 + x2, -Beta);//this is an invalid covariance function!
  // keep definition such that the value at the origin is 0
}
/* oesting: first derivative at t=1 */
void Doesting(double *x, cov_model *cov, double *v) 
{// FALSE VALUE FOR *x==0 and  Beta < 1
  double Beta = P0(OESTING_BETA),
    x2 = *x * *x;
  *v = 2 * *x  * (1 + (1-Beta) * x2) * POW(1 + x2, -Beta-1);
}
/* oesting: second derivative at t=1 */
void DDoesting(double *x, cov_model *cov, double *v)  
{// FALSE VALUE FOR *x==0 and  beta < 2
  double Beta = P0(OESTING_BETA),
    x2 = *x * *x;
  *v = 2 * (1 + (2 - 5 * Beta) * x2 + (1 - 3 * Beta + 2 * Beta * Beta) * 
	    x2  *x2) * POW(1 + x2, -Beta-2);
}
int checkoesting(cov_model *cov){
  cov->logspeed = RF_INF;
  cov->full_derivs = cov->rese_derivs;
  return NOERROR;
}
int initoesting(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  double Beta = P0(OESTING_BETA);
  cov->taylor[1][TaylorConst] = + Beta;
  cov->taylor[2][TaylorConst] = - 0.5 * Beta * (Beta + 1);  
  cov->tail[0][TaylorPow] = 2 - 2 * Beta;
  return NOERROR;
}
void rangeoesting(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[OESTING_BETA] = 0.0;
  range->max[OESTING_BETA] = 1.0;
  range->pmin[OESTING_BETA] = UNIT_EPSILON;
  range->pmax[OESTING_BETA] = 1.0;
  range->openmin[OESTING_BETA] = true;
  range->openmax[OESTING_BETA] = false;
}


/* penta */
void penta(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v)
{ ///
  double y=*x, y2 = y * y;
  *v = (y >= 1.0) ? 0.0 :
    (1.0 + y2 * (-7.333333333333333 
		 + y2 * (33.0 +
			 y * (-38.5 + 
			      y2 * (16.5 + 
				    y2 * (-5.5 + 
					  y2 * 0.833333333333333))))));
}
void Dpenta(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v)
{ 
  double y=*x, y2 = y * y;
  *v = (y >= 1.0) ? 0.0 :
    y * (-14.66666666666666667 + 
	 y2 * (132.0 + 
	       y * (-192.5 + 
		    y2 * (115.5 + 
			  y2 * (-49.5 + 
				y2 * 9.16666666666666667)))));
  
}
void Inversepenta(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) {
  *v = (*x==0.05) ? 1.0 / 1.6552838957365 : RF_NA;
}


/* power model */ 
#define POW_ALPHA 0
void power(double *x, cov_model *cov, double *v){
  double alpha = P0(POW_ALPHA), y = *x;
  *v = (y >= 1.0) ? 0.0 : POW(1.0 - y, alpha);
}
void TBM2power(double *x, cov_model *cov, double *v){
  // only alpha=2 up to now !
  double y = *x;
  if (P0(POW_ALPHA) != 2.0) 
    ERR("TBM2 of power only allowed for alpha=2");
  *v = (y > 1.0)  
    ? (1.0 - 2.0 * y *(ASIN(1.0 / y) - y + SQRT(y * y - 1.0) ))
    : 1.0 - y * (PI - 2.0 * y);
}
void Dpower(double *x, cov_model *cov, double *v){
  double alpha = P0(POW_ALPHA), y = *x;
  *v = (y >= 1.0) ? 0.0 : - alpha * POW(1.0 - y, alpha - 1.0);
}
int checkpower(cov_model *cov) {
  double alpha = P0(POW_ALPHA);
  double dim = 2.0 * alpha - 1.0;
  cov->maxdim = (ISNAN(dim) || dim >= INFDIM) 
    ? INFDIM-1 : (int) dim;
  cov->monotone = alpha >= (double) ((cov->tsdim / 2) + 1)
    ? COMPLETELY_MON : NORMAL_MIXTURE;
  return NOERROR;
}


// range definition:
// 0: min, theory, 1:max, theory
// 2: min, practically 3:max, practically
void rangepower(cov_model *cov, range_type *range){
  bool tcf = isTcf(cov->typus) || cov->isoown == SPHERICAL_ISOTROPIC;
  range->min[POW_ALPHA] = tcf
    ? (double) ((cov->tsdim / 2) + 1) // Formel stimmt so! Nicht aendern!
    : 0.5 * (double) (cov->tsdim + 1);
  range->max[POW_ALPHA] = RF_INF;
  range->pmin[POW_ALPHA] = range->min[POW_ALPHA];
  range->pmax[POW_ALPHA] = 20.0;
  range->openmin[POW_ALPHA] = false;
  range->openmax[POW_ALPHA] = true;
}


/* qexponential -- derivative of exponential */
#define QEXP_ALPHA 0
void qexponential(double *x, cov_model *cov, double *v){
  double 
    alpha = P0(QEXP_ALPHA),
    y = EXP(-*x);
  *v = y * (2.0  - alpha * y) / (2.0 - alpha);
}
void Inverseqexponential(double *x, cov_model *cov, double *v){
  double alpha = P0(QEXP_ALPHA);
  *v = -LOG( (1.0 - SQRT(1.0 - alpha * (2.0 - alpha) * *x)) / alpha);
} 
void Dqexponential(double *x, cov_model *cov, double *v) {
  double 
    alpha = P0(QEXP_ALPHA), 
    y = EXP(-*x);
  *v = y * (alpha * y - 1.0) * 2.0 / (2.0 - alpha);
}
void rangeqexponential(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[QEXP_ALPHA] = 0.0;
  range->max[QEXP_ALPHA] = 1.0;
  range->pmin[QEXP_ALPHA] = 0.0;
  range->pmax[QEXP_ALPHA] = 1.0;
  range->openmin[QEXP_ALPHA] = false;
  range->openmax[QEXP_ALPHA] = false;
}


/* spherical model */ 
void spherical(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){
  double y = *x;
  *v = (y >= 1.0) ? 0.0 : 1.0 + y * 0.5 * (y * y - 3.0);
}
// void Inversespherical(cov_model *cov){ return 1.23243208931941;}
void TBM2spherical(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){
  double y = *x , y2 = y * y;
  *v = (y>1.0) 
      ? (1.0- 0.75 * y * ((2.0 - y2) * ASIN(1.0/y) + SQRT(y2 -1.0)))
      : (1.0 - 0.375 * PI * y * (2.0 - y2));
}
void Dspherical(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){
  double y = *x;
  *v = (y >= 1.0) ? 0.0 : 1.5 * (y * y - 1.0);
}

void DDspherical(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){
  *v = (*x >= 1.0) ? 0.0 : 3 * *x;
}

int structspherical(cov_model *cov, cov_model **newmodel) {
  return structCircSph( cov, newmodel, 3);
}
void dospherical(cov_model VARIABLE_IS_NOT_USED *cov, gen_storage VARIABLE_IS_NOT_USED *s) { 
  // todo diese void do... in Primitive necessary??
  //  mppinfotype *info = &(s->mppinfo);
  //info->radius = cov->mpp.refradius;
}


double alphaIntSpherical(int a) {
  // int r^a C(r) \D r
  double A = (double) a;
  return 1.0 / (A + 1.0) - 1.5 / (A + 2.0) + 0.5 / (A + 4.0);
}

int initspherical(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  int  dim = cov->tsdim;

  if (hasNoRole(cov)) {
    if (cov->mpp.moments >= 1) SERR("too high moments required");
    return NOERROR;
  }

  if (hasAnyShapeRole(cov)) {

    assert(cov->mpp.maxheights[0] == 1.0);
    if (cov->mpp.moments >= 1) {
      cov->mpp.mM[1] = cov->mpp.mMplus[1] =  
	SurfaceSphere(dim - 1, 1) * alphaIntSpherical(dim - 1);
    }

  }

  else ILLEGAL_ROLE;

 return NOERROR;
}

/* stable model */
#define STABLE_ALPHA 0
void stable(double *x, cov_model *cov, double *v){
  double y = *x, alpha = P0(STABLE_ALPHA);  
  *v = (y==0.0) ? 1.0 : EXP(-POW(y, alpha));
}
void logstable(double *x, cov_model *cov, double *v, double *Sign){
  double y = *x, alpha = P0(STABLE_ALPHA);  
  *v = (y==0.0) ? 0.0 : -POW(y, alpha);
  *Sign = 1.0;
}
void Dstable(double *x, cov_model *cov, double *v){
  double z, y = *x, alpha = P0(STABLE_ALPHA);
  if (y == 0.0) {
    *v = (alpha > 1.0) ? 0.0 : (alpha < 1.0) ? INFTY : 1.0;
  } else {
    z = POW(y, alpha - 1.0);
    *v = -alpha * z * EXP(-z * y);
  }
}
/* stable: second derivative at t=1 */
void DDstable(double *x, cov_model *cov, double *v) 
{
  double z, y = *x, alpha = P0(STABLE_ALPHA), xalpha;
  if (y == 0.0) {
      *v = (alpha == 1.0) ? 1.0 : (alpha == 2.0) ? alpha * (1 - alpha) : INFTY;
  } else {
    z = POW(y, alpha - 2.0);
    xalpha = z * y * y;
    *v = alpha * (1.0 - alpha + alpha * xalpha) * z * EXP(-xalpha);
  }
}

void D3stable(double *x, cov_model *cov, double *v)
{
  double z, y = *x, alpha = P0(STABLE_ALPHA), xalpha;
  if (y == 0.0) {
      *v = (alpha == 1.0) ? -1.0 : (alpha == 2.0) ? 0 : -INFTY;
  } else {
    z = POW(y, alpha - 3.0);
    xalpha = z * y * y * y;
    *v = -alpha * ( 3*alpha*(xalpha -1) + alpha*alpha*(xalpha*xalpha - 3*xalpha + 1) + 2) * z * EXP(-xalpha);
  }
}

void D4stable(double *x, cov_model *cov, double *v)
{
  double z, y = *x, alpha = P0(STABLE_ALPHA), xalpha;
  if (y == 0.0) {
      *v = (alpha == 1.0) ? 1.0 : (alpha == 2.0) ? 0 : INFTY;
  } else {
    z = POW(y, alpha - 4.0);
    xalpha = z * y * y * y * y;
    *v = alpha*( alpha*alpha*alpha*(7*xalpha -6*xalpha*xalpha  +xalpha*xalpha*xalpha - 1 ) +
           +6*alpha*alpha*(-3*xalpha + xalpha*xalpha +1 )+
           11*alpha*(xalpha - 1 ) + 6 ) * z * EXP(-xalpha);

  }
}

void D5stable(double *x, cov_model *cov, double *v)
{
  double z, y = *x, alpha = P0(STABLE_ALPHA), xalpha;
  if (y == 0.0) {
      *v = (alpha == 1.0) ? -1.0 : (alpha == 2.0) ? 0 : -INFTY;
  } else {
    z = POW(y, alpha - 5.0);
    xalpha = z * y * y * y * y * y;
    *v = -alpha*( POW(alpha, 4)*(-15*xalpha +25*xalpha*xalpha - 10*POW(xalpha, 3) + POW(xalpha, 4)+ 1 ) +
                  10*alpha*alpha*alpha*(7*xalpha  -6*xalpha*xalpha + POW(xalpha, 3) - 1 )+
                  35*alpha*alpha*(-3*xalpha + xalpha*xalpha +  1 )+ 50*alpha*(xalpha -1 ) + 24 )* z * EXP(-xalpha);
  }
}


void Inversestable(double *x, cov_model *cov, double *v){
  double y = *x, alpha = P0(STABLE_ALPHA);  
  *v = y>1.0 ? 0.0 : y == 0.0 ? RF_INF : POW( - LOG(y), 1.0 / alpha);
}
void nonstatLogInversestable(double *x, cov_model *cov,
			     double *left, double *right){
  double 
    alpha = P0(STABLE_ALPHA),
    z = *x <= 0.0 ? POW( - *x, 1.0 / alpha) : 0.0;
  int d,
    dim = cov->tsdim;
  for (d=0; d<dim; d++) {
    left[d] = -z;
    right[d] = z;
  }
}


int checkstable(cov_model *cov) {
  double alpha = P0(STABLE_ALPHA);
  if (cov->tsdim > 2)
    cov->pref[CircEmbedCutoff] = cov->pref[CircEmbedIntrinsic] = PREF_NONE;
  if (alpha == 2.0)
    cov->pref[CircEmbed] = 2;
  
  cov->monotone = alpha <= 1.0 ? COMPLETELY_MON : NORMAL_MIXTURE;

  return NOERROR;
}
void rangestable(cov_model *cov, range_type *range){
  bool tcf = isTcf(cov->typus) || cov->isoown == SPHERICAL_ISOTROPIC;
  range->min[STABLE_ALPHA] = 0.0;
  range->max[STABLE_ALPHA] = tcf ? 1.0 : 2.0;
  range->pmin[STABLE_ALPHA] = 0.06;
  range->pmax[STABLE_ALPHA] =  range->max[STABLE_ALPHA];
  range->openmin[STABLE_ALPHA] = true;
  range->openmax[STABLE_ALPHA] = false;
}
void coinitstable(cov_model *cov, localinfotype *li) {
  coinitgenCauchy(cov, li);
}
void ieinitstable(cov_model *cov, localinfotype *li) {  
  ieinitgenCauchy(cov, li);
}


/* SPACEISOTROPIC stable model for testing purposes only */
void stableX(double *x, cov_model *cov, double *v){
  double y, alpha = P0(STABLE_ALPHA);
  y = x[0] * x[0] + x[1] * x[1];
  *v = (y==0.0) ? 1.0 : EXP(-POW(y, 0.5 * alpha));
}
void DstableX(double *x, cov_model *cov, double *v){
  double z, y, alpha = P0(STABLE_ALPHA);
  y = x[0] * x[0] + x[1] * x[1];
  if (y == 0.0) {
    *v = ((alpha > 1.0) ? 0.0 : (alpha < 1.0) ? INFTY : 1.0);
  } else {
    z = POW(y, 0.5 * alpha - 1.0);
    *v = -alpha * x[0] * z * EXP(- z * y);
  }
}
/* END SPACEISOTROPIC stable model for testing purposes only */



// ************************* bivariate powered exponential or bivariate stable


void kappa_biStable(int i, cov_model VARIABLE_IS_NOT_USED *cov, int *nr, int *nc){
    *nc = 1;
    if ( i == BIStablealpha || i == BIStablescale ) {
    *nr = 3;
    }
    if (i == BIStablecdiag ) {
        *nr = 2;
    }
    if (i == BIStablerho ) {
        *nr = 1;
    }
}

int checkbiStable(cov_model *cov) {


    int err;
    gen_storage s;
    gen_NULL(&s);
    s.check = true;

    if ((err = checkkappas(cov, false)) != NOERROR) return err;

 //   bistable_storage *S = cov->Sbistable;

     if ((err=initbiStable(cov, &s)) != NOERROR) return err;

    cov->vdim[0] = cov->vdim[1] = 2;

    return NOERROR;


  return NOERROR;
}


void rangebiStable(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){

  // *nudiag = P0(BInudiag],
  range->min[BIStablealpha] = 0.0;
  range->max[BIStablealpha] = 1;

  range->pmin[BIStablealpha] = 0.06;
  range->pmax[BIStablealpha] = 1;

  range->openmin[BIStablealpha] = true;
  range->openmax[BIStablealpha] = true;

 //   s = P0(BIs],
  range->min[BIStablescale] = 0.0;
  range->max[BIStablescale] = RF_INF;
  range->pmin[BIStablescale] = 1e-2;
  range->pmax[BIStablescale] = 100.0;
  range->openmin[BIStablescale] = true;
  range->openmax[BIStablescale] = true;

  //    *c = P0(BIc];
  // to do: check rhos
  
  //  int dim = cov->tsdim;
  //  double *scale = P(BIStablescale);

  range->min[BIStablerho] = -1;
  range->max[BIStablerho] = 1;
  range->pmin[BIStablerho] = -1;
  range->pmax[BIStablerho] = 1;
  range->openmin[BIStablerho] = false;
  range->openmax[BIStablerho] = false;

  range->min[BIStablecdiag] = 0;
  range->max[BIStablecdiag] = RF_INF;
  range->pmin[BIStablecdiag] = 0.001;
  range->pmax[BIStablecdiag] = 1000;
  range->openmin[BIStablecdiag] = false;
  range->openmax[BIStablecdiag] = true;

}

void biStablePolynome(double r, double alpha, double a, int dim, double *v ) {
    double x = POW(a*r, alpha);

    if (dim == 1) {
        *v = alpha*x - alpha + 1;
    }
    if ( dim == 2 || dim == 3 ) {
        *v = alpha*alpha*x*x - 3*alpha*alpha*x + 4*alpha*x + alpha*alpha - 4*alpha + 3;
    }

}

void biStableUnderInf(double r, double *alpha, double *a, int dim, double *res ) {

    double x = POW(a[i11]*r, alpha[i11]),
           y = POW(a[i21]*r, alpha[i21]),
           z = POW(a[i22]*r, alpha[i22]);
    double p1, p2, p3;
           biStablePolynome(r, alpha[i11], a[i11], dim, &p1 );
           biStablePolynome(r, alpha[i21], a[i21], dim, &p2 );
           biStablePolynome(r, alpha[i22], a[i22], dim, &p3 );

    if (r == 0) {
        *res = -1;
    }
    else {
        *res =  alpha[i11]*alpha[i22]/(alpha[i21]*alpha[i21])*
                POW(a[i11], alpha[i11])*POW(a[i22], alpha[i22])/POW(a[i21], 2*alpha[i21])*
                POW(r, alpha[i11] + alpha[i22] - 2*alpha[i21])*
                EXP(2*y - x - z)*p1*p3/(p2*p2);
    }
}

void biStableInterval(double *alpha, double *a, int dim, double *left, double *right ) {
    double middle = 1,
           fmiddle, fright, fleft;
    *left = *right = middle;

    biStableUnderInf(middle, alpha, a, dim, &fmiddle);

    fright = fleft = fmiddle;

    while (( fmiddle >= MIN(fleft, fright))  && (MIN(fleft, MIN(fright, fmiddle)) > epsilon ) )  {

       if ( fleft <= fmiddle )  {
         middle = *left;
         fmiddle = fleft;
         *left = *left/2;
       }

       if (fright <= fmiddle) {
         middle = *right;
         fmiddle = fright;
         *right = *right*2;
       }

       biStableUnderInf(*right, alpha, a, dim, &fright );
       biStableUnderInf(*left, alpha, a, dim,  &fleft );
     }

     if (MIN(fleft, MIN(fright, fmiddle)) <= epsilon ) {
       left = 0;
       right = 0;
     }

}

/*
 * Golden search algorithm from Numerical Recipes in C,
 * book by William H. Press
 *
 * (c, c)  - interval for searching
 *  *alpha, *a - array of parameters of biStable model
 *  *rhomax - maximum allowable rho for *alpha, *a, the result of
 * the searching
 * dim - dimension of the model
 * */


void biStableMinRho(cov_model *cov, double ax, double cx, double *rhomax) {

    double *alpha = cov->Sbistable->alpha;
    double *a = cov->Sbistable->scale;


    double bx = ax + (cx - ax)*GOLDENC,
           f1, f2, x0, x1, x2, x3, dummy;
    x0 = ax;
    x3 = cx;
    if ( FABS(cx - bx) > FABS(bx - ax) ) {
        x1 = bx;
        x2 = bx + GOLDENC*(bx - cx);
    } else {
        x2 = bx;
        x1 = bx - GOLDENC*(bx - ax);
    }
    biStableUnderInf(x1, alpha, a, cov->tsdim, &f1);
    biStableUnderInf(x2, alpha, a, cov->tsdim, &f2);

    while ( FABS(x3 - x0) > GOLDENTOL*(FABS(x1) + FABS(x2) ) ) {
        if (f2 < f1) {
            GOLDENSHIFT3(x0, x1, x2, GOLDENR*x1 + GOLDENC*x3 )
            biStableUnderInf(x2, alpha, a, cov->tsdim, &dummy);
            GOLDENSHIFT2(f1, f2, dummy )
        } else {
            GOLDENSHIFT3(x3, x2, x1, GOLDENR*x2 + GOLDENC*x0 )
            biStableUnderInf(x1, alpha, a, cov->tsdim, &dummy);
            GOLDENSHIFT2(f2, f1, dummy )
        }
    }
    if (f1 < f2) {
        *rhomax  = SQRT(f1);
    } else {
        *rhomax  = SQRT(f2);
    }

}


int initbiStable(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *stor) {
  double a[3],// lg[3],
    // aorig[3],
    *rho = P(BIStablerho),
    // *cdiag = P(BIStablecdiag),
    *alpha = P(BIStablealpha),
    *s = P(BIStablescale),
    rhomax = -2, //rhored = -2,
     left = 0,
     right = 0;
  int dim = cov->tsdim;

   NEW_STORAGE(bistable);
   bistable_storage *S = cov->Sbistable;

   // printf("\n initbiStable \n ");

    cov->Sbistable->scale[0] = a[i11] = 1/s[i11];
    cov->Sbistable->scale[1] = a[i21] = 1/s[i21];
    cov->Sbistable->scale[2] = a[i22] = 1/s[i22];

    cov->Sbistable->alpha[0] = alpha[i11];
    cov->Sbistable->alpha[1] = alpha[i21];
    cov->Sbistable->alpha[2] = alpha[i22];

    if( alpha[i21] < MAX(alpha[i11], alpha[i22])  ) {
        QERRC(BIStablealpha, "This combination of smoothness parameters is not allowed.");
    }

    if ( ( alpha[i11] == alpha[i21] ) &&  ( alpha[i22] == alpha[i21] ) &&
            ( POW(a[i21], alpha[i11]) < 0.5*POW(a[i11], alpha[i11])+0.5*POW(a[i22], alpha[i11]) ) ) {
        QERRC(BIStablealpha, "This combination of smoothness parameters and scale parameters is not allowed.");
    }

    if ( ( alpha[i11] == alpha[i21] ) &&  ( alpha[i11] > alpha[i22] ) &&
         ( a[i21] <= POW(0.5, 1/alpha[i11])*a[i11] ) ) {
        QERRC(BIStablealpha, "This combination of smoothness parameters and scale parameters is not allowed.");
    }

    if ( ( alpha[i22] == alpha[i21] ) &&  ( alpha[i22] > alpha[i11] ) &&
         ( a[i21] > POW(0.5, 1/alpha[i22])*a[i22] ) ) {
        QERRC(BIStablealpha, "This combination of smoothness parameters and scale parameters is not allowed.");
    }

    biStableInterval(alpha, a, dim, &left, &right );
    if ( (right == 0) && (left == 0)   ) {
        rhomax = 0;
    }

    biStableMinRho(cov, left, right, &rhomax);

    if (FABS(*rho) > rhomax ) {
        QERRC(BIStablealpha, "The value of cross-correlation parameter rho is too big.");
    }


    S->rhomax = rhomax;
    S->rhored=*rho/rhomax;

     cov->initialised = true;
     return NOERROR;
}

void coinitbiStable(cov_model *cov, localinfotype *li) {
    double
            thres = 1,
            *alpha = P(BIStablealpha);

    if ( ( alpha[0] <= thres ) &&  ( alpha[1] <= thres ) && ( alpha[2] <= thres ) ) {
        li->instances = 1;
        li->value[0] = CUTOFF_THIRD_CONDITION; //  q[CUTOFF_A]
        li->msg[0] =MSGLOCAL_OK;
    }
    else {
        li->instances = 1;
        li->value[0] = CUTOFF_THIRD_CONDITION ; //  q[CUTOFF_A]
        li->msg[0] = MSGLOCAL_JUSTTRY;
    }
}

void biStable (double *x, cov_model *cov, double *v) {
    int i;
    double
        //    *c = P(BIStablerho),
            alpha = P0(STABLE_ALPHA),
            y = *x, z;

    assert(BIStablealpha == STABLE_ALPHA);

 /*   biStable_storage *S = cov->SbiStable;
    assert(S != NULL);
    */


    for (i=0; i<3; i++) {
        z = y/P(BIStablescale)[i];
        P(STABLE_ALPHA)[0] = P(BIStablealpha)[i];
        stable(&z, cov, v + i);
    }
    P(BIStablealpha)[0] = alpha;
    v[3] = v[2];
    v[1] *= P0(BIStablerho);
    v[2] = v[1];
}



void DbiStable(double *x, cov_model *cov, double *v) {
    int i;
    double
        //    *c = P(BIStablerho),
            alpha = P0(STABLE_ALPHA),
            y = *x, z;
    assert(BIStablealpha == STABLE_ALPHA);

  /*  biStable_storage *S = cov->SbiStable;
    assert(S != NULL);
   */

    for (i=0; i<3; i++) {
        z = y/P(BIStablescale)[i];
        P(STABLE_ALPHA)[0] = P(BIStablealpha)[i];
        Dstable(&z, cov,  v + i);
        v[i] /= P(BIStablescale)[i];
    }
    P(BIStablealpha)[0] = alpha;
    v[3] = v[2];
    v[1] *= P0(BIStablerho);
    v[2] = v[1];
}


void DDbiStable(double *x, cov_model *cov, double *v) {
    int i;
    double
            alpha = P0(STABLE_ALPHA),
            y = *x, z;
    assert(BIStablealpha == STABLE_ALPHA);

  /*  biStable_storage *S = cov->SbiStable;
    assert(S != NULL);
    */

    for (i=0; i<3; i++) {
        z = y/P(BIStablescale)[i];
        P(STABLE_ALPHA)[0] = P(BIStablealpha)[i];
        DDstable(&z, cov,  v + i);
        v[i] /= P(BIStablescale)[i]*P(BIStablescale)[i];
    }
    P(BIStablealpha)[0] = alpha;
    v[3] = v[2];
    v[1] *= P0(BIStablerho);
    v[2] = v[1];
}
void D3biStable(double *x, cov_model *cov, double *v) {
    int i;
    double
     //       *c = P(BIStablerho),
            alpha = P0(STABLE_ALPHA),
            y = *x, z;
    assert(BIStablealpha == STABLE_ALPHA);

 /*   biStable_storage *S = cov->SbiStable;
    assert(S != NULL);
   */

    for (i=0; i<3; i++) {
        z = y/P(BIStablescale)[i];
        P(STABLE_ALPHA)[0] = P(BIStablealpha)[i];
        D3stable(&z, cov, v + i);
        v[i] /= P(BIStablescale)[i]*P(BIStablescale)[i]*P(BIStablescale)[i];
    }
    P(BIStablealpha)[0] = alpha;
    v[3] = v[2];
    v[1] *= P0(BIStablerho);
    v[2] = v[1];
}
void D4biStable(double *x, cov_model *cov, double *v) {
    int i;
    double
      //      *c = P(BIStablerho),
            alpha = P0(STABLE_ALPHA),
            y = *x, z;
 //   assert(BIStablealpha == STABLE_ALPHA);

/*    biStable_storage *S = cov->SbiStable;
    assert(S != NULL);
  */
//    assert(cov->initialised);


    for (i=0; i<3; i++) {
        z = y/P(BIStablescale)[i];
        P(STABLE_ALPHA)[0] = P(BIStablealpha)[i];
        D4stable(&z, cov, v + i);
        double dummy =P(BIStablescale)[i]*P(BIStablescale)[i];
        v[i] /= dummy*dummy;
    }
    P(BIStablealpha)[0] = alpha;
    v[3] = v[2];
    v[1] *= P0(BIStablerho);
    v[2] = v[1];
}



/* stein space-time model */
#define STEIN_NU 0
#define STEIN_Z 1
void kappaSteinST1(int i, cov_model *cov, int *nr, int *nc){
  *nc = 1;
  *nr = (i == STEIN_NU) ? 1 : (i==STEIN_Z) ?  cov->tsdim - 1 : -1;
}
void SteinST1(double *x, cov_model *cov, double *v){
/* 2^(1-nu)/Gamma(nu) [ h^nu K_nu(h) - 2 * tau (x T z) t h^{nu-1} K_{nu-1}(h) /
   (2 nu + d + 1) ]

   \|tau z \|<=1 hence \tau z replaced by z !!
*/
  int d,
    dim = cov->tsdim,
    time = dim - 1;
  double logconst, hz, y,
    nu = P0(STEIN_NU),
    *z=P(STEIN_Z);
  
  static double nuold=RF_INF;
  static double loggamma;
  static int dimold;

  if (nu != nuold || dimold != dim) {
    nuold = nu;
    dimold = dim;
    loggamma = lgammafn(nu);
  }

  hz = 0.0;
  y = x[time] * x[time];
  for (d=0; d<time; d++) {
    y += x[d] * x[d];
    hz += x[d] * z[d];
  }
  
  if ( y==0.0 ) *v = 1.0;
  else {
    y = SQRT(y);
    logconst = (nu - 1.0) * LOG(0.5 * y)  - loggamma;
    *v =  y * EXP(logconst + LOG(bessel_k(y, nu, 2.0)) - y)
      - 2.0 * hz * x[time] * EXP(logconst + LOG(bessel_k(y, nu - 1.0, 2.0)) -y) 
      / (2.0 * nu + dim);
  }

}

int checkSteinST1(cov_model *cov) {  
  double nu = P0(STEIN_NU), *z= P(STEIN_Z), absz;
  int d, spatialdim=cov->tsdim-1;

  for (d=0; d<= Nothing; d++) cov->pref[d] *= (nu < BesselUpperB[d]);
  if (nu >= 2.5) cov->pref[CircEmbed] = 2;
  if (spatialdim < 1) 
    SERR("dimension of coordinates, including time, must be at least 2");
  	
  for (absz=0.0, d=0;  d<spatialdim; d++)  absz += z[d] * z[d];
  if (ISNAN(absz))
    SERR("currently, components of z cannot be estimated by MLE, so NA's are not allowed");
  if (absz > 1.0 + UNIT_EPSILON && !GLOBAL_UTILS->basic.skipchecks) {
    SERR("||z|| must be less than or equal to 1");
  }
  return NOERROR;
}
double densitySteinST1(double *x, cov_model *cov) {
  double x2, wz, dWM, nu = P0(STEIN_NU), 
    *z=P(STEIN_Z);
  int d,
    dim = cov->tsdim,
    spatialdim = dim - 1;
  static double nuold = RF_INF;
  static int dimold = -1;
  static double constant;
  static double factor;
  if (nu != nuold || dimold != dim) {
    nuold = nu;
    dimold = dim;
    constant = lgammafn(nu) - lgammafn(nu +  0.5 * dim) - dim * M_LN_SQRT_PI;
    factor = nu + dim;
  }

  x2 = x[spatialdim] * x[spatialdim]; // v^2
  wz = 0.0;
  for (d=0; d<spatialdim; d++) {
    x2 += x[d] * x[d];  // w^2 + v^2
    wz += x[d] * z[d];
  }
  dWM = EXP(constant - factor * LOG(x2 + 1.0));
  return (1.0 + 2.0 * wz * x[spatialdim] + x2) * dWM
    / (2.0 * nu + (double) dim + 1.0);
}


int initSteinST1(cov_model *cov, gen_storage *s) {
  if (HAS_SPECTRAL_ROLE(cov)) {
    spec_properties *cs = &(s->spec);
    cs->density = densitySteinST1;
    return search_metropolis(cov, s);
  }

  else ILLEGAL_ROLE;

}
void spectralSteinST1(cov_model *cov, gen_storage *S, double *e){
  metropolis(cov, S, e);
}

void rangeSteinST1(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){  
  range->min[STEIN_NU] = 1.0;
  range->max[STEIN_NU] = RF_INF;
  range->pmin[STEIN_NU] = 1e-10;
  range->pmax[STEIN_NU] = 10.0;
  range->openmin[STEIN_NU] = true;
  range->openmax[STEIN_NU] = true;
 
  range->min[STEIN_Z] = RF_NEGINF;
  range->max[STEIN_Z] = RF_INF;
  range->pmin[STEIN_Z] = -10.0;
  range->pmax[STEIN_Z] = 10.0;
  range->openmin[STEIN_Z] = true;
  range->openmax[STEIN_Z] = true;
}


/* wave */
void wave(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) {
  double y = *x;
  *v = (y==0.0) ? 1.0 : (y==RF_INF) ? 0 : SIN(y) / y;
}
void Inversewave(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) {
  *v = (*x==0.05) ? 1.0/0.302320850755833 : RF_NA;
}
int initwave(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  if (HAS_SPECTRAL_ROLE(cov)) {
    return (cov->tsdim <= 2) ? NOERROR : ERRORFAILED;
  } 

  else ILLEGAL_ROLE;

}
void spectralwave(cov_model *cov, gen_storage *S, double *e) { 
  spectral_storage *s = &(S->Sspectral);
  /* see Yaglom ! */
  double x;  
  x = UNIFORM_RANDOM; 
  E12(s, cov->tsdim, SQRT(1.0 - x * x), e);
}



void Whittle(double *x, cov_model *cov, double *v) {
  double nu = GET_NU;
  *v = RU_logWM(*x, nu, nu, 0.0);
  *v = RU_WM(*x, nu, 0.0);
  assert(!ISNAN(*v));
}


void logWhittle(double *x, cov_model *cov, double *v, double *Sign) {
  double nu = GET_NU;
  *v = RU_logWM(*x, nu, nu, 0.0);
  assert(!ISNA(*v));
  *Sign = 1.0;
}


void NonStWhittle(double *x, double *y, cov_model *cov, double *v){ 
  *v = EXP(logNonStWM(x, y, cov, 0.0));
}

void logNonStWhittle(double *x, double *y, cov_model *cov, double *v, 
		     double *Sign){ 
  *Sign = 1.0;
  *v = logNonStWM(x, y, cov, 0.0);
}

void DWhittle(double *x, cov_model *cov, double *v) {
  *v =RU_DWM(*x, GET_NU, 0.0);
}

void DDWhittle(double *x, cov_model *cov, double *v)
// check calling functions, like hyperbolic and gneiting if any changings !!
{ 
  *v=RU_DDWM(*x, GET_NU, 0.0);
}


void D3Whittle(double *x, cov_model *cov, double *v)
// check calling functions, like hyperbolic and gneiting if any changings !!
{ 
  *v=RU_D3WM(*x, GET_NU, PisNULL(WM_NOTINV) ? 0.0 : SQRT2);
}

void D4Whittle(double *x, cov_model *cov, double *v)
// check calling functions, like hyperbolic and gneiting if any changings !!
{
  BUG;
  *v=RU_D4WM(*x, GET_NU, PisNULL(WM_NOTINV) ? 0.0 : SQRT2);
}

void InverseWhittle(double *x, cov_model *cov, double *v){
  double nu = GET_NU;
  *v = (*x == 0.05) ? 1.0 / ScaleWM(nu) : RF_NA;
}

void TBM2Whittle(double *x, cov_model *cov, double *v) 
{
  double nu = GET_NU;
  assert(PisNULL(WM_NOTINV));
  if (nu == 0.5) TBM2exponential(x, cov, v);
  else BUG;
}


double densityWhittle(double *x, cov_model *cov) {
  return densityWM(x, cov, PisNULL(WM_NOTINV) ? 0.0 : SQRT2);
}
int initWhittle(cov_model *cov, gen_storage *s) {
  if (HAS_SPECTRAL_ROLE(cov)) {
    if (PisNULL(WM_NU)) {
      spec_properties *cs = &(s->spec);
      if (cov->tsdim <= 2) return NOERROR;
      cs->density = densityWhittle; 
      int err=search_metropolis(cov, s);
      return err;
    } else return initMatern(cov, s);
  }

  else ILLEGAL_ROLE;

}

void spectralWhittle(cov_model *cov, gen_storage *S, double *e) { 
  spectral_storage *s = &(S->Sspectral);
  /* see Yaglom ! */
  if (PisNULL(WM_NOTINV)) {
    if (cov->tsdim <= 2) {
      double nu = GET_NU;
      E12(s, cov->tsdim, SQRT(POW(1.0 - UNIFORM_RANDOM, -1.0 / nu) - 1.0), e);
    } else {
      metropolis(cov, S, e);
    }
  } else spectralMatern(cov, S, e);
}


void DrawMixWM(cov_model VARIABLE_IS_NOT_USED *cov, double *random) { // inv scale
  // V ~ F in PSgen
  *random = -0.25 / LOG(UNIFORM_RANDOM);
}

double LogMixDensW(double VARIABLE_IS_NOT_USED *x, double logV, cov_model *cov) {
  double
    nu=GET_NU;
  return PisNULL(WM_NOTINV)
    ? (M_LN2  + 0.5 * logV) * (1.0 - nu) - 0.5 *lgammafn(nu)
    // - 0.25 /  cov->mpp[DRAWMIX_V]  - 2.0 * (LOG2 + logV) )
    : RF_NA;  /* !! */
}




// using nu^(-1-nu+a)/2 for g and v^-a e^{-1/4v} as density instead of frechet
// the bound 1/3 can be dropped
// static double eM025 = EXP(-0.25);
//void DrawMixNonStWM(cov_model *cov, double *random) { // inv scale
//  // V ~ F in stp
//  cov_model *nu = cov->sub[WM_NU];  
//  double minnu;
//  double alpha;
//
//  if (nu == NULL) {
//    minnu = P(WM_NU][0];
//  } else {
//    double minmax[2];
//    CovList[nu->nr].minmaxeigenvalue(nu, minmax);
//    minnu = minmax[0];
//  }
//  alpha = 1.0 + 0.5 /* 0< . < 1*/ * (3.0 * minnu - 0.5 * cov->tsdim);
//  if (alpha > 2.0) alpha = 2.0; // original choice
//  if (alpha <= 1.0) ERR("minimal nu too low or dimension too high");
//
// ERR("logmixdensnonstwm not programmed yet");
 /* 
  double beta = GLOBAL.mpp.beta,
    p = GLOBAL.mpp.p,
    logU;
  if (UNIFORM_RANDOM < p){
    cov_a->WMalpha = beta;
    logU =  LOG(UNIFORM_RANDOM * eM025);
    cov_a->WMfactor = -0.5 * LOG(0.25 * p * (beta - 1.0)) + 0.25;
  } else {
    cov_a->WMalpha = alpha;
    logU = LOG(eM025 + UNIFORM_RANDOM * (1.0 - eM025));
    cov_a->WMfactor = -0.5 * LOG(0.25 * (1.0 - p) * (alpha - 1.0));
  } 
  
  logmix!!

  *random = LOG(-0.25 / logU) / (cov_a->WMalpha - 1.0); //=invscale
  */
//}




//double LogMixDensNonStWM(double *x, double logV, cov_model *cov) {
//  // g(v,x) in stp
//  double z = 0.0;
//  ERR("logmixdensnonstwm not programmed yet");
  // wmfactor ist kompletter unsinn; die 2 Teildichten muessen addiert werden
  /*
  cov_model *calling = cov->calling,
    *Nu = cov->sub[0];
  if (calling == NULL) BUG;
   double nu,
    alpha = cov_a->WMalpha,
    logV = cov_a->logV,
    V = cov_a->V;
  
  if (Nu == NULL) 
    nu = P(WM_NU][0];
  else 
     FCTN(x, Nu, &nu);


   z = - nu  * M_LN2 // in g0  // eine 2 kuerzt sich raus
    + 0.5 * ((1.0 - nu) // in g0
    + alpha // lambda
    - 2.0 //fre*
    ) * logV
    - 0.5 * lgammafn(nu)  // in g0
    + cov_a->WMfactor // lambda
    - 0.125 / V   // g: Frechet
    + 0.125 * POW(V, 1.0 - alpha); // lambda: frechet

  if (!(z < 7.0)) {
    static double storage = 0.0; 
    if (gen_storage != logV) {
      if (PL >= PL_DETAILS) 
	PRINTF("alpha=%f, is=%f, cnst=%f logi=%f lgam=%f loga=%f invlogs=%f pow=%f z=%f\n",
	       alpha,V,
	       (1.0 - nu) * M_LN2 
	       , + ((1.0 - nu) * 0.5 + alpha - 2.0) * logV
	       ,- 0.5 * lgammafn(nu) 
	       , -cov_a->WMfactor
	       ,- 0.25 / V 
	       , + 0.25 * POW(V, - alpha)
	       , z);
      storage = logV;
    }
    //assert(z < 10.0);
  }
*/
//  return z;
//				      
//}







void Whittle2(double *x, cov_model *cov, double *v) {
  *v =RU_WM(*x, GET_NU, 0.0);
  assert(!ISNAN(*v));
}


void logWhittle2(double *x, cov_model *cov, double *v, double *Sign) {
  double nu = GET_NU;
  *v = RU_logWM(*x, nu, nu, 0.0);
  assert(!ISNA(*v));
  *Sign = 1.0;
}

void DWhittle2(double *x, cov_model *cov, double *v) {
  *v =RU_DWM(*x, GET_NU, 0.0);
}

void DDWhittle2(double *x, cov_model *cov, double *v)
// check calling functions, like hyperbolic and gneiting if any changings !!
{ 
  *v=RU_DDWM(*x, GET_NU, 0.0);
}


void D3Whittle2(double *x, cov_model *cov, double *v)
// check calling functions, like hyperbolic and gneiting if any changings !!
{ 
  *v=RU_D3WM(*x, GET_NU, 0.0);
}

void D4Whittle2(double *x, cov_model *cov, double *v)
// check calling functions, like hyperbolic and gneiting if any changings !!
{ 
  *v=RU_D4WM(*x, GET_NU, 0.0);
}

void InverseWhittle2(double *x, cov_model *cov, double *v){
  *v = (*x == 0.05) ? 1.0 / ScaleWM(GET_NU) : RF_NA;
}


/* Whittle-Matern or Whittle or Besset */ 
/// only lower parts of the matrices, including the diagonals are used when estimating !!

static bool Bi = !true;


/* Whittle-Matern or Whittle or Besset */ 

void biWM2basic(cov_model *cov, 
		double *a, double *lg,
		double *aorig, double *nunew) {
  // !! nudiag, nured has priority over nu
  // !! cdiag, rhored has priority over c

  double factor, beta, gamma, tsq, t1sq, t2sq, inf, infQ, discr,
    alpha , a2[3],
    dim = (double) cov->tsdim, 
    d2 = dim * 0.5, 
    *nudiag = P(BInudiag),
    nured = P0(BInured),
    *nu = P(BInu),
    *c = P(BIc),
    *s = P(BIs),
    *cdiag = P(BIcdiag),
    rho = P0(BIrhored);
  int i, 
    *notinvnu = PINT(BInotinvnu);

  nu[i11] = nudiag[0];
  nu[i22] = nudiag[1];
  nu[i21] = 0.5 * (nu[i11] + nu[i22]) * nured;
  
  for (i=0; i<3; i++) {
    aorig[i] = 1.0 / s[i];
    //if (Bi) print("%d %f %f \n", i, s[i], aorig[i]);
  } 

  if (PisNULL(BInotinvnu)) {
    for (i=0; i<3; i++) {
      a[i] = aorig[i];
      nunew[i] = nu[i];
    }
  } else {
    if (!notinvnu[0]) for (i=0; i<3; i++) nu[i] = 1.0 / nu[i];
    for (i=0; i<3; i++) {
      nunew[i] = nu[i] < MATERN_NU_THRES ? nu[i] : MATERN_NU_THRES;
      a[i] = aorig[i] * SQRT(2.0 * nunew[i]);
    }
  }


  for (i=0; i<3; i++) {
    a2[i] = a[i] * a[i];
    lg[i] = lgammafn(nunew[i]);
  }
  
  alpha = 2 * nunew[i21] - nunew[i11] - nunew[i22];


  // **************** ACHTUNG !!!!! *********
  // nicht gut, sollte in check einmal berechnet werden 
  // dies wiederspricht aber der MLE Maximierung, da dann
  // neu berechnet werden muss; verlg. natsc und cutoff (wo es nicht
  // programmiert ist !!)
  
  factor = EXP(lgammafn(nunew[i11] + d2) - lg[i11]
	       + lgammafn(nunew[i22] + d2) - lg[i22]
		   + 2.0 * (lg[i21]  - lgammafn(nunew[i21] + d2)
			    + nunew[i11] * LOG(a[i11]) + nunew[i22] * LOG(a[i22])
			    - 2.0 * nunew[i21] * LOG(a[i21]))
	);

  
  // alpha u^2 + beta u + gamma = 0
  gamma = (2.0 * nunew[i21] + dim) * a2[i11] * a2[i22] 
    - (nunew[i22] + d2) * a2[i11] * a2[i21]
    - (nunew[i11] + d2) * a2[i22] * a2[i21];
      
  beta = (2.0 * nunew[i21] - nunew[i22] + d2) * a2[i11]
      + (2.0 * nunew[i21] - nunew[i11] + d2) * a2[i22]
      - (nunew[i11] + nunew[i22] + dim) * a2[i21];
  
  //  if (Bi) print("%f %f %f %f %f\n"
  //		, 2.0 * nunew[i21], - nunew[i11], + d2 , a2[i22]
  //	    , (nunew[i11] + nunew[i22] + dim) * a2[i22]);

//  
  // if (Bi) print("\nalpha=%f beta=%f gamma=%f\n", alpha, beta, gamma);
  //  if (Bi)  print("\nnu=%f %f %f, a2=%f %f %f\n", 
  //		  nunew[0], nunew[1], nunew[2], a2[0], a2[1], a2[2]);
     
  //  if (Bi) print("%d %f %f %f NU22 %f\n", i22, nu[0], nu[1], nu[2], nu[i22]);
     
  if (nured == 1.0) { // lin. eqn only
    t2sq = (beta == 0.0) ? 0.0 : -gamma / beta;
    if (t2sq < 0.0) t2sq = 0.0;
    t1sq =  t2sq;
  } else { // quadratic eqn.
    discr = beta * beta - 4.0 * alpha * gamma;
    if (discr < 0.0) {
      t1sq = t2sq = 0.0;
    } else {
      discr = SQRT(discr);
      t1sq = (-beta + discr) / (2.0 * alpha);
      if (t1sq < 0.0) t1sq = 0.0;
      t2sq = (-beta - discr) / (2.0 * alpha);
      if (t2sq < 0.0) t2sq = 0.0;	  
    }
  }


  inf = nured == 1.0 ? 1.0 : RF_INF; // t^2 = infty  ; nudiag[1]>1.0 not
  //                                   allowed by definition
  for (i=0; i<3; i++) {
    tsq = (i == 0) ? 0.0 
      : (i == 1) ? t1sq
      : t2sq;
    
    infQ = POW(a2[i21] + tsq, 2.0 * nunew[i21] + dim) /
      (POW(a2[i11] + tsq, nunew[i11] + d2) 
       * POW(a2[i22] + tsq, nunew[i22] + d2));

    if (infQ < inf) inf = infQ;
  }

  c[i11] = cdiag[0];
  c[i22] = cdiag[1];
  c[i21] = rho * SQRT(factor * inf * c[i11] *  c[i22]);
 
  if (Bi) print("c=%f %f %f rho=%f %f %f\n", c[0], c[1],c[2], rho, factor, inf);
  Bi = false;
 
}

int initbiWM2(cov_model *cov, gen_storage *stor) {
  double a[3], lg[3], aorig[3], nunew[3], 
    *c = P(BIc),
    *cdiag = P(BIcdiag),
    *nu = P(BInu),
    *nudiag = P(BInudiag);
  bool check = stor->check;
  int i,
    *notinvnu = PINT(BInotinvnu);
  biwm_storage *S = cov->Sbiwm;
  assert(S != NULL);
  if (S->nudiag_given) {
    kdefault(cov, BInured, 1.0);
    if (check && !PisNULL(BInu)) {
      if (cov->nrow[BInu] != 3 || cov->ncol[BInu] != 1)
	QERRC(BInu, "must be a 3 x 1 vector");
      if (FABS(nu[i11] - nudiag[0]) > nu[i11] * epsilon || 
	  FABS(nu[i22] - nudiag[1]) > nu[i22] * epsilon ||
	  FABS(nu[i21] - 0.5 * (nudiag[i11] + nudiag[1]) * P0(BInured))
	  > nu[i21] * epsilon) {
	QERRC2(BInu, "does not match '%s' and '%s'.", BInudiag, BInured);
      }
    } else {
      if (PisNULL(BInu)) PALLOC(BInu, 3, 1);
      nu = P(BInu); 
      nu[i11] = nudiag[0];
      nu[i22] = nudiag[1];
      nu[i21] = 0.5 * (nu[i11] + nu[i22]) * P0(BInured);
    }
  } else {
    if (check && !PisNULL(BInured)) 
      QERRC1(BInured, "may not be set if '%s' is given", BInu);
    if (PisNULL(BInu)) 
      QERRC2(BInu, "either '%s' or '%s' must be set", BInu, BInured);
    if (PisNULL(BInudiag)) PALLOC(BInudiag, 2, 1);
    nudiag = P(BInudiag);
    nudiag[0] = nu[i11]; 
    nudiag[1] = nu[i22];
    if (PisNULL(BInured)) PALLOC(BInured, 1, 1);
    P(BInured)[0] =  nu[i21] / (0.5 * (nudiag[i11] + nudiag[1]));
  }

  if (!PisNULL(BInotinvnu) && !notinvnu[0]) 
    for (i=0; i<3; i++) nu[i] = 1.0 / nu[i];
 
  cov->full_derivs = cov->rese_derivs = 1; // kann auf 2 erhoeht werden, falls programmiert 
  for (i=0; i<3; i++) {
    int derivs = (int) (nu[i] - 1.0);
    if (cov->full_derivs < derivs) cov->full_derivs = derivs;
  }
  
  if (PisNULL(BIs)) {
    PALLOC(BIs, 3, 1);
    double *s = P(BIs);
    for (i=0; i<3; s[i++] = 1.0);
  }
  
  if  (S->cdiag_given) {
    if (PisNULL(BIrhored))
      QERRC2(BIrhored, "'%s' and '%s' must be set", BIcdiag, BIrhored);
    if (check && !PisNULL(BIc)) {
      if (cov->nrow[BIc] != 3 || cov->ncol[BIc] != 1)
	QERRC(BIc, "must be a 3 x 1 vector");
      if (FABS(c[i11] - cdiag[0]) > c[i11] * epsilon || 
	  FABS(c[i22] - cdiag[1]) > c[i22] * epsilon ) {
	QERRC1(BIc, "does not match '%s'.", BIcdiag);
      }
      double savec12 = c[i21];
      biWM2basic(cov, a, lg, aorig, nunew);
      if (FABS(c[i21] - savec12) > FABS(c[i21]) * epsilon)
 	QERRC1(BIc, "does not match '%s'.", BIrhored);
    } else {
      if (PisNULL(BIc)) PALLOC(BIc, 3, 1);
      c = P(BIc);
      biWM2basic(cov, a, lg, aorig, nunew);
   }
  } else {
    if (check && !PisNULL(BIrhored)) 
      QERRC1(BIrhored, "may not be set if '%s' is not given", BIcdiag);
    if (PisNULL(BIc)) QERRC2(BIc, "either '%s' or '%s' must be set",
			    BIc, BIcdiag);
    if (!ISNAN(c[i11]) && !ISNAN(c[i22]) && (c[i11] < 0.0 || c[i22] < 0.0))
      QERRC2(BIc, "variance parameter %s[0], %s[2] must be non-negative.",
	     BIc, BIc);
    if (PisNULL(BIcdiag)) PALLOC(BIcdiag, 2, 1);
    if (PisNULL(BIrhored)) PALLOC(BIrhored, 1, 1);
    cdiag = P(BIcdiag);
    cdiag[0] = c[i11];
    cdiag[1] = c[i22];
    double savec1 = c[i21];
    if (savec1 == 0.0)  P(BIrhored)[0] = 0.0; // wichtig falls c[0] oder c[2]=NA
    else {
      P(BIrhored)[0] = 1.0;
      biWM2basic(cov, a, lg, aorig, nunew);
      P(BIrhored)[0] = savec1 / c[i21];
    }
  }
  biWM2basic(cov, S->a, S->lg, S->aorig, S->nunew);

  cov->initialised = true;
  return NOERROR;
}


void kappa_biWM(int i, cov_model *cov, int *nr, int *nc){
  *nc = *nr = i < CovList[cov->nr].kappas ? 1 : -1;
  if (i==BInudiag || i==BIcdiag) *nr = 2; else
    if (i==BInu || i==BIs || i==BIc) *nr = 3;
}

void biWM2(double *x, cov_model *cov, double *v) {
  int i;
  double 
    *c = P(BIc),
    *nu = P(BInu),
    xx = *x;
  biwm_storage *S = cov->Sbiwm;

  assert(S != NULL);

  assert(cov->initialised);

  for (i=0; i<3; i++) {
    v[i] = c[i] * RU_WM(FABS(S->a[i] * xx), S->nunew[i], 0.0);
    if (!PisNULL(BInotinvnu) && nu[i] > MATERN_NU_THRES) {
      double w, y;
      y = FABS(S->aorig[i] * xx * INVSQRTTWO);
      Gauss(&y, cov, &w);
      *v = *v * MATERN_NU_THRES / nu[i] + 
	(1 - MATERN_NU_THRES / nu[i]) * w;
    }
  }
  v[3] = v[i22];
  v[2] = v[i21];

}

void biWM2D(double *x, cov_model *cov, double *v) {
  int i;
  double
    *c = P(BIc),
    *nu = P(BInu),
    xx = *x;
  biwm_storage *S = cov->Sbiwm;
  assert(S != NULL);
  assert(cov->initialised);

  for (i=0; i<3; i++) {
    v[i] = c[i] * S->a[i] * RU_DWM(FABS(S->a[i] * xx), S->nunew[i], 0.0);
    if (!PisNULL(BInotinvnu) && nu[i] > MATERN_NU_THRES) {
      double w, y,
	scale = S->aorig[i] * INVSQRTTWO;
      y = FABS(scale * xx);
      DGauss(&y, cov, &w);
      w *= scale;
      *v = *v * MATERN_NU_THRES / nu[i] + 
	(1 - MATERN_NU_THRES / nu[i]) * w;
    }
  }
  v[3] = v[i22];
  v[2] = v[i21];
}


void biWM2DD(double *x, cov_model *cov, double *v) {
  int i;
  double
    *c = P(BIc),
    *nu = P(BInu),
    xx = *x;
  biwm_storage *S = cov->Sbiwm;
  assert(S != NULL);
  assert(cov->initialised);

  for (i=0; i<3; i++) {
    v[i] = c[i] * S->a[i] *  S->a[i]  * RU_DDWM(FABS(S->a[i] * xx), S->nunew[i], 0.0);
    if (!PisNULL(BInotinvnu) && nu[i] > MATERN_NU_THRES) {
      double w, y,
    scale = S->aorig[i] * INVSQRTTWO;
      y = FABS(scale * xx);
      DDGauss(&y, cov, &w);
      w *= scale;
      *v = *v * MATERN_NU_THRES / nu[i] +
    (1 - MATERN_NU_THRES / nu[i]) * w;
    }
  }
  v[3] = v[i22];
  v[2] = v[i21];
}

void biWM2D3(double *x, cov_model *cov, double *v) {
  int i;
  double
    *c = P(BIc),
    *nu = P(BInu),
    xx = *x;
  biwm_storage *S = cov->Sbiwm;
  assert(S != NULL);
  assert(cov->initialised);

  for (i=0; i<3; i++) {
    v[i] = c[i] * S->a[i] *  S->a[i]  *  S->a[i]  *RU_D3WM(FABS(S->a[i] * xx), S->nunew[i], 0.0);
    if (!PisNULL(BInotinvnu) && nu[i] > MATERN_NU_THRES) {
      double w, y,
    scale = S->aorig[i] * INVSQRTTWO;
      y = FABS(scale * xx);
      D3Gauss(&y, cov, &w);
      w *= scale;
      *v = *v * MATERN_NU_THRES / nu[i] +
    (1 - MATERN_NU_THRES / nu[i]) * w;
    }
  }
  v[3] = v[i22];
  v[2] = v[i21];
}

void biWM2D4(double *x, cov_model *cov, double *v) {
  int i;
  double
    *c = P(BIc),
    *nu = P(BInu),
    xx = *x;
  biwm_storage *S = cov->Sbiwm;
  assert(S != NULL);
  assert(cov->initialised);

  for (i=0; i<3; i++) {
    v[i] = c[i] * S->a[i] *  S->a[i]  *  S->a[i]  * S->a[i]  *RU_D4WM(FABS(S->a[i] * xx), S->nunew[i], 0.0);
    if (!PisNULL(BInotinvnu) && nu[i] > MATERN_NU_THRES) {
      double w, y,
    scale = S->aorig[i] * INVSQRTTWO;
      y = FABS(scale * xx);
      D4Gauss(&y, cov, &w);
      w *= scale;
      *v = *v * MATERN_NU_THRES / nu[i] +
    (1 - MATERN_NU_THRES / nu[i]) * w;
    }
  }
  v[3] = v[i22];
  v[2] = v[i21];
}


int checkbiWM2(cov_model *cov) { 

  // !! nudiag, nured has priority over nu
  // !! cdiag, rhored has priority over c
  int err;
  gen_storage s;
  gen_NULL(&s);
  s.check = true;
 
  assert(PisNULL(BIrhored) || ISNAN(P0(BIrhored)) || P0(BIrhored) <= 1);
  if ((err = checkkappas(cov, false)) != NOERROR) return err;
  NEW_STORAGE(biwm);
  biwm_storage *S = cov->Sbiwm;
  S->nudiag_given = !PisNULL(BInudiag);
  S->cdiag_given = !PisNULL(BIcdiag);
  if ((err=initbiWM2(cov, &s)) != NOERROR) return err;

  cov->vdim[0] = cov->vdim[1] = 2;
 
  return NOERROR;
}

  
void rangebiWM2(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){

  // *nudiag = P0(BInudiag], 
  range->min[BInudiag] = 0.0;
  range->max[BInudiag] = RF_INF;
  range->pmin[BInudiag] = 1e-1;
  range->pmax[BInudiag] = 4.0; 
  range->openmin[BInudiag] = true;
  range->openmax[BInudiag] = true;
  
  // *nured12 = P0(BInured], 
  range->min[BInured] = 1;
  range->max[BInured] = RF_INF;
  range->pmin[BInured] = 1;
  range->pmax[BInured] = 5;
  range->openmin[BInured] = false;
  range->openmax[BInured] = true;
    
 // *nu = P0(BInu], 
  range->min[BInu] = 0.0;
  range->max[BInu] = RF_INF;
  range->pmin[BInu] = 1e-1;
  range->pmax[BInu] = 4.0;
  range->openmin[BInu] = true;
  range->openmax[BInu] = true;
 
 //   s = P0(BIs], 
  range->min[BIs] = 0.0;
  range->max[BIs] = RF_INF;
  range->pmin[BIs] = 1e-2;
  range->pmax[BIs] = 100.0;
  range->openmin[BIs] = true;
  range->openmax[BIs] = true;
  
  //    *cdiag = P0(BIcdiag]; 
  range->min[BIcdiag] = 0;
  range->max[BIcdiag] = RF_INF;
  range->pmin[BIcdiag] = 0.001;
  range->pmax[BIcdiag] = 1000;
  range->openmin[BIcdiag] = false;
  range->openmax[BIcdiag] = true;  
  
 //    *rho = P0(BIrhored]; 
  range->min[BIrhored] = -1;
  range->max[BIrhored] = 1;
  range->pmin[BIrhored] = -0.99;
  range->pmax[BIrhored] = 0.99;
  range->openmin[BIrhored] = false;
  range->openmax[BIrhored] = false;    

  //    *c = P0(BIc]; 
  range->min[BIc] = RF_NEGINF;
  range->max[BIc] = RF_INF;
  range->pmin[BIc] = 0.001;
  range->pmax[BIc] = 1000;
  range->openmin[BIc] = false;
  range->openmax[BIc] = true;  
  

//    *notinvnu = P0(BInotinvnu]; 
  range->min[BInotinvnu] = 0;
  range->max[BInotinvnu] = 1;
  range->pmin[BInotinvnu] = 0;
  range->pmax[BInotinvnu] = 1;
  range->openmin[BInotinvnu] = false;
  range->openmax[BInotinvnu] = false;    

}



void coinitbiWM2(cov_model *cov, localinfotype *li) {
    double
            thres = 1.5,
      //         *c = P(BIc),
            *nu = P(BInu);

    if ( ( nu[0] <= thres ) &&  ( nu[1] <= thres ) && ( nu[2] <= thres ) ) {
        li->instances = 1;
        li->value[0] = CUTOFF_THIRD_CONDITION; //  q[CUTOFF_A]
        li->msg[0] =MSGLOCAL_OK;
    }
    else {
        li->instances = 1;
        li->value[0] = CUTOFF_THIRD_CONDITION ; //  q[CUTOFF_A]
        li->msg[0] = MSGLOCAL_JUSTTRY;
    }
}



// PARS WM


/* Whittle-Matern or Whittle or Besset */ 
// only lower parts of the matrices, including the diagonals are used when estimating !!

void kappa_parsWM(int i, cov_model VARIABLE_IS_NOT_USED *cov, int *nr, int *nc){
  if (i==PARSnudiag) {
    *nr = 0; 
    *nc = 1;
  } else  *nc = *nr = -1;
}

void parsWMbasic(cov_model *cov) {
  // !! nudiag, nured has priority over nu
  // !! cdiag, rhored has priority over c

  double 
    dim = (double) cov->tsdim, 
    d2 = dim * 0.5, 
    *nudiag = P(PARSnudiag);
  int i, j, vdiag,
    vdim = cov->nrow[PARSnudiag], // hier noch nicht cov->vdim, falls parsWMbasic von check aufgerufen wird, und cov->vdim noch nicht gesetzt ist
    vdimP1 = vdim + 1;
 
  // **************** ACHTUNG !!!!! *********
  // nicht gut, sollte in check einmal berechnet werden 
  // dies wiederspricht aber der MLE Maximierung, da dann
  // neu berechnet werden muss; verlg. natsc und cutoff (wo es nicht
  // programmiert ist !!)

  for (vdiag=i=0; i<vdim; i++, vdiag+=vdimP1) {
    cov->q[vdiag] = 1.0;
    for (j=i+1; j<vdim; j++) {
      double half = 0.5 * (nudiag[i] + nudiag[j]);
      int idx = vdiag + j - i;
      cov->q[idx] = cov->q[vdiag + vdim * (j-i)] =
	EXP(0.5 * (lgammafn(nudiag[i] + d2) + lgammafn(nudiag[j] + d2)
		   - lgammafn(nudiag[i]) - lgammafn(nudiag[j])
		   + 2.0 * (lgammafn(half) - lgammafn(half + d2))
		   ));
    }
  }
}

void parsWM(double *x, cov_model *cov, double *v) {
  int i, j, vdiag,
    vdim = cov->vdim[0],
    vdimP1 = vdim + 1;
  double 
    *nudiag = P(PARSnudiag);

  parsWMbasic(cov);
  for (vdiag=i=0; i<vdim; i++, vdiag+=vdimP1) {
    for (j=i; j<vdim; j++) {
      double half = 0.5 * (nudiag[i] + nudiag[j]);      
      int idx = vdiag + j - i;
      v[idx] = v[vdiag + vdim * (j-i)] = RU_WM(*x, half, 0.0) * cov->q[idx];
    }
  }
}


void parsWMD(double *x, cov_model *cov, double *v) {
  int i, j, vdiag,
    vdim = cov->vdim[0],
    vdimP1 = vdim + 1;
  double 
    *nudiag = P(PARSnudiag);
  parsWMbasic(cov);
  for (vdiag=i=0; i<vdim; i++, vdiag+=vdimP1) {
    for (j=i; j<vdim; j++) {
      double half = 0.5 * (nudiag[i] + nudiag[j]);      
      int idx = vdiag + j - i;
      v[idx] = v[vdiag + vdim * (j-i)] = RU_DWM(*x, half, 0.0) * cov->q[idx];
    }
  }
}


int checkparsWM(cov_model *cov) { 
 
  double
    *nudiag = P(PARSnudiag);
  
  int i, err, 
    vdim = cov->nrow[PARSnudiag],
    vdimSq = vdim * vdim;
 
  cov->vdim[0] = cov->vdim[1] = vdim;
  if (vdim == 0) SERR1("'%s' not given", KNAME(PARSnudiag));
  if ((err = checkkappas(cov, true)) != NOERROR) return err;
  if (cov->q == NULL) QALLOC(vdimSq);
  
  cov->full_derivs = cov->rese_derivs = 1; 
  for (i=0; i<vdim; i++) {
    int derivs = (int) (nudiag[i] - 1.0);
    if (cov->full_derivs < derivs) cov->full_derivs = derivs;
  }

  /*
    #define dummyN (5 * ParsWMMaxVDim)
    double value[ParsWMMaxVDim], ivalue[ParsWMMaxVDim], 
    dummy[dummyN], 
    min = RF_INF;
    int j,
    ndummy = dummyN;
  */

  
  return NOERROR;
}

void rangeparsWM(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){

  // *nudiag = P0(PARSnudiag], 
  range->min[PARSnudiag] = 0.0;
  range->max[PARSnudiag] = RF_INF;
  range->pmin[PARSnudiag] = 1e-1;
  range->pmax[PARSnudiag] = 4.0; 
  range->openmin[PARSnudiag] = true;
  range->openmax[PARSnudiag] = true;
}





// ************************* NOT A COVARIANCE FCT

double SurfaceSphere(int d, double r) { 
    // d = Hausdorff-Dimension der Oberflaeche der Sphaere
   //  NOT   2 \frac{\pi^{d/2}}{\Gamma(d/2)} r^{d-1}, 
   //  BUT  2 \frac{\pi^{(d+1)/2}}{\Gamma((d+1)/2)} r^{d}, 
   double D = (double) d;
  // printf("r=%f, %f %f %f\n", r, D, POW(SQRTPI * r, D - 1.0), gammafn(0.5 * D));

   return 2.0 * SQRTPI * POW(SQRTPI * r, D) / gammafn(0.5 * (D + 1.0));

}

double VolumeBall(int d, double r) {
  //  V_n(R) = \frac{\pi^{d/2}}{\Gamma(\frac{d}{2} + 1)}R^n, 
 double D = (double) d;
 return POW(SQRTPI * r, D) / gammafn(0.5 * D + 1.0);  
}


void ball(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) { 
  // isotropic function, expecting only distance; BALL_RADIUS=1.0
  assert(*x >= 0);
  *v = (double) (*x <= BALL_RADIUS);
}

void Inverseball(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v){
  *v = *x > 1.0 ? 0.0 : BALL_RADIUS;
}


int struct_ball(cov_model *cov, cov_model **newmodel){
  ASSERT_NEWMODEL_NOT_NULL;
  if (hasMaxStableRole(cov)) {
    return addUnifModel(cov, BALL_RADIUS, newmodel);
  } else {
    ILLEGAL_ROLE;
  }

  return NOERROR;
}

int init_ball(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  assert(s != NULL);
  if (hasNoRole(cov)) return NOERROR;
  
  if (hasAnyShapeRole(cov)) {
    cov->mpp.maxheights[0] = 1.0;
    
    if (cov->mpp.moments >= 1) {
      cov->mpp.mM[1] = cov->mpp.mMplus[1] = VolumeBall(cov->tsdim, BALL_RADIUS);
      int i;
      for (i=2; i<=cov->mpp.moments; i++)  
	cov->mpp.mM[i] = cov->mpp.mMplus[i] = cov->mpp.mM[1];
    }
  }
  
  else ILLEGAL_ROLE;


  return NOERROR;
}


void do_ball(cov_model VARIABLE_IS_NOT_USED *cov, gen_storage VARIABLE_IS_NOT_USED *s) { 
  assert(s != NULL);
 
}



/// Poisson polygons


double meanVolPolygon(int dim, double beta) {
  double kd = VolumeBall(dim, 1.0),
    kdM1 = VolumeBall(dim-1, 1.0);
  return intpow(dim * kd / (kdM1 * beta), dim) / kd;
}

void Polygon(double *x, cov_model VARIABLE_IS_NOT_USED *cov, double *v) { 
  polygon_storage *ps = cov->Spolygon;
  assert(ps != NULL);
  *v = (double) isInside(ps->P, x);
}
 
void Inversepolygon(double VARIABLE_IS_NOT_USED *x, cov_model *cov, double *v){
  polygon_storage *ps = cov->Spolygon;
  int d,
    dim = cov->tsdim;

  if (ps == NULL) {
    *v = RF_NA;
    return;
  }
  polygon *P = ps->P;
  if (P != NULL) {
    double max = 0.0;
    for (d=0; d<dim; d++) {
      double y = FABS(P->box0[d]);
      if (y > max) max = y;
      y = FABS(P->box1[d]);
      if (y > max) max = y;
    }
  } else {
    BUG;

    *v = POW(meanVolPolygon(dim, P0(POLYGON_BETA)) / VolumeBall(dim, 1.0), 
	     1.0/dim);
    // to do kann man noch mit factoren multiplizieren, siehe
    // strokorb/schlather, max
    // um unabhaengigkeit von der Dimension zu erlangen
  }
}

void InversepolygonNonstat(double VARIABLE_IS_NOT_USED *v, cov_model *cov,
			   double *min, double *max){
  polygon_storage *ps = cov->Spolygon;
  int d,
    dim = cov->tsdim;
  assert(ps != NULL);
  if (ps == NULL) {
    for (d=0; d<dim; d++) min[d] = max[d] = RF_NA;
    return;
  }
  polygon *P = ps->P;
  if (P != NULL) {
     for (d=0; d<dim; d++) {
      min[d] = P->box0[d];
      max[d] = P->box1[d];   
    }
  } else { // gibt aquivalenzradius eines "mittleres" Polygon zurueck

    BUG;

    double size = POW(meanVolPolygon(dim, P0(POLYGON_BETA)) / 
		      VolumeBall(dim, 1.0), 1.0/dim);    
    // to do kann man noch mit factoren multiplizieren, siehe
    // strokorb/schlather, max-stabile Modelle mit gleichen tcf
    for (d=0; d<dim; d++) {
      min[d] = -size;
      max[d] = size;
    }
  }
}

int check_polygon(cov_model *cov) {
  int err;
  assert(cov->tsdim <= MAXDIM_POLY);
  if (cov->tsdim != 2)
    SERR("random polygons only defined for 2 dimensions"); // to do
  assert(cov->tsdim == cov->xdimown);
  kdefault(cov, POLYGON_BETA, 1.0);
  if ((err = checkkappas(cov)) != NOERROR) return err;
  cov->deterministic = FALSE;
  return NOERROR;
}

void range_polygon(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[POLYGON_BETA] = 0;
  range->max[POLYGON_BETA] = RF_INF;
  range->pmin[POLYGON_BETA] = 1e-3;
  range->pmax[POLYGON_BETA] = 1e3;
  range->openmin[POLYGON_BETA] = true;
  range->openmax[POLYGON_BETA] = true;
}


int struct_polygon(cov_model VARIABLE_IS_NOT_USED *cov,
		   cov_model VARIABLE_IS_NOT_USED **newmodel){
  /*
  ASSERT_NEWMODEL_NOT_NULL;
  double beta = P0(POLYGON_BETA);
  if (hasAnyShapeRole(cov)) {
    double 
      dim = cov->tsdim,
      safety = P0(POLYGON_SAFETY), // to do : zhou approach !
      equiv_cube_length = POW(beta, -1.0/dim);
    return addUnifModel(cov,  // to do : zhou approach !
			0.5 * equiv_cube_length * safety,
			newmodel);
  } else {
    ILLEGAL_ROLE;
  }
  */
  BUG;

  return NOERROR;
}


polygon_storage *create_polygon() {
  polygon_storage *ps;
  if ((ps = (polygon_storage*) MALLOC(sizeof(polygon_storage))) == NULL)
    return NULL;
  if ((ps->P = (polygon*)  MALLOC(sizeof(polygon))) == NULL) {
    FREE(ps);
    return NULL;
  }
  polygon_NULL(ps);
  return ps;
}


int init_polygon(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  int i, err,
    dim = cov->tsdim;
  double beta = P0(POLYGON_BETA),
    lambda = beta; // / VolumeBall(dim - 1, 1.0) * (dim * VolumeBall(dim, 1.0));
  assert(s != NULL);
  if (cov->Spolygon == NULL) {
    if ((cov->Spolygon = create_polygon()) ==NULL) return ERRORMEMORYALLOCATION;
  }
   
  // nicht nur aber auch zur Probe
  struct polygon_storage *ps = cov->Spolygon;
  assert(ps != NULL && ps->P != NULL);

  if (!false) {
    freePolygon(ps->P); 
    if ((err=rPoissonPolygon(ps->P, lambda, true)) != NOERROR)
      SERR1("poisson polygon cannot be simulated (error=%d)", err);
  } else {
    BUG; // valgrind memalloc loss ! + lange Laufzeiten ?!
    if ((err=rPoissonPolygon2(ps, lambda, true)) != NOERROR)
      SERR1("Poisson polygon cannot be simulated (error=%d)", err);
  }
 
  if (hasAnyShapeRole(cov)) {
    double c = meanVolPolygon(dim, beta);
    cov->mpp.maxheights[0] = 1.0; 
   for (i=1; i<=cov->mpp.moments; i++) cov->mpp.mM[i] = cov->mpp.mMplus[i] = c;	
  }  else ILLEGAL_ROLE;

  return NOERROR;
}


#define USER_TYPE 0
#define USER_DOM 1
#define USER_ISO 2
#define USER_VDIM 3
#define USER_BETA 4
#define USER_VARIAB 5
#define USER_FCTN 6
#define USER_FST 7
#define USER_SND 8
#define USER_ENV 9
#define USER_LAST USER_ENV
void evaluateUser(double *x, double *y, bool Time, cov_model *cov,
		  sexp_type *which, double *Res) {
  SEXP  res,
    env = PENV(USER_ENV)->sexp; 
  int i,
    vdim = cov->vdim[0] * cov->vdim[1],
    ncol = cov->ncol[USER_BETA],
    n = cov->xdimown;
  double *beta = P(USER_BETA);  
 
  assert(which != NULL);
  if (cov->nrow[USER_VARIAB] >= 2 && PINT(USER_VARIAB)[1] != -2) {
    if (Time) {
      addVariable( (char *) "T", x + (--n), 1, 1, env); 
    }
    switch (n) {
    case 3 : addVariable( (char *) "z", x+2, 1, 1, env); 		
    case 2 : addVariable( (char *) "y", x+1, 1, 1, env); 		
    case 1 : addVariable( (char *) "x", x+0, 1, 1, env); 		
      break;
    default:
      BUG;
    }
  } else {
    addVariable( (char *) "x", x, n, 1, env);	
    if (y != NULL) addVariable( (char *) "y", y, n, 1, env);
  }

  res = eval(which->sexp, env);
  if (beta == NULL) {
    for (i=0; i<vdim; i++) {
      Res[i] = REAL(res)[i]; 
    }
  } else {
    Ax(beta, REAL(res), vdim, ncol, Res);
  }
}


void kappaUser(int i, cov_model *cov, int *nr, int *nc){
  *nc = *nr = i < CovList[cov->nr].kappas ? 1 : -1;
  if (i == USER_VDIM) *nr = SIZE_NOT_DETERMINED;
  if (i == USER_VARIAB) *nr=SIZE_NOT_DETERMINED;
  if (i == USER_BETA) *nr=*nc=SIZE_NOT_DETERMINED;
}

void User(double *x, cov_model *cov, double *v){
  evaluateUser(x, NULL, Loc(cov)->Time, cov, PLANG(USER_FCTN), v);
}
void UserNonStat(double *x, double *y, cov_model *cov, double *v){
  evaluateUser(x, y, false, cov, PLANG(USER_FCTN), v);
}
void DUser(double *x, cov_model *cov, double *v){
  evaluateUser(x, NULL, Loc(cov)->Time, cov, PLANG(USER_FST), v);
}
void DDUser(double *x, cov_model *cov, double *v){
  evaluateUser(x, NULL, Loc(cov)->Time, cov, PLANG(USER_SND), v);
}


int checkUser(cov_model *cov){
  cov_fct *C = CovList + cov->nr;

  kdefault(cov, USER_DOM, XONLY);
  if (PisNULL(USER_ISO)) {
    Types type = (Types) P0INT(USER_TYPE);
    if (isVariogram(type)) kdefault(cov, USER_ISO, ISOTROPIC);
    else if (isProcess(type) || isShape(type)) 
      kdefault(cov, USER_ISO, CARTESIAN_COORD);
    else SERR1("type='%s' not allowed", TYPENAMES[type]);
  }
  int
    *dom = PINT(USER_DOM), 
    *iso = PINT(USER_ISO),
    *vdim =PINT(USER_VDIM);
  bool 
    Time,
    fctn = !PisNULL(USER_FCTN),
    fst = !PisNULL(USER_FST),
    snd = !PisNULL(USER_SND);
  int err, 
    *nrow = cov->nrow,
    *variab = PINT(USER_VARIAB), //codierung variab=1:x, 2:y 3:z, 4:T
    nvar = cov->nrow[USER_VARIAB],
    *pref = cov->pref;


  if (nvar < 1) SERR("variables not of the required form ('x', 'y', 'z', 'T')");
  
  if (cov->domown != *dom) 
    SERR2("wrong domain (requ='%s'; provided='%s')", 
	  DOMAIN_NAMES[cov->domown], DOMAIN_NAMES[*dom]);
  if (cov->isoown != *iso)
    SERR2("wrong isotropy assumption (requ='%s'; provided='%s')",
	  ISONAMES[cov->isoown], ISONAMES[*iso]);

  if (PENV(USER_ENV) == NULL) BUG;

  if (!PisNULL(USER_BETA)) {
    if (vdim == NULL) kdefault(cov, USER_VDIM, nrow[USER_BETA]);
    else if (*vdim != nrow[USER_BETA]) 
      SERR4("number of columns of '%s' (=%d) does not equal '%s' (=%d)",
	    KNAME(USER_BETA), nrow[USER_BETA], KNAME(USER_VDIM), *vdim);
    cov->vdim[0] = nrow[USER_BETA];
    cov->vdim[1] = 1;
  } else {
    if (PisNULL(USER_VDIM)) kdefault(cov, USER_VDIM, 1);  
    cov->vdim[0] = P0INT(USER_VDIM);
    cov->vdim[1] = cov->nrow[USER_VDIM] == 1 ? 1 : PINT(USER_VDIM)[1];
  }
  if (cov->nrow[USER_VDIM] > 2) SERR1("'%s' must be a scalar or a vector of length 2", KNAME(USER_VDIM));

  if ((err = checkkappas(cov, false)) != NOERROR) return err;

  if (variab[0] != 1 && (variab[0] != 4 || nvar > 1)) SERR("'x' not given");
  if (nvar > 1) {
    variab[1] = std::abs(variab[1]); // integer!
    //                              it is set to its negativ value
    //                              below, when a kernel is defined
    if (variab[1] == 3) SERR("'z' given but not 'y'"); 
  } 
  Time = variab[nvar-1] == 4;

  if (((nvar >= 3 || variab[nvar-1] == 4)
       && (!ISNA(GLOBAL.coords.xyz_notation) && 
	   !GLOBAL.coords.xyz_notation))
      //  ||
      //  (nrow[USER_VARIAB] == 1 && !ISNA_INT(GLOBAL.coords.xyz_notation) 
      //  && GLOBAL.coords.xyz_notation)
      )
    SERR("mismatch of indicated xyz-notation");

  if (Time xor Loc(cov)->Time)
    SERR("If 'T' is given, it must be given both as coordinates and as variable 'T' in the function 'fctn'");

  if ((nvar > 2 || (nvar == 2 && variab[1] != 2)) && cov->domown == KERNEL)
    SERR1("'%s' not valid for anisotropic models", coords[COORDS_XYZNOTATION]);
  
  if (nvar == 2 && variab[1] == 2) {
    // sowohl nonstat also auch x, y Schreibweise moeglich
    if (ISNA(GLOBAL.coords.xyz_notation))
      SERR1("'%s' equals 'NA', but should be a logical value.", 
	   coords[COORDS_XYZNOTATION]);     
    if (cov->domown == KERNEL && GLOBAL.coords.xyz_notation==2) // RFcov
      SERR1("'%s' is not valid for anisotropic models", 
	   coords[COORDS_XYZNOTATION]);
  }
 
  if (nvar > 1) {
    if (cov->domown == XONLY) {
      if (cov->isoown == ISOTROPIC) {
	SERR("two many variables given for motion invariant function");
      } else if (cov->isoown == SPACEISOTROPIC && nvar != 2)
	SERR("number of variables does not match a space-isotropic model");
    } else {
      if (nvar == 2 && variab[1] == 2) variab[1] = -2;//i.e. non domain (kernel)
    }
  }

  if (!ISNA(GLOBAL.coords.xyz_notation)) {    
    if ((GLOBAL.coords.xyz_notation == 2 && cov->domown == KERNEL)
	||
	((nvar > 2 || (nvar == 2 && cov->domown==XONLY)) && variab[1] == -2)) {
      SERR("domain assumption, model and coordinates do not match.");
    }
  }

  if ((cov->xdimown == 4 && !Loc(cov)->Time) || cov->xdimown > 4)
    SERR("Notation using 'x', 'y', 'z' and 'T' allows only for 3 spatial dimensions and an additional time component.");


  if (fctn) {
    C->F_derivs = C->RS_derivs = 0;
    pref[Direct] = pref[Sequential] = pref[CircEmbed] = pref[Nothing] = 5;
    if (fst) {
      C->F_derivs = C->RS_derivs = 1;
      pref[TBM] = 5;
      if (snd) {
	C->F_derivs = C->RS_derivs = 2;
      }
    } else { // !fst
      if (snd) SERR2("'%s' given but not '%s'",
		     KNAME(USER_SND), KNAME(USER_FST));
    }
  } else { // !fctn
    if (fst || snd) 
      SERR3("'%s' or '%s' given but not '%s'", 
	    KNAME(USER_SND), KNAME(USER_FST), KNAME(USER_FCTN));
  }
   return NOERROR;
}


bool TypeUser(Types required, cov_model *cov, int VARIABLE_IS_NOT_USED depth) {
  int *type = PINT(USER_TYPE);
  if (type == NULL) return false;
  if (!isShape((Types) type[0])) return false;
  return TypeConsistency(required, (Types) type[0]);
}

void rangeUser(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[USER_TYPE] = TcfType;
  range->max[USER_TYPE] = TrendType;
  range->pmin[USER_TYPE] = TcfType;
  range->pmax[USER_TYPE] = TrendType;
  range->openmin[USER_TYPE] = false;
  range->openmax[USER_TYPE] = false;

  range->min[USER_DOM] = XONLY;
  range->max[USER_DOM] = KERNEL;
  range->pmin[USER_DOM] = XONLY;
  range->pmax[USER_DOM] = KERNEL;
  range->openmin[USER_DOM] = false;
  range->openmax[USER_DOM] = false;

  range->min[USER_ISO] = ISOTROPIC;
  range->max[USER_ISO] = CARTESIAN_COORD;
  range->pmin[USER_ISO] = ISOTROPIC;
  range->pmax[USER_ISO] = CARTESIAN_COORD;
  range->openmin[USER_ISO] = false;
  range->openmax[USER_ISO] = false;

  range->min[USER_VDIM] = 1;
  range->max[USER_VDIM] = INFDIM;
  range->pmin[USER_VDIM] = 1;
  range->pmax[USER_VDIM] = 10;
  range->openmin[USER_VDIM] = false;
  range->openmax[USER_VDIM] = true;

  range->min[USER_BETA] = RF_NEGINF;
  range->max[USER_BETA] = RF_INF;
  range->pmin[USER_BETA] = -1e5;
  range->pmax[USER_BETA] = 1e5;
  range->openmin[USER_BETA] = true;
  range->openmax[USER_BETA] = true;

  range->min[USER_VARIAB] = -2;
  range->max[USER_VARIAB] = 4;
  range->pmin[USER_VARIAB] = 1;
  range->pmax[USER_VARIAB] = 4;
  range->openmin[USER_VARIAB] = false;
  range->openmax[USER_VARIAB] = false;
  
#define user_n 4
  int idx[user_n] = {USER_FCTN, USER_FST, USER_SND, USER_ENV};
  for (int i=0; i<user_n; i++) {
    int j = idx[i];
    range->min[j] = RF_NAN;
    range->max[j] = RF_NAN;
    range->pmin[j] = RF_NAN;
    range->pmax[j] = RF_NAN;
    range->openmin[j] = false;
    range->openmax[j] = false; 
  }

  int kappas = CovList[cov->nr].kappas;
  for (int i=USER_LAST + 1; i<kappas; i++) {
    range->min[i] = RF_NEGINF;
    range->max[i] = RF_INF;
    range->pmin[i] = 1e10;
    range->pmax[i] = -1e10;
    range->openmin[i] = true;
    range->openmax[i] = true; 
  }


  
}



// Sigma(x) = diag>0 + A'xx'A
void kappa_Angle(int i, cov_model *cov, int *nr, int *nc){
  int dim = cov->xdimown;
  *nr = i == ANGLE_DIAG ? dim : 1;
  *nc = i <= ANGLE_DIAG && (dim==3 || i != ANGLE_LATANGLE) &&  
    (dim==2 || i != ANGLE_RATIO)  ? 1 : -1;
}

void AngleMatrix(cov_model *cov, double *A) {
  double
    c = COS(P0(ANGLE_ANGLE)),
    s = SIN(P0(ANGLE_ANGLE)),
    *diag = P(ANGLE_DIAG);
  int 
    dim = cov->xdimown ;
  assert(dim ==2 || dim == 3);
  
  if (dim == 2) {
    A[0] = c;
    A[1] = s;
    A[2] = -s;
    A[3] = c;
  } else {
    double 
      pc = COS(P0(ANGLE_LATANGLE)),
      ps = SIN(P0(ANGLE_LATANGLE));
    /*
    c -s 0    pc 0 -ps   c*pc  -s  -c*ps
    s  c 0  *  0 1  0  = s*pc   c  -s*ps
    0  0 1    ps 0  pc     ps   0   pc 
    */
    A[0] = c * pc;
    A[1] = s * pc;
    A[2] = ps;
    A[3] = -s;
    A[4] = c;
    A[5] = 0.0;
    A[6] = -c * ps;
    A[7] = -s * ps;
    A[8] = pc;
  }

  if (diag!= NULL) {
    int k,d,j;
    for (k=d=0; d<dim; d++) {
      for (j=0; j<dim; j++) {
	A[k++] *= diag[j];
      }
    }
  } else {
    double ratio = P0(ANGLE_RATIO);
    A[1] /= ratio;
    A[3] /= ratio;
  }  
}



void Angle(double *x, cov_model *cov, double *v) { /// to do: extend to 3D!
  double A[9];
  int 
    dim = cov->xdimown ;
  
  //  print("%d\n", dim);//

  AngleMatrix(cov, A);
  Ax(A, x, dim, dim, v); 
}


void invAngle(double *x, cov_model *cov, double *v) { /// to do: extend to 3D!
  double A[9],
    c = COS(P0(ANGLE_ANGLE)),
    s = SIN(P0(ANGLE_ANGLE)),
    *diag = P(ANGLE_DIAG);
  int d, 
      dim = cov->xdimown ;
  
  bool inf = x[0] == RF_INF;
  for (d=1; d<dim; d++) inf &= x[d] == RF_INF;
  if (inf) {
    for (d=0; d<dim; v[d++] = RF_INF);
    return;
  }

  bool neginf = x[0] == RF_NEGINF;
  for (d=1; d<dim; d++) neginf &= x[d] == RF_NEGINF;
  if (neginf) {
    for (d=0; d<dim; v[d++] = RF_NEGINF);
    return;
  }
  
  if (dim == 2) {
    A[0] = c;
    A[1] = -s;
    A[2] = s;
    A[3] = c;
  } else {
    double pc = COS(P0(ANGLE_LATANGLE)),
      ps = SIN(P0(ANGLE_LATANGLE));

    A[0] = c * pc;
    A[1] = -s;
    A[2] = -c * ps;
    A[3] = s * pc;
    A[4] = c;
    A[5] = -s * ps;
    A[6] = ps;
    A[7] = 0.0;
    A[8] = pc;
  }

  if (diag!= NULL) {
    int  j, k;
    for (k=d=0; d<dim; d++) {
      for (j=0; j<dim; j++) {
	A[k++] /= diag[d];
      }
    }
  } else {
    double ratio = P0(ANGLE_RATIO);
    A[2] *= ratio;
    A[3] *= ratio;
  }

  Ax(A, x, dim, dim, v);
}

int checkAngle(cov_model *cov){
  int dim = cov->xdimown;

  if (dim != 2 && dim != 3)
    SERR1("'%s' only works for 2 and 3 dimensions", NICK(cov));

  if (PisNULL(ANGLE_DIAG)) {
    if (PisNULL(ANGLE_RATIO)) {
      SERR2("either '%s' or '%s' must be given",
	    KNAME(ANGLE_RATIO), KNAME(ANGLE_DIAG))
    } else if (dim != 2) { 
      SERR1("'%s' may be given only if dim=2",  KNAME(ANGLE_RATIO))
    }
  } else {
    if (!PisNULL(ANGLE_RATIO)) 
      SERR2("'%s' and '%s' may not given at the same time",
	    KNAME(ANGLE_RATIO), KNAME(ANGLE_DIAG));
  }
  cov->vdim[0] = dim;
  cov->vdim[1] = 1;
  cov->mpp.maxheights[0] = RF_NA;
  cov->matrix_indep_of_x = true;
  return NOERROR;
}
 
void rangeAngle(cov_model *cov, range_type* range){
  cov_model *prev = cov->calling;
  assert(prev != NULL);
  range->min[ANGLE_ANGLE] = 0.0;
  range->max[ANGLE_ANGLE] = 
    prev->vdim[0] == SCALAR && isSymmetric(prev->typus) &&
    isDollar(prev) ? PI : TWOPI;
  
  //printf("range = %f %d %d %d\n", range->max[ANGLE_ANGLE],
  //	 prev->vdim[0], isSymmetric(prev->typus),
  //	 isDollar(prev)); //assert(prev->vdim[0]<=0);
  
  range->pmin[ANGLE_ANGLE] = 0.0;
  range->pmax[ANGLE_ANGLE] = range->max[ANGLE_ANGLE];
  range->openmin[ANGLE_ANGLE] = false;
  range->openmax[ANGLE_ANGLE] = true;

  range->min[ANGLE_LATANGLE] = 0.0;
  range->max[ANGLE_LATANGLE] = PI;
  range->pmin[ANGLE_LATANGLE] = 0.0;
  range->pmax[ANGLE_LATANGLE] = PI;
  range->openmin[ANGLE_LATANGLE] = false;
  range->openmax[ANGLE_LATANGLE] = true;

  range->min[ANGLE_RATIO] = 0;
  range->max[ANGLE_RATIO] = RF_INF;
  range->pmin[ANGLE_RATIO] = 1e-5;
  range->pmax[ANGLE_RATIO] = 1e5;
  range->openmin[ANGLE_RATIO] = false;
  range->openmax[ANGLE_RATIO] = true;

  range->min[ANGLE_DIAG] = 0;
  range->max[ANGLE_DIAG] = RF_INF;
  range->pmin[ANGLE_DIAG] = 1e-5;
  range->pmax[ANGLE_DIAG] = 1e5;
  range->openmin[ANGLE_DIAG] = false;
  range->openmax[ANGLE_DIAG] = true;
}
 


void idcoord(double *x, cov_model *cov, double *v){
  int d,
    vdim = cov->vdim[0];
  for (d=0; d<vdim; d++) v[d]=x[d];
}
int checkidcoord(cov_model *cov){
  if (cov->isoprev != cov->isoown) SERR("unequal iso's");// return ERRORWRONGISO;
  cov->vdim[0] = cov->xdimown;
  cov->vdim[1] = 1;
  return NOERROR;
}


// obsolete??!!
#define NULL_TYPE 0
void NullModel(double VARIABLE_IS_NOT_USED *x, 
	       cov_model VARIABLE_IS_NOT_USED *cov, 
	       double VARIABLE_IS_NOT_USED *v){ return; }
void logNullModel(double VARIABLE_IS_NOT_USED *x, 
		  cov_model VARIABLE_IS_NOT_USED *cov, 
		  double VARIABLE_IS_NOT_USED *v, 
		  int VARIABLE_IS_NOT_USED *Sign){ return; }
bool TypeNullModel(Types required, cov_model *cov, int VARIABLE_IS_NOT_USED depth) {
  Types type = (Types) P0INT(NULL_TYPE);
  return TypeConsistency(required, type); 
}
void rangeNullModel(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[NULL_TYPE] = TcfType;
  range->max[NULL_TYPE] = OtherType;
  range->pmin[NULL_TYPE] = TcfType;
  range->pmax[NULL_TYPE] = OtherType;
  range->openmin[NULL_TYPE] = false;
  range->openmax[NULL_TYPE] = false;
}


int checkMath(cov_model *cov){
  int i,
    variables = CovList[cov->nr].kappas,
    err = NOERROR;
  if (variables > 2) kdefault(cov, variables-1, 1.0);
  if ((cov->typus == TrendType  || cov->typus == ShapeType) &&
      !isCoordinateSystem(cov->isoown)) {
    //printf("checmath: %s %d\n", NAME(cov), variables);
    //    PMI(cov->calling);
    //*** RFcov[phi,0].17-> RMmult[C0,0].18-> RMprod[phi,0].19-> RMplus[C1,1].20-> ******   RMmult, *   ****** [25,22]
    SERR("full coordinates needed");
  }

  //printf("checmath: %s %d\n", NAME(cov), variables);


  for (i=0; i<variables; i++) {
    cov_model *sub = cov->kappasub[i];
    //printf("   %d %d\n", i, sub == NULL);
    if (sub != NULL) {
      if (i >= 2) SERR("only numerics allowed");
      bool plus = CovList[sub->nr].cov == Mathplus ||
	CovList[sub->nr].check == checkplus ||
	CovList[sub->nr].cov == Mathminus
	;
       if ((err = CHECK(sub, cov->tsdim, cov->xdimown, 
			plus ? cov->typus : ShapeType, 
			// auch falls cov = TrendType ist
			cov->domown, cov->isoown,
			1, cov->role)) != NOERROR){
	return err;  
      }
      if  (sub->vdim[0] != 1 || sub->vdim[1] != 1)
	SERR("only scalar functions are allowed");
      setbackward(cov, sub); 
    } else if (PisNULL(i)) {
      if (i==0 || (CovList[cov->nr].cov!=Mathplus && 
		   CovList[cov->nr].cov!=Mathminus && 
		   CovList[cov->nr].cov!=Mathbind
		   )) SERR("not enough arguments given")
      else break;
    }
  }

 cov->pref[TrendEval] = 5;
  cov->pref[Direct] = 1;
  return NOERROR;
}


void rangeMath(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  int i,
    variables = CovList[cov->nr].kappas;
  
  cov->maxdim = cov->xdimown;
  assert(cov->maxdim > 0);
  for (i=0; i<variables; i++) {
    range->min[i] = RF_NEGINF;
    range->max[i] = RF_INF;
    range->pmin[i] = -1e5;
    range->pmax[i] = 1e5;
    range->openmin[i] = true;
    range->openmax[i] = true;
  }
}

void Mathminus(double *x, cov_model *cov, double *v){
  MATH_DEFAULT;
  double f = P0(MATH_FACTOR); 
  if (ISNA(f) || ISNAN(f)) f = 1.0;
  *v = ( (PisNULL(1) && cov->kappasub[1]==NULL) ? -w[0] : w[0 ]- w[1]) * f;
  //printf("minus (%f - %f) * %f  = %f %d\n", w[0], w[1], f, *v, (PisNULL(1) && cov->kappasub[1]==NULL));
}

void Mathplus(double *x, cov_model *cov, double *v){
  MATH_DEFAULT;
   double f = P0(MATH_FACTOR); 
   if (ISNA(f) || ISNAN(f)) f = 1.0;
   *v = ( (PisNULL(1) && cov->kappasub[1]==NULL)? w[0] : w[0] + w[1]) * f;
}

void Mathdiv(double *x, cov_model *cov, double *v){
  MATH_DEFAULT;
   double f = P0(MATH_FACTOR); 
   if (ISNA(f) || ISNAN(f)) f = 1.0;
*v =  (w[0] / w[1]) * f;

}

void Mathmult(double *x, cov_model *cov, double *v){
  MATH_DEFAULT;
   double f = P0(MATH_FACTOR); 
   if (ISNA(f) || ISNAN(f)) f = 1.0;
 *v = (w[0] * w[1]) * f;
}
 

#define IS_IS 1
void Mathis(double *x, cov_model *cov, double *v){
  int i,								
    variables = CovList[cov->nr].kappas; 		
  double w[3];							
  for (i=0; i<variables; i++) {					
    cov_model *sub = cov->kappasub[i];				
    if (sub != NULL) {						
      COV(x, sub, w + i);						
    } else {  
      w[i] = i == IS_IS ? P0INT(i) : P0(i);	      
    }					
  }									
  switch((int) w[IS_IS]) {
  case 0 : *v = (double) (FABS(w[0] - w[2]) <= GLOBAL.nugget.tol); break;
  case 1 : *v = (double) (FABS(w[0] - w[2]) > GLOBAL.nugget.tol); break;
  case 2 : *v = (double) (w[2] + GLOBAL.nugget.tol >= w[0]); break;
  case 3 : *v = (double) (w[2] + GLOBAL.nugget.tol > w[0]); break;
  case 4 : *v = (double) (w[0] + GLOBAL.nugget.tol >= w[2]); break;
  case 5 : *v = (double) (w[0] + GLOBAL.nugget.tol > w[2]); break;
  default : BUG;
  }
  //print("%f %f %f %d\n", w[IS_IS], w[0], w[2], (int) *v);
}

void rangeMathIs(cov_model *cov, range_type *range){
  rangeMath(cov, range); 
  
  range->min[IS_IS] = 0;
  range->max[IS_IS] = 5;
  range->pmin[IS_IS] = 0;
  range->pmax[IS_IS] = 5;
  range->openmin[IS_IS] = false;
  range->openmax[IS_IS] = false;
}

void Mathbind(double *x, cov_model *cov, double *v){
  int vdim = cov->vdim[0];
  MATH_DEFAULT_0(vdim); 

  double f = P0(CovList[cov->nr].kappas - 1); 
  if (ISNA(f) || ISNAN(f)) f = 1.0;
  for (i=0; i<vdim; i++) v[i] = w[i] * f; 
}

int check_bind(cov_model *cov) {  
  int err;
  int 
    variables = CovList[cov->nr].kappas - 1; // last is factor !!
  if ((err = checkMath(cov)) != NOERROR) return err;

  // int i;
  //  for (i =0; i<variables; i++)
    // printf("%d %d %d null: %d %d\n", i, cov->nrow[i], cov->ncol[i], PisNULL(i), 	  cov->kappasub[i] == NULL );


  while (variables > 0 && cov->nrow[variables - 1] == 0 &&
	 cov->kappasub[variables - 1] == NULL) variables--;
  cov->vdim[0] = variables;
  cov->vdim[1] = 1;
  cov->ptwise_definite = pt_mismatch;

  return NOERROR;
}



void Mathc(double VARIABLE_IS_NOT_USED *x, cov_model *cov, double *v) {
  double f = P0(CONST_C); 
  *v =  ISNA(f) || ISNAN(f) ?  1.0 : f;
}

int check_c(cov_model *cov){
  bool tcf = isTcf(cov->typus);

  if (tcf) {
    // the following defines a positive constant on the level of a trend 
    // to be a trend, and not a positive definite function
    // For spatially constant covariance functions, see RMconstant

    if (cov->calling == NULL) BUG;
    cov_model *pp = cov->calling->calling;

    if (pp == NULL ||
	( cov->calling->nr == PLUS &&
	  !isNegDef(pp->typus) && !isTrend(pp->typus)
	)) {
      // printf("%d %d %d %s\n", pp==NULL, !isNegDef(pp->typus), !isTrend(pp->typus), TYPENAMES[pp->typus]);
      // PMI(cov);
      return ERRORFAILED; // by definition,
    }
  }
  if (cov->kappasub[0] != NULL) SERR("only numerics allowed");
  cov->ptwise_definite =  
    P0(CONST_C) > 0 ? pt_posdef : P0(CONST_C) < 0 ? pt_negdef : pt_zero;
  if (tcf) 
    MEMCOPY(cov->pref, PREF_ALL, sizeof(pref_shorttype));
  return NOERROR;
}


void rangec(cov_model *cov, range_type *range){
  bool tcf = isTcf(cov->typus);
  range->min[CONST_C] = tcf ? 0 :  RF_NEGINF;
  range->max[CONST_C] = RF_INF;
  range->pmin[CONST_C] = tcf ? 0 : -1e5;
  range->pmax[CONST_C] = 1e5;
  range->openmin[CONST_C] = !tcf;
  range->openmax[CONST_C] = true;
}

 

void proj(double *x, cov_model *cov, double *v){
  double f = P0(PROJ_FACTOR); 
  if (ISNA(f) || ISNAN(f)) f = 1.0;
   
  *v = x[P0INT(PROJ_PROJ) - 1] * f;
}

int checkproj(cov_model *cov){
  int
    isoown = cov->isoown;
  kdefault(cov, PROJ_PROJ, 1);
  kdefault(cov, PROJ_FACTOR, 1.0);
  kdefault(cov, PROJ_ISO, CARTESIAN_COORD);
  int iso = P0INT(PROJ_ISO);

//if (warok) {pmi(cov->calling->calling->calling->calling->calling->calling->calling);BUG;}

  if (isoown != iso && (iso != UNREDUCED || !isCoordinateSystem(isoown))) {
    SERR2("Offered system ('%s') does not match the required one ('%s')",
	  ISONAMES[isoown], ISONAMES[iso]);
  }

//warok = true;

  return NOERROR;
}

void rangeproj(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[PROJ_PROJ] = 0;
  range->max[PROJ_PROJ] = cov->xdimown;
  range->pmin[PROJ_PROJ] = 1;
  range->pmax[PROJ_PROJ] = cov->xdimown;
  range->openmin[PROJ_PROJ] = false;
  range->openmax[PROJ_PROJ] = false;

  range->min[PROJ_FACTOR] = RF_NEGINF;
  range->max[PROJ_FACTOR] = RF_INF;
  range->pmin[PROJ_FACTOR] = -1e5;
  range->pmax[PROJ_FACTOR] = 1e5;
  range->openmin[PROJ_FACTOR] = true;
  range->openmax[PROJ_FACTOR] = true;

  range->min[PROJ_ISO] = ISOTROPIC;
  range->max[PROJ_ISO] = UNREDUCED;
  range->pmin[PROJ_ISO] = ISOTROPIC;
  range->pmax[PROJ_ISO] = UNREDUCED;
  range->openmin[PROJ_ISO] = false;
  range->openmax[PROJ_ISO] = false;
}

