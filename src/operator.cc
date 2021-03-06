/*
 Authors 
 Martin Schlather, schlather@math.uni-mannheim.de

 Definition of correlation functions and derivatives of hypermodels

 Copyright (C) 2005 -- 2017 Martin Schlather
               2015-2017 Olga Moreva (cutoff, modified)

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
#include <R_ext/Applic.h>
#include "RF.h"
#include "Operator.h"
#include "variogramAndCo.h"
#include "cubicsolver.h"



#define BINARY_P 0
#define BINARY_CORR 1
#define BINARY_CENTRED 2

/*
  Felix' integral 
  + trafo  v = (1-r) / (1+r)
  + taylor 
  + GR 2.211/2

  V ~ 25 : Grenze zur Asymptoic
  GR 8.357


fctn = function(rho, t) EXP(-t^2 / (1+rho)) / SQRT(1.0-rho^2)
C = function(r, t) (2*pi)^(-1) * integrate(fctn, 0, r, t=t)$value
fctn1 = function(rho, t) EXP(-t^2 * rho / 2) / SQRT(rho) / (1 + rho)
C1 = function(r, t)  EXP(-t^2 / 2) / (2*pi) * integrate(fctn1, (1-r)/(1+r), 1, t=t)$value
deltaC = function(r, t) C(r,t) - C1(r,t)
deltaC(0.5, 0.5)

for (r in seq(0, 1, len=10)) for (t in seq(0.1, 2, len=10)) print(deltaC(r,t))#  //
                      


for (r in seq(0.1, 1, len=10)) for (t in seq(0.1, 2, len=10)) {
  print(deltaC(r,t))#  //
 
  a = t^2 /2

  n = 10
  d = 0:n
  fak = c(1, cumprod(d[-1]))
  s = cumsum(a^d / fak)
  
  v = (1-r) / (1+r)
  (U = 2 * SQRT(v) * sum((-v)^d / (2 * d + 1) * s))
  v =  1
  (O = 2 * SQRT(v) * sum((-v)^d / (2 * d + 1) * s))
  (O - U) / (2 * pi)
  C(r, t)
  
  v = (1-r) / (1+r)
  (U = 2 * ATAN(SQRT(v)) + 2 * SQRT(v) * sum((-v)^d / (2 * d + 1) * (EXP(-a) * s - 1)) )
  v =  1
  (O = 2 * ATAN(SQRT(v)) + 2 * SQRT(v) * sum((-v)^d / (2 * d + 1) * (EXP(-a) * s - 1)) )
  (O - U) / (2 * pi) 
  C(r, t)
  print( (O - U) / (2 * pi)  - C1(r,t))#  //


model = RM e x p()
x = seq(0.1, 1, len=10)
t = 1.67
for (r in x) print(C1(r,t=t))#  //
RFcov(model, LOG(x))
RFcov(RMbernoulli(model, t=t), LOG(x))
for (r in x) print(C1(r,t=t))  #  //

 */


#define Binary_Eps 1e-13
void binary(double *x, cov_model *cov, double *v) {
  cov_model *next = cov->sub[0];
  double a, var, V, r, expMa, Vd,
    s, Oned, sum, sumOne, summand, summandOne, 
    d,
    factor,
    t = P0(BINARY_P),
    p = pnorm(t, 0, 1.0, true, false);
  
  COV(ZERO, next, &var);
  COV(x, next, &r);
  
  if (t == 0.0) {
    *v = ((M_1_PI * ASIN(r / var) + 0.5) - p) * p;
  } else {      
    a = 0.5 * t * t / var;
    expMa = EXP(-a);
    r /= var;
    
    // als BA-Arbeit
    if (r < -0.9)
      ERR("correlation of submodel must be >= -0.9 for numerical reasons");
    
    V = (1 - r) / (1 + r);
    
    double ad = expMa;
    d = 0.0;
    sum = sumOne = 0.0;
    Vd = 1;
    Oned = 1;
    s = ad;
    factor = (s - 1.0) / (2.0 * d + 1.0);
    summand = Vd * factor;
    summandOne = Oned * factor;
    while (FABS(summand) > Binary_Eps || FABS(summandOne) > Binary_Eps) {
      sum += summand;
      sumOne += summandOne;
      d += 1.0;
      ad *= a / d;
      s += ad;
      Vd *= -V;
      Oned *= -1;
      factor =  (s - 1.0) / (2.0 * d + 1.0);
      summand = Vd * factor;
      summandOne = Oned * factor;
    }
    sum += summand;
    sumOne += summandOne;

    // 0.25 = 2 ATAN(SQRT(1.0)) / 2pi
    *v = 0.25 + INVPI * (sumOne - (ATAN(SQRT(V)) + SQRT(V) * sum));
  }

  if (!P0INT(BINARY_CENTRED)) {
    *v += p * p;
  }

  if (P0INT(BINARY_CORR)) {
    *v /= p;
  }
}


int checkbinary(cov_model *cov) {
  // to do extend to multivariate

  WARN_NEWDEFINITIONS;

  cov_model
    *next = cov->sub[0];
  double v;
  int i,
    vdim = cov->vdim[0],
    err = NOERROR;
  if (cov->vdim[0] != cov->vdim[1]) BUG;
  kdefault(cov, BINARY_P, 0.0);
  //  if (P0(BINARY_P) != 0.0) SERR("currently only threshold=0 is possible");//todo
  kdefault(cov, BINARY_CORR, 1);
  kdefault(cov, BINARY_CENTRED, 1);
  if ((err = CHECK(next, cov->tsdim,  cov->xdimprev, PosDefType,
		     cov->domown, cov->isoown,
		     SUBMODEL_DEP, cov->role)) != NOERROR) return err;
  setbackward(cov, next);
  for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = 1.0;
  COV(ZERO, next, &v);
  return NOERROR;
}


void rangebinary(cov_model  VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[BINARY_P] = RF_NEGINF; 
  range->max[BINARY_P] = RF_INF;
  range->pmin[BINARY_P] = -4.0;
  range->pmax[BINARY_P] = 4.0;
  range->openmin[BINARY_P] = false;
  range->openmax[BINARY_P] = false;

  range->min[BINARY_CORR] = 0; 
  range->max[BINARY_CORR] = 1;
  range->pmin[BINARY_CORR] = 0;
  range->pmax[BINARY_CORR] = 1;
  range->openmin[BINARY_CORR] = false;
  range->openmax[BINARY_CORR] = false;

  range->min[BINARY_CENTRED] = 0; 
  range->max[BINARY_CENTRED] = 1;
  range->pmin[BINARY_CENTRED] = 0;
  range->pmax[BINARY_CENTRED] = 1;
  range->openmin[BINARY_CENTRED] = false;
  range->openmax[BINARY_CENTRED] = false;
}



//////////////////////////////////////////////////////////////////////
// extremal gaussian 

void extrgauss(double *x, cov_model *cov, double *v) {
  // BrownResnick to binary Gaussian
  cov_model *next = cov->sub[0];
  double var, z;
  
  COV(ZERO, next, &var);
  COV(x, next, &z);
  *v = 1.0 - SQRT(0.5 * (1.0 - z / var));
}


int check_extrgauss(cov_model *cov) {
  // to do extend to multivariate
  cov_model
    *next = cov->sub[0];
  double v;
  int i,
    vdim = cov->vdim[0],
    err = NOERROR;
  if (cov->vdim[0] != cov->vdim[1]) BUG;
  if ((err = CHECK(next, cov->tsdim,  cov->xdimprev, PosDefType,
		     cov->domown, cov->isoown,
		     SUBMODEL_DEP, cov->role)) != NOERROR) return err;
  setbackward(cov, next);
  for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = 1.0;
  COV(ZERO, next, &v);
  if (v != 1.0) SERR("only correlation functions allowed");
  return NOERROR;
}


//////////////////////////////////////////////////////////////////////
// Brown Resnick

#define BR_FACTOR 0.25  //  KSH: '/ 4' 
#define BR_SEMI_FACTOR (2 * BR_FACTOR) // hier Semi-Variogram
void brownresnick(double *x, cov_model *cov, double *v) {
  // BrownResnick to binary Gaussian
  cov_model *next = cov->sub[0];
  double z;  

  assert(cov->role == ROLE_COV);

  COV(ZERO, next, &z);
  COV(x, next, v);
  *v = 2.0 * pnorm(SQRT((z - *v) * BR_SEMI_FACTOR), 0.0, 1.0, false, false);
}

void Dbrownresnick(double *x, cov_model *cov, double *v) {
  // b = BR_SEMI_FACTOR
  // gamma(h) = C(0) - C(h) 
  // varphi(SQRT(gamma * b)) * (b C') / SQRT(gamma * b)
  // varphi density standard normal

  // BrownResnick to binary Gaussian
  cov_model *next = cov->sub[0];
  double z, abl, s;
  
  //  

  if ((cov->role != ROLE_COV && cov->role != ROLE_MAXSTABLE) || 
      cov->taylorN <= 1) BUG;

  if (cov->taylor[1][TaylorPow] == 0.0) { // const??
    *v = 0.0;
    return;
  }
  
  if (*x != 0.0) {
    COV(ZERO, next, &z);
    COV(x, next, v);
    
     assert(CovList[next->gatternr].D != NULL);
    assert(CovList[next->gatternr].D != ErrCov);

    Abl1(x, next, &abl);   // Vorsicht: abl = -\gamma' 


    abl *= BR_SEMI_FACTOR;
    s = SQRT((z - *v) * BR_SEMI_FACTOR); // SQRT(c * gamma)
    *v = dnorm(s, 0.0, 1.0, false) * abl / s; // -\varphi * \gamma' / sqrt\gamma
    //                                      =-2 \varphi * (-C') / (2 sqrt\gamma)
    //                              mit C= c_0 -\gamma
 
    assert(*v <= 0);

  } else {
    if (cov->taylor[1][TaylorPow] < 1.0) {     
      *v = RF_NEGINF;
    } else if (cov->taylor[1][TaylorPow] == 1.0) {
      *v = FABS(cov->taylor[1][TaylorConst]);
      assert(*v > 0.0);
    } else BUG;
  }
  // 2 * c * varphi(s) * gamma' / s
}


void DDbrownresnick(double *x, cov_model *cov, double *v) {
  // D = \varphi * b C' / SQRT(\gamma b)

  // b = BR_SEMI_FACTOR
  // gamma(h) = C(0) - C(h) 
  // varphi(SQRT(gamma * b)) [(b C')^2 / [2 sqrt (gamma * b)]
  //                          +(b C'') / sqrt (gamma * b)
  //                          +1/2 * (b C')^2 / sqrt (gamma * b)^3 ]
 // varphi density standard normal

  cov_model *next = cov->sub[0];
  double z, abl, abl2, s0, s;
  
  if (cov->role != ROLE_COV && cov->role != ROLE_MAXSTABLE) BUG;
  if (cov->taylor[1][TaylorPow] == 0.0) {
    *v = 0.0;
    return;
  }
   
  if (*x != 0.0) {
    COV(ZERO, next, &z);
    COV(x, next, v);
    Abl1(x, next, &abl);
    Abl2(x, next, &abl2);
    s0 = (z - *v) * BR_SEMI_FACTOR;
    s = SQRT(s0); // SQRT(c * gamma)
    abl  *= BR_SEMI_FACTOR;
    abl2 *= BR_SEMI_FACTOR;
    *v = dnorm(s, 0.0, 1.0, false) / s * (abl2 + abl * abl * 0.5 * (1/s0 + 1));

    assert(*v >= 0);

  } else {
    *v = cov->taylor[1][TaylorPow]==1 ? 0.0 : RF_INF;
  }
}


void D3brownresnick(double *x, cov_model *cov, double *v) {
  // D = \varphi * b C' / SQRT(\gamma b)

  // b = BR_SEMI_FACTOR
  // gamma(h) = C(0) - C(h) 
  // varphi(SQRT(gamma * b)) [(b C')^3 / [4 sqrt (gamma * b)]
  //                         +3 (b C') (b C'')/ [2 sqrt (gamma * b)]
  //                         + (b C''') / [sqrt (gamma * b)]
  //                         + (b C')^2 / [2 sqrt (gamma * b)]
  //                         +3(b C') (b C'') / [2 sqrt (gamma * b)^3]
  //                         +3(b C')^3 / [4 sqrt (gamma * b)^5]

  cov_model *next = cov->sub[0];
  double z, abl, abl2, abl3, s0, s;
  
  if (cov->role != ROLE_COV && cov->role != ROLE_MAXSTABLE) BUG;
  if (cov->taylor[1][TaylorPow] == 0.0) {
    *v = 0.0;
    return;
  }
   
  if (*x != 0.0) {
    COV(ZERO, next, &z);
    COV(x, next, v);
    Abl1(x, next, &abl);
    Abl2(x, next, &abl2);
    Abl3(x, next, &abl3);
    s0 = (z - *v) * BR_SEMI_FACTOR;
    s = SQRT(s0); // SQRT(c * gamma)
    abl  *= BR_SEMI_FACTOR;
    abl2 *= BR_SEMI_FACTOR;
    abl3 *= BR_SEMI_FACTOR;
    *v = dnorm(s, 0.0, 1.0, false) / s * 
      (abl3 + 
       1.5 * abl2 * abl * (1/s0 + 1) +
       abl * abl * abl * (0.25 + 0.5 / s0 + 0.75 / (s0 * s0)));

    // printf("br x=%f v=%e s=%f abl=%f s0=%f abl2=%f\n", *x, *v, s, abl, s0, abl2);
    //  assert(*v >= 0);

  } else {
    *v = cov->taylor[1][TaylorPow]==1 ? 0.0 : RF_NEGINF; // da zweite Ableitung im Ursprung Pol +\infty hat.
  }
}

int TaylorBrownresnick(cov_model *cov) {
  cov_model  
   *next = cov->sub[0];
 int idx = isPosDef(next->typus); // Taylorentw. in 2 Glieder falls pos def.
 assert(idx == 0);

  //  assert(!idx);

  if (next->taylor[idx][TaylorPow] >= 2.0) {//for pos/neg def only == 2 possible
    cov->full_derivs = 1;
  } else cov->full_derivs = 0;
  cov->rese_derivs = next->rese_derivs;
  if (cov->rese_derivs > 3) cov->rese_derivs = 3;
 
   // else if (next->taylor[idx][TaylorPow] == 2) {
  //  if (next->taylorN < 2 + idx) cov->rese_derivs = 0;
    // else if (cov->rese_derivs > 2) cov->rese_derivs = 2; 
  //  }
   
  if (next->taylorN >= 1 + idx &&  next->taylor[idx][TaylorConst] < 0.0) {
    // 2 \Phi(SQRT(gamma * br_f)) =
    //1+ 2 * phi(0) * SQRT(gamma * br_f) - 2 phi(0) / 6 *SQRT(gamma*br_f)^3
    //sei gamma(x) = c x^alpha + d x^beta, beta > alpha. Dann gilt
    // also  2 \Phi(SQRT(gamma * br_f)) ~
    // 1 + 2 * phi(0) * SQRT((c x^alpha + d x^beta)* br_f)
    //   - 2 * phi(0) / 6 * SQRT((c x^alpha + d x^beta) *br_f)^3
    // = 1 + 2 * phi(0)[ (c f x^a)^{1/2} (1 + 0.5 d / c * x^{b-a})
    //                   (c f x^a)^{3/2} (1 + 1.5 d / c * x^{b-a}) ]
    // ~ 1 + 2 * phi(0) (c f x^a)^{1/2}
    //     + phi(0)(c f)^{1/2} d / c * x^{b - a/2})            (*)
    //     + 2 * phi(0) (c f x^a)^{3/2}                        (**)
    // 
    //
    // da a <= 2 und b > a gilt:
    // 1. Abltg ~ phi(0) (c f)^{1/2} x^{a/2 - 1}
    //           + phi(0) (c f)^{1/2} d / c * (b - a/2) x^{b - a/2 - 1} 
    //           + 3 phi(0 (c f)^{3/2} a x^{3/2 * a - 1} 
    // 2. Abltg ~ phi(0) (c f)^{1/2} (a/2 - 1) x^{a/2 - 2} ~ infty,   a != 2
    // und fuer a=2 gilt folgendes: da gamma neg def ist e^{-\gamma} pos def
    // und mit sasvari folgt aus der Taylor entwicklung dass (a=2) < b <= 4).
    // Andererseits dominiert x{3/2 a} den Term x^{b - a/2} falls 
    //  3/2 a < b - a/2  <=> b > 2a = 4. 
    // Also ist b=4 ein Grenzfall, wo beide (*) und (**) eine Rolle spielen.
    // Ansonsten nur (*).
    //
 
    assert(next->taylor[idx][TaylorConst] <= 0.0);
     
    cov->taylorN = 2;
    cov->taylor[0][TaylorConst] = 1.0;
    cov->taylor[0][TaylorPow] = 0.0;    
    double
      next_taylor_const =  - next->taylor[idx][TaylorConst],
      g = SQRT(next_taylor_const  * BR_SEMI_FACTOR * 0.5 / M_PI);
    cov->taylor[1][TaylorConst] = - 2 * g;
    cov->taylor[1][TaylorPow] = 0.5 * next->taylor[idx][TaylorPow];
    if (next->taylor[idx][TaylorPow] == 2) {
      if (next->taylorN >= 2 + idx) {
	cov->taylorN = 3;
	if (next->taylor[idx + 1][TaylorConst] != 0) {
	  cov->taylor[2][TaylorConst] = 
	    g * next->taylor[idx + 1][TaylorConst] / next_taylor_const;
	  cov->taylor[2][TaylorPow] = 
	    next->taylor[idx + 1][TaylorPow] - 0.5*next->taylor[idx][TaylorPow];
	} else {	// Spezialfall:L fractionelle BR
	  cov->taylor[2][TaylorConst] = 0.0;
	  cov->taylor[2][TaylorPow] = 4.0;
	}
	if (next->taylor[idx + 1][TaylorPow] == 4) {
	  cov->taylor[1][TaylorConst] += 2 * g * next_taylor_const * BR_SEMI_FACTOR;
	}
      } else cov->taylorN = 0;
    }
  } else cov->taylorN = 0;

  
  if (next->tailN >= 1) {
    cov->tailN = 1;    
    cov->tail[0][TaylorPow] = -0.5 * next->tail[0][TaylorPow];
    if (next->tail[0][TaylorPow] > 0) {  
      assert( next->tail[0][TaylorConst] < 0.0);
      double next_tail_const = - next->tail[0][TaylorConst];
      cov->tail[0][TaylorConst] = 
	2.0 / SQRT(2.0 * M_PI * BR_SEMI_FACTOR * next_tail_const);
      cov->tail[0][TaylorExpConst] = 0.5 * BR_SEMI_FACTOR * next_tail_const;
      cov->tail[0][TaylorExpPow] = next->tail[0][TaylorPow];
    } else {
      cov->tail[0][TaylorConst] = 
	2.0 / SQRT(2.0 * M_PI * BR_SEMI_FACTOR * next->tail[0][TaylorConst])
	* EXP(-0.5 * BR_SEMI_FACTOR * next->tail[0][TaylorConst]);
      cov->tail[0][TaylorPow] = cov->tail[0][TaylorExpConst] = 
	cov->tail[0][TaylorExpPow] = 0.0;
    }     
  } else cov->tailN = 0;

  if (cov->taylorN < 1) cov->rese_derivs = 0;

  return NOERROR;
}

int checkbrownresnick(cov_model *cov) {
  // to do extend to multivariate
  cov_model  
   *next = cov->sub[0];
  int i, err, 
    vdim = cov->vdim[0],
    dim = cov->tsdim;
  if (cov->vdim[0] != cov->vdim[1]) BUG;

  if ((err = CHECK(next, dim,  dim, VariogramType, cov->domown, 
		   cov->isoown, SUBMODEL_DEP, 
		   hasMaxStableRole(cov) ? ROLE_MAXSTABLE : ROLE_COV))
      != NOERROR) {
    return err;
  } 
  setbackward(cov, next);
  cov->monotone = isBernstein(next) ? GNEITING_MON : 
    isMonotone(next) ? MONOTONE : NOT_MONOTONE;

  if ((err = TaylorBrownresnick(cov)) != NOERROR) return err;



  for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = 1.0;
  MEMCOPY(cov->pref, CovList[cov->nr].pref, sizeof(pref_shorttype)); 

 
  return NOERROR;  
}



int struct_brownresnick(cov_model *cov, cov_model VARIABLE_IS_NOT_USED **newmodel) {  
  cov_model *next = cov->sub[0];

  if (cov->role == ROLE_SMITH) {
    if (1 > next->taylorN  || 1 > next->tailN) 
      SERR2("role '%s' not possible for submodel '%s'", 
	    ROLENAMES[cov->role], NICK(next));

    BUG;

    // shape ist somit die Ableitung, falls d=1 und i.w. die 
    // zweifache Ableitung, falls d=3

    // hier ist auch Taylor zu setztn fuer den neuen Shape

  } else ILLEGAL_ROLE;


  return NOERROR;
}

int init_brownresnick(cov_model VARIABLE_IS_NOT_USED *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  int err;
  // cov_model *next = cov->sub[0];
  if ((err = TaylorBrownresnick(cov)) != NOERROR) return err;
  return NOERROR;
}

void do_brownresnick(cov_model *cov, gen_storage *s) {
  cov_model *next = cov->sub[0];
  DO(next, s); // nicht gatternr
}


void BR2EG(double *x, cov_model *cov, double *v) {
  // BrownResnick to binary Gaussian
  cov_model *next = cov->sub[0];
  double z;
  COV(ZERO, next, &z);
  COV(x, next, v);
  z = 2.0 * pnorm(SQRT( (z - *v) * BR_SEMI_FACTOR), 0.0, 1.0, true, false) -1.0;
  *v = 1.0 - 2.0 * z * z;
}

int check_BR2EG(cov_model *cov) {
  // to do extend to multivariate
  cov_model  
   *next = cov->sub[0];
  double v, t, alpha;
  int err, i,
    vdim = cov->vdim[0];
   if (cov->vdim[0] != cov->vdim[1]) BUG;
  
  if ((err = CHECK(next, cov->tsdim, cov->xdimown, PosDefType, 
		     cov->domown, cov->isoown, 
		     SCALAR, cov->role)) != NOERROR)  return err;
  setbackward(cov, next);
  for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = 1.0;
  if (next->pref[Nothing] == PREF_NONE) return ERRORPREFNONECOV;

  // Erfc(x) = 2(1 - Phi(x * SQRT(2)))
  // Erf(x) = 2 Phi(x * SQRT(2)) - 1
  // Erf^{-1}(x) = Phi^{-1}( (y + 1) / 2 ) / SQRT(2) 

  // Sei c = 1-2 * Erf(SQRT(semivario / 4))^2 .
  // Also c = 1 - 2 [ 2 Phi(SQRT(semivario / 2)) - 1]^2
  // Umgekehrt semivario = 4 * { Erf^{-1} (sqrt[0.5 (1 - c)]) } ^2
  // mit c = 0 folgt SQRT(0.5 (1-c)) = 1 / SQRT(2)
  // semivario = 2 * {Phi^{-1}( (1 / SQRT(2) + 1) / 2 ) } ^2

  alpha = 0.0;
  COV(ZERO, next, &v);
  t = qnorm(0.5 * (1.0 + INVSQRTTWO), 0.0, 1.0, true, false);
  t *=  t / (BR_SEMI_FACTOR * (1.0 - alpha)); // 1/2 wegen Erf->qnorm
  if (v > t)
    SERR2("variance equals %f, but must be at most 4(Erf^{-1}(1/2))^2 = %f",
	  v, t);
  return NOERROR;  
}



void BR2BG(double *x, cov_model *cov, double *v) {
  // BrownResnick to binary Gaussian
  cov_model *next = cov->sub[0];
  double z;
  COV(ZERO, next, &z);
  COV(x, next, v); 
  z = 2.0 * pnorm(SQRT( (z - *v) * BR_SEMI_FACTOR), 0.0, 1.0, true, false) -1;
  *v = COS(M_PI * z);
}

int check_BR2BG(cov_model *cov) {
  // to do extend to multivariate
  cov_model *next = cov->sub[0];
  double v, t, alpha;
  int err, i,
    vdim = cov->vdim[0];
  if (cov->vdim[0] != cov->vdim[1]) BUG;
  if ((err = CHECK(next, cov->tsdim, cov->xdimown, PosDefType,
		     cov->domown, cov->isoown, 
		     SCALAR, cov->role)) != NOERROR)  return err;
  setbackward(cov, next);
   for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = 1.0;
  if (next->pref[Nothing] == PREF_NONE) return ERRORPREFNONECOV;

  // Erfc(x) = 2(1 - Phi(x * SQRT(2)))
  // Erf(x) = 2 Phi(x * SQRT(2)) - 1
  // Erf^{-1}(x) = Phi^{-1}( (y + 1) / 2 ) / SQRT(2) 


  // Sei c = COS(pi * Erf(SQRT(semivario / 4))) .
  // Also c = COS(pi * [2 * Phi(SQRT(semivario / 2)) - 1] )
  // Umgekehrt semivario = 2 * { Phi^{-1}(0.5 * [arcCOS( c ) / pi + 1]) }^2
  // mit c = 0 folgt arcCOS(c)/ pi + 1 = 3/2
  // semivario = 2 * { Phi^{-1}( 3 / 4) }^2
  
  COV(ZERO, next, &v);
  alpha = 0.0; // to do
  t = qnorm(0.75, 0.0, 1.0, false, false);
  t *= t / (BR_SEMI_FACTOR * (1.0 - alpha)); // 1/2 wegen Erf->qnorm
 
  if (v > t) { 
     SERR2("variance equals %f, but must be at most 4(Erf^{-1}(1 / 2))^2 = %f", 
	 v, t);
    }
  return NOERROR;   
}

// #define UNIF_LOGREFAREA dim
#define SIGN_P 0
void randomSign(double *x, cov_model *cov, double *v) { 
  cov_model *next = cov->sub[0];
  assert(next != NULL);
  COV(x, next, v);
  (*v) *= cov->q[0];
  // printf("%f ", cov->q[0]);
}

void lograndomSign(double *x, cov_model *cov, double *v, double *Sign) { 
  cov_model *next = cov->sub[0];
  assert(next != NULL);
  LOGCOV(x, next, v, Sign);
  (*Sign) *= cov->q[0];
}

void randomSignInverse(double *v, cov_model *cov, double *x){
  cov_model *next = cov->sub[0];
  INVERSE(v, next, x);
}
void randomSignNonstatInverse(double *v, cov_model *cov, double *x, double *y){
  cov_model *next = cov->sub[0];
  NONSTATINVERSE(v, next, x, y);
}

int check_randomSign(cov_model *cov) {
  cov_model *next = cov->sub[0];
  int err, 
    size = 1;
  if (cov->q == NULL) QALLOC(size);
  
  kdefault(cov, 0, 0.5);
  if ((err = checkkappas(cov)) != NOERROR) return err;
  if ((err = CHECK(next, cov->tsdim, cov->xdimown, ShapeType,
		     cov->domown, cov->isoown, 
		     SCALAR, cov->role)) != NOERROR)  return err;
  setbackward(cov, next);
  return NOERROR;
}

void range_randomSign(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[SIGN_P] = 0; 
  range->max[SIGN_P] = 1;
  range->pmin[SIGN_P] = 0.0001;
  range->pmax[SIGN_P] = 0.9999;
  range->openmin[SIGN_P] = false;
  range->openmax[SIGN_P] = false;
}

int init_randomSign(cov_model *cov, gen_storage *s) {
  cov_model *next = cov->sub[0];
  int err;
  double Eminus;
  assert(next != NULL);
  if ((err=INIT(next, cov->mpp.moments, s))!= NOERROR) return err;
  if (next->fieldreturn && next->loggiven) 
    SERR("log return is incompatible with random Sign");
  
  if (cov->mpp.moments >= 1) {
    cov->mpp.mM[0] = next->mpp.mM[0];
    cov->mpp.mMplus[0] = next->mpp.mMplus[0];
    Eminus = cov->mpp.mMplus[1] - cov->mpp.mM[1];
    cov->mpp.mMplus[1] = 
      P(SIGN_P)[0] * (cov->mpp.mMplus[1] - Eminus) + Eminus; 
    cov->mpp.mM[1] = 0.0;
  }
  cov->mpp.maxheights[0] = next->mpp.maxheights[0];
  cov->fieldreturn = next->fieldreturn;
  cov->origrf = false;
  cov->rf = next->rf;  

  // assert(cov->mpp.maxheights[0] == 1.00);

  return err;
}

void do_randomSign(cov_model *cov, gen_storage *s) {
  cov_model *next = cov->sub[0];
  assert(next != NULL);
  DO(next, s); // nicht gatternr
  cov->q[0] = 2.0 * (UNIFORM_RANDOM <= P0(SIGN_P)) - 1.0;
  if (cov->q[0] != 1.0 && next->fieldreturn) { 
    assert(cov->q[0] == -1.0);
    if (next->loggiven) ERR("log return is incompatible with random Sign");
    int i,
      endfor = Loc(next)->totalpoints;
    double *rf = cov->rf;
    for (i=0; i<endfor; i++) rf[i] = -rf[i];
  }
}


int struct_randomSign(cov_model *cov, cov_model **newmodel) {  
  if (cov->role == ROLE_GAUSS || hasPoissonRole(cov)) {
    int err = STRUCT(cov->sub[0], newmodel);
    //  assert(cov->sub[0]->mpp.maxheights[0] == 1.0);
    return err;
  }
  SERR1("'%s' not allowed in this context.", NICK(cov));
}



#define MASTEIN_NU 0
#define MASTEIN_DELTA 1
void MaStein(double *x, cov_model *cov, double *v) {
  cov_model *next = cov->sub[0];
  double  nuG, gammas, v1, v2,
    nu = P0(MASTEIN_NU),
    delta = P0(MASTEIN_DELTA);

  COV(ZERO, next, &v1);
  COV(x + 1, next, &v2);
  nuG = nu + v1 - v2;
  if (nuG >= 80.0) {
    ERR("Whittle Matern function cannot be evaluated with parameter value b+g(t) greater than 80.");
  }
  gammas = lgammafn(nu + delta) - lgammafn(nu) -lgammafn(nuG + delta);
  double s = *x;
  *v = (s==0.0) ? EXP(lgammafn(nuG) + gammas)
    : 2.0 * EXP(nuG * LOG(0.5 * s) + gammas + LOG(bessel_k(s, nuG, 2.0)) - s);
}

int check_MaStein(cov_model *cov) {
  cov_model *next = cov->sub[0];
  int err;

  if (cov->xdimown != 2) SERR("reduced dimension must be 2");

  if ((err = checkkappas(cov)) != NOERROR) return err;
  if ((err = CHECK(next, 1, 1, VariogramType, XONLY,
		     SYMMETRIC, SCALAR, ROLE_COV)) != NOERROR) return err;
  if (cov->ncol[MASTEIN_NU] != 1 || cov->nrow[MASTEIN_NU] != 1) 
    SERR("nu not scalar");
  if (cov->ncol[MASTEIN_DELTA] != 1 || cov->nrow[MASTEIN_DELTA] != 1) 
    SERR("d not scalar");
  // no setbackward
  cov->maxdim = next->maxdim;
  return NOERROR;
  // no setbackward !
}

void range_MaStein(cov_model *cov, range_type *range){
  range->min[MASTEIN_NU] = 0.0; // 
  range->max[MASTEIN_NU] = RF_INF;
  range->pmin[MASTEIN_NU] = 1e-2;
  range->pmax[MASTEIN_NU] = 10.0;
  range->openmin[MASTEIN_NU] = true;
  range->openmax[MASTEIN_NU] = true;

  range->min[MASTEIN_DELTA] = 0.5 * (double) (cov->tsdim - 1); // d
  range->max[MASTEIN_DELTA] = RF_INF;
  range->pmin[MASTEIN_DELTA] = range->min[MASTEIN_DELTA];
  range->pmax[MASTEIN_DELTA] = 10;
  range->openmin[MASTEIN_DELTA] = false;
  range->openmax[MASTEIN_DELTA] = true;
}



/* bivariate shift model */
#define SHIFT_DELAY 0
void kappashift(int i, cov_model *cov, int *nr, int *nc){
  *nc = 0;
  *nr = i < CovList[cov->nr].kappas  ? cov->tsdim : -1;
}
void shift(double *x, cov_model *cov, double *v) {
  cov_model *next = cov->sub[0];
  double y[ShiftMaxDim],
    z[ShiftMaxDim] = { RF_NAN },
    *jh, *ih, 
    *pv = v,
    *h = P(SHIFT_DELAY);
  int i, j, d,
    tsdim = cov->tsdim,
    vdim = cov->vdim[0],
    vdimM1 = vdim - 1,
    vdimSq = vdim * vdim,
    vdimP1 = vdim + 1;
  
  COV(x, next, v);
  for (i=vdimP1; i<vdimSq; i+=vdimP1) v[i] = v[0];
  
  //  for (d=0; d<tsdim; d++) z[d] = 0.0;
  for (jh=h-tsdim, j=-1; j<vdimM1; j++, jh+=tsdim) {
    for (d=0; d<tsdim; d++) z[d] = x[d];
    if (j >= 0) for (d=0; d<tsdim; d++) z[d] += jh[d];
    for (ih=h-tsdim, i=-1; i<vdimM1; i++, ih+=tsdim, pv++) {
      if (i==j) continue;
      if (i < 0) for (d=0; d<tsdim; d++) y[d] = z[d];
      else for (d=0; d<tsdim; d++) y[d] = z[d] - ih[d];  
      COV(y, next, pv);
    }
  }
}




int checkshift(cov_model *cov) {
  cov_model *next = cov->sub[0];
  int err,
    dim = cov->tsdim;
      
  if (cov->xdimown > ShiftMaxDim)
    SERR2("For technical reasons max. dimension for ave is %d. Got %d.", 
	  StpMaxDim, cov->xdimown);

  if ((err = checkkappas(cov)) != NOERROR) return err;
  if ((err = CHECK(next, dim, dim, PosDefType, XONLY,
		     (dim > 1) ? SYMMETRIC : ISOTROPIC, 
		     SCALAR, ROLE_COV)) != NOERROR) 
    return err;
  setbackward(cov, next);
  cov->vdim[0] = cov->vdim[1] = cov->ncol[SHIFT_DELAY] + 1;
  return NOERROR;
}

void rangeshift(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[SHIFT_DELAY] = RF_NEGINF;
  range->max[SHIFT_DELAY] = RF_INF;
  range->pmin[SHIFT_DELAY] = -1000;
  range->pmax[SHIFT_DELAY] = 1000;
  range->openmin[SHIFT_DELAY] = true;
  range->openmax[SHIFT_DELAY] = true;
}



/* Vector - Delta Delta^T */
#define VECTOR_A 0
#define VECTOR_D 1
void vector(double *x, cov_model *cov, double *v) {
  /*
    r = \| x\|
    d/d x_i f(r) = f'(r) x_i / r
    d^2/d x_i^2 f(r) = f''(r) x_i^2/r^2 + f'(r) (r^2 - x_i^2) / r^3
    d^2/d x_i x_j f(r) = f''(r) x_i x_j/r^2 + f'(r) (- x_i x_j) / r^3

    r = 0:
    d/d x_i f(r) |_0 = 0
    d^2/d x_i^2 f(r) |_0 = f''(0)
    d^2/d x_i x_j f(r) = 0

    Vector (n = dimension of spatial field)
    r=0
    L = n * f''(0)

    operator:  -0.5 * (a + 1) laplace + a * hessian,  a in [-1,1]; a=1
  */
  cov_model *next = cov->sub[0];
  double norm[2], normSq0, normL2, normT2, D, D2, diag,
    a = P0(VECTOR_A),
    b = - 0.5 * (1.0 + a);
  int i,
    td = cov->tsdim,
    dim = P0INT(VECTOR_D),
    dimP1 = dim + 1,
    dimsq = dim * dim;
  
  normL2 = normT2 = 0.0;
  for (i=0; i<dim; i++) normL2 += x[i] * x[i];
  for (; i<td; i++) normT2 += x[i] * x[i];
  if (next->isoown == ISOTROPIC) {
    normSq0 = normL2 + normT2;
  } else {
    normSq0 = normL2;
    norm[1] = SQRT(normT2); 
  }
  norm[0] = SQRT(normSq0);
  Abl1(norm, next, &D);
  Abl2(norm, next, &D2);

  if (normSq0 == 0.0) {
    diag = (b * dim + a) * D2;
    for (i=0; i<dimsq; i++) {
      //    print("%f D2 %f\n", diag, D2);
      v[i] = (i % dimP1 == 0) ? diag : 0.0; 
    }
  } else {
    double 
      D2nsq = D2 / normSq0,
      D1n = D / norm[0],
      D1n3 = D / (norm[0] * normSq0),
      delta = a * (D2nsq - D1n3),
      Vector = b * ((D2nsq - D1n3) * normL2 + dim * D1n);
    int k,l;
    
    diag = Vector + a * D1n;    
    for (i=k=0; k<dim; k++) {
      for (l=0; l<dim; l++, i++) {
	v[i] = (i % dimP1 == 0) ? diag : 0.0; 
	v[i] += delta * x[k] * x[l];
      }
    }
  }
}


/* Vector - Delta Delta^T */
void vectorAniso(double *x, cov_model *cov, double *v) {
  /*
    operator:  -0.5 * (a + 1) laplace + a * hessian,  a in [-1,1]; a=1
  */

  cov_model *next = cov->sub[0];
  double laplace,
    a = P0(VECTOR_A),
    b = - 0.5 * (1.0 + a);
  int i, j, k, endfor,
    dim = P0INT(VECTOR_D),    
    dimP1 = dim + 1,
    dimsq = dim * dim,
    tsxdim = cov->tsdim,
    xdimsq = tsxdim * tsxdim,
    xdimP1 = tsxdim + 1,
    dimxdim = dim * tsxdim;

  ALLOC_EXTRA(D, xdimsq);
 
  // should pass back the hessian
  HESSE(x, next, D);
  laplace = 0.0;


  //  print("return matrix %d\n", xdimsq);
  //  for (i=0; i<xdimsq; i+=tsxdim) {
  //      for (j=i; j <i+tsxdim; j++) {
  //	  print("%f;%d  ", D[j], j);
  //      }
  //      print("\n");
  //  }


  for (i=0; i<dimxdim; i+=xdimP1) {
    laplace += D[i];
    //      print("lplace %d %f\n", i, D[i]);
  }
  laplace *= b;

  k = 0;
  for (i=0; i<dimxdim; i+=tsxdim) {  
    endfor = i + dim; 
    for (j=i; j<endfor; j++) {
      //	  print(">> %d %d %f %f %f\n", k, j, D[j], a, laplace );
      v[k++] = D[j] * a;
    }
  }
  
  for (i=0; i<dimsq; i+=dimP1) {
    v[i] += laplace;
  }

}

int checkvector(cov_model *cov) {
  cov_model *next = cov->sub[0];
  int err, i,
    dim = cov->tsdim;
  isotropy_type isotropy = cov->domown;

  kdefault(cov, VECTOR_A, 0.5); // a
  kdefault(cov, VECTOR_D, 
	   (isotropy==SPACEISOTROPIC  || isotropy==ZEROSPACEISO) 
	   ? dim - 1 : dim); // Dspace
  if ((err = checkkappas(cov)) != NOERROR) return err;

  if ( (isotropy==SPACEISOTROPIC || isotropy==ZEROSPACEISO) 
       && P0INT(VECTOR_D) != dim - 1) {
    SERR1("for spatiotemporal submodels '%s' must be applied to spatial part",
	 NICK(cov));
  }
  
  if (cov->tsdim != cov->xdimown || cov->tsdim != cov->xdimprev)
    return ERRORDIM;

  cov->nr = VECTOR; // wegen nr++ unten;
  if ((err = CHECK(next, dim, 1, PosDefType, cov->domown, ISOTROPIC,
		     SCALAR, ROLE_COV)) != NOERROR) {
    //     print("err=%d\n", err);
    if ((err = CHECK(next, dim, dim, PosDefType, cov->domown, ZEROSPACEISO,
		       SCALAR, ROLE_COV)) != NOERROR) {
      //	print("err2=%d\n", err);
      if ((err = CHECK(next, dim, dim, PosDefType, cov->domown, SYMMETRIC,
			 SCALAR, ROLE_COV)) != NOERROR) {
	//	 print("err3=%d\n", err);
	return err;
      }
    }
  }

  setbackward(cov, next);
  int diffpref = MIN(2, PREF_BEST - cov->pref[CircEmbed]);
  if (diffpref > 0) cov->pref[CircEmbed] += diffpref;

  for (i=0; i<dim; i++) cov->mpp.maxheights[i] = RF_NA;

  if (next->full_derivs < 2 && !next->hess) {
    //    print("%s (%d): full_derivs %d  %d\n", NICK(to), 
    //	  next->nr, next->full_derivs, CovList[next->nr].F_derivs);
    //  PMI(next);
    
       SERR("2nd derivative of submodel not defined (for the given paramters)");
  }
  isotropy = next->isoown;

  if (isotropy != ISOTROPIC && isotropy != SPACEISOTROPIC) {
      if (!next->hess) SERR("hess matrix not defined");
      cov->nr++;
  }

  cov->vdim[0] = cov->vdim[1] = P0INT(VECTOR_D);
  next->delflag = DEL_COV;

  EXTRA_STORAGE;

  return NOERROR;
}

void rangevector(cov_model *cov, range_type *range){
  range->min[VECTOR_A] = -1.0;
  range->max[VECTOR_A] = 1.0;
  range->pmin[VECTOR_A] = -1.0;
  range->pmax[VECTOR_A] = 1.0;
  range->openmin[VECTOR_A] = false;
  range->openmax[VECTOR_A] = false;

  range->min[VECTOR_D] = 1.0;
  range->max[VECTOR_D] = cov->tsdim;
  range->pmin[VECTOR_D] = 1.0;
  range->pmax[VECTOR_D] = cov->tsdim;
  range->openmin[VECTOR_D] = false;
  range->openmax[VECTOR_D] = false;
}



int zzz=0;
void kappadivcurl(int i, cov_model VARIABLE_IS_NOT_USED *cov,
		  int *nr, int *nc){
  *nc = 1;
  *nr = i == 0  ? SIZE_NOT_DETERMINED : -1;
} 


/* Rot - Delta Delta^T */
void curl(double *x, cov_model *cov, double *v) {
  /*
    r = \| x\|
    d/d x_i f(r) = f'(r) x_i / r
    d^2/d x_i^2 f(r) = f''(r) x_i^2/r^2 + f'(r) (r^2 - x_i^2) / r^3
    d^2/d x_i x_j f(r) = f''(r) x_i x_j/r^2 + f'(r) (- x_i x_j) / r^3

    r = 0:
    d/d x_i f(r) |_0 = 0
    d^2/d x_i^2 f(r) |_0 = f''(0)
    d^2/d x_i x_j f(r) = 0

    Rot (n = dimension of spatial field)
    r=0
    L = n * f''(0)

    operator:  -0.5 * (a + 1) rot + a * hessian,  a in [-1,1]; a=1/2
  */
 
  cov_model *next = cov->sub[0];
  cov_fct *N = CovList + next->nr; // !!! nicht gatternr !!!!
  double norm[2], normSq0, normL2, normT2, D, D2, D3, diag,
      a = -1.0, // curl free
      b = - 0.5 * (1.0 + a);
  int i,
      td = cov->tsdim,
      dim = td,
      // currently no space-time allowed -> BA thesis
      dimP1 = dim + 1,
      dimP2 = dim + 2,
      dimP3 = dim + 3,
      dimP2sqM1 =  dimP2 * dimP2 -1; // for three dimensions much
  // more complicated
 
  normL2 = normT2 = 0.0;
  for (i=0; i<dim; i++) normL2 += x[i] * x[i];
  for (; i<td; i++) normT2 += x[i] * x[i];
  if (next->isoown == ISOTROPIC) {
    normSq0 = normL2 + normT2;
  } else {
    normSq0 = normL2;
    norm[1] = SQRT(normT2); 
  }
  norm[0] = SQRT(normSq0);
  N->D(norm, next, &D);
  N->D2(norm, next, &D2);

  //  printf("next %s\n", N->name);

  N->D3(norm, next, &D3);


  if (normSq0 == 0.0) {
    for (i=0; i<=dimP2sqM1; i++) v[i] = 0.0;

    N->cov(norm, next, v); // v[0]
    
    diag = (b * dim + a) * D2;
    for (i=dimP3; i<dimP2sqM1; i+=dimP3) v[i] = diag; // diag 2,3

    N->D2(norm, next, v + dimP1);
    v[dimP1] *= 2.0;
    v[dimP2 * dimP1] = v[dimP1];
    N->D4(norm, next, v + dimP2sqM1);
    v[dimP2sqM1] *= (8.0 / 3.0); 
  } else {
    double 
      zz[2],
      D2nsq = D2 / normSq0,
      D1n = D / norm[0],
      D3n = D3 / norm[0],
      D1n3 = D / (norm[0] * normSq0),
      delta = a * (D2nsq - D1n3),
      div = b * ((D2nsq - D1n3) * normL2 + dim * D1n);
    int k,l;
    
    N->cov(norm, next, v); // v[0]
    
    zz[0] = x[0];
    zz[1] = x[1];
    for (i=1; i<=dim; i++) { // kante links und oben
	v[i] = -(v[i * dimP2] = zz[i-1] * D1n);
    }
 
    diag = div + a * D1n;    
    for (i= dimP3, k=0; k<dim; k++, i+=2) {
      for (l=0; l<dim; l++, i++) {
	v[i] = (i % dimP3 == 0) ? diag : 0.0; 
	v[i] += delta * x[k] * x[l];
      }
    }

    v[dimP1 * dimP2] = v[dimP1] = - v[dimP3] - v[2 * dimP3]; // ecken

    for (i=1; i<=dim; i++) { // kanten unten und hinten
	v[dimP2 * dimP1 + i] =
	    - (v[dimP2 * (i+1) - 1] = zz[i-1] * (D3n + D2nsq - D1n3));
    }

    N->D4(norm, next, v + dimP2sqM1); // rechts unten 
    v[dimP2sqM1] += 2.0 * D3n - D2nsq + D1n3;
 
  } 

// for (i=0; i<=15; i++) {
//      if (i!=15) v[i] = 0.0;
//  }

}

#define DIVCURL_WHICH 0
/* Div - Delta Delta^T */
void diverge(double *x, cov_model *cov, double *w) { // div -free !!
  /*
    r = \| x\|
    d/d x_i f(r) = f'(r) x_i / r
    d^2/d x_i^2 f(r) = f''(r) x_i^2/r^2 + f'(r) (r^2 - x_i^2) / r^3
    d^2/d x_i x_j f(r) = f''(r) x_i x_j/r^2 + f'(r) (- x_i x_j) / r^3

    r = 0:
    d/d x_i f(r) |_0 = 0
    d^2/d x_i^2 f(r) |_0 = f''(0)
    d^2/d x_i x_j f(r) = 0

    Div (n = dimension of spatial field)
    r=0
    L = n * f''(0)

    operator:  -0.5 * (a + 1) div + a * hessian,  a in [-1,1]; a=1
  */
  cov_model *next = cov->sub[0];
  
  cov_fct *N = CovList + next->nr; // gatter oder keine gatter??
  double
    norm[2], normSq0, normL2, normT2, D, D2, D3, diag,
      a = 1.0, // divergenz free
      b = - 0.5 * (1.0 + a);
  int i,
      td = cov->tsdim,
      dim = td, 
      // currently no space-time allowed -> BA thesis
      dimP1 = dim + 1,
      dimP2 = dim + 2,
      dimP3 = dim + 3,
      dimP2sqM1 =  dimP2 * dimP2 -1; // for three dimensions much
  // more complicated
  double *v = PisNULL(DIVCURL_WHICH) ? w : cov->Sextra->a;
  
  normL2 = normT2 = 0.0;
  for (i=0; i<dim; i++) normL2 += x[i] * x[i];
  for (; i<td; i++) normT2 += x[i] * x[i];
  if (next->isoown == ISOTROPIC) {
    normSq0 = normL2 + normT2;
  } else {
    normSq0 = normL2;
    norm[1] = SQRT(normT2); 
  }
  norm[0] = SQRT(normSq0);
  N->D(norm, next, &D);
  N->D2(norm, next, &D2);
  N->D3(norm, next, &D3);


  if (normSq0 == 0.0) {
    for (i=0; i<=dimP2sqM1; i++) v[i] = 0.0;

    N->cov(norm, next, v); // v[0]
    
    diag = (b * dim + a) * D2;
    for (i=dimP3; i<dimP2sqM1; i+=dimP3) v[i] = diag;  // diag 2,3

    N->D2(norm, next, v + dimP1);
    v[dimP1] *= 2.0;
    v[dimP2 * dimP1] = v[dimP1];
    N->D4(norm, next, v + dimP2sqM1);

    //  printf("%s %f\n", N->name,v[dimP2sqM1]); 
    
    v[dimP2sqM1] *= (8.0 / 3.0);    
  } else {
    double 
      zz[2],
      D2nsq = D2 / normSq0,
      D1n = D / norm[0],
      D3n = D3 / norm[0],
      D1n3 = D / (norm[0] * normSq0),
      delta = a * (D2nsq - D1n3),
      div = b * ((D2nsq - D1n3) * normL2 + dim * D1n);
    int k,l;
    
    N->cov(norm, next, v); // v[0]
    
    zz[0] = -x[1];
    zz[1] = x[0];
    for (i=1; i<=dim; i++) { // kante links und oben
	v[i] = -(v[i * dimP2] = zz[i-1] * D1n);
    }
     
    diag = div + a * D1n;    
    for (i= dimP3, k=0; k<dim; k++, i+=2) {
      for (l=0; l<dim; l++, i++) {
	v[i] = (i % dimP3 == 0) ? diag : 0.0; 
	v[i] += delta * x[k] * x[l];
      }
    }

    v[dimP1 * dimP2] = v[dimP1] = - v[dimP3] - v[2 * dimP3]; // ecken

    for (i=1; i<=dim; i++) { // kanten unten und hinten
	v[dimP2 * dimP1 + i] =
	    -(v[dimP2 * (i+1) - 1] =  zz[i-1] * (D3n + D2nsq - D1n3));
    }

    N->D4(norm, next, v + dimP2sqM1); // rechts unten 
    v[dimP2sqM1] += 2.0 * D3n - D2nsq + D1n3;
  }

  if (!PisNULL(DIVCURL_WHICH)) {
    int len = NROW(DIVCURL_WHICH),
      size = (int) cov->q[0];
    for (i=0; i<len; i++) {
      int ii = PINT(DIVCURL_WHICH)[i] - 1;
      for (int j=0; j<len; j++) {
	w[i + j * len] = v[ii + (PINT(DIVCURL_WHICH)[j] - 1) * size];
      }
    }
  }
 
  /*
  printf("\n\n%f %f \n", x[0], x[1]);//
  for (i=0; i<=15; i++) {
    printf("%f ", v[i]);//
  }
  */


//  for (i=0; i<=15; i++) {
//      if (i!=15) v[i] = 0.0;
//  }

}

int checkdivcurl(cov_model *cov) {
  cov_model  *next = cov->sub[0];
  // been programmed yet 
  int err, i,
      dim = cov->tsdim,
      spacedim;
  isotropy_type isotropy;

  // statt STATIONARY : VARIOGRAM ?? s. Paper mit Scheuerer

  if ((err = CHECK(next, dim, 1, PosDefType, cov->domown, ISOTROPIC,
		     SCALAR, ROLE_COV)) != NOERROR) {
    if ((err = CHECK(next, dim, 1, PosDefType, cov->domown, SPACEISOTROPIC,
		       SCALAR, ROLE_COV)) != NOERROR)
    return err;
  }

  //  PMI(next);
  
  if (next->full_derivs < 4) SERR("4th derivative of submodel not defined");
  if (cov->tsdim != 2) SERR("currently coded only for dim=2");
  isotropy = next->isoown;
  spacedim = dim - (isotropy == SPACEISOTROPIC);

  if (isotropy != ISOTROPIC && isotropy != SPACEISOTROPIC)
      SERR("submodel must be spaceisotropic");
  if (spacedim != 2)
       SERR("model currently only defined for the plane");
  
  setbackward(cov, next);
  int diffpref = MIN(2, PREF_BEST - cov->pref[CircEmbed]);
  if (diffpref > 0) cov->pref[CircEmbed] += diffpref;

  int components =  spacedim + 2; // only correct for spacedim == 2!!!
  int nwhich = NROW(DIVCURL_WHICH);
  if (nwhich > 0) {
    for (i=0; i<nwhich; i++) {
      if (PINT(DIVCURL_WHICH)[i] <= 0 || PINT(DIVCURL_WHICH)[i] > components) 
	SERR4("value %s[%d]=%d outside range 1,...,%d.",
	      KNAME(i), i, PINT(DIVCURL_WHICH)[i], components);
    }
    EXTRA_STORAGE;
    ALLOC_EXTRA(a, components * components);
  } else {
    nwhich = components;
  }
  for (i=0; i<dim; i++) cov->mpp.maxheights[i] = RF_NA;
  cov->vdim[0] = cov->vdim[1] = nwhich;
  assert(spacedim == 2);
  next->delflag = DEL_COV;
  if (cov->q == NULL) {
    QALLOC(1);
    cov->q[0] = components;
  }
  
  return NOERROR;
}


void rangedivcurl(cov_model *cov, range_type *range){
  cov_model  *next = cov->sub[0];
  int  dim = cov->tsdim,
    spacedim = dim - (next->isoown == SPACEISOTROPIC),
    max = spacedim + 2;
  if (spacedim != 2)
    ERR("div and curl currently programmed only for spatial dimension 2.");
  range->min[DIVCURL_WHICH] = 1;
  range->max[DIVCURL_WHICH] = max;
  range->pmin[DIVCURL_WHICH] = 1;
  range->pmax[DIVCURL_WHICH] = max;
  range->openmin[DIVCURL_WHICH] = false;
  range->openmax[DIVCURL_WHICH] = false;
}


/*

#define LP_P 0
void lp(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];  
  double z,
    p = P0(LP_P);
  int d,
    dim = cov->tsdim;
  for (z=0.0, d=0; d<dim; d++) {
    z += POW(FABS(x[d]), p);
  }
  z = POW(z, 1 / p);
  
  COV(&z, next, v);
}
int checklp(cov_model *cov) {
  bool skipchecks = GLOBAL.general.skipchecks;
  cov_model *next = cov->sub[0];  
  int err;

  kdefault(cov, 0, 1.0);
  if ((err = checkkappas(cov)) != NOERROR) return err;
 
  if ((err = CHECK(next, cov->tsdim + 1, 1, PosDefType, XONLY,ISOTROPIC,
		     SCALAR, ROLE_COV)) 
	!= NOERROR) return err;
  next->delflag = DEL_COV;

  // no setbackward
  double p =P0(LP_P);
  if (p==1) {
    cov->maxdim = next->maxdim - 1;  // true for p==1
    if (cov->maxdim == 0) cov->maxdim = 1;
  } else if (p==2) {
    cov->maxdim = next->maxdim;
  } else if (!skipchecks) SERR("p must be 1 or 2.");
  cov->monotone = next->monotone;
  cov->semiseparatelast = false;
  updatepref(cov, next);

  assert(false); // nicht verwendete Fkt

  return NOERROR;
}
void rangelp(cov_model *cov, range_type *range){
  range->min[LP_P] = 0.0;
  range->max[LP_P] = 2.0;
  range->pmin[LP_P] = 0.01;
  range->pmax[LP_P] = 2.0;
  range->openmin[LP_P] = true;
  range->openmax[LP_P] = false;
}

*/

#define MA1_ALPHA 0
#define MA1_BETA 1
void ma1(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];  
  double z,
    alpha = P0(MA1_ALPHA),
    theta = P0(MA1_BETA);
  COV(x, next, &z);
  *v = POW(theta / (1 - (1-theta) * z), alpha);  
}
int checkma1(cov_model *cov) {
//  bool skipchecks = GLOBAL.general.skipchecks;
  cov_model *next = cov->sub[0];  
//  double p;
  int err;

  kdefault(cov, 0, 1.0);
  kdefault(cov, 1, 0.5);
  if ((err = checkkappas(cov)) != NOERROR) return err;
       
  if ((err = CHECK(next, cov->tsdim, cov->xdimown, PosDefType, cov->domown,
		     cov->isoown, SCALAR, ROLE_COV)) 
	!= NOERROR) return err;

  //cov->semiseparatelast = cov->separatelast =false;
  cov->logspeed = 0.0;
  //  updatepref(cov, next);
  setbackward(cov, next);
  cov->mpp.maxheights[0] = 1.0;
  return NOERROR;
}
void rangema1(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[MA1_ALPHA] = 0.0;
  range->max[MA1_ALPHA] = RF_INF;
  range->pmin[MA1_ALPHA] = 0.01;
  range->pmax[MA1_ALPHA] = 10;
  range->openmin[MA1_ALPHA] = true;
  range->openmax[MA1_ALPHA] = true;

  range->min[MA1_BETA] = 0.0;
  range->max[MA1_BETA] = 1.0;
  range->pmin[MA1_BETA] = 0.0;
  range->pmax[MA1_BETA] = 1.0;
  range->openmin[MA1_BETA] = true;
  range->openmax[MA1_BETA] = true;
}


void ma2(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];  
  double z, z0;
  COV(ZERO, next, &z0);
  COV(x, next, &z);
  z = z0 - z;
  *v = (z == 0) ? 1.0 : (1.0 - EXP(-z)) / z;  
}
int checkma2(cov_model *cov) {
//  bool skipchecks = GLOBAL.general.skipchecks;
  cov_model *next = cov->sub[0];  
//  double p;
  int err;

  if ((err = checkkappas(cov)) != NOERROR) return err;
    
  if ((err = CHECK(next, cov->tsdim, cov->xdimown, VariogramType, cov->domown,
		     cov->isoown, SCALAR, ROLE_COV)) 
	!= NOERROR) return err;

  // cov->semiseparatelast = cov->separatelast =false;
  cov->logspeed = 0.0;
 //  updatepref(cov, next);
  setbackward(cov, next);
  cov->mpp.maxheights[0] = 1.0;

  return NOERROR;
}


#define M_M 0
void kappaM(int i, cov_model *cov, int *nr, int *nc){
  *nc = SIZE_NOT_DETERMINED;
  *nr = i < CovList[cov->nr].kappas  ? SIZE_NOT_DETERMINED : -1;
}

void M(cov_model *cov, double *Z, double *V) {
  // M C M^t
  cov_model *next = cov->sub[0];
  int 
    *ncol = cov->ncol,
    *nrow = cov->nrow;
  double 
    alpha = 1.0,
    beta = 0.0,
    *M = P(M_M);
//      SUBROUTINE DGEMM(TRANSA,TRANSB,M,N,K, ALPHA,A,LDA,B,LDB, BETA,C,LDC)
// C := alpha*op( A )*op( B ) + beta*C,
// M : rows of op(A)
// N : cols of op(B)
// K : cols of op(A)= rows of op(B)
// LD-X : first dimension of X   \command{\link{RMflat}} \tab constant in space \cr
 


 if (next->vdim[0] == 1) {
    F77_CALL(dgemm)("N", "T", nrow, nrow, ncol, Z, M, nrow, M, nrow, 
		    &beta, V, nrow); 
  } else {
    ALLOC_EXTRA2(MZ,*nrow * *ncol); 
    F77_CALL(dgemm)("N", "N", nrow, ncol, ncol, &alpha, M, nrow, Z, ncol, 
		    &beta, MZ, nrow); 
    F77_CALL(dgemm)("N", "T", nrow, nrow, ncol, &alpha, MZ, nrow, M, nrow, 
		    &beta, V, nrow);
  }
}
void Mstat(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  int    ncol = cov->ncol[M_M];
  assert(cov->Sextra != NULL);
  ALLOC_EXTRA(z, ncol * ncol);
  COV(x, next, z);  
  M(cov, z, v);
}
void Mnonstat(double *x, double *y, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  int    ncol = cov->ncol[M_M];
  ALLOC_EXTRA(z, ncol * ncol);
  NONSTATCOV(x, y, next, z);  
  M(cov, z, v);
}

int checkM(cov_model *cov) { 
  cov_model *next = cov->sub[0];
  int err, i,
    nrow = cov->ncol[M_M];

  if (nrow > MAXMPPVDIM) 
    SERR2("the maximum multivariate dimension is %d, but %d is given by the user", MAXMPPVDIM, nrow);
  
  if ((err = checkkappas(cov)) != NOERROR) return err;
  cov->vdim[0] = cov->vdim[1] = cov->nrow[M_M]; // zwingend vor C-HECK


  if ((err = CHECK(next, cov->tsdim, cov->xdimown, PosDefType, cov->domown, 
		   cov->isoown, cov->ncol[M_M], ROLE_COV)) != NOERROR) {
    //    MERR(err); printf("x");
    return err;
  }

  setbackward(cov, next);
  for (i=0; i<nrow; i++) cov->mpp.maxheights[i] = RF_NA;

  EXTRA_STORAGE;
 
  return NOERROR;
}

sortsofparam sortof_M(int k, int row, int col) {
  return k<0 ? VARPARAM : (row==col) ? SDPARAM : SIGNEDSDPARAM;
}
  
void rangeM(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){ 
  range->min[M_M] = RF_NEGINF;
  range->max[M_M] = RF_INF;
  range->pmin[M_M] = -1e5;
  range->pmax[M_M] = 1e5;
  range->openmin[M_M] = true;
  range->openmax[M_M] = true; 
}






#define SCATTER_STEP 0
#define SCATTER_MAX 1
void kappaScatter(int i, cov_model *cov, int *nr, int *nc){
  *nc = 1;
  *nr = i < CovList[cov->nr].kappas ? 0 : -1;
}

void Scatter(double *xx, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  double *xmin = cov->Sscatter->xmin;
  int i, d,
    vdim = cov->vdim[0] * cov->vdim[1],
    tsxdim = cov->xdimown;
  scatter_storage *s = cov->Sscatter;
  
  for (i=0; i<vdim; i++) v[i]=0.0;

  for (d=0; d<tsxdim; d++) {
    if (P(SCATTER_STEP)[d] <= 0) {
    } else {
      s->xmin[d] = (double) s->min[d] * s->step[d] + xx[d];
    }
  }
   
  int							
    *nx = s->nx,						
    *start = s->min,					
    *end = s->max;						
  double						
    *x = s->x,						
    *xstart = xmin,					
    *inc = s->step;						
							
  for (d=0; d<tsxdim; d++) {				
    nx[d] = start[d];					
    x[d] = xstart[d];					
  }

  while (true) {
    COV(x, next, s->value);
    for (i=0; i<vdim; i++) v[i] += s->value[i];
    STANDARDINKREMENT;
  }
}


int checkScatter(cov_model *cov) {
  cov_model *next = cov->sub[0];
  mpp_param *gp = &(GLOBAL.mpp);
  int d, nr, err,
    dim = cov->xdimown,
    vdim = cov->vdim[0] * cov->vdim[1];
 
  if (PisNULL(SCATTER_MAX)) {
    PALLOC(SCATTER_MAX, dim, 1);
    for (d=0; d<dim; d++) P(SCATTER_MAX)[d] = gp->scatter_max[d];
  } else if ((nr = cov->nrow[SCATTER_MAX]) < dim) {
    int j, *m = PINT(SCATTER_MAX);
    assert(nr > 0);
    PtoNULL(SCATTER_MAX);
    PALLOC(SCATTER_MAX, dim, 1);
    for (j=d=0; d<dim; d++)  {
      P(SCATTER_MAX)[d] = m[j++];
      j = j % nr;
    }
  }
  if (PisNULL(SCATTER_STEP)) {
    PALLOC(SCATTER_STEP, dim, 1);
    for (d=0; d<dim; d++) P(SCATTER_STEP)[d] = gp->scatter_step[d];
  } else if ((nr = cov->nrow[SCATTER_STEP]) < dim) {
    int j;
    double *m = P(SCATTER_STEP);
    assert(nr > 0);
    PtoNULL(SCATTER_MAX);
    PALLOC(SCATTER_MAX, dim, 1);
    for (j=d=0; d<dim; d++)  {
      P(SCATTER_MAX)[d] = m[j++];
      j = j % nr;
    }
  }
  if ((err = CHECK_VDIM(next, cov->tsdim, cov->xdimown, ShapeType, cov->domown, 
			cov->isoown, cov->vdim[0], cov->vdim[1],
			cov->role)) != NOERROR)
    return err;
  setbackward(cov, next);
  
  if (cov->Sscatter == NULL || cov->Sscatter->dim != dim ||
      cov->Sscatter->vdim != vdim) {
    NEW_STORAGE(scatter);
    cov->Sscatter->vdim = vdim;
    cov->Sscatter->dim = dim;
    ALLOC_NEWINT(Sscatter, nx, dim, nx);
    ALLOC_NEWINT(Sscatter, min, dim, min);
    ALLOC_NEWINT(Sscatter, max, dim, max);
    ALLOC_NEW(Sscatter, x, dim, x);
    ALLOC_NEW(Sscatter, step, dim, step);
    ALLOC_NEW(Sscatter, xmin, dim, xmin);
    ALLOC_NEW(Sscatter, value, vdim, value);
    location_type *loc = Loc(cov);
    for (d=0; d<dim; d++) {
      if (P(SCATTER_STEP)[d] <= 0) {
	if (loc->grid) {
	  step[d] = loc->xgr[d][XSTEP];
	} else SERR1("non-positive '%s' only allowed for grids", 
		     KNAME(SCATTER_STEP));
      } else {
	step[d] = P(SCATTER_STEP)[d];
      }
      if (P(SCATTER_MAX)[d] < 0) {
	if (P(SCATTER_STEP)[d] > 0) {
	  NONSTATINVERSE(&(GLOBAL.mpp.about_zero), next, xmin, x);
	} else {
	  min[0] = 0;
	  max[d] = loc->xgr[d][XLENGTH];
	}
      } else {
	if (P(SCATTER_STEP)[d] > 0) {
	  min[d] = -(max[d] = P(SCATTER_MAX)[d]);
	} else {
	  min[d] = max[d] = NA_INTEGER;
	}
      }
      max[d]++; // da auf '<' getestet wird
      xmin[d] = (double) min[d] * step[d];
    }
  }

  cov->taylorN = cov->tailN = MISMATCH;

  return NOERROR;
}
  
void rangeScatter(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){ 
  range->min[SCATTER_STEP] = RF_NEGINF;
  range->max[SCATTER_STEP] = RF_INF;
  range->pmin[SCATTER_STEP] = 1e-5;
  range->pmax[SCATTER_STEP] = 1e5;
  range->openmin[SCATTER_STEP] = true;
  range->openmax[SCATTER_STEP] = true; 

  range->min[SCATTER_MAX] = -1;
  range->max[SCATTER_MAX] = MAXINT;
  range->pmin[SCATTER_MAX] = 0;
  range->pmax[SCATTER_MAX] = 999;
  range->openmin[SCATTER_MAX] = false;
  range->openmax[SCATTER_MAX] = true;
}



int struct_scatter(cov_model VARIABLE_IS_NOT_USED *cov, 
		   cov_model VARIABLE_IS_NOT_USED  **newmodel){
  return NOERROR;
}

int init_scatter(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  int t, i;
  //vdim = cov->vdim[0];
  cov_model *next = cov->sub[0];

  if (cov->vdim[1] != 1) 
    SERR("matrix-valued shape functions cannot be initialised");
  if (hasAnyShapeRole(cov)) {
    for (i=0; i<=cov->mpp.moments; i++) 
      cov->mpp.mM[i] = cov->mpp.mMplus[i] = RF_NA;

    Scatter(ZERO, cov, cov->mpp.maxheights); 

    if (next->taylor[0][TaylorPow] < 0.0) {
      cov->taylorN = next->taylorN;
      for (i=0; i < next->taylorN; i++) 
	for (t=TaylorConst; t <= TaylorPow; t++)
	  cov->taylor[i][t] = next->taylor[i][t];
    } else {
      cov->taylorN = 1;
      cov->taylor[0][TaylorConst] = cov->mpp.maxheights[0];
      cov->taylor[0][TaylorPow] = 0.0;
    } 
 
    cov->tailN = next->tailN;
    for (i=0; i < next->tailN; i++) 
      for (t=TaylorConst; t <= TaylorExpPow; t++)
	cov->tail[i][t] = next->tail[i][t];
     
  }
  
  else ILLEGAL_ROLE;

  return NOERROR;
}






#define SCHUR_M 0
#define SCHUR_DIAG 1
#define SCHUR_RED 2
void kappaSchur(int i, cov_model *cov, int *nr, int *nc){
  double *M = P(SCHUR_M);
  int vdim = cov->nrow[M != NULL ? SCHUR_M : SCHUR_DIAG]; 
  *nc = i == SCHUR_M ? vdim : 1;
  *nr = i == SCHUR_RED ? vdim * (vdim-1) / 2 
    : i < CovList[cov->nr].kappas ? vdim : -1;
}

void SchurMult(double VARIABLE_IS_NOT_USED *x, cov_model *cov, double *v){
  double *M = P(SCHUR_M);
  int i,
    vdim = cov->vdim[0];
  if (!PisNULL(SCHUR_M)) {
    int nrow2 = cov->nrow[SCHUR_M] * cov->nrow[SCHUR_M];
    for (i=0; i<nrow2; i++) v[i] *= M[i]; 
  } else {
    int j, k;
    double 
      *q = cov->q,
      *diag=P(SCHUR_DIAG),
      *red=P(SCHUR_RED);
    for (i=0; i<vdim; i++) q[i] = SQRT(diag[i]);
    for (k=i=0; i<vdim; i++) {
      for (j=0; j<vdim; j++, k++) {
	v[k] *= q[i] * q[j];
      }
    }
    for (k=i=0; i<vdim; i++) {
      for (j=0; j<vdim; j++, k++) {
	v[i + vdim * j] *= red[k];	
  	v[j + vdim * i] *= red[k];
      }
    }    
  }
}

void Schurstat(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  COV(x, next, v); 
  SchurMult(x, cov, v);
}
void DSchur(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  Abl1(x, next, v); 
  SchurMult(x, cov, v);
}
void D2Schur(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  Abl2(x, next, v); 
  SchurMult(x, cov, v);
}
void D3Schur(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  Abl3(x, next, v); 
  SchurMult(x, cov, v);
}
void D4Schur(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  Abl4(x, next, v); 
  SchurMult(x, cov, v);
}

void Schurnonstat(double *x, double *y, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  NONSTATCOV(x, y, next, v);  
  SchurMult(x, cov, v);
}
int checkSchur(cov_model *cov) {
  cov_model *next = cov->sub[0];
  double *C,
    *M=P(SCHUR_M),
    *diag=P(SCHUR_DIAG),
    *red=P(SCHUR_RED);
  int i,j, k, l,
    err = NOERROR, 
    vdim = cov->nrow[M != NULL ? SCHUR_M : SCHUR_DIAG],
    vdimP1 = vdim + 1,
    bytes = vdim * vdim * sizeof(double),
    *nrow = cov->nrow,
    *ncol = cov->ncol;

  cov->vdim[0] = cov->vdim[1] = vdim;
  if ((err = CHECK(next, cov->tsdim, cov->xdimown, PosDefType, cov->domown, 
		     cov->isoown, nrow[SCHUR_M], ROLE_COV)) != NOERROR) 
    return err;
  setbackward(cov, next);

  if (M == NULL) {
    if (diag == NULL || red == NULL)
      SERR3("either '%s' and '%s' or '%s' must be given", 
	    KNAME(SCHUR_DIAG), KNAME(SCHUR_RED), KNAME(SCHUR_M));
    for (i=0; i<vdim; i++) 
      if (diag[i]<0) SERR1("elements of '%s' negative.", KNAME(SCHUR_DIAG));
    C = (double*) MALLOC(bytes);
    for (k=l=i=0; i<vdim; i++, l+=vdimP1) {
      for (j=0; j<vdim; j++, k++) {
	C[i + vdim * j] = C[i * vdim + j] = red[k];
      }
      C[l] = 1.0;
    }
    F77_CALL(dpofa)(C, ncol, ncol, &err); // C i s now cholesky
    if (err != 0)
      SERR3("%d x %d matrix '%s' is not (strictly) positive definite",
	    nrow[SCHUR_M], ncol[SCHUR_M], KNAME(SCHUR_M));   
    QALLOC(vdim);
  } else {
    if (diag != NULL || red != NULL)
      SERR3("if '%s' is given, neither '%s' nor '%s' might be given.",
	   KNAME(SCHUR_M),  KNAME(SCHUR_DIAG), KNAME(SCHUR_RED))
    C = (double*) MALLOC(bytes);
    MEMCOPY(C, M, bytes);
    F77_CALL(dpofa)(C, ncol, ncol, &err); // C i s now cholesky
    if (err != 0)
      SERR3("%d x %d matrix '%s' is not (strictly) positive definite",
	    nrow[SCHUR_M], ncol[SCHUR_M], KNAME(SCHUR_M));
  }
    
  FREE(C);
  for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = 1.0;

  /*

    Check symmetry??

 for (vdiag = i=0; i<vdim; i++, vdiag+=vdimP1) {
    if (rhored[vdiag] != 1.0) SERR("diagonal elements do not equal 1.0");
    if (value[i]  <  -GLOBAL.direct.svdtolerance) {
      SERR("' r  hored' has negative eigenvalues");
    }
  }
  for (vdiag=i=0; i<vdim; i++, vdiag+=vdimP1) {
    for (j=i+1; j<vdim; j++) {
      if (rhored[ vdiag + j - i] != rhored[vdiag + vdim * (j-i)]) 
	SERR("' rh ored' not symmetric");
    }
  }
  */


  return err;
}
  
void rangeSchur(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){ 
  range->min[SCHUR_M] = RF_NEGINF;
  range->max[SCHUR_M] = RF_INF;
  range->pmin[SCHUR_M] = -1e5;
  range->pmax[SCHUR_M] = 1e5;
  range->openmin[SCHUR_M] = true;
  range->openmax[SCHUR_M] = true; 

  range->min[SCHUR_DIAG] = 0;
  range->max[SCHUR_DIAG] = RF_INF;
  range->pmin[SCHUR_DIAG] = 0;
  range->pmax[SCHUR_DIAG] = 1e5;
  range->openmin[SCHUR_DIAG] = false;
  range->openmax[SCHUR_DIAG] = true; 

  range->min[SCHUR_RED] = 0;
  range->max[SCHUR_RED] = 1;
  range->pmin[SCHUR_RED] =  0;
  range->pmax[SCHUR_RED] = 1;
  range->openmin[SCHUR_RED] = false;
  range->openmax[SCHUR_RED] = false; 
}


#define ID_VDIM 0
void IdStat(double *x, cov_model *cov, double *v) {
  //  cov_model 
  //    *key = cov->key;
  // if (key != NULL) CovList[key->nr].cov(x, key, v);
  // else {
    cov_model *next = cov->sub[0];
    COV(x, next, v);
    // }
}
void IdNonStat(double *x, double *y, cov_model *cov, double *v){
  // cov_model 
  //    *key = cov->key;
  //  if (key != NULL) CovList[key->nr].nonstat_cov(x, y, key, v);
  // else {
    cov_model *next = cov->sub[0];
    NONSTATCOV(x, y, next, v);
    // }
}

int checkId(cov_model *cov) {
  cov_model *next = cov->sub[0];
  int err;

  cov->vdim[0] = cov->vdim[1] = 
    !PisNULL(ID_VDIM) ? P0INT(ID_VDIM) : SUBMODEL_DEP;
  if ((err = CHECK(next, cov->tsdim, cov->xdimown, PosDefType, cov->domown,
		     cov->isoown, cov->vdim, cov->role)) !=NOERROR) return err;
  if (cov->vdim[0] == SUBMODEL_DEP) {
    cov->vdim[0] = next->vdim[0];
    cov->vdim[1] = next->vdim[1];
  }
  cov->logspeed = next->logspeed;
  setbackward(cov, next);
   
  return NOERROR;
}

void DId(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  Abl1(x, next, v);
}

void DDId(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  Abl2(x, next, v);
}
void TBM2Id(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  TBM2CALL(x, next, v)
}
void IdInverse(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  INVERSE(x, next, v);
}

int initId(cov_model *cov, gen_storage *S) {
  cov_model *next = cov->sub[0];
  return INIT(next, cov->mpp.moments, S);
}
void spectralId(cov_model *cov, gen_storage *S, double *e) { 
  //  spectral_storage *s = &(S->Sspectral);
  cov_model *next = cov->sub[0];
  SPECTRAL(next, S, e); // nicht nr
}
void coinitId(cov_model *cov, localinfotype *li) {
  cov_model *next = cov->sub[0];
  CovList[next->nr].coinit(next, li); // nicht nr
}
void ieinitId(cov_model *cov, localinfotype *li) {
  cov_model *next = cov->sub[0];
  CovList[next->nr].ieinit(next, li); // nicht nr
}
void rangeId(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){ 
  range->min[ID_VDIM] = 1.0;
  range->max[ID_VDIM] = RF_INF;
  range->pmin[ID_VDIM] = 1.0;
  range->pmax[ID_VDIM] = 10;
  range->openmin[ID_VDIM] = false;
  range->openmax[ID_VDIM] = true;
}


#define EXP_N 0
#define EXP_STANDARDISED 1
void Exp(double *x, cov_model *cov, double *v, int n, bool standardize){
  double v0, 
    s = 0.0, 
    w = 1.0;
  cov_model *next = cov->sub[0];
  int k,
    vdim = cov->vdim[0],
    vdimq = vdim * vdim;

  COV(x, next, v);
  if (vdim == 1) {
    for (k=0; k<=n; k++) {
      s += w;
      w *= *v / (double) (k+1);
    }    
    *v = EXP(*v) - s;
    if (standardize) {
      Exp(ZERO, cov, &v0, n, false);
      *v /= v0;
    }
  } else {
    int i;
    BUG;
    // fehlt die multiplication von links und rechts mit C(0)^{-1/2}
    for (i=0; i<vdimq; i++) v[i] = EXP(v[i]); 
  }
}

void Exp(double *x, cov_model *cov, double *v){	
  Exp(x, cov, v, P0INT(EXP_N), P0INT(EXP_STANDARDISED));
}


void nonstatExp(double *x, double *y, cov_model *cov, double *v, int n, 
		bool standardised){
  cov_model *next = cov->sub[0];
  double v0, 
    s = 0.0, 
    w = 1.0;
  int k,
    vdim = cov->vdim[0],
    vdimq = vdim * vdim;

  NONSTATCOV(x, y, next, v);
  if (vdim == 1) {
    for (k=0; k<=n; k++) {
      s += w;
      w *= *v / (double) (k+1);
    }    
    *v = EXP(*v) - s;
    if (standardised) {
      nonstatExp(ZERO, ZERO, cov, &v0, n, false);
      *v /= v0;
    }
  } else {
    int i;
    BUG;
    // fehlt die multiplication von links und rechts mit C(0)^{-1/2}
    for (i=0; i<vdimq; i++) v[i] = EXP(v[i]); 
  }
}

void nonstatExp(double *x, double *y, cov_model *cov, double *v){	
  nonstatExp(x, y, cov, v, P0INT(EXP_N), P0INT(EXP_STANDARDISED));
}

void DExp(double *x, cov_model *cov, double *v){
  double D;
  cov_model *next = cov->sub[0];
  int n = P0(EXP_N);
  assert(cov->vdim[0] == 1);
  
  Abl1(x, next, &D);
  Exp(x, cov, v, n - 1, false);
  *v *= -D;

  if (P0INT(EXP_STANDARDISED)) {
    double v0;
    Exp(ZERO, cov, &v0, n, false);
    *v /= v0;
  }
}

  // Hier fehlt die multidim variante (Hesse matrix)
  

void DDExp(double *x, cov_model *cov, double *v){
  double D, Abl2, w;
  cov_model *next = cov->sub[0];
  int n = P0INT(EXP_N);
  assert(cov->vdim[0] == 1);

  Abl1(x, next, &D);
  Abl2(x, next, &Abl2);
  Exp(x, cov, v, n - 2, false);
  Exp(x, cov, &w, n - 1, false);

  *v =  D * D * *v + Abl2 * w; // Achtung + D2 nicht - D2, da D2 Ableitung einer Cov.

  if (P0INT(EXP_STANDARDISED)) {
    double v0;
    Exp(ZERO, cov, &v0, n, false);
    *v /= v0;
  }
}

int checkExp(cov_model *cov) {
  cov_model *next = cov->sub[0];
  int err, i,
      vdim = cov->vdim[0];

  kdefault(cov, EXP_N, -1);
  if (!isPosDef(next->typus) && P0INT(EXP_N) != -1)
    SERR("for variograms only n=-1 allowed");
  kdefault(cov, EXP_STANDARDISED, 1);
 
  if ((err = CHECKPD2ND(next, cov->tsdim,  cov->xdimown, cov->isoown,
			SCALAR, ROLE_COV)) != NOERROR) return err;

  next->delflag = DEL_COV - 10;

  setbackward(cov, next);
  if (cov->vdim[0] > 1 && P0INT(EXP_N) != -1)
    SERR1("'%s' must be '-1' in the multivariate case", KNAME(EXP_N));
  if (cov->vdim[0] > 1) SERR("multivariate case not programmed yet");
 
  if (next->domown == XONLY) {
    cov_fct *C = CovList + cov->nr;
    cov->pref[CircEmbed] = C->pref[CircEmbed];
    cov->pref[Direct] = C->pref[Direct];
    cov->pref[Sequential] = C->pref[Sequential];    
    if (!isVariogram(cov->typus))
      SERR1("negative definite function expected -- got '%s'",
	    TYPENAMES[cov->typus]);
  } else {
    if (!isPosDef(cov)) SERR1("positive definite function expected -- got '%s'",
			      TYPENAMES[cov->typus]);
 
  }

  double height= isVariogram(next->typus) && !isPosDef(next->typus) ? 1.0 : RF_NA;
  for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = height;

  cov->monotone = 
    (isBernstein(next)) ? NORMAL_MIXTURE : 
    isMonotone(next->monotone) ? MONOTONE : NOT_MONOTONE;
  cov->logspeed = 0.0;
  return NOERROR;
}


void rangeExp(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){ 
  range->min[EXP_N] = -1;
  range->max[EXP_N] = RF_INF;
  range->pmin[EXP_N] = -1;
  range->pmax[EXP_N] = 5;
  range->openmin[EXP_N] = false;
  range->openmax[EXP_N] = true;

  range->min[EXP_STANDARDISED] = 0;
  range->max[EXP_STANDARDISED] = 1;
  range->pmin[EXP_STANDARDISED] = 0;
  range->pmax[EXP_STANDARDISED] = 1;
  range->openmin[EXP_STANDARDISED] = false;
  range->openmax[EXP_STANDARDISED] = false;
}



void Pow(double *x, cov_model *cov, double *v){
  double v0, v1,
    alpha = P0(POW_ALPHA);
  cov_model *next = cov->sub[0];
 
  COV(ZERO, next, &v0);
  COV(x, next, &v1);
  *v = POW(v0, alpha) - POW(v0 - v1, alpha); 
  //  print("Pow %f %f %f v=%f\n", v0, v1, alpha, *v);
}

void DPow(double *x, cov_model *cov, double *v){
  double v0, v1, gamma,
    alpha = P0(POW_ALPHA);
  cov_model *next = cov->sub[0];

  Abl1(x, next, v);
  if (alpha == 1.0) return;
  COV(ZERO, next, &v0);
  COV(x, next, &v1);
  gamma =  v0 - v1;
  *v *= - alpha * POW(gamma, alpha -1.0);
  // Achtung  "-" , da covarianzfunktion angenommen
}

void DDPow(double *x, cov_model *cov, double *v){
  double D, v0, v1, gamma,
    alpha = P0(POW_ALPHA);
  cov_model *next = cov->sub[0];

  Abl2(x, next, v);
  if (alpha == 1.0) return;
  Abl1(x, next, &D);
  COV(ZERO, next, &v0);
  COV(x, next, &v1);
  gamma =  v0 - v1;
  *v *= - alpha *  POW(gamma, alpha - 2.0) * ((alpha - 1.0) * D + gamma * (*v));
  // Achtung  "-" , da covarianzfunktion angenommen
}

void InversePow(double *x, cov_model *cov, double *v) {
  // invPow only defined for submodel having variance 1 !!!!
  cov_model *next = cov->sub[0];
  double v0,
    alpha = P0(POW_ALPHA);
    
  COV(ZERO, next, &v0);
  v0 = v0 - POW(POW(v0, alpha) - *x, 1 / alpha);
  INVERSE(&v0, next, v);

  /*
  COV(x, next, v);
  double y = v0 - *v;
  if (y < 0.0 || y > 1.0) {
    if (y > -1e-14 && y < 0.0)  y=0.0;
    else if (y < 1.0 + 1e-14)  y=1.0;
    else {
      //PRINTF("covariance value %e (1 - %e = %e) at %e\n", *v, *v, 1.0-*v, *x);
      ERR("invPow valid only for non-negative covariance models with variance 1");
    }
  }
  
  *v = 1.0 - POW(y, 1.0 / alpha);
  //  print("%f %f %f\n", y, *v, P0(POW_ALPHA));
  //  print("Inv %f %f %f v=%f\n", *x,  P0(POW_ALPHA),y, *v);
  */

}

int checkPow(cov_model *cov) {
  cov_model *next = cov->sub[0];
  int err;
  //Types type;
  //print("OK %d %d\n", cov->domown, cov->isoown);
  if ((err = checkkappas(cov)) != NOERROR) return err;
  if (cov->domown != XONLY) return ERRORSTATVARIO;
  cov->nr = isVariogram(cov) ? POW : SHAPEPOW;
  if ((err = CHECK(next, cov->tsdim, cov->xdimown, cov->typus, //PosDefType, 
		   cov->domown, cov->isoown, SCALAR, ROLE_COV)) != NOERROR){
    // print("OK %d\n",err);
    return err;  
  }
  // print("xOK %d\n",err);
  setbackward(cov, next);      
  assert(cov->vdim[0] == 1);
  cov->mpp.maxheights[0] = RF_NA;
  cov->monotone =
    isMonotone(next->monotone) && P0(POW_ALPHA) > 0 ? MONOTONE : NOT_MONOTONE;
  
  return NOERROR;
}

void rangePow(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){ 
  if (isVariogram(cov)) {
    range->min[POW_ALPHA] = 0.0;
    range->max[POW_ALPHA] = 1.0;
    range->pmin[POW_ALPHA] = 0.01;
    range->pmax[POW_ALPHA] = 1.0;
    range->openmin[POW_ALPHA] = true;
    range->openmax[POW_ALPHA] = false;
  } else {
    range->min[POW_ALPHA] = RF_NEGINF;
    range->max[POW_ALPHA] = RF_INF;
    range->pmin[POW_ALPHA] = -10;
    range->pmax[POW_ALPHA] = 10;
    range->openmin[POW_ALPHA] = true;
    range->openmax[POW_ALPHA] = true;
  }
}



////////////////////////////////////////////////////////////////////



void shapePow(double *x, cov_model *cov, double *v){
  double 
    alpha = P0(POW_ALPHA);
  cov_model *next = cov->sub[0];
  COV(x, next, v);
  *v = POW(*v, alpha);
  //  print("Pow %f %f %f v=%f\n", v0, v1, alpha, *v);
}

void DshapePow(double *x, cov_model *cov, double *v){
  double v0, 
    alpha = P0(POW_ALPHA);
  cov_model *next = cov->sub[0];

  Abl1(x, next, v);
  if (alpha == 1.0) return;
  COV(ZERO, next, &v0);
  *v *= - alpha * POW(v0, alpha -1.0);
  // Achtung  "-" , da covarianzfunktion angenommen
}

void DDshapePow(double *x, cov_model *cov, double *v){
  double D, v0,
    alpha = P0(POW_ALPHA);
  cov_model *next = cov->sub[0];

  Abl2(x, next, v);
  if (alpha == 1.0) return;
  Abl1(x, next, &D);
  COV(x, next, &v0);
  *v *= alpha *  POW(v0, alpha - 2.0) * ((alpha - 1.0) * D + v0 * (*v));
  // Achtung  "-" , da covarianzfunktion angenommen
}

void InverseShapePow(double *x, cov_model *cov, double *v) {
  // invPow only defined for submodel having variance 1 !!!!
  cov_model *next = cov->sub[0];
  double alpha = P0(POW_ALPHA);
     
  INVERSE(x, next, v);
  *v = POW(*v, 1.0 / alpha);
  //  print("%f %f %f\n", y, *v, P0(POW_ALPHA));
  //  print("Inv %f %f %f v=%f\n", *x,  P0(POW_ALPHA),y, *v);
}








////////////////////////////////////////////////////////////////////


/* qam */
void kappaqam(int i, cov_model *cov, int *nr, int *nc){
  *nc = 1;
  *nr = i < CovList[cov->nr].kappas  ? cov->nsub-1 : -1;
}


void qam(double *x, cov_model *cov, double *v) {
  cov_model *next = cov->sub[0];
  int i, 
    nsub = cov->nsub;
  double sum, s, w,
    *theta = P(QAM_THETA);
  
  sum = 0.0;
  for (i=1; i<nsub; i++) {
    cov_model * sub = cov->sub[i];
    COV(x, sub, &s);
    INVERSE(&s, next, &w);
    sum += theta[i - 1] * w * w;
  }

  sum = SQRT(sum);
  COV(&sum, next, v); 
}

int checkqam(cov_model *cov) {
  cov_model *next = cov->sub[0],
    *sub;
  int i, err, 
    nsub = cov->nsub,
    nsubM1 = nsub - 1;
  double v, sum;
      
  if ((err = checkkappas(cov)) != NOERROR) return err;
  
  //  cov->monotone = NORMAL_MIXTURE;
  sum = 0.0;
  for (i=0; i<nsubM1; i++) {
    sum += P(QAM_THETA)[i];
  }
  if (FABS(sum - 1.0) > 1e-14) SERR("theta must sum up to 1");    

  if ((err = CHECK(next, 1,  1, PosDefType, cov->domown, cov->isoown, 
		     SCALAR, ROLE_COV)) != NOERROR)
    return err;
  if (!isNormalMixture(next->monotone))
    SERR("phi is not a normal mixture");   
 
  for (i=1; i<nsub; i++) {
    sub = cov->sub[i];
    if ((err = CHECK(sub, cov->tsdim, cov->tsdim, PosDefType,
		       cov->domown, cov->isoown,  SCALAR, ROLE_COV)) != NOERROR)
      return err;
    COV(ZERO, sub, &v);
    if (v != 1.0) SERR("unit variance required");
    setbackward(cov, sub);
  } 

  INVERSE(ZERO, next, &v);

  if (ISNAN(v)) SERR1("inverse function of '%s' unknown", NICK(next));

  cov->logspeed = 0.0;   
  return NOERROR;
}


void rangeqam(cov_model *cov, range_type *range){
  double pmax = 2.0 / (double)  (cov->nsub-1);
  range->min[QAM_THETA] = 0.0;
  range->max[QAM_THETA] = 1.0;
  range->pmin[QAM_THETA] = 0.0;
  range->pmax[QAM_THETA] = pmax;
  range->openmin[QAM_THETA] = false;
  range->openmax[QAM_THETA] = false;
}



/* mqam */
void kappamqam(int i, cov_model *cov, int *nr, int *nc) {
  int nsub = cov->nsub - 1;
  *nc = (i==QAM_THETA) ? 1 : -1;
  *nr = i == QAM_THETA ? nsub : -1;
  
  // print("kappa %d nsub=%d %d %d\n", i, nsub, *nc, *nr);

}
void mqam(double *x, cov_model *cov, double *v) {
  cov_model *next = cov->sub[0];
  int i, j, k, l,
    vdim = cov->vdim[0],
    vdimP1 = vdim + 1;
  double s0,
    *theta = P(QAM_THETA),
    s[MAXSUB];
  
  for (i=0; i<vdim; i++) {
    cov_model * sub = cov->sub[i+1];
    COV(x, sub, &s0);
    INVERSE(&s0, next, s + i);
    s[i] *= theta[i] * s[i];
  }
    
  for (j=0; j<vdim; j++) {
    l = k = j * vdimP1; // nicht vdim
    for (i=j; i<vdim; i++, k++, l+=vdim) {
      s0 = SQRT(s[i] + s[j]);
      COV(&s0, next, v + k);
      v[l] = v[k]; 
      // if (k != l) v[k] *= rho[k];
      // assert (rho[l] == rho[k]) ;
            
    }
  }

}

int checkmqam(cov_model *cov) {
  int err,
    nsub = cov->nsub,
    vdim = nsub - 1;     

  //  NotProgrammedYet("");

  if ((err = checkqam(cov)) != NOERROR) return err;
  cov->vdim[0] = cov->vdim[1] = vdim;
  return NOERROR;
}

void rangemqam(cov_model *cov, range_type *range){
  //double pmax = 2.0 / (double)  (cov->nsub-1);

  rangeqam(cov, range);
}



/////////////// NATSC

void natsc(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  double invscale, y;

  INVERSE(&GLOBAL.gauss.approx_zero, next, &invscale);
  y = *x * invscale;
  // letzteres darf nur passieren wenn dim = 1!!
  COV(&y, next, v);
}

void Dnatsc(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  int i,
    vdim = cov->vdim[0],
    vdimSq = vdim * vdim;
  double invscale, y;
 
  assert(CovList[next->nr].inverse != NULL);
  INVERSE(&GLOBAL.gauss.approx_zero, next, &invscale);
  y = *x * invscale;

  Abl1(&y, next, v);
  for (i=0; i<vdimSq; i++) v[i] *= invscale; 
}

void DDnatsc(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  int i,
    vdim = cov->vdim[0],
    vdimSq = vdim * vdim;
  double invscale, y, invScSq;

  assert(CovList[next->nr].inverse != NULL);
  INVERSE(&GLOBAL.gauss.approx_zero, next, &invscale);
  y = *x * invscale;
  invScSq = invscale * invscale;

  Abl2(&y, next, v);
  for (i=0; i<vdimSq; i++) v[i] *= invScSq; 
}

void Inversenatsc(double *x, cov_model *cov, double *v) {
  cov_model *next = cov->sub[0];
  double invscale, modelinv;
  assert(CovList[next->nr].inverse != NULL);
  INVERSE(x, next, &modelinv);    
  INVERSE(&GLOBAL.gauss.approx_zero, next, &invscale);
  *v = modelinv / invscale;
}

int checknatsc(cov_model *cov) {
  cov_model *next = cov->sub[0];
  int err;


  assert(isNatsc(cov));

  if ((err = CHECK(next, cov->tsdim, cov->xdimown, PosDefType, cov->domown,
		   cov->isoown, SUBMODEL_DEP, ROLE_COV))
      != NOERROR) {

    return err;
  }

  if (next->domown == cov->domown && next->isoown == cov->isoown) {
    next->delflag = DEL_COV - 12;
  }

  if (CovList[next->nr].inverse == NULL) {
    SPRINTF(ERRORSTRING, "natural scaling is not defined for %s", 
	    NICK(next));
    return ERRORFAILED;
  }
 
  double invscale;
  INVERSE(&GLOBAL.gauss.approx_zero, next, &invscale);

  if (invscale == RF_NAN)
    SERR1("inverse function of '%s' unknown", NICK(next));


  cov->logspeed = 0.0;
  setbackward(cov, next);

  cov->vdim[0] = next->vdim[0];
  cov->vdim[1] = next->vdim[1];

  return NOERROR;
}


int initnatsc(cov_model *cov, gen_storage *s){
  //  location_type *loc = Loc(cov);

  if (cov->role == ROLE_GAUSS) {
    cov_model *next = cov->sub[0];
    return INIT(next, cov->mpp.moments, s);
  }

  else if (cov->role == ROLE_BROWNRESNICK || cov->role == ROLE_SMITH ||
	   cov->role == ROLE_SCHLATHER || cov->role == ROLE_POISSON
	   || cov->role == ROLE_POISSON_GAUSS) {
   
    SERR("natsc for max-stable processes and poisson process not programmed yet");

  }

  else ILLEGAL_ROLE;

  return NOERROR;
}

void donatsc(cov_model *cov, gen_storage *s){
  cov_model *next = cov->sub[0];
  DO(next, s);
}

void spectralnatsc(cov_model *cov, gen_storage *S, double *e) {
  //  spectral_storage *s = &(S->Sspectral);
  cov_model *next = cov->sub[0];
  int d,
    dim = cov->tsdim;
  double invscale;

  INVERSE(&GLOBAL.gauss.approx_zero, next, &invscale);  
  SPECTRAL(next, S, e);
  for (d=0; d<dim; d++) e[d] *=  invscale;
}

void coinitnatsc(cov_model *cov, localinfotype *li) {
  cov_model *next = cov->sub[0];
  cov_fct *C = CovList + next->nr; // not gatternr
  if ( C->coinit == NULL) 
    ERR("# cannot find coinit -- please inform author");
  C->coinit(next, li); // not gatternr
}

void ieinitnatsc(cov_model *cov, localinfotype *li) {
  cov_model *next = cov->sub[0];
  cov_fct *C = CovList + next->nr; // not gatternr
  if ( C->ieinit == NULL) // not gatternr
    ERR("# cannot find ieinit -- please inform author");
  C->ieinit(next, li); // not gatternr
}

void tbm2natsc(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[0];
  double invscale, y;

  INVERSE(&GLOBAL.gauss.approx_zero, next, &invscale);
  y = x[0] * invscale;
     
  TBM2CALL(&y, next, v)
}

//////////////////////////////////////

void truncsupport(double *x, cov_model *cov, double *v){
  // truncsupport erwartet dass vorher $ kommt, sofern skalenmischung
  cov_model *next = cov->sub[0];
  int xdimown = cov->xdimown;
  double dist,
    radius = P0(TRUNC_RADIUS); // default -1
  if (xdimown > 1) {
    int i;
    dist = 0.0;
    for (i=0; i<xdimown; i++) dist += x[i] * x[i];
    dist = SQRT(dist);
  } else dist = FABS(*x);

  if (radius>=0 && dist > radius) { *v=0.0; return; }
  FCTN(x, next, v);

  // printf("ts %f %f\n", x, v);
}

int checktruncsupport(cov_model *cov) {
  cov_model *next=cov->sub[0];
  int err,
    dim = cov->tsdim; // taken[MAX DIM],
 
  cov->maxdim=INFDIM;
  cov->monotone = isMonotone(next->monotone) ? MONOTONE : NOT_MONOTONE;

  if (cov->tsdim != cov->xdimown || cov->tsdim != cov->xdimprev)
    return ERRORDIM;

  if ((err = CHECK(next, dim, dim, ShapeType, cov->domown,
		     cov->isoown, SUBMODEL_DEP, cov->role)) != NOERROR) {
    //       print("error !!\n");
    return err;
  }

  next->delflag = DEL_COV - 20;
  setbackward(cov, next);

  return NOERROR;
}


void truncsupportInverse(double VARIABLE_IS_NOT_USED *x,
			 cov_model *cov, double *v){
  *v = P0(TRUNC_RADIUS);
}

void rangetruncsupport(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range) {
  range->min[TRUNC_RADIUS] = 0;      // < 0 : unset
  range->max[TRUNC_RADIUS] = RF_INF;
  range->pmin[TRUNC_RADIUS] = 0.00001;
  range->pmax[TRUNC_RADIUS] = 100.0;
  range->openmin[TRUNC_RADIUS] = true;
  range->openmax[TRUNC_RADIUS] = true;
}

int struct_truncsupport(cov_model *cov, cov_model **newmodel) {  
  int err;

  ASSERT_NEWMODEL_NOT_NULL;

  if (hasPoissonRole(cov) || hasMaxStableRole(cov)) {
    if ((err = addUnifModel(cov, P0(TRUNC_RADIUS), newmodel)) != NOERROR)
      return err;
  } else ILLEGAL_ROLE_STRUCT;
 

  switch (cov->role) {
  case ROLE_POISSON_GAUSS :
    BUG; // was denn das da?
    double invscale;
    addModel(newmodel, GAUSS);       
    addModel(newmodel, DOLLAR);
    kdefault(*newmodel, DSCALE, INVSQRTTWO);
    addModel(newmodel, TRUNCSUPPORT);
    InverseGauss(&GLOBAL.mpp.about_zero, cov, &invscale);
    kdefault(*newmodel, TRUNC_RADIUS, invscale);
    break;      
  case ROLE_POISSON : // optimierte density
    return addUnifModel(cov, 1.0, newmodel);
  case ROLE_MAXSTABLE :   
  case ROLE_SMITH : 
    return addUnifModel(cov, 1.0, newmodel);
  default : ILLEGAL_ROLE_STRUCT;
  }

  return NOERROR;
}

int init_truncsupport(cov_model *cov, gen_storage *s) {
  int i, err,
    vdim = cov->vdim[0];
  assert(cov->vdim[0] == cov->vdim[1]);

  if (cov->role == ROLE_BROWNRESNICK || cov->role == ROLE_SMITH ||
      cov->role == ROLE_SCHLATHER || cov->role == ROLE_POISSON
      || cov->role == ROLE_POISSON_GAUSS) {
    
    cov_model *next = cov->sub[0];
    //    double
    //      radius = P0(TRUNC_RADIUS); // default -1
    
    if ((err = INIT(next, cov->mpp.moments, s)) != NOERROR) return err;
    //   if (radius>=0 && radius < cov->mpp.refradius) cov->mpp.refradius = radius;
    
    // Eplus, M2 are assumed to be still precise !!
    for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = next->mpp.maxheights[i];

    return NOERROR;
  }

  else ILLEGAL_ROLE;
}


void do_truncsupport(cov_model *cov, gen_storage *s) {
  //  mppinfotype *info = &(s->mppinfo);
  cov_model *next = cov->sub[0];
  int i, 
    vdim = cov->vdim[0];
 //  double
  //    radius = P0(TRUNC_RADIUS); // default -1
  assert(cov->vdim[0] == cov->vdim[1]);
  
  DO(next, s);  
  for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = next->mpp.maxheights[i];

  // if (radius>=0 && radius < info->radius) info->radius = radius;
}





void tbm3(double *x, cov_model *cov, double *v, double tbmdim){
  cov_model *next = cov->sub[TBMOP_COV];
  int i,
    vdim = cov->vdim[0],
    vdimsq = vdim * vdim;
  assert(cov->vdim[0] == cov->vdim[1]);
   
  double v1[MAXTBMVDIM * MAXTBMVDIM];
  COV(x, next, v); // x has dim 2, if turning planes
  //   print(" cov=%4.4f %f %f %f %f\n ", x[0], v[0], v[1], v[2], v[3]);
  if (x[0] != 0.0) {

    //I(next);
    
    Abl1(x, next, v1);
    //  print(" D=%4.4f ", v1);
    for (i=0; i<vdimsq; i++) v[i] += *x * v1[i] / tbmdim;
  }
}

typedef struct TBM2_integr{
  cov_model *cov;
  double *x;
  // bool layers;
} TBM2_integr;

void TBM2NumIntegrFct(double *u,  int n, void *ex) {
  int i;
  double z[2];
  TBM2_integr *info;
  info = (TBM2_integr*) ex;
  cov_model *cov=info->cov;
  double *x = info->x;
    
  for (i=0; i<n; i++) {
    z[0] = x[0] * SQRT(1.0 - u[i] * u[i]);
    tbm3(z, cov, u + i, 1.0);
  }
}

void tbm2num(double *x, cov_model *cov, double *v){
  TBM2_integr info;
#define MAXSUBDIVISIONS 100
  static double a = 0, 
      b = 1, 
      eps = 1e-10; // 1e-15
  static int maxsubdivisions = MAXSUBDIVISIONS, 
      lenw = 4 * MAXSUBDIVISIONS;
  double abserr, work[4 * MAXSUBDIVISIONS];
  int subdivisions, integralevaluations, err, iwork[MAXSUBDIVISIONS];

  info.cov = cov; // a bit strange: this is tbm2, but will call tbm3...
  //                 in TBM2NumIntegrFct ...
  info.x = x;
  Rdqags(TBM2NumIntegrFct, (void *) &info, &a, &b, &eps, &eps,
	 v, &abserr, &integralevaluations, &err,
	 &maxsubdivisions, &lenw, &subdivisions, iwork, work);
}


void tbm(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[TBMOP_COV];
  int 
    fulldim = P0INT(TBMOP_FULLDIM),
    tbmdim = P0INT(TBMOP_TBMDIM);
    //vdimsq = cov->vdim * cov->vdim;
  //  print("x=%4.4f t=%4.4f ", x[0], x[1]); 

  if (cov->role != ROLE_COV) COV(x, next, v)

  else if (fulldim == tbmdim + 2) tbm3(x, cov, v, (double) tbmdim);

  else if (fulldim == 2 && tbmdim == 1) {
    if (CovList[next->nr].tbm2 != NULL) TBM2CALL(x, next, v)
    else tbm2num(x, cov, v);
  }

  else XERR(ERRORTBMCOMBI);

}

void Dtbm(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[TBMOP_COV];
  int i,
    fulldim = P0INT(TBMOP_FULLDIM),
    vdim = cov->vdim[0],
    vdimsq = vdim * vdim;
  assert(cov->vdim[0] == cov->vdim[1]);
  double v1[MAXTBMVDIM * MAXTBMVDIM],
    tbmdim = (double) P0INT(TBMOP_TBMDIM),
    f = 1.0 + 1.0 / tbmdim;

  BUG;// to do : Taylor-Entwicklung im Ursprung

  if (fulldim == tbmdim + 2) {
    Abl1(x, next, v); // x has dim 2, if turning planes
    Abl2(x, next, v1);
    for (i=0; i<vdimsq; i++) {
      v[i] = v[i] * f + *x * v1[i] / tbmdim;
    }
  }

  else XERR(ERRORTBMCOMBI);
}


int checktbmop(cov_model *cov) {
  cov_model *next=cov->sub[TBMOP_COV];
  tbm_param *gp  = &(GLOBAL.tbm);
  int err; 


  //  printf("here\n");

  kdefault(cov, TBMOP_FULLDIM, PisNULL(TBMOP_TBMDIM) || gp->tbmdim >= 0
	   ? gp->fulldim : P0INT(TBMOP_TBMDIM) - gp->tbmdim);
  kdefault(cov, TBMOP_TBMDIM, gp->tbmdim > 0 
	   ? gp->tbmdim : P0INT(TBMOP_FULLDIM) + gp->tbmdim);
  kdefault(cov, TBMOP_LAYERS, gp->layers);
  if ((err = checkkappas(cov)) != NOERROR)  return err;

  int 
    tbmdim = P0INT(TBMOP_TBMDIM),
    fulldim = P0INT(TBMOP_FULLDIM),
    vdim = cov->vdim[0];
  double
    storedlayer = P0(TBMOP_LAYERS);
  bool layers = !ISNAN(storedlayer) ? storedlayer :
    cov->xdimown == tbmdim + 1 && cov->isoown == SPACEISOTROPIC;
  if(cov->vdim[0] != cov->vdim[1]) BUG;
  if (tbmdim >= fulldim)
     SERR4("'%s' (=%d) must be less than '%s' (=%d)", 
	   KNAME(TBMOP_TBMDIM), tbmdim, KNAME(TBMOP_FULLDIM), fulldim);
  if (cov->tsdim > fulldim + layers) return ERRORWRONGDIM;
   //printf("%d %d %d %d\n",cov->xdimown,tbmdim,layers, cov->isoown);
  if (cov->xdimown > tbmdim + layers) {
    SERR("dimension of coordinates does not match reduced dimension of tbm");
  }

  if ((err = CHECK(next,  cov->tsdim, cov->xdimown, PosDefType, cov->domown,
		     cov->isoown, SUBMODEL_DEP, ROLE_COV)) != NOERROR) {
    return err;
  }

  if (next->pref[TBM] == PREF_NONE) return ERRORPREFNONE;

  if (cov->isoown != ISOTROPIC && cov->isoown != SPACEISOTROPIC) {
     return ERRORANISO;
  }
  if (!isVariogram(cov->typus) || cov->domown != XONLY) {
    return ERRORSTATVARIO;
  }


  cov->maxdim = 0;  
  setbackward(cov, next);
  cov->monotone=NOT_MONOTONE;
  cov->maxdim = fulldim + layers;  
  cov->rese_derivs = next->rese_derivs - 1;
  cov->finiterange = ((fulldim - tbmdim) % 2 == 0) && next->finiterange == true;

   if (vdim > MAXTBMVDIM) 
    SERR2("vdim (%d) exceeds max. value of vdim in tbm3 (%d)", vdim,MAXTBMVDIM);

  // only after being sure that the subsequent model does not cause
  // problems. So the time dimension should be fixed.
  // This is not absolutely safe programming, but should be OK.
  // But difficult to get around for MLE calls that do not allow 
  // for NAs values in integer variables.
  P(TBMOP_LAYERS)[0] = layers;

  return NOERROR;
}


void rangetbm_common(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range, bool tbmop ){ 
  int 
    TBMDIM = tbmop ? TBMOP_TBMDIM : TBM_TBMDIM,
    FULLDIM = tbmop ? TBMOP_FULLDIM : TBM_FULLDIM,
    LAYERS = tbmop ? TBMOP_LAYERS : TBM_LAYERS;
 
  range->min[FULLDIM] = 1.0;
  range->max[FULLDIM] = RF_INF;
  range->pmin[FULLDIM] = 1.0;
  range->pmax[FULLDIM] = 100;
  range->openmin[FULLDIM] = false;
  range->openmax[FULLDIM] = true;

  range->min[TBMDIM] = RF_NEGINF;
  range->max[TBMDIM] = RF_INF;
  range->pmin[TBMDIM] = RF_NEGINF;
  range->pmax[TBMDIM] = 100;
  range->openmin[TBMDIM] = false;
  range->openmax[TBMDIM] = true;

  range->min[LAYERS] = 0.0;
  range->max[LAYERS] = 1.0;
  range->pmin[LAYERS] = 0.0;
  range->pmax[LAYERS] = 1.0;
  range->openmin[LAYERS] = false;
  range->openmax[LAYERS] = false;
}

void rangetbmop(cov_model *cov, range_type *range){ 
  rangetbm_common(cov, range, true);
}





int set_stein_q(cov_model *next, double r, double d, double *q) {
  double C0, phi0, phi1, phi2, 
    zero = 0.0, 
    rP1 = r + 1.0,
    rM1 = r - 1.0, 
    dsq = d * d;

  COV(&zero, next, &C0);
  COV(&d, next, &phi0);
  Abl1(&d, next, &phi1); // derivative of
  phi1 *= d;             // scaled fctn at 1
  Abl2(&d, next, &phi2); // 2nd derivative of
  phi2 *= dsq;            // scaled fctn at 1
  q[LOCAL_R] =  r * d;   
  q[INTRINSIC_A2] = (phi2 - phi1) / (3.0 * r * rP1) ;
  q[INTRINSIC_B]  = (r == 1.0) ? 0.0 : q[INTRINSIC_A2] / (rM1 * dsq);
  q[INTRINSIC_A2] = (q[INTRINSIC_A2] - phi1 / 3.0 - phi2 / 6.0) / dsq;
  q[INTRINSIC_A0] = 0.5 * rM1 / rP1 * phi2 + phi1 / rP1 - phi0;

  //print("check: %f %e r=%f intr:%f %f  %f\n", 
  //      phi2, phi1, r, q[INTRINSIC_B],  q[INTRINSIC_A0],  q[INTRINSIC_A2]);


//   assert(false);
//   printf("%f %f %f+%f \n", q[INTRINSIC_B], q[INTRINSIC_A2], q[INTRINSIC_A0], C0);

  if ((q[INTRINSIC_B]  < 0.0) || (q[INTRINSIC_A2] < 0.0) ||
      (q[INTRINSIC_A0] + C0 < 0.0)) {
    
    //   printf("%f %f %f+%f \n", q[INTRINSIC_B], q[INTRINSIC_A2], q[INTRINSIC_A0], C0);
    return MSGLOCAL_INITINTRINSIC;
  }

  return NOERROR;
}



int set_cutoff_q2variate(cov_model *cov, double VARIABLE_IS_NOT_USED a,
			 double d, double  VARIABLE_IS_NOT_USED *q) {
    // auf modell ebene, d.h. co->sub[0] oder stein->sub[0]
  double aa, bb, cc,  //coefficients for cubic polynomial
    phi0[4], phi1[4], phi2[4], phi3[4], phi4[4],
    radius = MAXINT;
  double roots[3][2];
 int n, m, l;
 //printf("\n ------------ set_cutoff_q2variate ---------------- \n ");
 assert(cov->calling->SlocalCE != NULL);
 localCE_storage *s = cov->calling->SlocalCE;

    COV(&d, cov, phi0);
   // COV(&d,  cov->sub[0], &phi01);
    Abl1(&d, cov, phi1);
    Abl2(&d, cov, phi2);
    Abl3(&d, cov, phi3);
    Abl4(&d, cov, phi4);


/*
    printf("\n\n ---------------- \n\n");//
    printf("d = %f\n", d);//
    printf("phi0[0] = %f\n", phi0[0]);//
    printf("phi1[0] = %f\n", phi1[0]);//
    printf("phi2[0] = %f\n", phi2[0]);//
    printf("phi3[0] = %f\n", phi3[0]);//
    printf("phi4[0] = %f\n", phi4[0]);//
    printf("\n\n ---------------- \n\n");//
*/
    //bivariate Whittle  Cauchy Stable
    n = 5; m = 6; l = 7;

    //assert(false);
    s->is_bivariate_cutoff = true;

    if (cov->vdim[0] > 2 || cov->vdim[1] > 2) BUG;
    if (phi1[1] != phi1[2] || phi2[1] != phi2[2] || phi3[1] != phi3[2] || phi4[1] != phi4[2])
        return MSGLOCAL_NOTSYMMETRICMULTIVARIATE;

    for (int j = 0; j < 4; j++) {
         radius = MAXINT;
        s->q[j][CUTOFF_CUBE_N] = n;
        s->q[j][CUTOFF_CUBE_M] = m;
        s->q[j][CUTOFF_CUBE_L] = l;

        cubicsolver( phi4[j], (n+m+l-6)*phi3[j], (n*m + n*l + m*l - 3*(n+m+l) + 7)*phi2[j], (n-1)*(m-1)*(l-1)*phi1[j] ,
        roots);

        //find a positive root of the cubic polynomial
        for (int i = 0; i < 3; i++) {
            if (roots[i][1] == 0 && roots[i][0] < radius  && roots[i][0] >= 0) {
                radius= roots[i][0];
            }
        }

        //if no positive root, cannot do anything
        //printf("\n ::::::::::: set_cutoff_q2variate ::::::::::::: radius[%d] = %f, d = %f\n", j, radius + d, d);
        if ( radius  == MAXINT) return MSGLOCAL_NOPOSITIVEROOT;

        aa = -(phi3[j]*radius*radius + phi2[j]*(m + l - 3)*radius + phi1[j]*(m-1)*(l-1))/(n*(m - n)*(l - n) * POW(radius, n - 1));
        bb = -(phi3[j]*radius*radius + phi2[j]*(n + l - 3)*radius + phi1[j]*(n-1)*(l-1))/(m*(n - m)*(l - m) * POW(radius, m - 1));
        cc = -(phi3[j]*radius*radius + phi2[j]*(m + n - 3)*radius + phi1[j]*(m-1)*(n-1))/(l*(m - l)*(n - l) * POW(radius, l - 1));


        s->q[j][CUTOFF_CONSTANT] = - phi0[j] + aa * POW(radius, n) + bb * POW(radius, m) + cc * POW(radius, l);
        // printf("\n ::::::::::: set_cutoff_q2variate ::::::::::::: C[%d] = %f, \n", j,  s->q[j][CUTOFF_CONSTANT]);

   //     printf("\n ::::::::::::::: set_cutoff_q2variate::::::::::::::, cov->calling->SlocalCE->q[%d][CUTOFF_CONSTANT] = %f \n ",
     //          j, cov->calling->SlocalCE->q[j][CUTOFF_CONSTANT]);
        if (s->q[j][CUTOFF_CONSTANT] < -1) return MSGLOCAL_SIGNPHI;

        radius = radius + d;

        //j*5 is very bad
        s->q[j][CUTOFF_R] = radius;
        s->q[j][CUTOFF_CUBE_A] = aa;
        s->q[j][CUTOFF_CUBE_B] = bb;
        s->q[j][CUTOFF_CUBE_C] = cc;
    }

    if (s->q[1][CUTOFF_R] > s->q[0][CUTOFF_R] ||
            s->q[1][CUTOFF_R] > s->q[3][CUTOFF_R] ) return MSGLOCAL_WRONGRADII;
//PMI(cov);
    //printf("\n ::::::::::: set_cutoff_q2variate ::::::::::::: end \n");

      return NOERROR;
}



//TO DO: choose epsilon properly
#define EPSILON_C 0.1
int set_cutoff_q(cov_model *cov, double a, double d, double *q) {
    assert(cov->vdim[0] > 0 && cov->vdim[0] < 3);
    if (cov->vdim[0] > 1) return set_cutoff_q2variate(cov, a, d, q);

    // auf modell ebene, d.h. co->sub[0] oder stein->sub[0]
  double aa, bb, cc,  //coefficients for cubic polynomial
    phi0, phi1,
    phi2 = RF_NA,
    phi3 = RF_NA,
    phi4 = RF_NA,
    a2 = a * a,
    //   d2 = d*d,
    radius = -1;
  int n, m, l;


    //  if ((err = covcpy(&neu, cov)) != NOERROR) return err;
    // CovList[cov->gatternr].cov(&d, neu, &phi0);
    // CovList[cov->gatternr].D(&d, neu, &phi1);
    //FREE(neu);

    COV(&d, cov, &phi0);
    Abl1(&d, cov, &phi1);


    cov->calling->SlocalCE->is_bivariate_cutoff = FALSE;

    //If it is a variogram, then we must determine a constant such that Const - Variogram is a valied covariance
    // The constant depends on a and d
    //    cov->calling->typus

    if (cov->typus == VariogramType) {

        if (a == 0.5) {
            COV(&d, cov, &q[CUTOFF_CONSTANT]);
            q[CUTOFF_CONSTANT] = -q[CUTOFF_CONSTANT];


            q[CUTOFF_B] = -2*phi1*SQRT(d);
            q[CUTOFF_THEOR] =
	      POW(1.0 - 0.5* (q[CUTOFF_CONSTANT] + phi0) / phi1 /d, 1/a);
            q[LOCAL_R] = d * q[CUTOFF_THEOR];
            q[CUTOFF_ASQRTR] = POW(q[LOCAL_R], a);

	} else if (a == 1 ) {
            //second derivative
            Abl2(&d, cov, &phi2);
            if (phi2 <= 0) {
                return MSGLOCAL_SIGNPHISND;
            }
            //if second derivative is positive at d, then
            q[CUTOFF_CONSTANT] = -phi0+phi1*phi1/(2*phi2) + EPSILON_C;
            q[CUTOFF_B] = 0.25*phi1*phi1/(q[CUTOFF_CONSTANT] + phi0);
            q[CUTOFF_THEOR] = POW(1.0 - 2 * ( q[CUTOFF_CONSTANT] + phi0) / phi1 /d, 1/a);
            q[LOCAL_R] = d * q[CUTOFF_THEOR];
            q[CUTOFF_ASQRTR] = POW(q[LOCAL_R], a);

         } else if (a == CUTOFF_THIRD_CONDITION) {

            Abl2(&d, cov, &phi2);
            Abl3(&d, cov, &phi3);
            Abl4(&d, cov, &phi4);

            double roots[3][2];

            //de Wijsian model
            n = 4; m = 6; l = 7;

            cubicsolver( phi4, (n+m+l-6)*phi3, (n*m + n*l + m*l - 3*(n+m+l) + 7)*phi2, (n-1)*(m-1)*(l-1)*phi1 ,
            roots);


            //find a positive root of the cubic polynomial
            for (int i = 0; i < 3; i++) {
                if (roots[i][1] == 0 && roots[i][0] > radius ) {
                    radius = roots[i][0];
                }
            }
            //if no positive root, cannot do anything
            if (radius <= 0.0) return MSGLOCAL_NOPOSITIVEROOT;


            aa = -(phi3*radius*radius + phi2*(m + l - 3)*radius + phi1*(m-1)*(l-1))/(n*(m - n)*(l - n) * POW(radius, n - 1));
            bb = -(phi3*radius*radius + phi2*(n + l - 3)*radius + phi1*(n-1)*(l-1))/(m*(n - m)*(l - m) * POW(radius, m - 1));
            cc = -(phi3*radius*radius + phi2*(m + n - 3)*radius + phi1*(m-1)*(n-1))/(l*(m - l)*(n - l) * POW(radius, l - 1));




            q[CUTOFF_CONSTANT] = -phi0 + aa * POW(radius, n) + bb * POW(radius, m) + cc * POW(radius, l);

            if (q[CUTOFF_CONSTANT] <= 0.0) return MSGLOCAL_SIGNPHI;

            radius = radius + d;

            q[LOCAL_R] = radius;
            q[CUTOFF_ASQRTR] = radius;
            q[CUTOFF_CUBE_A] = aa;
            q[CUTOFF_CUBE_B] = bb;
            q[CUTOFF_CUBE_C] = cc;
            q[CUTOFF_CUBE_N] = n;
            q[CUTOFF_CUBE_M] = m;
            q[CUTOFF_CUBE_L] = l;

        } else BUG; // olga

    } else {

        //now we are in a positive definite function case
        if (phi0 <= 0.0) return MSGLOCAL_SIGNPHI;
        if (phi1 >= 0.0) return MSGLOCAL_SIGNPHIFST;



        //find  parameters of the method
        if (a != CUTOFF_THIRD_CONDITION) {
	    phi1 *= d;

            if (phi1 >= 0.0) return MSGLOCAL_SIGNPHIFST;

            q[CUTOFF_B] =//sound qconstant even if variance of submodel is not 1
	      POW(-phi1 / (2.0 * a2 * phi0), 2.0 * a) * phi0 / POW(d, 2.0 * a2);
            //  assert(false);
            q[CUTOFF_THEOR] = POW(1.0 - 2.0 * a2 * phi0 / phi1, 1.0 / a);
            q[LOCAL_R] = d * q[CUTOFF_THEOR];
            q[CUTOFF_ASQRTR] = POW(q[LOCAL_R], a);

        } else if (a == CUTOFF_THIRD_CONDITION){

            //a = CUTOFF_THIRD_CONDITION

            Abl2(&d, cov, &phi2);
            Abl3(&d, cov, &phi3);
            Abl4(&d, cov, &phi4);


            double roots[3][2];

            //Whittle  model
            n = 5; m = 6; l = 7;

            cubicsolver( phi4, (n+m+l-6)*phi3, (n*m + n*l + m*l - 3*(n+m-l) + 7)*phi2, (n-1)*(m-1)*(l-1)*phi1 ,
            roots);

            //find a positive root of the cubic polynomial
            for (int i = 0; i < 3; i++) {
                if (roots[i][1] == 0 && roots[i][0] > radius ) {
                    radius = roots[i][0];
                }
            }

            //if no positive root, cannot do anything
            if (radius <= 0.0) return MSGLOCAL_NOPOSITIVEROOT;


            aa = -(phi3*radius*radius + phi2*(m + l - 3)*radius + phi1*(m-1)*(l-1))/(n*(m - n)*(l - n) * POW(radius, n - 1));
            bb = -(phi3*radius*radius + phi2*(n + l - 3)*radius + phi1*(n-1)*(l-1))/(m*(n - m)*(l - m) * POW(radius, m - 1));
            cc = -(phi3*radius*radius + phi2*(m + n - 3)*radius + phi1*(m-1)*(n-1))/(l*(m - l)*(n - l) * POW(radius, l - 1));

            q[CUTOFF_CONSTANT] = 1 - phi0 + aa * POW(radius, n) + bb * POW(radius, m) + cc * POW(radius, l);

            if (q[CUTOFF_CONSTANT] <= 0.0) return MSGLOCAL_SIGNPHI;

            radius = radius + d;

            q[LOCAL_R] = radius;
            q[CUTOFF_CUBE_A] = aa;
            q[CUTOFF_CUBE_B] = bb;
            q[CUTOFF_CUBE_C] = cc;

            q[CUTOFF_CUBE_N] = n;
            q[CUTOFF_CUBE_M] = m;
            q[CUTOFF_CUBE_L] = l;


        } else
            BUG;
    }
    return NOERROR; // olga
}


int check_local(cov_model *cov,  
		 Methods method, int maxq, 
		 getlocalparam init, // next->coinit
		 set_local_q_type set_local) {
  location_type *loc = Loc(cov);
  int i, msg, 
    dim = cov->tsdim,
    err=NOERROR;
  //dim = cov->tsdim; // timespacedim -- needed ?;
  double  *q, q2[LOCAL_MAX],
    d=RF_NA;
  cov_model *next = cov->sub[0];
  localinfotype li;
  //ce_param *gp  = &(GLOBAL.localce); // ok

  //print("\n \n entering check local from %d:%s\n", cov->calling->nr,
  //CovList[cov->calling->nr].name);

  int vdim = SUBMODEL_DEP;

    if ((err = CHECK(next, dim,  1,
             method == CircEmbedCutoff ? PosDefType : VariogramType,
             cov->domown, cov->isoown, vdim, ROLE_COV)) != NOERROR) {
      //  print("\n \n \n ::: checl_local :: if :::  dim = %d --- next->vdim[0] = %d::: \n \n \n", dim, next->vdim[0]);
      if ( method != CircEmbedCutoff ||
     (err = CHECK(next, dim,  1,
                    VariogramType,
                   cov->domown, cov->isoown, vdim, ROLE_COV)) != NOERROR)
              return err;
  }
 
    if (vdim > 2) SERR("vdim of submodel must be less than 3")


  // no setbackward ?!
  setbackward(cov, next); 
  if (next->pref[method] == PREF_NONE) return ERRORPREFNONE;

  if (init == NULL){
    return ERRORUNKNOWNMETHOD;	  
  }

  // no changing in pref by submodel !!
  if (cov->q != NULL) {
    FREE(cov->q);    
    cov->q = NULL;
    // ERR("q not NULL in check_local -- ask author");
  } 
  QALLOC(maxq);
  q = cov->q;
  for (i = 0; i < maxq; i++) q2[i] = RF_NA; // q2 will be copied to cov->q;
  //  {int i; print("%d %f\n", cov->qlen); for(i=0;i<9;i++)print("%f ", cov->q[i]); print("\n");  assert(false);}
  
  
  if (PisNULL(pLOC_DIAM)) {  
    double 
      diameter = GetDiameter(loc);
    if (PL>=PL_DETAILS) { LPRINT("diameter %f\n", diameter); }
    kdefault(cov, pLOC_DIAM, diameter);
  } else {
    d = P0(pLOC_DIAM);
  }

  if (PisNULL(pLOC_A)) {
    if (CovList[next->nr].implemented[method]) {
      assert(init != NULL);

      init(next, &li);
      if (li.instances == 0) {
	SERR("parameter values do not allow for finding second parameter");
      }
      q[LOCAL_R] = RF_INF;

      // print("%f\n", d); assert(false);

      msg = MSGLOCAL_FAILED;
      for (i=0; i<li.instances; i++) {
	// 	print("li %d %d %d %f %d\n", i, li.instances, li.msg[i], li.value[i],  msg);
	if (li.msg[i] <= msg) {
	  //	
	  err = set_local(next, li.value[i], d, q2);
	  //
	  if (err == NOERROR && (li.msg[i] < msg || q2[LOCAL_R] < q[LOCAL_R])){
	    MEMCOPY(q, q2, sizeof(double) * maxq);
	    msg = li.msg[i];
	    if (PisNULL(pLOC_A)) {
	      PALLOC(pLOC_A, 1, 1); 
 	    }
	    P(pLOC_A)[0] = li.value[i]; // co: a; ie: rawr
	  }
	}
      }
      cov->q[LOCAL_MSG] = msg;

      if (msg == MSGLOCAL_FAILED) SERR2("'%s' did not work out correctly. Likely, invalid parameter values have been chosen for '%s'", NICK(cov), NICK(next));
    } else {
      SERR("2nd parameter is neither given nor can be found automatically");
    }

  } else {
    if (cov->ncol[pLOC_A] != 1 || cov->nrow[pLOC_A] != 1) 
      SERR1("'%s' must be a scale", KNAME(pLOC_A));
    err = set_local(next, P0(pLOC_A), d, q2);
    MEMCOPY(q, q2, sizeof(double) * maxq);
  }

  cov->pref[CircEmbed] = 5;

  return err;
}


void co(double *x, cov_model *cov, double *v) {
  cov_model *next = cov->sub[0];
  double  y=*x, *q=cov->q,
    diameter = P0(pLOC_DIAM),  
    a = P0(pLOC_A);
 
  assert(cov->role == ROLE_COV);

  //cov->vdim[0] or next->vdim[0]?
  //if ( cov->SlocalCE->is_multivariate_cutoff == true ) {

  if ( cov->SlocalCE->is_bivariate_cutoff  ) {

      if (y <= diameter) {
                COV(x, next, v);
                for (int i = 0; i <4; i++) {
                    v[i] += cov->SlocalCE->q[i][CUTOFF_CONSTANT];
                }
        }
      else {
          for (int i = 0; i < 4; i++) {
               v[i] = (y >= cov->SlocalCE->q[i][CUTOFF_R]) ? 0.0
                                       :  cov->SlocalCE->q[i][CUTOFF_CUBE_A]*
                                                            POW((cov->SlocalCE->q[i][CUTOFF_R] - y), cov->SlocalCE->q[i][CUTOFF_CUBE_N]) +
                                          cov->SlocalCE->q[i][CUTOFF_CUBE_B]*
                                                            POW((cov->SlocalCE->q[i][CUTOFF_R] - y), cov->SlocalCE->q[i][CUTOFF_CUBE_M]) +
                                             cov->SlocalCE->q[i][CUTOFF_CUBE_C]*
                                                            POW((cov->SlocalCE->q[i][CUTOFF_R] - y), cov->SlocalCE->q[i][CUTOFF_CUBE_L]);


          }
      }

  } else {
      if (y <= diameter) {
            //If it is a variogram (actually -variogram), add a constant. If it is positive definite, go below
            if (cov->sub[0]->typus == VariogramType ) {
                COV(x, next, v);
                *v = *v+q[CUTOFF_CONSTANT];
            } else
            {
                COV(x, next, v)
            }

        }

        else {
            if (a != CUTOFF_THIRD_CONDITION) {
                *v = (y >= q[LOCAL_R]) ? 0.0
                                       : q[CUTOFF_B] * POW(q[CUTOFF_ASQRTR] - POW(y, a), 2.0 * a);
            }
            else {
                *v = (y >= q[LOCAL_R]) ? 0.0
                                       :  q[CUTOFF_CUBE_A] * POW((q[LOCAL_R] - y), q[CUTOFF_CUBE_N]) +
                                          q[CUTOFF_CUBE_B] * POW((q[LOCAL_R] - y), q[CUTOFF_CUBE_M]) +
                                             q[CUTOFF_CUBE_C] * POW((q[LOCAL_R] - y), q[CUTOFF_CUBE_L]);
                //BUG;
            }
        }

  }


}
int check_co(cov_model *cov) {
  cov_model *next = cov->sub[0];
  int err;

  NEW_STORAGE(localCE);
  assert(cov->SlocalCE != NULL);
  // 1 ? 2? More than 2? or inside set_cutoff_q2variate

  //PMI(cov);assert(next->vdim[0]>0);

  //  printf("\n ----------- check_co ----------- \n ");
  // printf("\n ----------- next->vdim[0] = %d ----------- \n ", next->vdim[0]);
  err = check_local(cov, CircEmbedCutoff, CUTOFF_MAX,
                                         CovList[next->nr].coinit, set_cutoff_q);
  //printf("\n\n ------ check_co = %d ------ \n\n", next->SlocalCE->is_bivariate_cutoff);
  // printf("\n\n ------ check_co err = %d ------ \n\n", err);
  if (err != NOERROR ) return err;


  return NOERROR;
/*  return check_local(cov, CircEmbedCutoff, CUTOFF_MAX,
		     CovList[next->nr].coinit,
		     set_cutoff_q);
                         */
}


bool alternativeparam_co(cov_model VARIABLE_IS_NOT_USED *cov){
  return false;
}

void range_co(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){

  range->min[pLOC_DIAM] = 0.0; //  CUTOFF_DIAM
  range->max[pLOC_DIAM] = RF_INF;
  range->pmin[pLOC_DIAM] = 1e-10;
  range->pmax[pLOC_DIAM] = 1e10;
  range->openmin[pLOC_DIAM] = true;
  range->openmax[pLOC_DIAM] = true;

  range->min[pLOC_A] = 0.0; // cutoff_a
  range->max[pLOC_A] = RF_INF;
  range->pmin[pLOC_A] = 0.5;
  range->pmax[pLOC_A] = 2.0;
  range->openmin[pLOC_A] = true;
  range->openmax[pLOC_A] = true;
}


void Stein(double *x, cov_model *cov, double *v) {
  cov_model *next = cov->sub[0];
  double y=*x, z, *q=cov->q,
    diameter = P0(pLOC_DIAM);

  // printf("diameter %f\n", diameter);

  assert(cov->role == ROLE_COV);

  if (y <= diameter) {
    COV(x, next, v);
    *v += q[INTRINSIC_A0] + q[INTRINSIC_A2] * y * y;

    //       print("%f %f %f %f %d %d \n ", 
    //	y, v[0], q[INTRINSIC_A0], q[INTRINSIC_A2], INTRINSIC_A0, INTRINSIC_A2);
  } else {
     z = q[LOCAL_R] - y;
     *v = (z <= 0.0) ? 0.0 : q[INTRINSIC_B] * z * z * z / y;
  }
}

int check_Stein(cov_model *cov)
{  
  cov_model *next = cov->sub[0]; // dito
  return check_local(cov, CircEmbedIntrinsic, INTRINSIC_MAX, 
		     CovList[next->nr].ieinit,
		     set_stein_q);
}  

bool alternativeparam_Stein(cov_model *cov) {
  P(pLOC_A)[0] *= 2.0;
  return true;
}

void range_Stein(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range) {
  range->min[pLOC_DIAM] = 0.0; 
  range->max[pLOC_DIAM] = RF_INF;
  range->pmin[pLOC_DIAM] = 0.01;
  range->pmax[pLOC_DIAM] = 100;
  range->openmin[pLOC_DIAM] = true;
  range->openmax[pLOC_DIAM] = true;

  range->min[pLOC_R] = 1.0; // stein_r
  range->max[pLOC_R] = RF_INF;
  range->pmin[pLOC_R] = 1.0;
  range->pmax[pLOC_R] = 20.0;
  range->openmin[pLOC_R] = false;
  range->openmax[pLOC_R] = true;
}



void strokorb(double *x, cov_model *cov, double *v) {
  // BrownResnick to strokorb Gaussian
  cov_model *next = cov->sub[0];
  int dim = cov->tsdim;
  double u;
      int idx = 0;
  
  u = 2 * *x;
  switch (dim) {
  case 1 :
    Abl1(&u, next, v);
    *v = - *v;
    //printf("u=%f %e\n", u, *v);
    break;
  case 3 :
     if (*x == 0.0) {
      while (idx < next->taylorN && (next->taylor[idx][TaylorPow] ==0.0 || 
				     next->taylor[idx][TaylorPow] ==1.0)) idx++;
      if (idx >= next->taylorN) BUG;
      double p = next->taylor[idx][TaylorPow];
      if (p > 3.0) BUG;
      // > 3 ist mathematisch vermutlich nicht moeglich, da bei strokorb
      // das submodel die entwicklung 1 - c x (oder rauher) hat
      *v = p < 3.0 ? RF_INF 
	: next->taylor[idx][TaylorConst] * p * (p - 1) * POW(2, p-2)/ M_PI;//3.0
    } else {
      Abl2(&u, next, v);
      *v /= (M_PI * *x);
    }
    break;
  default: BUG;
  }

  if (*v<0) {
     BUG;
  }
  
  assert(*v >= 0);
  assert(*x >= 0);
  //  assert(false);
}


int checkstrokorb(cov_model *cov) {
  cov_model *next = cov->sub[0];
  int 
    dim = cov->tsdim,
    err = NOERROR;

  if ((err = CHECK(next, cov->tsdim, cov->xdimprev, TcfType,
		     cov->domown, cov->isoown,
		     SCALAR, ROLE_COV)) != NOERROR) return err;
  if (!next->deterministic) SERR("only deterministic submodels allowed"); 

  if (!isGneiting(next)) 
    SERR("member of the Gneiting-Schaback class as submodel needed");
  switch(dim) {
  case 1: 
    if (next->rese_derivs < 1) SERR("submodel must be once differentiable");
    break;
  case 3: 
    if (next->rese_derivs < 2) SERR("submodel must be twice differentiable");
    break;
  default:
    SERR("only dimensions 1 and 3 are allowed");
  }

  if (!(hasMaxStableRole(cov) || hasNoRole(cov) || hasDistrRole(cov))) {
    SERR1("'%s' may be used only as a shape function with max-stable field simulation", NICK(cov));
  }

  if (next->tailN < 1)
    SERR2("%d members of the Taylor expansion at infinity of '%s', but at least order 1 required.", next->tailN, NICK(next));


  cov->taylorN = cov->tailN = 1;
  cov->tail[0][TaylorExpConst] = next->tail[0][TaylorExpConst];
  cov->tail[0][TaylorExpPow] = next->tail[0][TaylorExpPow];
  
  switch(dim) {
  case 1 : 
    cov->taylor[0][TaylorConst] =
      -next->taylor[1][TaylorConst] * next->taylor[1][TaylorPow];
    cov->taylor[0][TaylorPow] = next->taylor[1][TaylorPow] - 1.0;
    
    if (next->tail[0][TaylorExpPow] != 0.0) {
      if (next->tail[0][TaylorExpConst] == 0.0) BUG;
      cov->tail[0][TaylorConst] = next->tail[0][TaylorConst]
	* next->tail[0][TaylorExpConst] * next->tail[0][TaylorExpPow];
      cov->tail[0][TaylorPow] = 
	next->tail[0][TaylorPow] + next->tail[0][TaylorExpPow] - 1;
    } else {
      if (next->tail[0][TaylorExpConst] != 0.0) BUG;
      if (next->tail[0][TaylorPow] >= 0) {
	if (next->tail[0][TaylorConst] == 0) SERR("trivial submodel");
	SERR1("'%s' is not integrable", NICK(next));
      }
      cov->tail[0][TaylorConst] = 
	- next->tail[0][TaylorConst] * next->tail[0][TaylorPow];
      cov->tail[0][TaylorPow] = next->tail[0][TaylorPow] - 1.0;
    }
    break;
  case 3 : 
    int idx;
    idx = 1;
    
    if (next->taylorN <= 1)
      SERR2("%d members of the Taylor expansion of '%s' known, but at least 2 members required.", next->taylorN, NICK(next));
    
    if (next->taylor[idx][TaylorPow] == 1.0) {	
      if (next->taylorN <= idx + 1)
	SERR3("%d members of the Taylor expansion of '%s' known, but at least %d members required.", next->taylorN, NICK(next), idx + 1);
	idx++;
    } else assert(next->taylor[idx][TaylorPow] < 1.0);
    
    cov->taylor[0][TaylorPow] = (next->taylor[idx][TaylorPow] - 2.0) - 1.0;
    cov->taylor[0][TaylorConst] = next->taylor[idx][TaylorConst] / M_PI * 
      next->taylor[idx][TaylorPow] * (next->taylor[idx][TaylorPow] - 1) *
      POW(2.0,  cov->taylor[0][TaylorPow]); // 
    

    if (next->tail[0][TaylorExpPow] != 0.0) {
      if (next->tail[0][TaylorExpConst] == 0.0) BUG;
      // Achtung ! Reihenfolge der Berechnung!
      double f = next->tail[0][TaylorExpConst] * next->tail[0][TaylorExpPow];
      cov->tail[0][TaylorPow] = next->tail[0][TaylorPow] + 
	 2.0 * (next->tail[0][TaylorExpPow] - 1.0);
      cov->tail[0][TaylorConst]= next->tail[0][TaylorConst] * f * f *
	POW(2.0, cov->tail[0][TaylorPow]);
      cov->tail[0][TaylorPow] -= 1.0;
      cov->tail[0][TaylorExpConst] *= POW(2.0, cov->tail[0][TaylorExpPow]);
    } else {
      // Achtung ! Reihenfolge der Berechnung!
      if (next->tail[0][TaylorExpConst] != 0.0) BUG;
      cov->tail[0][TaylorPow] = next->tail[0][TaylorPow] - 2.0;      
      cov->tail[0][TaylorConst] = next->tail[0][TaylorConst] / M_PI
	* next->tail[0][TaylorPow] * (next->tail[0][TaylorPow] - 1.0) 
	* POW(2.0, next->tail[0][TaylorPow] - 2.0);
      cov->tail[0][TaylorPow] -= 1.0;
    }
    break;
  default: BUG;
  }

  setbackward(cov, next);

  return NOERROR;
}



int init_strokorb(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  if (cov->role == ROLE_MAXSTABLE || hasNoRole(cov) || hasDistrRole(cov)) {
    
    //cov_model *next = cov->sub[0];
    //    double
    //      radius = P(TRUNC_RADIUS)[0]; // default -1
    
    // if ((err = INIT(next, 0, s)) != NOERROR) return err;

    //if (radius>=0 && radius < cov->mpp.refradius) cov->mpp.refradius = radius;
    // Eplus, M2 are assumed to be still precise !!
    
    assert(cov->vdim[0] == 1 && cov->vdim[1] == 1);
    cov->mpp.maxheights[0] = RF_NA;
    
  }
  
  else ILLEGAL_ROLE;

  /*
    if ((err = CHECK(cov->sub[0], cov->tsdim, cov->xdimprev, 
		   ShapeType, // 
		   cov->domown, cov->isoown,
		   SCALAR, 
		   ROLE_MAXSTABLE//  otherwise do will fail
		   )) != NOERROR) return err;
  */

  cov->mpp.maxheights[0] = 1.0; // all with be covered by pgs
  if (cov->mpp.moments >= 1) {
    cov->mpp.mM[1] = cov->mpp.mMplus[1] = 1;     
  }

  return NOERROR;
}


void do_strokorb(cov_model VARIABLE_IS_NOT_USED *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  BUG;
}


////////////////// strokorbBall

#define STROKORBBALL_DIM 0 // Inner
int checkstrokorbBall(cov_model *cov) {
  cov_model
    //*key = cov->key,
    *next = cov->sub[0];
  int 
    dim = cov->tsdim,
    err = NOERROR;

  assert(cov->key == NULL);
  if ((err = CHECK(next, dim, cov->xdimprev, TcfType,
		   cov->domown, cov->isoown,
		   SCALAR, ROLE_COV)) != NOERROR) return err;
  if (!isGneiting(next)) 
    SERR("member of the Gneiting-Schaback class as submodel needed");

  switch(dim) {
  case 1: 
    if (next->rese_derivs < 2) SERR("submodel must be twice differentiable");
    break;
  case 3: 
    if (next->rese_derivs < 3) 
      SERR("submodel must be three times differentiable");
    break;
  default:
    SERR("only dimensions 1 and 3 are allowed");
  }
  
  if (!(hasMaxStableRole(cov) || hasNoRole(cov) || hasDistrRole(cov)))
    SERR1("'%s' may be used only as a shape function with max-stable field simulation", NICK(cov));
  
  if (next->tailN < 1)
    SERR2("%d members of the Taylor expansion at infinity of '%s' found, but at least 1 is required.", next->tailN, NICK(next));
  
  if (next->taylorN < 2)
    SERR2("%d members of the Taylor expansion of '%s' found, but at least 2 is required.", next->taylorN, NICK(next));

  setbackward(cov, next);

  return NOERROR;
}


void ScaleToVar(cov_model *local, cov_model *remote, 
		int VARIABLE_IS_NOT_USED variant) {
  // ACHTUNG!! from and to sind hier vertauscht!! 
  assert(local->nr==POWER_DOLLAR && remote->nr==LOC);
  //  int dim = local->tsdim;
  double scale = PARAM0(local, POWSCALE);
  // scale im entfernten model setzen
  PARAM(remote, LOC_SCALE)[0] = scale;
  remote->deterministic = false;
}
  

    
int struct_strokorbBall(cov_model *cov, cov_model **newmodel) {  
  int  err,
    dim = cov->tsdim;
  
  ASSERT_NEWMODEL_NOT_NULL;
  
  if (cov->role == ROLE_MAXSTABLE) {
    addModel(newmodel, BALL, cov);    
    addModel(newmodel, POWER_DOLLAR);
    kdefault(*newmodel, POWSCALE, 1.0);    
    kdefault(*newmodel, POWPOWER, -dim);    
    kdefault(*newmodel, POWVAR,  1.0 / VolumeBall(dim, BALL_RADIUS));    
    cov_model *pts=NULL, *scale=NULL;
    if ((err = covCpy(&pts, *newmodel)) != NOERROR) return err;
    

    if (CovList[cov->nr].kappas < 2) {
      if ((err = covCpy(&scale, cov)) != NOERROR) return err;
      // !! inverse scale gegenueber paper
      scale->nr = STROKORB_BALL_INNER;
      kdefault(scale, STROKORBBALL_DIM, dim);
      addModel(&scale, RECTANGULAR, *newmodel); 
      kdefault(scale, RECT_APPROX, false);
      kdefault(scale, RECT_ONESIDED, true);
      (*newmodel)->kappasub[POWSCALE] = scale;
    } else { // for testing only
      addModelKappa(*newmodel, POWSCALE, UNIF); 
      kdefault((*newmodel)->kappasub[POWSCALE], UNIF_MIN, P0(0));
      kdefault((*newmodel)->kappasub[POWSCALE], UNIF_MAX, P0(1));
    }
      
    addModel(&pts, RECTANGULAR);
    addModel(&pts, LOC);
    kdefault(pts, LOC_SCALE, 1.0);
    kdefault(pts, LOC_POWER, -dim);
    addModelKappa(pts, LOC_SCALE, NULL_MODEL); 
    kdefault(pts->kappasub[LOC_SCALE], NULL_TYPE, RandomType);

    addSetParam(newmodel, pts, ScaleToVar, true, 0);
    addModel(newmodel, PTS_GIVEN_SHAPE); // to do : unif better ?!
    (*newmodel)->sub[PGS_LOC] = pts;
    pts->calling = *newmodel;

  } else ILLEGAL_ROLE_STRUCT;
  
  return NOERROR;
}


void rangestrokorbball(cov_model  VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[0] = RF_NEGINF; 
  range->max[0] = RF_INF;
  range->pmin[0] = -4.0;
  range->pmax[0] = 4.0;
  range->openmin[0] = false;
  range->openmax[0] = false;

  range->min[1] = RF_NEGINF; 
  range->max[1] = RF_INF;
  range->pmin[1] = -4.0;
  range->pmax[1] = 4.0;
  range->openmin[1] = false;
  range->openmax[1] = false;
}
/*
int init_strokorbBall(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  if (cov->role == ROLE_MAXSTABLE) {
    
    //cov_model *next = cov->sub[0];
    //    double
    //      radius = P(TRUNC_RADIUS)[0]; // default -1
    
    // if ((err = INIT(next, 0, s)) != NOERROR) return err;

    //if (radius>=0 && radius < cov->mpp.refradius) cov->mpp.refradius = radius;
    // Eplus, M2 are assumed to be still precise !!
    
    cov->mpp.maxheight = RF_NA;
    
  }
  
  else ILLEGAL_ROLE;

  cov->mpp.maxheight = 1.0; // all with be covered by pgs
  if (cov->mpp.moments >= 1) {
    cov->mpp.mM[1] = cov->mpp.mMplus[1] = 1;     
  }

  return NOERROR;
}


void do_strokorbBall(cov_model VARIABLE_IS_NOT_USED *cov, gen_storage VA// RIABLE_IS_NOT_USED *s) {
//   assert(false);
// }

// */



void strokorbBallInner(double *x, cov_model *cov, double *v) {
  // only proportional to a density !!!
  cov_model *next = cov->sub[0];

  int dim = cov->nr != STROKORB_BALL_INNER || PisNULL(STROKORBBALL_DIM) 
    ? cov->tsdim : P0INT(STROKORBBALL_DIM);
  if (*x <= 0) {
    *v = 0.0;
    return;
  }

  double y = 2 * *x;
  switch (dim) {
  case 1 :
    // \chi''(s)*s [ diameter ] -> 4\chi''(2 s)*s
    Abl2(&y, next, v);
    *v *= 2.0 * y;
    //printf("u=%f %e\n", u, *v);
    break;
  case 3 :
    double w;
    //[chi''(s)-chi'''(s) * s] * s/3
    //printf("y=%f\n", y);
    Abl2(&y, next, v);
    Abl3(&y, next, &w);
    *v = 2.0 * (*v - w * y) * y / 3.0;
    break;
  default: BUG;
  }
  if (*v<0) {
     BUG;
  }  
  //  assert(*v >= 0);  
}


int check_strokorbBallInner(cov_model *cov) {
  cov_model *next = cov->sub[0];
  int err;
   
  ROLE_ASSERT(ROLE_DISTR);
  
  if ((err = checkkappas(cov)) != NOERROR) return err;
  if (cov->tsdim != 1) SERR("only dimension 1 allowed");
  if ((err = checkstrokorbBall(cov)) != NOERROR) return err;
  switch(P0INT(STROKORBBALL_DIM)) {
  case 1: 
    if (next->rese_derivs < 2) SERR("submodel must be twice differentiable");
    break;
  case 3: 
    if (next->rese_derivs < 3) 
      SERR("submodel must be three times differentiable");
    break;
  default:
    SERR("only dimensions 1 and 3 are allowed");
  }

  if (next->tailN < 1 || next->taylorN < 2) 
    SERR1("taylor expansions of '%s' not programmed yet", NICK(next));
  double tep = next->tail[0][TaylorExpPow],
    tp = next->tail[0][TaylorPow];
 
  cov->taylorN = cov->tailN = 1;
  cov->tail[0][TaylorExpConst] = POW(2.0, tep) * next->tail[0][TaylorExpConst];
  cov->tail[0][TaylorExpPow] = tep;
  
  int idx = 1;
  if (next->taylor[1][TaylorPow] == (int) next->taylor[1][TaylorPow]) {
    assert(next->taylor[1][TaylorPow] == 1.0);
    // 2 sollte nie auftreten, laeuft aber gleich
    if (next->taylorN >= 3) idx++;
    else SERR1("%s does not have a long enough taylor development programmed", 
	      NICK(next));
  }
  double TP = next->taylor[idx][TaylorPow];

  switch(P0INT(STROKORBBALL_DIM)) {
  case 1 :
    if (tep != 0.0) {
      cov->tail[0][TaylorPow] = tp + 2.0 * (tep - 1.0) + 1.0;// !! +1, da noch ein x drauf-multipliziert wird
      cov->tail[0][TaylorConst] = next->tail[0][TaylorExpConst] * tep;
      cov->tail[0][TaylorConst] *= cov->tail[0][TaylorConst];
     } else {
      cov->tail[0][TaylorConst] = tp * (tp -1.0);
      cov->tail[0][TaylorPow] = tp - 1.0; // !! nicht -2.0, da noch ein x drauf-multipliziert wird
    }
         
    cov->taylor[0][TaylorConst] = TP * (TP - 1.0);
    cov->taylor[0][TaylorPow] = TP - 1.0;
    break;
  case 3 : 
    if (tep != 0.0) {
      double dummy = next->tail[0][TaylorExpConst] * tep;
      cov->tail[0][TaylorConst] = dummy * dummy * dummy / 3.0;
      cov->tail[0][TaylorPow] = tp + 3 * tep - 1.0;
    } else {
      cov->tail[0][TaylorConst] = tp * (tp - 1.0) * (3.0 - tp) / 3.0;
      cov->tail[0][TaylorPow] = tp - 1.0; // !! nicht -2.0, da noch ein x drauf-multipliziert wird
    }
        
    cov->taylor[0][TaylorConst] = TP * (TP - 1.0) * (3.0 - TP) / 3.0;
    cov->taylor[0][TaylorPow] = TP - 2.0;
    break;
  default: BUG;
  }

  cov->tail[0][TaylorConst] *= 2.0 * next->tail[0][TaylorConst] * 
    POW(2.0, cov->tail[0][TaylorPow]);
  cov->taylor[0][TaylorConst] *= 2.0 * next->taylor[idx][TaylorConst] * 
    POW(2.0, cov->taylor[0][TaylorPow]);

  return NOERROR;
}

int init_strokorbBallInner(cov_model VARIABLE_IS_NOT_USED *cov, gen_storage VARIABLE_IS_NOT_USED *s) {
  // 
  cov_model *next = cov->sub[0];
  //  int err,
  //   dim = cov->tsdim;

  //printf("ball inner\n");

  /* if ((err = CHECK(cov->sub[0], dim, cov->xdimprev, 
		   ShapeType,
		   cov->domown, cov->isoown,
		   SCALAR, 
		   ROLE_MAXSTABLE
		   )) != NOERROR) return err;
  */
  if (!next->deterministic) SERR("only deterministic submodels allowed"); // u.a. Taylor fehlt

  assert(cov->vdim[0] == 1 && cov->vdim[1] == 1);
  cov->mpp.maxheights[0] = 1.0; 
  cov->mpp.mM[0] = cov->mpp.mMplus[0] = 1;     
  if (cov->mpp.moments >= 1) {
    cov->mpp.mM[1] = cov->mpp.mMplus[1] = 1;     
  }

  return NOERROR;
}

void do_strokorbBallInner(cov_model VARIABLE_IS_NOT_USED *cov, gen_storage VARIABLE_IS_NOT_USED *s) {  
  //cov_model *next = cov->sub[0];  
  //DO(next, s); // nicht gatternr
}


void range_strokorbBallInner(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[STROKORBBALL_DIM] = 1; 
  range->max[STROKORBBALL_DIM] = 3;
  range->pmin[STROKORBBALL_DIM] = 1;
  range->pmax[STROKORBBALL_DIM] = 3;
  range->openmin[STROKORBBALL_DIM] = false;
  range->openmax[STROKORBBALL_DIM] = false;
}



void strokorbPoly(double *x, cov_model *cov, double *v) {
  // only proportional to a density !!!
  cov_model *next = cov->sub[0];
  COV(x, next, v);
}


int checkstrokorbPoly(cov_model *cov) {
  cov_model
    //    *key = cov->key,
    *next = cov->sub[0];
  int 
    dim = cov->tsdim,
    err = NOERROR;

  assert(cov->key == NULL);
  if ((err = CHECK(next, dim, cov->xdimprev, TcfType,
		   cov->domown, cov->isoown,
		   SCALAR, ROLE_COV)) != NOERROR) return err;
  if (!isGneiting(next)) 
    SERR("member of the Gneiting-Schaback class as submodel needed");
  
  if (dim != 2) SERR("only dimension 2 currently programmed");
  if (!(hasMaxStableRole(cov) || hasNoRole(cov))) {
    SERR1("'%s' may be used only as a shape function with max-stable field simulation", NICK(cov));
  }

  setbackward(cov, next);

  return NOERROR;
}



void poly2unif(cov_model *local, cov_model *remote, 
		int VARIABLE_IS_NOT_USED variant) {
  assert(local->nr==POLYGON && remote->nr==UNIF && local->Spolygon != NULL &&
	 local->Spolygon->P != NULL);
  polygon *P = local->Spolygon->P;
  assert(P->e != NULL);
  int d,
    dim = local->tsdim;
  assert(dim == 2);

  for (d=0; d<dim; d++) {
    //printf("poly2unif %d %f %f %ld\n", d, P->box0[d],  P->box1[d], (long int) P);
    PARAM(remote, UNIF_MIN)[d] = P->box0[d];
    PARAM(remote, UNIF_MAX)[d] = P->box1[d];
  }
  remote->deterministic = false;
}
  


int struct_strokorbPoly(cov_model *cov, cov_model **newmodel) {  
  cov_model *sub = cov->sub[0];
  int 
    dim = cov->tsdim;
  double var = 1.0;
  cov_model *pts=NULL, *shape=NULL;

  ASSERT_NEWMODEL_NOT_NULL;
    
  if (cov->role == ROLE_MAXSTABLE) {
    if (sub->nr != BROWNRESNICK) 
      SERR1("only tcf '%s' allowed", CovList[BROWNRESNICK].nick);
    sub = sub->sub[0];
    if (isDollar(sub)) {
      var = PARAM0(sub, DVAR);    
      sub = sub->sub[0];
    }
    if (sub->nr != BROWNIAN || PARAM0(sub, BROWN_ALPHA) != 1.0) {
      SERR2("Numerical inverse Laplace transform has not been implemented yet. Currently, only '%s' with parameter %s=1 is a valid submodel", CovList[BROWNIAN].nick,
	    CovList[BROWNIAN].kappanames[BROWN_ALPHA]);
    }
    
    addModel(&pts, UNIF, NULL, true);
    kdefault(pts, UNIF_NORMED, (int) false);
    PARAMALLOC(pts, UNIF_MIN, dim, 1);
    PARAMALLOC(pts, UNIF_MAX, dim, 1);
   
   
    addModel(&shape, POLYGON, NULL, true);
    addModelKappa(shape, POLYGON_BETA, ARCSQRT_DISTR);
    kdefault(shape->kappasub[POLYGON_BETA], ARCSQRT_SCALE, 1.0 / var);
    addSetParam(&shape, pts, poly2unif, true, 0);
   
    addModel(newmodel, PTS_GIVEN_SHAPE);
    kdefault(*newmodel, PGS_NORMED, false);
    kdefault(*newmodel, PGS_ISOTROPIC, false);
    shape->calling = *newmodel;
    pts->calling = *newmodel;
    (*newmodel)->sub[PGS_LOC] = pts;
    (*newmodel)->sub[PGS_FCT] = shape;

  } else ILLEGAL_ROLE_STRUCT;

  return NOERROR;
}




void mult_inverse(double *x, cov_model *cov, double *v) {
  cov_model *next = cov->sub[0];
  FCTN(x, next, v);
  *v = 1.0 / *v;
}


void mult_inverseNonstat(double *x, double *y, cov_model *cov, double *v) {
  cov_model *next = cov->sub[0];
  NONSTATCOV(x, y, next, v);
  *v = 1.0 / *v;
}


int checkmult_inverse(cov_model *cov) {
  cov_model
    *next = cov->sub[0];
  assert(cov->vdim[0] == 1 && cov->vdim[1] == 1 );
   int err = NOERROR;
  if ((err = CHECK(next, cov->tsdim,  cov->xdimprev, ShapeType,
		   cov->domown, cov->isoown,
		   SUBMODEL_DEP, cov->role)) != NOERROR) return err;
  setbackward(cov, next);
  cov->mpp.maxheights[0] = RF_NA;
  return NOERROR;
}




////////////////////


void addSetParam(cov_model **newmodel, cov_model* remote, 
		 param_set_fct set, bool performdo, int variant, int nr) {
  set_storage *S;
  assert(SETPARAM_LOCAL == 0);
  addModel(newmodel, nr);
  kdefault(*newmodel, SET_PERFORMDO, performdo);
  NEW_COV_STORAGE(*newmodel, set);
  S = (*newmodel)->Sset;
  // S->from wird unten gesetzt !
  S->remote = remote;
  S->set = set;  
  S->variant = variant;   
}   
void addSetParam(cov_model **newmodel, cov_model * remote, 
		 param_set_fct set,  bool performdo, 
		 int variant) {
  addSetParam(newmodel, remote, set, performdo, variant, SETPARAM);
}
void addSetDistr(cov_model **newmodel, cov_model * remote, 
		 param_set_fct set,  bool performdo, int variant) {
  addSetParam(newmodel, remote, set, performdo, variant, SET_DISTR);
}


void setparamStat(double *x, cov_model *cov, double *v){
  COV(x, cov->sub[SETPARAM_LOCAL], v);
}

void setparamNonStat(double *x, double *y, cov_model *cov, double *v){
  NONSTATCOV(x, y, cov->sub[SETPARAM_LOCAL], v);
}

void Dsetparam(double *x, cov_model *cov, double *v){
  Abl1(x, cov->sub[SETPARAM_LOCAL], v);
}

void DDsetparam(double *x, cov_model *cov, double *v){
  Abl2(x, cov->sub[SETPARAM_LOCAL], v);
}

void D3setparam(double *x, cov_model *cov, double *v){
  Abl3(x, cov->sub[SETPARAM_LOCAL], v);
}
void D4setparam(double *x, cov_model *cov, double *v){
  Abl4(x, cov->sub[SETPARAM_LOCAL], v);
}

void Inverse_setparam(double *v, cov_model *cov, double *x){
  cov_model *next = cov->sub[SETPARAM_LOCAL];
  INVERSE(v, next, x);
}

void NonstatInverse_setparam(double *v, cov_model *cov, double *x, double *y){
  cov_model *next = cov->sub[SETPARAM_LOCAL];
  NONSTATINVERSE(v, next, x, y);
  // printf("nonstatinvere %f %f\n", x[0], y[0]);
}
void LogNonstatInverse_setparam(double *v, cov_model *cov, double *x, double *y){
  cov_model *next = cov->sub[SETPARAM_LOCAL];
  NONSTATLOGINVERSE(v, next, x, y);
  // printf("nonstatinvere %f %f\n", x[0], y[0]);
}

int checksetparam(cov_model *cov) {
  cov_model *next = cov->sub[SETPARAM_LOCAL];
  int err, 
    dim = cov->tsdim,
    xdim = cov->xdimown,
    role = cov->role;
  Types type = cov->typus;
  domain_type dom = cov->domown;
  isotropy_type iso = cov->isoown;

  kdefault(cov, SET_PERFORMDO, true);

  if (type == RandomType || next->typus== RandomType) {
    BUG;
  }
  if ((err = CHECK(next, dim, xdim, type, dom, iso, 
		   SUBMODEL_DEP, role)) != NOERROR) return err;
  setbackward(cov, next);
  cov->vdim[0] = next->vdim[0];
  cov->vdim[1] = next->vdim[1];
  cov->deterministic = false;

  TaylorCopy(cov, next);

  //if (cov->xdimown == 1) crash();
 
  // ACHTUNG ! weder SETPARAM_FROM (da nur link) noch SETPARAM_SET (da 
  //           i.a. keine echten Modelle) werden ueberprueft!
 
  return NOERROR;
}


bool Typesetparam(Types required, cov_model *cov, int depth) {
  return TypeConsistency(required, cov->sub[SETPARAM_LOCAL], depth-1);
}

void spectralsetparam(cov_model *cov, gen_storage *s, double *e){
   SPECTRAL(cov->sub[SETPARAM_LOCAL], s, e);  // nicht gatternr
}

int initsetparam(cov_model *cov, gen_storage *s){
  cov_model *next= cov->sub[SETPARAM_LOCAL];
  set_storage *X = cov->Sset;

  assert(X != NULL);
  int err, i,
    vdim = cov->vdim[0];
  if (cov->vdim[0] != cov->vdim[1]) BUG;

  if ((err = INIT(next, cov->mpp.moments, s)) != NOERROR)
    return err;
  if (X->remote != NULL) {
    assert(X->set != NULL);
    X->set(cov->sub[0], X->remote, X->variant);
  }
  TaylorCopy(cov, next);
  for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = next->mpp.maxheights[i];
  return NOERROR;
}


void dosetparam(cov_model *cov, gen_storage *s) {
  bool performDo = P0INT(SET_PERFORMDO);
  if (performDo) DO(cov->sub[SETPARAM_LOCAL], s); 
}

void covmatrix_setparam(cov_model *cov, double *v) {
  cov_model *next = cov->sub[SETPARAM_LOCAL];
  CovList[next->nr].covmatrix(next, v);
}

char iscovmatrix_setparam(cov_model *cov) {
  cov_model *next = cov->sub[SETPARAM_LOCAL];
  return CovList[next->nr].is_covmatrix(next);
}

void range_setparam(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[SET_PERFORMDO] = 0; 
  range->max[SET_PERFORMDO] = 1;
  range->pmin[SET_PERFORMDO] = 0;
  range->pmax[SET_PERFORMDO] = 1;
  range->openmin[SET_PERFORMDO] = false;
  range->openmax[SET_PERFORMDO] = false;
}



void trafo(double *x, cov_model *cov, double *v){  
  cov_model *next = cov->sub[0];
  assert(next != NULL);
  COV(x, next, v);
}
void logtrafo(double *x, cov_model *cov, double *v, 
		     double *Sign){  
  cov_model *next = cov->sub[0];
  assert(next != NULL);
  LOGCOV(x, next, v, Sign);
  assert(P0INT(TRAFO_ISO) == next->isoown); 
}
void nonstattrafo(double *x, double *y, cov_model *cov, double *v){  
  cov_model *next = cov->sub[0];
  assert(next != NULL);
  //  printf("%f %f; %f %f\n", x[0], x[1], y[0], y[1]);
  NONSTATCOV(x, y, next, v);
}
void lognonstattrafo(double *x, double *y, cov_model *cov, double *v, 
		     double *Sign){  
  cov_model *next = cov->sub[0];
  assert(P0INT(TRAFO_ISO) == next->isoown); 
  assert(next != NULL);
  LOGNONSTATCOV(x, y, next, v, Sign);
}
int checktrafo(cov_model *cov){
  if (PisNULL(TRAFO_ISO)) SERR("parameter not given");
  // kdefault(cov, TRAFO_ISO, ISOTROPIC);
  //
  
  if (cov->nsub == 0) addModel(cov, 0, IDCOORD);
  assert(cov->nsub == 1);
  cov_model *next = cov->sub[0];
  int err = NOERROR;
 
  if (!equal_coordinate_system(cov->isoown, P0INT(TRAFO_ISO))) {
    if (cov->isoown != EARTH_COORDS && cov->isoown != EARTH_SYMMETRIC) {
       return ERRORWRONGISO; // trafo nicht (mehr) schaffbar 
    }
  }
  
 
  if ((next->nr == IDCOORD && 
       equal_coordinate_system(cov->isoown, P0INT(TRAFO_ISO)) &&
	 cov->isoown != P0INT(TRAFO_ISO))
      || 
      (next->nr != IDCOORD &&
       equal_coordinate_system(cov->isoown, P0INT(TRAFO_ISO)) &&
       cov->isoown != P0INT(TRAFO_ISO) && 
       UpgradeToCoordinateSystem(cov->isoown) != P0INT(TRAFO_ISO) )
      ) {
    SERR2("offered system ('%s') does not match the required one ('%s')",
	  ISONAMES[cov->isoown], ISONAMES[P0INT(TRAFO_ISO)]);
  }
  
  if ((err = CHECK(next, cov->tsdim, cov->xdimown, cov->typus, cov->domown, 
		   P0INT(TRAFO_ISO), -1, ROLE_COV)) != NOERROR){

    return err;  
  }
  if (!atleastSpecialised(next->isoown, P0INT(TRAFO_ISO))) {
    SERR2("offered system ('%s') does not match the required one ('%s')",
	 ISONAMES[next->isoown], ISONAMES[P0INT(TRAFO_ISO)]);
  }
  cov->vdim[0] = next->vdim[0];
  cov->vdim[1] = next->vdim[1];
  
  return NOERROR;
}
bool Typetrafo(Types required, cov_model *cov, int depth) {
  if (cov->sub[0] == NULL) return required==ShapeType;
  return TypeConsistency(required, cov->sub[0], depth-1);
}

void rangetrafo(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[TRAFO_ISO] = ISOTROPIC;
  range->max[TRAFO_ISO] = UNREDUCED;
  range->pmin[TRAFO_ISO] = ISOTROPIC;
  range->pmax[TRAFO_ISO] = UNREDUCED;
  range->openmin[TRAFO_ISO] = false;
  range->openmax[TRAFO_ISO] = false;
}




int checktrafoproc(cov_model *cov) { // auch fuer TrendEval
  cov_model *next = cov->sub[0],
    *key = cov->key;  
  location_type *loc = Loc(cov);
  int err;  
  ROLE_ASSERT_GAUSS;
  assert(cov->nsub == 1);
  if (cov->key != NULL) {
    if ((err = CHECK(key, 3 + loc->Time, 3, ProcessType, XONLY, CARTESIAN_COORD,
		     SUBMODEL_DEP, cov->role == ROLE_BASE ? ROLE_BASE 
		     : ROLE_GAUSS)) != NOERROR) return err;
  } else {
    if ((err = CHECK(next, cov->tsdim, cov->xdimown, cov->typus, cov->domown, 
		     P0INT(TRAFO_ISO), -1, ROLE_COV)) != NOERROR){
      return err;  
    }
    if (!TypeConsistency(VariogramType, cov, MAXINT)) 
      SERR("definite function needed");
  }

  cov->vdim[0] = next->vdim[0];
  cov->vdim[1] = next->vdim[1];
  
  return NOERROR;
}


int structtrafoproc(cov_model  VARIABLE_IS_NOT_USED *cov,
		   cov_model VARIABLE_IS_NOT_USED **newmodel){// auch fuer TrendEval
  cov_model *sub = cov->sub[0];
  int i, k,
    err;
  ROLE_ASSERT_GAUSS;

  if (P0INT(TRAFO_ISO) != CARTESIAN_COORD ||
      cov->calling == NULL || cov->calling->isoown != EARTH_COORDS)
    SERR("currently only earth-to-cartesian allowed");

  if (cov->key != NULL) BUG;
 
  // ACHTUNG In der Reihenfolge, da (i) Loc einzuverwendet ist
  // (ii) umgehaengt werden muss da Trafo ownloc verwendet, ownloc
  // aber durch die Erdtrafo ueberschreiben wird. Somit muss
  // ownloc gesichert werden. Wird eingehaengt, so dass bei Fehlern
  // ownloc dennoch zum Schluss geloescht wird.


  TransformLoc(cov, true, true, false);
  SetLoc2NewLoc(sub, PLoc(cov)); // passt das?
  
  location_type *loc = Loc(cov);
  if (loc->len != 1) SERR("trafo currently only possible for a single data set");
  int 
    spatialdim = loc->spatialdim,
    xdimprev = cov->xdimprev,
    newdim = spatialdim == 2 || spatialdim == 3 ? 3 : NA_INTEGER,
    spatialpts = loc->spatialtotalpoints;
  bool 
    Time = loc->Time;
  double aequ, pol, X[4], T[3],
    *x = loc->x,
    *y = (double *) MALLOC(sizeof(double) * newdim * spatialpts),
    *yy = y;
  

  if (Time) MEMCOPY(T, loc->T, sizeof(double) * 3);
  if (STRCMP(GLOBAL.coords.newunits[0], UNITS_NAMES[units_km]) == 0) {
    aequ = 6378.1;
    pol =  6356.8;
  } else {
    aequ = 3963.17;
    pol = 3949.93;
  }

  if (loc->grid) BUG;

  loc->Time = false;
  cov->xdimprev = spatialdim;
  for (i=0; i<spatialpts; i++, x+=spatialdim) {
    Earth2Cart(x, cov, aequ, pol, X); 
    for (k=0; k<3; *(yy++) = X[k++]);
  }
  loc->Time = Time;
  cov->xdimprev = xdimprev;

  //pmi(cov, 0);  printf("\n");  PrintLoc(1, loc, true);  PMI(cov);  assert(!loc->grid);

  loc_set(y, NULL, T, 3, 3, spatialpts, 0, Time, false, false, cov);

  if ((err = covCpy(&(cov->key), sub)) == NOERROR) {
    if (cov->key != NULL) COV_DELETE(&(cov->key));
    goto ErrorHandling;
  }
  addModel(&(cov->key), GAUSSPROC);

  if ((err = CHECK(cov->key, 3 + loc->Time, 3, ProcessType, 
		   XONLY, CARTESIAN_COORD,
		   SUBMODEL_DEP, cov->role == ROLE_BASE ? ROLE_BASE 
		   : ROLE_GAUSS)) != NOERROR) goto ErrorHandling;

 ErrorHandling :
  FREE(y);
  
  return err;
}


int inittrafoproc(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *s){// auch fuer TrendEval
  cov_model *key = cov->key;
  int err;

  if (cov->vdim[0] != 1) NotProgrammedYet("");
  if ((err = INIT(key, 0, s)) != NOERROR) return err;
  
  cov->fieldreturn = true;
  cov->origrf = false;
  cov->rf = key->rf;
  cov->simu.active = true;
  return NOERROR;
}


void dotrafoproc(cov_model *cov, gen_storage *s){
  cov_model *key = cov->key;
  DO(key, s);   
}




void nonstatprod(double *x, double *y, cov_model *cov, double *v){  
  location_type *loc = Loc(cov);
  cov_model *next = cov->sub[0];
  int 
    dummy = loc->i_row,
    rows = next->vdim[0],
    cols = next->vdim[1],
    vdimsq = rows * cols;
  ALLOC_EXTRA(w, vdimsq);
  loc->i_row = loc->i_col;
  FCTN(y, next, w);
  loc->i_row = dummy;

  if (vdimsq == 1) {
    FCTN(x, next, v);
    *v *= *w;
  } else {
    ALLOC_EXTRA(u, vdimsq);
    FCTN(x, next, u); 
    matmulttransposed(u, w, v, cols, rows, cols);
  }
}


int checkprod(cov_model *cov) {
  //  location_type *loc = Loc(cov);
  if (cov->sub[0]  == NULL) {
    addModel(cov, 0, IDCOORD);
  }
  cov_model *next = cov->sub[0];
  int err;
  isotropy_type iso = UpgradeToCoordinateSystem(cov->isoown);
  if (iso == ISO_MISMATCH) return ERRORFAILED;

  //if (cov->isoown != EARTH_COORD) return ERRORFAILED; printf("loeschen\n");

  // STOPAFTER(cov->isoown == EARTH_COORD,APMI(cov));

  if ((err = CHECK(next, cov->tsdim, cov->xdimown, ShapeType,
		   XONLY, iso, SUBMODEL_DEP, ROLE_BASE
		   )) != NOERROR) return err;
  
  setbackward(cov, next);
  cov->vdim[0] = next->vdim[0]; 
  cov->vdim[1] = next->vdim[1];
  EXTRA_STORAGE;
  cov->pref[Direct] = 1;
  cov->pref[Specific] = cov->pref[Nothing]= 5;
  
  return NOERROR;
  
}




int checkprodproc(cov_model *cov) { // auch fuer TrendEval
  int err;  
  if ((err = checkprod(cov))) return err;
  if (cov->vdim[0] != 1) NotProgrammedYet("");
  return NOERROR;
}


int structprodproc(cov_model  VARIABLE_IS_NOT_USED *cov,
		   cov_model VARIABLE_IS_NOT_USED **newmodel){// auch fuer TrendEval
   return NOERROR;
}


int initprodproc(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *s){// auch fuer TrendEval
  int err;
  if (cov->vdim[0] != 1) NotProgrammedYet("");

  if ((err = check_fctn(cov)) != NOERROR) return err;
  
  ROLE_ASSERT_GAUSS;
  err = FieldReturn(cov);
  cov->simu.active = err == NOERROR;
  if (PL>= PL_STRUCTURE) PRINTF("\n'%s' is now initialized.\n", NAME(cov));
  return err;
}

void doprodproc(cov_model *cov, gen_storage *s){
  location_type *loc = Loc(cov);					
  long i,
    vdim = cov->vdim[0],
    totvdim = loc->totalpoints * vdim;
  double *res = cov->rf;
  assert(res != NULL);

  Fctn(NULL, cov, res);

  if (s->prodproc_random) {
    double g = GAUSS_RANDOM(1.0);  
    for(i=0; i<totvdim; i++) res[i] *= g;
  }
}





void nonstatsum(double *x, double *y, cov_model *cov, double *v){  
  location_type *loc = Loc(cov);
  cov_model *next = cov->sub[0];
  int i,
    dummy = loc->i_row,
    rows = next->vdim[0],
    cols = next->vdim[1],
    vdimsq = rows * cols;
  ALLOC_EXTRA(w, vdimsq);
  loc->i_row = loc->i_col;
  FCTN(y, next, w);
  loc->i_row = dummy;
  FCTN(x, next, v);
  for (i=0; i<vdimsq; i++) v[i] += w[i];
}

int checksum(cov_model *cov) {
  //  location_type *loc = Loc(cov);
  if (cov->sub[0] == NULL) {
    addModel(cov, 0, IDCOORD);
  }
  cov_model *next = cov->sub[0];
  int err;

  if ((err = CHECK(next, cov->tsdim, cov->xdimown, ShapeType,
		     XONLY, cov->isoown,
		     SUBMODEL_DEP, cov->role)) != NOERROR) return err;
  
  setbackward(cov, next);
  if (cov->vdim[0] != cov->vdim[1]) 
    SERR("sub model must return symmetric a square matrix");
  EXTRA_STORAGE;
  return NOERROR;
  
}


#define NUMBER_DERIV 10
#define NUMBER_NORM 20
#define DIM_MAX 10

