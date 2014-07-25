/*
 Authors 
 Martin Schlather, schlather@math.uni-mannheim.de

 Definition of auxiliary correlation functions 

Note:
 * Never use the below functions directly, but only by the functions indicated 
   in RFsimu.h, since there is gno error check (e.g. initialization of RANDOM)
 * VARIANCE, SCALE are not used here 
 * definitions for the random coin method can be found in MPPFcts.cc
 * definitions for genuinely anisotropic or nonsta     tionary models are in
   SophisticatedModel.cc; hyper models also in Hypermodel.cc

 Copyright (C) 2001 -- 2003 Martin Schlather
 Copyright (C) 2004 -- 2004 Yindeng Jiang & Martin Schlather
 Copyright (C) 2005 -- 2014 Martin Schlather

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


#include <math.h>
#include <stdio.h> 
#include "RF.h"
#include "Covariance.h"
#include <R_ext/Lapack.h>
//#include <R_ext/Applic.h>
//#include <R_ext/Utils.h>     
//#include <R_ext/BLAS.h> 





// $
void kappaS(int i, cov_model *cov, int *nr, int *nc){
  switch(i) {
  case DVAR : case DSCALE :
    *nr = *nc = 1;
    break;
  case DANISO :
    *nc = SIZE_NOT_DETERMINED;
    *nr = cov->xdimown;
    break;
  case DALEFT :
    *nr = SIZE_NOT_DETERMINED;
    *nc = cov->xdimown;
    break;
  case DPROJ : 
    *nr = SIZE_NOT_DETERMINED;
    *nc = 1;
    break;
  default : *nr = *nc = -1;
  }
}


// simple transformations as variance, scale, anisotropy matrix, etc.  
void Siso(double *x, cov_model *cov, double *v){


  //  printf("call Siso %s %s %s\n", NAME(cov), NAME(cov->calling),
  //	 isDollar(cov) ? NAME(cov->sub[0]) : "-");
  //PMI(cov);

  cov_model *next = cov->sub[DOLLAR_SUB];
  int i,
    vdim = cov->vdim2[0],
    vdimSq = vdim * vdim;
  double y,
    *aniso=P(DANISO),
    *scale =P(DSCALE),
    var = P0(DVAR);
  
  //  printf("\nSiso\n");

  y = *x;
  if (aniso != NULL) y = fabs(y * aniso[0]);

  if (scale != NULL) 
    y = scale[0]>0.0 ? y / scale[0] 
      : (y == 0.0 && scale[0]==0.0) ? 0.0 : RF_INF;
      
  // letzteres darf nur passieren wenn dim = 1!!
  COV(&y, next, v);

  //printf("end isoS\n");

  // PMI(cov);
  //  PMI(cov->calling->calling);
  // print("$ %f %f %f %d\n", *x, var, v[0], vdimSq);

  for (i=0; i<vdimSq; i++) v[i] *= var; 

  // print("$ %f %f %f %d\n", *x, var, v[0], vdimSq);
  //  error("von wo wird aufgerufen, dass nan in var?? und loc:x not given?");

}
  

// simple transformations as variance, scale, anisotropy matrix, etc.  
void logSiso(double *x, cov_model *cov, double *v, double *sign){
  cov_model *next = cov->sub[DOLLAR_SUB];
  int i,
    vdim = cov->vdim2[0],
    vdimSq = vdim * vdim;
  double y, 
    *aniso=P(DANISO),
    *scale =P(DSCALE),
    logvar = log(P0(DVAR));
  
  y = *x;
  if (aniso != NULL) y = fabs(y * aniso[0]);

  if (scale != NULL) 
    y = scale[0]>0.0 ? y / scale[0] 
      : (y == 0.0 && scale[0]==0.0) ? 0.0 : RF_INF;
      
  LOGCOV(&y, next, v, sign);
  for (i=0; i<vdimSq; i++) v[i] += logvar; 
}
 
void Sstat(double *x, cov_model *cov, double *v){
  logSstat(x, cov, v, NULL);
}

void logSstat(double *x, cov_model *cov, double *v, double *sign){
  cov_model *next = cov->sub[DOLLAR_SUB],
    *Aniso = cov->kappasub[DALEFT];
  double 
    var = P0(DVAR),
    *scale =P(DSCALE), 
    *z = cov->Sdollar->z,
    *aniso=P(DANISO);
  int i,
    nproj = cov->nrow[DPROJ],
    vdim = cov->vdim2[0],
    vdimSq = vdim * vdim;
 
  if (nproj > 0) {
    int *proj = PINT(DPROJ);
    if (z == NULL) z = cov->Sdollar->z = (double*) MALLOC(nproj*sizeof(double));

    if (scale == NULL || scale[0] > 0.0) {
      if (scale == NULL)  for (i=0; i<nproj; i++) z[i] = x[proj[i] - 1];
      else {
	double invscale = 1.0 / scale[0];
	for (i=0; i<nproj; i++) z[i] = invscale * x[proj[i] - 1];
      }
    } else {
      // projection and aniso may not be given at the same time
      for (i=0; i<nproj; i++)
	z[i] = (x[proj[i] - 1] == 0 && scale[0] == 0) ? 0.0 : RF_INF;
    } 
    if (sign==NULL) COV(z, next, v) else LOGCOV(z, next, v, sign);
  } else if (Aniso != NULL) {
    int dim = Aniso->vdim2[0];
    if (z == NULL) 
      z = cov->Sdollar->z =(double*) MALLOC(dim * sizeof(double));
    FCTN(x, Aniso, z);
    if (sign == NULL) COV(z, next, v) else LOGCOV(z, next, v, sign);
  } else if (aniso==NULL && (scale == NULL || scale[0] == 1.0)) {
    if (sign==NULL) COV(x, next, v) else LOGCOV(x, next, v, sign);
  } else {
    int xdimown = cov->xdimown;
    double *xz;
    if (z == NULL) 
      z = cov->Sdollar->z =(double*) MALLOC(xdimown * sizeof(double)); 
    if (aniso!=NULL) {
      int j, k,
	nrow=cov->nrow[DANISO], 
	ncol=cov->ncol[DANISO];
      for (k=i=0; i<ncol; i++) {
	z[i] = 0.0;
	for (j=0; j<nrow; j++) {
	  z[i] += aniso[k++] * x[j];
	}
      }
      xz = z;
    } else xz = x;    
    if (scale != NULL) {
      if (scale[0] > 0.0) {
	double invscale = 1.0 / scale[0];
	for (i=0; i < xdimown; i++) z[i] = invscale * xz[i];
      } else {
	for (i=0; i < xdimown; i++)
	  z[i] = (xz[i] == 0.0 && scale[0] == 0.0) ? 0.0 : RF_INF;
      }
    }
    if (sign==NULL) COV(z, next, v) else LOGCOV(z, next, v, sign);
  }

  if (sign==NULL) {
    for (i=0; i<vdimSq; i++) v[i] *= var; 
  } else {
    double logvar = log(var);
    for (i=0; i<vdimSq; i++) v[i] += logvar; 
  }

}

void Snonstat(double *x, double *y, cov_model *cov, double *v){
  logSnonstat(x, y, cov, v, NULL);
}

void logSnonstat(double *x, double *y, cov_model *cov, double *v, 
		 double *sign){
  cov_model 
    *next = cov->sub[DOLLAR_SUB],
    *Aniso = cov->kappasub[DALEFT];
  double 
    *z1 = cov->Sdollar->z,
    *z2 = cov->Sdollar->z2,
    var = P0(DVAR),
    *scale =P(DSCALE),
    *aniso=P(DANISO);
  int i,
    nproj = cov->nrow[DPROJ],
    vdim = cov->vdim2[0],
    vdimSq = vdim * vdim;

  //  PMI(cov, "$$$$");
  
  if (nproj > 0) {
    int *proj = PINT(DPROJ);
    if (z1 == NULL) 
      z1 = cov->Sdollar->z = (double*) MALLOC(nproj * sizeof(double));
    if (z2 == NULL) 
      z2 = cov->Sdollar->z2 = (double*) MALLOC(nproj * sizeof(double));
    if (scale==NULL || scale[0] > 0.0) {
      double invscale = scale==NULL ? 1.0 :  1.0 / scale[0];
      for (i=0; i<nproj; i++) {
	z1[i] = invscale * x[proj[i] - 1];
	z2[i] = invscale * y[proj[i] - 1];	
      }
    } else {
      double s = scale[0]; // kann auch negativ sein ...
      for (i=0; i<nproj; i++) {
	z1[i] = (x[proj[i] - 1] == 0.0 && s == 0.0) ? 0.0 : RF_INF;
	z2[i] = (y[proj[i] - 1] == 0.0 && s == 0.0) ? 0.0 : RF_INF;
      }
    }
  } else if (Aniso != NULL) {
    int dim = Aniso->vdim2[0];
    if (z1 == NULL) 
      z1 = cov->Sdollar->z =(double*) MALLOC(dim * sizeof(double));
    if (z2 == NULL) 
      z2 = cov->Sdollar->z2 =(double*) MALLOC(dim * sizeof(double));
    FCTN(x, Aniso, z1);
    FCTN(y, Aniso, z2);
    if (sign == NULL) NONSTATCOV(z1, z2, next, v)
      else LOGNONSTATCOV(z1, z2, next, v, sign);
  } else if (aniso==NULL && (scale==NULL || scale[0] == 1.0)) {
    z1 = x;
    z2 = y;
  } else {
    int xdimown = cov->xdimown;
    double *xz1, *xz2;
    if (z1 == NULL) 
      z1 = cov->Sdollar->z = (double*) MALLOC(xdimown * sizeof(double));
    if (z2 == NULL) 
      z2 = cov->Sdollar->z2 = (double*) MALLOC(xdimown * sizeof(double));
    if (aniso != NULL) {
      int j, k,
	nrow=cov->nrow[DANISO],
	ncol=cov->ncol[DANISO];
      for (k=i=0; i<ncol; i++) {
	z1[i] = z2[i] =0.0;
	for (j=0; j<nrow; j++, k++) {
	  z1[i] += aniso[k] * x[j];
	  z2[i] += aniso[k] * y[j];
	}
      }
      xz1 = z1;
      xz2 = z2;
    } else {
      xz1 = x;
      xz2 = y;
    }
    if (scale != NULL) {
      double s = scale[0];
      if (s > 0.0) {
	double invscale = 1.0 / s;
	for (i=0; i<xdimown; i++) {
	  z1[i] = invscale * xz1[i];
	  z2[i] = invscale * xz2[i];
	}
      } else {
	for (i=0; i<nproj; i++) {
	  z1[i] = (xz1[i] == 0.0 && s == 0.0) ? 0.0 : RF_INF;
	  z2[i] = (xz2[i] == 0.0 && s == 0.0) ? 0.0 : RF_INF;
	}
      }
    }
  }
  
  if (sign == NULL) {
    NONSTATCOV(z1, z2, next, v);
    for (i=0; i<vdimSq; i++) v[i] *= var; 
  } else {
    double logvar = log(var);
    LOGNONSTATCOV(z1, z2, next, v, sign);
    for (i=0; i<vdimSq; i++) v[i] += logvar; 
  }
}


void covmatrixS(cov_model *cov, double *v) {
  location_type *loc = Loc(cov);	      
  cov_model *next = cov->sub[DOLLAR_SUB];
  location_type *locnext = Loc(next);
  assert(locnext != NULL);
  int i, err, tot, totSq,
    dim = loc->timespacedim,
    vdim = cov->vdim2[0];
  double var = P0(DVAR);
  assert(dim == cov->tsdim);
    
  if ((err = alloc_cov(cov, dim, vdim, vdim)) != NOERROR) 
    error("memory allocation error in 'covmatrixS'");

  if ((!PisNULL(DSCALE) && P0(DSCALE) != 1.0) || 
      !PisNULL(DANISO) || !PisNULL(DPROJ)) {
    CovarianceMatrix(cov, v); 
    return;
  }

  if (next->xdimprev != next->xdimown) {
    BUG; // fuehrt zum richtigen Resultat, sollte aber nicht
    // vorkommen!
    CovarianceMatrix(cov, v); 
    return;
  }

  int next_gatter = next->gatternr,
    next_xdim = next->xdimprev;
 
  next->gatternr = cov->gatternr;
  next->xdimprev = cov->xdimprev;
  CovList[next->nr].covmatrix(next, v);//hier wird uU next->totalpoints gesetzt
  next->gatternr = next_gatter;
  next->xdimprev = next_xdim;

  // PMI(cov, "covmatrix S");
  if (locnext==NULL) loc_set(cov, locnext->totalpoints);
  tot = cov->vdim2[0] * locnext->totalpoints;
  totSq = tot * tot;
  if (var == 1.0) return;
  for (i=0; i<totSq; v[i++] *= var);
}

char iscovmatrixS(cov_model *cov) {
  cov_model *sub = cov->sub[DOLLAR_SUB];
  return (int) ((PisNULL(DSCALE) || P0(DSCALE) ==1.0) &&
		cov->kappasub[DALEFT]==NULL &&
		PARAMisNULL(sub, DPROJ) &&
		PARAMisNULL(sub, DANISO)) * CovList[sub->nr].is_covariance(cov);
}

void DS(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[DOLLAR_SUB];
  assert(cov->kappasub[DALEFT] == NULL);
  int i,
    vdim = cov->vdim2[0],
    vdimSq = vdim * vdim,
    nproj = cov->nrow[DPROJ];
  double y[2], varSc,
    *scale =P(DSCALE),
    *aniso=P(DANISO),
    spinvscale = 1.0;

  if (aniso != NULL) {
    spinvscale *= aniso[0];
    // was passiert, wenn es aniso nicht vom TypeIso ist ??
  }
  if (scale != NULL) spinvscale /= scale[0];
  varSc = P0(DVAR) * spinvscale;

  if (nproj == 0) {
    y[0] = x[0] * spinvscale; 
    y[1] = (cov->isoown==ISOTROPIC || cov->ncol[DANISO]==1) ? 0.0
      : x[1] * aniso[3]; // temporal; temporal scale
  } else {
    BUG;
    // for (i=0; i<nproj; i++) {
    //   y[i] = spinvscale * x[proj[i] - 1];
  }

  Abl1(y, next, v);
  for (i=0; i<vdimSq; i++) v[i] *= varSc; 
}

void DDS(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[DOLLAR_SUB];
  assert(cov->kappasub[DALEFT] == NULL);
  int i,
    vdim = cov->vdim2[0],
    vdimSq = vdim * vdim,
    nproj = cov->nrow[DPROJ],
    *proj = PINT(DPROJ);
  double y[2], varScSq,
    *scale =P(DSCALE),
    *aniso=P(DANISO),
    spinvscale = 1.0;
  
  if (aniso != NULL) {
    spinvscale *= aniso[0];
    // was passiert, wenn es aniso nicht vom TypeIso ist ??
  }
  if (scale != NULL) spinvscale /= scale[0];
  varScSq = P0(DVAR) * spinvscale * spinvscale;
  
  if (nproj == 0) {
    y[0] = x[0] * spinvscale;
    y[1] = (cov->isoown==ISOTROPIC || cov->ncol[DANISO]==1) ? 0.0
      : x[1] * aniso[3]; // temporal; temporal scale
  } else {
    BUG;
    for (i=0; i<nproj; i++) {
      y[0] += x[proj[i] - 1] * x[proj[i] - 1];
    }
    y[0] = sqrt(y[0]) * spinvscale;
  }
  Abl2(y, next, v);
  for (i=0; i<vdimSq; i++) v[i] *= varScSq; 
}


void D3S(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[DOLLAR_SUB];
  assert(cov->kappasub[DALEFT] == NULL);
  int i,
    vdim = cov->vdim2[0],
    vdimSq = vdim * vdim,
    nproj = cov->nrow[DPROJ],
    *proj = PINT(DPROJ);
  double y[2], varScS3,
    *scale =P(DSCALE),
    *aniso=P(DANISO),
    spinvscale = 1.0;

  if (aniso != NULL) {
    spinvscale *= aniso[0];
    // was passiert, wenn es aniso nicht vom TypeIso ist ??
  }
  if (scale != NULL) spinvscale /= scale[0];
  varScS3 = P0(DVAR) * spinvscale * spinvscale * spinvscale;
  
  if (nproj == 0) {
    y[0] = x[0] * spinvscale;
    y[1] = (cov->isoown==ISOTROPIC || cov->ncol[DANISO]==1) ? 0.0
      : x[1] * aniso[3]; // temporal; temporal scale
  } else {
    BUG;
    for (i=0; i<nproj; i++) {
      y[0] += x[proj[i] - 1] * x[proj[i] - 1];
    }
    y[0] = sqrt(y[0]) * spinvscale;
  }
  Abl3(y, next, v);
  for (i=0; i<vdimSq; i++) v[i] *= varScS3; 
}

void D4S(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[DOLLAR_SUB];
  assert(cov->kappasub[DALEFT] == NULL);
  int i,
    vdim = cov->vdim2[0],
    vdimSq = vdim * vdim,
    nproj = cov->nrow[DPROJ],
    *proj = PINT(DPROJ);
  double y[2], varScS4,
    *scale =P(DSCALE),
    *aniso=P(DANISO),
    spinvscale = 1.0;

  if (aniso != NULL) {
    spinvscale *= aniso[0];
    // was passiert, wenn es aniso nicht vom TypeIso ist ??
  }
  if (scale != NULL) spinvscale /= scale[0];
  varScS4 = spinvscale * spinvscale;
  varScS4 *= varScS4 * P0(DVAR);
  if (nproj == 0) {
    y[0] = x[0] * spinvscale;
    y[1] = (cov->isoown==ISOTROPIC || cov->ncol[DANISO]==1) ? 0.0
      : x[1] * aniso[3]; // temporal; temporal scale
  } else {
    BUG;
    for (i=0; i<nproj; i++) {
      y[0] += x[proj[i] - 1] * x[proj[i] - 1];
    }
    y[0] = sqrt(y[0]) * spinvscale;
  }
  Abl3(y, next, v);
  for (i=0; i<vdimSq; i++) v[i] *= varScS4; 
}



void nablahessS(double *x, cov_model *cov, double *v, bool nabla){
  cov_model *next = cov->sub[DOLLAR_SUB],
    *Aniso = cov->kappasub[DALEFT];
  assert(cov->kappasub[DALEFT] == NULL);
  int i, endfor,
    dim = cov->nrow[DANISO],// == ncol == x d i m ?
    xdimown = cov->xdimown,
    nproj = cov->nrow[DPROJ];
  double *xy, *vw,
    *y = cov->Sdollar->y,
    *z = cov->Sdollar->z,
    *w = cov->Sdollar->z2,
    *scale =P(DSCALE),
    *aniso=P(DANISO),
    var = P0(DVAR);
  if (nproj != 0) BUG;
  if (Aniso != NULL) BUG;
  if (dim != xdimown) BUG;
	  
  
  if (aniso != NULL) {  
    if (z == NULL) 
      z = cov->Sdollar->z = (double*) MALLOC(sizeof(double) * xdimown);
    if (w == NULL) 
      w = cov->Sdollar->z2 = (double*) MALLOC(sizeof(double) * xdimown);
    xA(x, aniso, xdimown, xdimown, z);
    xy = z;
    vw = w;
  } else {
    xy = x;
    vw = v;
  }

  if (scale != NULL) {
    if (y == NULL) 
      y = cov->Sdollar->y =(double*) MALLOC(sizeof(double) * xdimown);
    assert(scale[0] > 0.0);
    double spinvscale = 1.0 / scale[0];
    var *= spinvscale;
    if (!nabla) var *= spinvscale; // gives spinvscale^2 in case of hess
    for (i=0; i<xdimown; i++) y[i] = xy[i] * spinvscale;
    xy = y;
  }

  endfor = xdimown;
  if (nabla) {
    NABLA(xy, next, vw);
  } else {
    HESSE(xy, next, vw);
    endfor *= xdimown;
  }
     
  if (aniso != NULL) {  
    if (nabla) Ax(aniso, vw, xdimown, xdimown, v);
    else XCXt(aniso, vw, v, xdimown, xdimown);
  }

  for (i=0; i<endfor; i++) v[i] *= var; 
}

void nablaS(double *x, cov_model *cov, double *v){
  nablahessS(x, cov, v, true);
}
void hessS(double *x, cov_model *cov, double *v){
  nablahessS(x, cov, v, false);
}


 

void inverseS(double *x, cov_model *cov, double *v) {
  cov_model *next = cov->sub[DOLLAR_SUB];
  if (cov->kappasub[DALEFT] != NULL)
    ERR("inverse can only be calculated if 'Aniso' not an arbitrary function"); 
  int i,
    dim = cov->xdimown,
    nproj = cov->nrow[DPROJ];
  //    *proj = (int *)P(DPROJ];
  double y, 
    s = 1.0,
    *scale =P(DSCALE),
    *aniso=P(DANISO),
    var = P0(DVAR);

  if (dim != 1) BUG;

  if (aniso != NULL) {
    if (isMiso(Type(aniso, cov->nrow[DANISO], cov->ncol[DANISO]))) 
      s /= aniso[0];
    else NotProgrammedYet(""); // to do
  }
  if (scale != NULL) s *= scale[0];  
  if (nproj == 0) {
    y= *x / var; // inversion, so variance becomes scale
  } else {
    BUG;  //ERR("nproj is not allowed in invS");
  }
  
  if (CovList[next->nr].inverse == ErrCov) BUG;
  //PMI(next);
  INVERSE(&y, next, v);
 
  for (i=0; i<dim; i++) v[i] *= s; //!
}


void nonstatinverseS(double *x, cov_model *cov, double *left, double*right,
		     bool log){
  cov_model
    *next = cov->sub[DOLLAR_SUB],
    *Aniso = cov->kappasub[DALEFT];
 
  // if (cov->kappasub[DALEFT] != NULL)
  //  ERR("inverse can only be calculated if 'Aniso' not an arbitrary function"); 
  int i,
    dim = cov->tsdim,
    nproj = cov->nrow[DPROJ];
  //    *proj = (int *)P(DPROJ];
  double y, 
    s = 1.0,
    *scale =P(DSCALE),
    *aniso=P(DANISO),
    var = P0(DVAR);
 
 

  if (nproj == 0) {
    y= *x / var; // inversion, so variance becomes scale
  } else {
    BUG;  //ERR("nproj is not allowed in invS");
  }
    
  if (CovList[next->nr].nonstat_inverse == ErrInverseNonstat) BUG;
  if (log) {
    NONSTATLOGINVERSE(&y, next, left, right);
  } else {
    NONSTATINVERSE(&y, next, left, right);
  }

 
  if (aniso != NULL) {
    if (isMiso(Type(aniso, cov->nrow[DANISO], cov->ncol[DANISO]))) s/=aniso[0];
    else {
      dollar_storage *S = cov->Sdollar;
      int  
	ncol = cov->ncol[DANISO],
	nrow = cov->nrow[DANISO],
	ncnr = ncol * nrow,
	bytes = ncnr * sizeof(double),
	size = ncol * sizeof(double);
      bool redo;
      if (ncol != nrow) BUG;
      if ((redo = S->save_aniso == NULL)) {
	S->save_aniso = (double *) MALLOC(bytes);
	S->inv_aniso = (double *) MALLOC(bytes);
      }
      double *LR = cov->Sdollar->z;
      if (LR == NULL) LR = cov->Sdollar->z = (double*) MALLOC(size);
      double *save = S->save_aniso,
	*inv = S->inv_aniso;
      if (!redo) {
	for (i=0; i<ncnr; i++) if ((redo = save[i] != P(DANISO)[i])) break;
      }
      if (redo) {
	MEMCOPY(save, P(DANISO), bytes);
	MEMCOPY(inv, P(DANISO), bytes);
	if (invertMatrix(inv, nrow) != NOERROR)
	  error("inversion of anisotropy matrix failed");
      }
      
      MEMCOPY(LR, right, size);
      xA(LR, inv, nrow, ncol, right);
      
      MEMCOPY(LR, left, size);
      xA(LR, inv, nrow, ncol, left);      
    }    
  }

  if (Aniso != NULL) {
    if (aniso != NULL) BUG;

    //  PMI(Aniso);

    if (CovList[Aniso->nr].inverse == ErrInverse) 
      error("inverse of anisotropy matrix function unknown");
    int 
      nrow = Aniso->vdim2[0],
      ncol = Aniso->vdim2[1],
       size = nrow * sizeof(double);
    //      printf("ncol %d %d\n", ncol, nrow);
    if (cov->xdimown != ncol || ncol != 1)
      error("anisotropy function not of appropriate form");
    double *LR = cov->Sdollar->z;
    if (LR == NULL) LR = cov->Sdollar->z = (double*) MALLOC(size);
    
    MEMCOPY(LR, right, size);
    INVERSE(LR, Aniso, right);
    

    //    printf("right %f->%f  %f->%f", LR[0], right[0], LR[1], right[1]);
      
    MEMCOPY(LR, left, size);
    INVERSE(LR, Aniso, left);
  }

  if (scale != NULL) s *= scale[0];  
  if (s != 1.0) {
    for (i=0; i<dim; i++) {
      left[i] *= s; //!
      right[i] *= s;
    //      printf("i=%d l=%f %f \n", i, left[i], right[i]);      
    }
  }

  //APMI(cov->calling);

}

void nonstatinverseS(double *x, cov_model *cov, double *left, double*right) {
  nonstatinverseS(x, cov, left, right, false);
}

void nonstat_loginverseS(double *x, cov_model *cov, double *left, double*right){
 nonstatinverseS(x, cov, left, right, true);
}

void coinitS(cov_model *cov, localinfotype *li) {
  cov_model *next = cov->sub[DOLLAR_SUB];
  if ( CovList[next->nr].coinit == NULL)
    ERR("# cannot find coinit -- please inform author");
  CovList[next->nr].coinit(next, li);
}
void ieinitS(cov_model *cov, localinfotype *li) {
  cov_model *next = cov->sub[DOLLAR_SUB];
  
  if ( CovList[next->nr].ieinit == NULL)
    ERR("# cannot find ieinit -- please inform author");
  CovList[next->nr].ieinit(next, li);
}

void tbm2S(double *x, cov_model *cov, double *v){
  cov_model *next = cov->sub[DOLLAR_SUB];
  cov_fct *C = CovList + next->nr; // kein gatternr, da isotrop
  double y[2],  *xy,
    *scale =P(DSCALE),
    *aniso = P(DANISO);
  assert(cov->kappasub[DALEFT] == NULL);
  
  assert(cov->nrow[DPROJ] == 0);
  if (aniso!=NULL) {
    if (cov->ncol[DANISO]==2) {  // turning layers
      y[0] = x[0] * aniso[0]; // spatial 
      y[1] = x[1] * aniso[3]; // temporal
      assert(aniso[1] == 0.0 && aniso[2] == 0.0);
      if (y[0] == 0.0) C->cov(y, next, v); 
      else C->tbm2(y, next, v);
    } else {
      assert(cov->ncol[DANISO]==1);
      if (cov->nrow[DANISO] == 1) { // turning bands
	y[0] = x[0] * aniso[0]; // purely spatial 
	C->tbm2(y, next, v);
      } else { // turning layers, dimension reduction
	if (P0(DANISO) == 0.0) {
	  y[0] = x[1] * aniso[1]; // temporal 
	  C->cov(y, next, v); 
	} else {
	  y[0] = x[0] * aniso[0]; // temporal 
	  C->tbm2(y, next, v);
	}
      }
    }
    xy = y;
  } else xy = x;

  if (scale != NULL) {
    double s = scale[0];
    if (s > 0) { 
      double invscale = 1.0 / s;
      if (cov->xdimown == 2){
	y[0] = xy[0] * invscale; // spatial 
	y[1] = xy[1] * invscale; // temporal
	if (y[0] == 0.0) C->cov(y, next, v); 
	else C->tbm2(y, next, v);
      } else {
	y[0] = xy[0] * invscale; // purely spatial 
	C->tbm2(y, next, v);
      }
    } else {
      y[0] = (s < 0.0 || xy[0] != 0.0) ? RF_INF : 0.0;
      if (cov->xdimown == 2)
	y[1] = (s < 0.0 || xy[1] != 0.0) ? RF_INF : 0.0;
      C->tbm2(y, next, v);
    }
  }
  *v *= P0(DVAR);
}


// TODO : Aniso=Matrix: direkte implementierung in S,
// sodass nicht ueber initS gegangen werden muss, sondern
//  e  < -  e^\top Aniso


int TaylorS(cov_model *cov) {
  cov_model 
    *next = cov->sub[DOLLAR_SUB],
    *sub = cov->key == NULL ? next : cov->key;
  int i;

  if (PisNULL(DPROJ) && PisNULL(DALEFT) && PisNULL(DANISO)) {
    double scale = PisNULL(DSCALE) ? 1.0 : P0(DSCALE);
    cov->taylorN = sub->taylorN;  
    for (i=0; i<cov->taylorN; i++) {
      cov->taylor[i][TaylorPow] = sub->taylor[i][TaylorPow];
      cov->taylor[i][TaylorConst] = sub->taylor[i][TaylorConst] *
	P0(DVAR) * pow(scale, -sub->taylor[i][TaylorPow]);   
    }

    cov->tailN = sub->tailN;  
    for (i=0; i<cov->tailN; i++) {
      cov->tail[i][TaylorPow] = sub->tail[i][TaylorPow];
      cov->tail[i][TaylorExpPow] = sub->tail[i][TaylorExpPow];
      cov->tail[i][TaylorConst] = sub->tail[i][TaylorConst] *
	P0(DVAR) * pow(scale, -sub->tail[i][TaylorPow]);   
      cov->tail[i][TaylorExpConst] = sub->tail[i][TaylorExpConst] *
	pow(scale, -sub->tail[i][TaylorExpPow]);
    }
  } else {
    cov->taylorN = cov->tailN = 0;
  }
  return NOERROR;
}

int checkS(cov_model *cov) {
  static bool print_warn_Aniso = true;

  // hier kommt unerwartet  ein scale == nan rein ?!!
  cov_model 
    *next = cov->sub[DOLLAR_SUB],
    *Aniso = cov->kappasub[DALEFT],
    *sub = cov->key == NULL ? next : cov->key;
  int i, err,
    xdimown = cov->xdimown,
    xdimNeu = xdimown,
    *proj = PINT(DPROJ),
    nproj = cov->nrow[DPROJ];
  // bool skipchecks = GLOBAL.general.skipchecks;
  matrix_type type = TypeMany;
  
  
  assert(isAnyDollar(cov));
  if (!isDollarProc(cov)) cov->nr = DOLLAR; // wegen nr++ unten !
  
  // cov->q[1] not NULL then A has been given
  
  // if (cov->method == SpectralTBM && cov->xdimown != next->xdimprev)
  //  return ERRORDIM;
  
  if (cov->q == NULL && !PisNULL(DALEFT)) {
    if (GLOBAL.internal.warn_Aniso && print_warn_Aniso) {
      print_warn_Aniso = false;
      PRINTF("NOTE! Starting with RandomFields 3.0, the use of 'Aniso' is different from\n the former 'aniso' insofar that 'Aniso' is multiplied from the right by 'x'\n(whereas 'aniso' had been multiplied from the left by 'x').\nSet RFoptions(newAniso=FALSE) to avoid this message.\n");
    }
    // here for the first time
    if (!PisNULL(DANISO)) return ERRORANISO_T; 
    int j, k,
      lnrow = cov->nrow[DALEFT],
      lncol = cov->ncol[DALEFT],
      total = lncol * lnrow;
	
    double
      *pA = P(DALEFT); 
    PALLOC(DANISO, lncol, lnrow); // !! ACHTUNG col, row gekreuzt
    for (i=k=0; i<lnrow; i++) {
      for (j=i; j<total; j+=lnrow) P(DANISO)[k++] = pA[j];
    }
    PFREE(DALEFT);
    cov->q = (double*) CALLOC(1, sizeof(double));
    cov->qlen = 1;
  }
 
  if ((err = checkkappas(cov, false)) != NOERROR) {
    return err;
  }
  kdefault(cov, DVAR, 1.0);
  if (Aniso != NULL) {
    if (!isDollarProc(cov) && CovList[Aniso->nr].check!=checkAngle)
      cov->domown = KERNEL;

    if (!PisNULL(DANISO) || !PisNULL(DPROJ) || !PisNULL(DSCALE))
      SERR("if Aniso is an arbitrary function, only 'var' may be given as additional parameter"); 
    if (cov->isoown != SYMMETRIC && cov->isoown != CARTESIAN_COORD) {
      //      printf("iso = %d\n", cov->isoown);
      //PMI(cov, -1);
      //      assert(false);
      return ERRORANISO;
    } //else  printf("OK iso = %d\n", cov->isoown);
 
    cov->full_derivs = cov->rese_derivs = 0;
    cov->loggiven = true;
    if ((err = CHECK(Aniso, cov->tsdim, cov->xdimown, ShapeType, XONLY,
		     CARTESIAN_COORD, SUBMODEL_DEP, cov->role)) != NOERROR) {
      return err;
    }

    if (Aniso->vdim2[1] != 1)
      SERR4("'%s' returns a matrix of dimension %d x %d, but dimension %d x 1 is need.",
	    KNAME(DANISO), Aniso->vdim2[0], Aniso->vdim2[1], cov->xdimown);

    // PMI(cov);

    if (cov->key==NULL) {
      if ((err = CHECK(sub, Aniso->vdim2[0], Aniso->vdim2[0], cov->typus,  
		       cov->domown, 
		       cov->isoown, SUBMODEL_DEP, cov->role)) != NOERROR) {
	return err;
      }
    }
    cov->pref[Nugget] = cov->pref[RandomCoin] = cov->pref[Average] = 
      cov->pref[Hyperplane] = cov->pref[SpectralTBM] = cov->pref[TBM] = 
      PREF_NONE;

    if (CovList[Aniso->nr].cov != Angle) {
      sub->pref[CircEmbed] = sub->pref[CircEmbedCutoff] = 
	sub->pref[CircEmbedIntrinsic] = sub->pref[Sequential] = 
	sub->pref[Markov] = sub->pref[Specific] = PREF_NONE;
    }
 
  } else if (!PisNULL(DANISO)) { // aniso given
    int *idx=NULL,
      nrow = cov->nrow[DANISO],
      ncol = cov->ncol[DANISO];
    bool quasidiag;

    idx = (int *) MALLOC((nrow > ncol ? nrow : ncol) * sizeof(int));
    if (nrow==0 || ncol==0) SERR("dimension of the matrix is 0");
    if (!PisNULL(DPROJ)) return ERRORANISO_T;
    if (xdimown < nrow) {
      if (PL >= PL_ERRORS) {LPRINT("xdim=%d != nrow=%d\n", xdimown, nrow);}
      SERR("#rows of anisotropy matrix does not match dim. of coordinates");
    }
    if (xdimown != cov->tsdim && nrow != ncol)
      SERR("non-quadratic anisotropy matrices only allowed if dimension of coordinates equals spatio-temporal dimension");

    analyse_matrix(P(DANISO), nrow, ncol, 
		   &(cov->diag),
		   &quasidiag, // &(cov->quasidiag), 
		   idx, // cov->idx
		   &(cov->semiseparatelast),
		   &(cov->separatelast));
    free(idx); idx=NULL;
    type = Type(P(DANISO), nrow, ncol);
    
    cov->full_derivs = cov->rese_derivs 
      = (xdimown == 1 || nrow == ncol) ? 2 : 0;
    cov->loggiven = true;
  
    // printf("hiere\n");
 
    switch (cov->isoown) {
    case ISOTROPIC : 
      if (cov->tsdim != 1) return ERRORANISO;
      break;
    case SPACEISOTROPIC :  
      cov->full_derivs =  cov->rese_derivs = 2;
      if (nrow != 2 || !isMdiag(type))
	SERR("spaceisotropy needs a 2x2 diagonal matrix");
      break;      
    case ZEROSPACEISO : 
      if (!isMdiag(type)) return ERRORANISO;
      break;      
    case VECTORISOTROPIC :
      if (!isMiso(type)) return ERRORANISO; 
      break;
    case SYMMETRIC: 
      break;
    case PREVMODELI : BUG;      
    case CARTESIAN_COORD : 
      if (!isProcess(cov->typus)) return ERRORANISO;
      break;
    default : BUG;
    }

    //  printf(" A hiere\n");
    
    if (!cov->diag) 
      cov->pref[Nugget] = cov->pref[RandomCoin] = cov->pref[Average] = cov->pref[Hyperplane] = PREF_NONE;
    if (cov->isoown != SPACEISOTROPIC && !isMiso(type))
      cov->pref[SpectralTBM] = cov->pref[TBM] = PREF_NONE;
   
    if (cov->key==NULL) {
      if ((err = CHECK(next, ncol, ncol, cov->typus, cov->domown, 
		 ncol==1 && !isProcess(cov->typus) ? ISOTROPIC : cov->isoown, 
		 SUBMODEL_DEP, cov->role))
	  != NOERROR) {
	return err;
      }
      if (next->domown == cov->domown && next->isoown == cov->isoown &&
	  xdimown > 1) next->delflag = DEL_COV - 7;
    } else {
      if ((err = CHECK(cov->key, ncol, ncol, cov->typus, cov->domown, 
		       ncol==1 && 
		       !isProcess(cov->typus) ? ISOTROPIC : cov->isoown,
		       SUBMODEL_DEP, cov->role)) != NOERROR) return err;
    }
  
  } else { // PisNULL(DANISO) 
    
    int tsdim = cov->tsdim;
    if (nproj > 0) {
      if (cov->ncol[DPROJ] != 1) SERR("proj must be a vector");
      if (cov->xdimprev != cov->xdimown) return ERRORANISO;
      for (i=0; i<nproj; i++) {
	int idx = proj[i] - 1;
	if (idx >= xdimown)
	  SERR2("%d-th value of 'proj' (%d) out of range", i, proj[i]);
      }
      xdimNeu = nproj;
      tsdim = nproj;

      switch (cov->isoown) {
      case ISOTROPIC : if (cov->tsdim != 1) return ERRORANISO;
	break;
      case SPACEISOTROPIC :  
	if (nproj != 2 || xdimown != 2 || tsdim != 2)
	  SERR("spaceisotropy needs a 2x2 diagonal matrix");
	break;      
      case ZEROSPACEISO : return ERRORANISO; // ginge z.T.; aber kompliziert
	break;      
      case VECTORISOTROPIC : return ERRORANISO; 
	break;
      case SYMMETRIC: case CARTESIAN_COORD: 
	break;
      case PREVMODELI : BUG;      
	break;
      default : BUG;
      }
 
    }

    if (cov->key==NULL) {
      //PMI(next);
      if ((err = CHECK(next, tsdim, xdimNeu, cov->typus, cov->domown,
		       cov->isoown, 
		       cov->vdim2[0], // SUBMODEL_DEP; geaendert 20.7.14
		       cov->role)) != NOERROR) {
	return err;
      }

      if (next->domown == cov->domown &&
	  next->isoown == cov->isoown) // darf nicht nach C-HECK als allgemeine Regel ruebergezogen werden, u.a. nicht wegen stat/nicht-stat wechsel !!
	// hier geht es, da domown und isoown nur durchgegeben werden und die Werte      // bereits ein Schritt weiter oben richtig/minimal gesetzt werden.
	next->delflag = DEL_COV - 8;
    } else {
      if ((err = CHECK(cov->key, tsdim, xdimNeu, cov->typus,
		       cov->domown, cov->isoown,
		       SUBMODEL_DEP, cov->role)) != NOERROR) return err;
    }

  } // end no aniso
 
  //  printf("tsdim = %d\n", cov->tsdim);

  if (( err = checkkappas(cov, false)) != NOERROR) {
    return err;
  }


  //     printf("here %d\n", cov->maxdim);
  
  setbackward(cov, sub);
  if ((Aniso != NULL || !PisNULL(DANISO) || !PisNULL(DPROJ)) && 
      cov->maxdim < cov->xdimown) cov->maxdim = cov->xdimown;
 	
  if (cov->isoown != ISOTROPIC && !isDollarProc(cov)) { // multivariate kann auch xdimNeu == 1 problematisch sein
    cov->nr++;
  }
  
  if (xdimNeu > 1) {
    cov->pref[CircEmbedCutoff] = cov->pref[CircEmbedIntrinsic] = 0;
  }

  // printf("cov-Nr = %d\n", cov->nr);
  
  // 30.10.11 kommentiert:
  //  cov->pref[CircEmbedCutoff] = cov->pref[CircEmbedIntrinsic] = 
  //      cov->pref[TBM] = cov->pref[SpectralTBM] = 0;
  if ( (PisNULL(DANISO) || isMiso(type)) && PisNULL(DPROJ)) {
    //    print("logspeed %s %f\n", NICK(sub), sub->logspeed);
    cov->logspeed = sub->logspeed * P0(DVAR);
  }
  //////////////// 

  if (sub->pref[Nugget] > PREF_NONE) cov->pref[Specific] = 100;
  if (nproj == 0) cov->matrix_indep_of_x = sub->matrix_indep_of_x;

  if ((err = TaylorS(cov)) != NOERROR) return err;
  // printf("here2\n");
  
  if (cov->Sdollar != NULL && cov->Sdollar->z != NULL)
    DOLLAR_DELETE(&(cov->Sdollar));
  if (cov->Sdollar == NULL) {
    cov->Sdollar = (dollar_storage*) MALLOC(sizeof(dollar_storage));
    DOLLAR_NULL(cov->Sdollar);
  } 
  assert(cov->Sdollar->z == NULL);
  
  if (isProcess(cov->typus)) {
    MEMCOPY(cov->pref, PREF_NOTHING, sizeof(pref_shorttype)); 
  }
 
  if (GLOBAL.coords.coord_system == earth && 
      isCartesian(CovList[next->nr].domain) &&
      GLOBAL.internal.warn_scale &&
      P0(DSCALE) < (strcmp(GLOBAL.coords.newunits[0], "km") == 0 ? 10 : 6.3)) {
    char msg[300];

    //  printf("%d %d\n", P0(DSCALE), strcmp(GLOBAL.general.newunits[0], "km") == 0 ? 100 : 63); assert(false);

    sprintf(msg, "value of scale parameter equals '%4.2f',\nwhich is less than 100, although models defined on R^3 are used in the\ncontext of earth coordinates where larger scales are expected.\n(This warning appears only ones per session.)\n", P0(DSCALE)); 
    GLOBAL.internal.warn_scale = false;
    warning(msg);
  }
  //  PMI(cov, 1);
  //  printf("here XX\n");
  //PMI(cov->calling);
   return NOERROR;
}



bool TypeS(Types required, cov_model *cov) {
  cov_model *sub = cov->key==NULL ? cov->sub[0] : cov->key;

  // printf("\n\n\nxxxxxxxxxxxx %d %s %s\n\n", TypeConsistency(required, sub), TYPENAMES[required], NICK(sub));
  
  // if (required == ProcessType) crash(); //assert(false);

  return (required==TcfType || required==PosDefType || required==NegDefType
	  || required==ShapeType || required==TrendType 
	  || required==ProcessType || required==GaussMethodType)
    && TypeConsistency(required, sub);
}


void spectralS(cov_model *cov, storage *s, double *e) {
  cov_model *next = cov->sub[DOLLAR_SUB];
  int d,
    ncol = PisNULL(DANISO) ? cov->tsdim : cov->ncol[DANISO];
  double sube[MAXTBMSPDIM],
    *scale =P(DSCALE),
    invscale = 1.0;
  
  SPECTRAL(next, s, sube); // nicht gatternr

  // Reihenfolge nachfolgend extrem wichtig, da invscale auch bei aniso
  // verwendet wird


  if (scale != NULL) invscale /= scale[0];
  // print("sube %f %f %f %d %d\n", sube[0], sube[1], invscale, cov->tsdim, ncol);
  
  if (!PisNULL(DANISO)) {
    int j, k, m,
      nrow = cov->nrow[DANISO],
      total = ncol * nrow;
    double
      *A = P(DANISO); 
    for (d=0, k=0; d<nrow; d++, k++) {
      e[d] = 0.0;
      for (m=0, j=k; j<total; j+=nrow) {
	e[d] += sube[m++] * A[j] * invscale;
      }
    }
  } else { 
    for (d=0; d<ncol; d++) e[d] = sube[d] * invscale;
  }

}


void rangeS(cov_model *cov, range_type* range){
  int i;

  range->min[DVAR] = 0.0;
  range->max[DVAR] = RF_INF;
  range->pmin[DVAR] = 0.0;
  range->pmax[DVAR] = 100000;
  range->openmin[DVAR] = false;
  range->openmax[DVAR] = true;

  range->min[DSCALE] = 0.0;
  range->max[DSCALE] = RF_INF;
  range->pmin[DSCALE] = 0.0001;
  range->pmax[DSCALE] = 10000;
  range->openmin[DSCALE] = true;
  range->openmax[DSCALE] = true;

  for (i=DANISO; i<= DALEFT; i++) {
    range->min[i] = RF_NEGINF;
    range->max[i] = RF_INF;
    range->pmin[i] = -10000;
    range->pmax[i] = 10000;
    range->openmin[i] = true;
    range->openmax[i] = true;
  }
  
  range->min[DPROJ] = 1;
  range->max[DPROJ] = cov->tsdim;
  range->pmin[DPROJ] = 1;
  range->pmax[DPROJ] =  cov->tsdim;
  range->openmin[DPROJ] = false;
  range->openmax[DPROJ] = false;
}


void ScaleDollarToLoc(cov_model *to, cov_model *from,
		      int VARIABLE_IS_NOT_USED depth) {

  assert(!PARAMisNULL(to, LOC_SCALE));
  assert(isDollar(from));
  assert(!PARAMisNULL(from, DSCALE));
  PARAM(to, LOC_SCALE)[0] = PARAM0(from, DSCALE);
}

bool ScaleOnly(cov_model *cov) {
  return isDollar(cov) && 
    PisNULL(DALEFT) && cov->kappasub[DALEFT] == NULL && 
    PisNULL(DPROJ) && cov->kappasub[DPROJ] == NULL &&
    PisNULL(DANISO) &&  cov->kappasub[DANISO] == NULL &&
    (PisNULL(DVAR) || P0(DVAR)==1.0) && cov->kappasub[DVAR] == NULL;
}



int addScales(cov_model **newmodel, double anisoScale, cov_model *scale,
	      double Scale) {
  //  cov_model *calling = (*newmodel)->calling;

  if (anisoScale != 1.0) {
    addModel(newmodel, LOC);
    kdefault(*newmodel, LOC_SCALE, anisoScale);
  }

  if (scale != NULL) {
    if (!isRandom(scale)) SERR("unstationary scale not allowed yet");
    addModel(newmodel, LOC);
    addSetDistr(newmodel, scale->calling, ScaleDollarToLoc, true, MAXINT);

    //   assert((*newmodel)->Sset != NULL);
    // APMI(*newmodel);
    
  } else {
    if (Scale != 1.0) {
      addModel(newmodel, LOC);
      kdefault(*newmodel, LOC_SCALE, Scale);
    }
  }  
  return NOERROR;
}

int structS(cov_model *cov, cov_model **newmodel) {
  cov_model *local = NULL,
    *dummy = NULL,
    *next = cov->sub[DOLLAR_SUB],
    *Aniso = cov->kappasub[DALEFT],
    *scale = cov->kappasub[DSCALE];
  int err = NOERROR; 
  // cov_model *sub;

  //   printf("%s %d %s\n", ROLENAMES[cov->role], cov->role, TYPENAMES[cov->typus]);
  //  if (cov->role != ROLE_GAUSS) {
  //  APMI(cov->calling);
  //crash();
  //  }
  // assert(false);

  
  if (cov->kappasub[DVAR] != NULL) {
    GERR("models including arbitrary functions for 'var' cannot be simulated yet");
    } 

  switch (cov->role) {
   case ROLE_SMITH : 
     if (Aniso != NULL && CovList[Aniso->nr].check != checkAngle)
       GERR("complicated models including arbitrary functions for 'Aniso' cannot be simulated yet for Smith models");


    ASSERT_NEWMODEL_NOT_NULL;
     
    if (!PisNULL(DANISO) || !PisNULL(DALEFT))
      GERR("anisotropy parameter not allowed yet");
    if (!PisNULL(DPROJ)) GERR("projections not allowed yet");

    if ((err = STRUCT(next, newmodel)) > NOERROR) return err;

    addModel(newmodel, DOLLAR);
    if (!PisNULL(DVAR)) kdefault(*newmodel, DVAR, P0(DVAR));
    if (!PisNULL(DSCALE)) kdefault(*newmodel, DSCALE, P0(DSCALE));
    if (!next->deterministic) GERR("random shapes not programmed yet");
    
    break;
  case ROLE_MAXSTABLE : // eigentlich nur von RPSmith moeglich !
    ASSERT_NEWMODEL_NOT_NULL;
    if (Aniso != NULL && CovList[Aniso->nr].check != checkAngle)
       GERR("complicated models including arbitrary functions for 'Aniso' cannot be simulated yet for Smith models");

    if (!next->deterministic) GERR("random shapes not programmed yet");
    if (!PisNULL(DPROJ)) GERR("projections not allowed yet");
    // P(DVAR) hat keine Auswirkungen
    if (!PisNULL(DANISO) || !PisNULL(DALEFT)) {
      if (!isMonotone(next) || !isIsotropic(next->isoown) ||
	  PisNULL(DANISO) || cov->ncol[DANISO] != cov->nrow[DANISO])
	GERR("anisotropy parameter only allowed for simple models up to now");
    }

    //APMI(cov);
    
    assert(cov->calling != NULL); 
    double anisoScale;
  
    if (!PisNULL(DANISO)) {
      anisoScale = 1.0 / getMinimalAbsEigenValue(P(DANISO), cov->nrow[DANISO]);
      if (!R_FINITE(anisoScale)) 
	GERR("singular anisotropy matrices not allowed");
    } else anisoScale = 1.0;
  
    if (cov->calling->nr == SMITHPROC) {
      if ((err = STRUCT(next, newmodel)) == NOERROR && *newmodel != NULL) {

	(*newmodel)->calling = cov;
	//   PMI(shape); 
	//APMI(*newmodel);
	Types type = 
	  TypeConsistency(PointShapeType, *newmodel) ? PointShapeType :
	  TypeConsistency(RandomType, *newmodel) ? RandomType :
	  TypeConsistency(ShapeType, *newmodel) ? ShapeType :
	  OtherType;
	double Scale =  PisNULL(DSCALE) ? 1.0 : P0(DSCALE);
	
	if (type == RandomType) { // random locations given;
	  // so, it must be of pgs type (or standard):
	  
	  if ((err = CHECK_R(*newmodel, cov->tsdim)) != NOERROR) {
	    goto ErrorHandling;
	  }
	  dummy = *newmodel;

	  if ((err=addScales(&dummy, anisoScale, scale, Scale))!=NOERROR){
	    goto ErrorHandling;
	  }	  

	  *newmodel = NULL;     
	  assert(cov->vdim2[0] == cov->vdim2[1]);

	  
	  //APMI(dummy);

	  if ((err = addPointShape(newmodel, cov, dummy, cov->tsdim, 
				   cov->vdim2[0])) != NOERROR) {
	    goto ErrorHandling; 
	  }
	  if (*newmodel == NULL) BUG;
	  (*newmodel)->calling = cov;
	  // APMI(cov);
	} else {
	  if (type == PointShapeType && 
	      (err = addScales((*newmodel)->sub + PGS_LOC, anisoScale, scale, 
			       Scale)) != NOERROR) goto ErrorHandling;
	  if ((err = CHECK(*newmodel, cov->tsdim, cov->xdimprev, type, 
			   cov->domprev, cov->isoprev, cov->vdim2, 
			   ROLE_MAXSTABLE))
	      != NOERROR) {
	    goto ErrorHandling;
	  }
	  if (type==PointShapeType) {	
	    if ((err = PointShapeLocations(*newmodel, cov)) != NOERROR) 
	      goto ErrorHandling;
	  } else if (type == ShapeType) {
	    dummy = *newmodel;
	    *newmodel = NULL;
	    // suche nach geeigneten locationen
	    if ((err = addScales(&local, anisoScale, scale, Scale))!=NOERROR)
	      goto ErrorHandling;
	    if ((err = addPointShape(newmodel, dummy, NULL, local,
				     cov->tsdim, cov->vdim2[0]))
		!= NOERROR) 
	      goto ErrorHandling; 
	    //printf("e ddd\n");
	  } else BUG;
	} // ! randomtype
      } else { // S TRUCT does not return anything
	int err2;
	//assert(false);
	// XERR(err);
	//		APMI(*newmodel);
	if ((err2 = addPointShape(newmodel, cov, NULL, cov->tsdim, cov->vdim2[0]))
	    != NOERROR) {
	  if (err == NOERROR) err = err2;
	  goto ErrorHandling; 
	}
	err = NOERROR;
      }

    } else { // not from RPsmith
      GERR("unexpected call of 'structS'. Please contact author.");
      // 
     if ((err = STRUCT(next, newmodel)) > NOERROR) return err;
    }
    
    //  PMI(cov);

    //  APMI(*newmodel);
   
    break;
  case ROLE_GAUSS :
    if (isProcess(cov->typus)) {
      cov->nr = DOLLAR_PROC;
      return structSproc(cov, newmodel); // kein S-TRUCT !!
    } 

    ASSERT_NEWMODEL_NOT_NULL;
    if (Aniso!= NULL) {
      GERR("complicated models including arbitrary functions for Aniso cannot be simulated yet");
    } 

    if (cov->key != NULL) COV_DELETE(&(cov->key));

    if (cov->prevloc->distances) 
      GERR("distances do not allow for more sophisticated simulation methods");
    
    if ((err = STRUCT(next, newmodel)) > NOERROR) return err;

    addModel(newmodel, DOLLAR);
    if (!PisNULL(DVAR)) kdefault(*newmodel, DVAR, P0(DVAR));
    if (!PisNULL(DSCALE) ) kdefault(*newmodel, DSCALE, P0(DSCALE));
    if (!next->deterministic) GERR("random shapes not programmed yet");
    
    break;
 
  default :
    //  PMI(cov, "structS");
    GERR1("changes in scale/variance not programmed yet for '%s'", 
	  ROLENAMES[cov->role]);      
    }
  
  ErrorHandling:
  
    if (dummy != NULL) COV_DELETE(&dummy);
    if (local != NULL) COV_DELETE(&local);

   
  return err;
}




int initS(cov_model *cov, storage *s){
  // am liebsten wuerde ich hier die Koordinaten transformieren;
  // zu grosser Nachteil ist dass GetDiameter nach trafo 
  // grid def nicht mehr ausnutzen kann -- umgehbar?!
  cov_model *next = cov->sub[DOLLAR_SUB],
    *varM = cov->kappasub[DVAR],
    *scaleM = cov->kappasub[DSCALE],
    *anisoM = cov->kappasub[DANISO],
    *anisoleftM = cov->kappasub[DALEFT],
    *projM = cov->kappasub[DPROJ];
  int 
    vdim = cov->vdim2[0],
    nm = cov->mpp.moments,
    nmvdim = (nm + 1) * vdim,
    err = NOERROR;
  bool 
    maxstable = hasExactMaxStableRole(cov);// Realisationsweise 

  if (hasAnyShapeRole(cov)) { // Average  !! ohne maxstable selbst !!
    double
      var[MAXMPPVDIM],
      scale = PisNULL(DSCALE)  ? 1.0 : P0(DSCALE);
    int i,
      dim = cov->tsdim;
    

    if (!PisNULL(DPROJ) || !PisNULL(DALEFT) || projM!= NULL || 
	anisoM!=NULL||
	(anisoleftM != NULL && 
	 (CovList[anisoleftM->nr].check != checkAngle || !anisoleftM->deterministic)
			     )) {

       //      APMI(cov);

       SERR("(complicated) anisotropy ond projection not allowed yet in Poisson related models");
     }


    // Achtung I-NIT_RANDOM ueberschreibt mpp.* !!
    if (varM != NULL) {
      int nm_neu = nm == 0 && !maxstable ? 1 : nm;
      if ((err = INIT_RANDOM(varM, nm_neu, s, P(DVAR))) != NOERROR) return err; 
      
      int nmP1 = varM->mpp.moments + 1;
      for (i=0; i<vdim; i++) {
	int idx = i * nmP1;
	var[i] = maxstable ? P0(DVAR) : varM->mpp.mM[idx + 1];      
      }
    } else for (i=0; i<vdim; var[i++] = P0(DVAR));

    if (scaleM != NULL) {
      if (dim + nm < 1) SERR("found dimension <= 0");
      int dim_neu = maxstable ? nm : (dim + nm) < 1 ? 1 : dim + nm; 
    if ((err = INIT_RANDOM(scaleM, dim_neu, s, P(DSCALE)))
	!= NOERROR) return err;
      scale = maxstable ? P0(DSCALE) : scaleM->mpp.mM[1];      
    }
    if ((err = INIT(next, nm, s)) != NOERROR) return err;


    for (i=0; i < nmvdim; i++) {
      cov->mpp.mM[i] = next->mpp.mM[i]; 
      cov->mpp.mMplus[i] = next->mpp.mMplus[i]; 
    }

    if (varM != NULL && !maxstable) {
      for (i=0; i < nmvdim; i++) {
	cov->mpp.mM[i] *= varM->mpp.mM[i];
	cov->mpp.mMplus[i] *= varM->mpp.mMplus[i];
      }
    } else {
      int j, k;
      double pow_var;
      for (k=j=0; j<vdim; j++) { 
	pow_var = 1.0;
	for (i=0; i <= nm; i++, pow_var *= var[j], k++) {	
	  cov->mpp.mM[k] *= pow_var;
	  cov->mpp.mMplus[k] *= pow_var;
	}
      }
    }
    if (scaleM != NULL && !maxstable) {
      if (scaleM->mpp.moments < dim) SERR("moments can not be calculated.");
      int j, k,
	nmP1 = scaleM->mpp.moments + 1;
      for (k=j=0; j<vdim; j++) { 
	double pow_scale = scaleM->mpp.mM[dim + j * nmP1];
	for (i=0; i <= nm; i++, k++) {
	  cov->mpp.mM[k] *= pow_scale;
	  cov->mpp.mMplus[k] *= pow_scale;
	}
      }
    } else {
      double pow_scale = pow(scale, dim);
      for (i=0; i < nmvdim; i++) {
	cov->mpp.mM[i] *= pow_scale;
	cov->mpp.mMplus[i] *= pow_scale;
      }
    }

    if (!PisNULL(DANISO)) {      
      if (cov->nrow[DANISO] != cov->ncol[DANISO])
	SERR("only square anisotropic matrices allowed");
      double invdet = fabs(1.0 / getDet(P(DANISO), cov->nrow[DANISO]));
      if (!R_FINITE(invdet))
	SERR("determinant of the anisotropy matrix could not be calculated.");
      
      for (i=0; i < nmvdim; i++) {
	cov->mpp.mM[i] *= invdet;
	cov->mpp.mMplus[i] *= invdet;
      }

    }

    if (anisoleftM != NULL) {  
      assert(anisoM == NULL && CovList[anisoleftM->nr].check == checkAngle);
      int 
	ncol = anisoleftM->vdim2[0],
	nrow = anisoleftM->vdim2[1];
      if (nrow != ncol) SERR("only square anisotropic matrices allowed");
      double invdet,
	*diag = PARAM(anisoleftM, ANGLE_DIAG);
      if (diag != NULL) {
	invdet = 1.0;
	for (i=0; i<ncol; i++) invdet /= diag[i];
      } else {
	invdet = PARAM0(anisoleftM, ANGLE_RATIO);
      }
      invdet = fabs(invdet);      
      if (!R_FINITE(invdet))
	SERR("determinant of the anisotropy matrix could not be calculated.");
      
      for (i=0; i < nmvdim; i++) {
	cov->mpp.mM[i] *= invdet;
	cov->mpp.mMplus[i] *= invdet;
      }

    }

    
 
    if (R_FINITE(cov->mpp.unnormedmass)) {
      if (vdim > 1) BUG;
      cov->mpp.unnormedmass = next->mpp.unnormedmass * var[0];
    } else {
      for (i=0; i<vdim; i++)
	cov->mpp.maxheights[i] = next->mpp.maxheights[i] * var[i];
    }
 
    //    printf("inis %f %f %d %f\n", cov->mpp.maxheights[0] , next->mpp.maxheights[0] , varM == NULL, varM == NULL ? P0(DVAR) : 1.0);
 
    //    cov->mpp.refradius *= scale;
    //cov->mpp.refsd *= scale;
  }


  else if (cov->role == ROLE_GAUSS) {
    cov_model 
      *key = cov->key,
      *sub = key == NULL ? next : key;
    assert(sub != NULL);

    assert(key == NULL || ({PMI(cov);false;}));//
      
   
    if ((err=INIT(sub, 0, s)) != NOERROR) return err;
   
  }

  else if (cov->role == ROLE_BASE) {
    cov_model 
      *key = cov->key,
      *sub = key == NULL ? next : key;
    assert(sub != NULL);

    assert(key == NULL || ({PMI(cov);false;}));//
      
   
    if ((err=INIT(sub, 0, s)) != NOERROR) return err;
    
  } else SERR("initiation of scale and/or variance failed");

 if ((err = TaylorS(cov)) != NOERROR) return err;

  //  APMI(cov);

 return NOERROR;

}


void doS(cov_model *cov, storage *s){
   cov_model
     *varM = cov->kappasub[DVAR],
     *scaleM = cov->kappasub[DSCALE];
   int i,
     vdim = cov->vdim2[0];

    if (varM != NULL && !varM->deterministic && !isRandom(varM)) {
      assert(!PisNULL(DVAR));
      DO(varM, s);
    }
    
    if (scaleM != NULL && !scaleM->deterministic && !isRandom(scaleM)) {
      assert(!PisNULL(DSCALE));
      DO(scaleM, s);
    }

    //    PMI(cov);


  if (hasAnyShapeRole(cov)) {
    cov_model *next = cov->sub[DOLLAR_SUB];
         
    DO(next, s);// nicht gatternr
    for (i=0; i<vdim; i++)
      cov->mpp.maxheights[i] = next->mpp.maxheights[i] * P0(DVAR);
    return;
  }  
  
  else if (cov->role == ROLE_GAUSS) {    
    double 
      *res = cov->rf,
      sd = sqrt(P0(DVAR));
    int 
      totalpoints = Gettotalpoints(cov);
    assert(res != NULL);
    if (cov->key == NULL) error("Unknown structure in 'doS'.");

    DO(cov->key, s); 

    //    PMI(cov);
    //    printf("totalpoints");

    if (sd != 1.0) for (i=0; i<totalpoints; i++) res[i] *= (res_type) sd;
    return;
  } 

  //PMI(cov->calling->calling);
  //crash();

  ERR("unknown option in 'doS' "); 
}



int checkplusmal(cov_model *cov) {
  cov_model *sub;
  int i, j, err, dim, xdim, role;
  
  assert(cov->Splus == NULL);  
  dim = cov->tsdim;
  xdim = cov->xdimown;
  role = cov->role;
  
  for (i=0; i<cov->nsub; i++) {

    sub = cov->sub[i];
     
    if (sub == NULL) 
      SERR("+ or *: named submodels are not given in a sequence!");

    Types type = cov->typus;
    domain_type dom = cov->domown;
    isotropy_type iso = cov->isoown;

    //    printf("start\n");
    err = ERRORTYPECONSISTENCY;
    for (j=0; j<=1; j++) { // nur trend als abweichender typus erlaubt
      // 
      //printf("type = %s %s %d\n", TYPENAMES[type], NICK(sub), j);
  
      //  if (dom == XONLY) { printf("loeschen\n");continue;}

    if (TypeConsistency(type, sub) &&
	  (err = CHECK(sub, dim, xdim, type, dom, iso, 
		       i == 0 ? SUBMODEL_DEP : cov->vdim2[0], role))
	  == NOERROR) break;

    //assert(err == ERRORTYPECONSISTENCY);

      type = TrendType;
      dom = XONLY;
      iso = CARTESIAN_COORD;
    }
    //      printf("OK err=%d\n", err);
    // MERR(err);
    
   if (err != NOERROR) {
      //printf("sub %d %s\n", i, NICK(sub));
      //      APMI(cov);
      return err;
    }

   //   printf("A OK err=%d\n", err);

    if (cov->typus == sub->typus) {
      //printf("cov->iso = %d %d\n", cov->isoown, sub->isoprev);
      setbackward(cov, sub);
    } else {  
      updatepref(cov, sub);
      cov->tbm2num |= sub->tbm2num;
      if (CovList[cov->nr].vdim == SUBMODEL_DEP && 
	  (sub==cov->sub[0] || sub==cov->key)) { // strange todo
	cov->vdim2[0] = sub->vdim2[0];
	cov->vdim2[1] = sub->vdim2[1];
      }
      cov->deterministic &= sub->deterministic;
    };
    //  printf("C OK err=%d\n", err);

    if (i==0) {
      cov->vdim2[0]=sub->vdim2[0];  // to do: inkonsistent mit vorigen Zeilen !!
      cov->vdim2[1]=sub->vdim2[1];  // to do: inkonsistent mit vorigen Zeilen !!
      if (cov->vdim2[0] <= 0) BUG;
      cov->matrix_indep_of_x = sub->matrix_indep_of_x;
    } else {
      cov->matrix_indep_of_x &= sub->matrix_indep_of_x;
      if (cov->vdim2[0] != sub->vdim2[0] || cov->vdim2[1] != sub->vdim2[1]) {
	SERR4("multivariate dimensionality is different in the submodels (%s is %d-variate; %s is %d-variate)", NICK(cov->sub[0]), cov->vdim2[0], NICK(sub), sub->vdim2[0]);
      }
    }
  }

  // !! incorrect  !!
  cov->semiseparatelast = false; 
  cov->separatelast = false; 

  //  printf("OK err=%d ende\n", err);
   //  PMI(cov, -1);

  return NOERROR;
}





// see private/old.code/operator.cc for some code including variable locations
void select(double *x, cov_model *cov, double *v) {
  int len,
    *element = PINT(SELECT_SUBNR);
  cov_model *sub = cov->sub[*element];
  assert(cov->vdim2[0] == cov->vdim2[1]);
  if (*element >= cov->nsub) error("select: element out of range");
  COV(x, sub, v);
  if ( (len = cov->nrow[SELECT_SUBNR]) > 1) {
    int i, m,
      vdim = cov->vdim2[0],
      vsq = vdim * vdim;
    double *z = cov->Sdollar->z;
    if (z == NULL) z = cov->Sdollar->z =(double*) MALLOC(sizeof(double) * vsq);
    for (i=1; i<len; i++) {
      sub = cov->sub[element[i]];
      COV(x, sub, z);
      for (m=0; m<vsq; m++) v[m] += z[m]; 
    }
  }
}
  

void covmatrix_select(cov_model *cov, double *v) {
  int len = cov->nrow[SELECT_SUBNR];
  
  if (len == 1) {
    int element = P0INT(SELECT_SUBNR);
    cov_model *next = cov->sub[element];
    if (element >= cov->nsub) error("select: element out of range");
    CovList[next->nr].covmatrix(next, v);

    // {  int i,j,k, tot=Loc(cov)->totalpoints; printf("\nXcovmat select\n");
    //  for (k=i=0; i<tot*tot; i+=tot) {
    //   for (j=0; j<tot; j++) printf("%f ", v[k++]);
    //  printf("\n");  }}

    // crash();
    //  PMI(next);
      
  }  else StandardCovMatrix(cov, v);
}

char iscovmatrix_select(cov_model VARIABLE_IS_NOT_USED *cov) {  return 2; }

int checkselect(cov_model *cov) {
  int err;

  assert(cov->Splus == NULL);

  kdefault(cov, SELECT_SUBNR, 0);
  if ((err = checkplus(cov)) != NOERROR) return err;

  if ((err = checkkappas(cov)) != NOERROR) return err;

  if (cov->Sdollar != NULL && cov->Sdollar->z != NULL)
    DOLLAR_DELETE(&(cov->Sdollar));
  if (cov->Sdollar == NULL) {
    cov->Sdollar = (dollar_storage*) MALLOC(sizeof(dollar_storage));
    DOLLAR_NULL(cov->Sdollar);
  } 
  assert(cov->Sdollar->z == NULL);

  return NOERROR;
}


void rangeselect(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){
  range->min[SELECT_SUBNR] = 0;
  range->max[SELECT_SUBNR] = MAXSUB-1;
  range->pmin[SELECT_SUBNR] = 0;
  range->pmax[SELECT_SUBNR] = MAXSUB-1;
  range->openmin[SELECT_SUBNR] = false;
  range->openmax[SELECT_SUBNR] = false;
}




void plusStat(double *x, cov_model *cov, double *v){
  cov_model *sub;
  int i, m,
    nsub=cov->nsub,
    vdim = cov->vdim2[0],
    vsq = vdim * vdim;
  assert(cov->vdim2[0] == cov->vdim2[1]);
  assert(cov->Sdollar != NULL);
  double *z = cov->Sdollar->z;
    if (z == NULL) z = cov->Sdollar->z = (double*) MALLOC(sizeof(double) * vsq);
  
  //PMI(cov->calling);
  //print("%d %s %s\n", vsq, NICK(cov->sub[0])),
  for (m=0; m<vsq; m++) v[m] = 0.0;
  for(i=0; i<nsub; i++) {
    sub = cov->sub[i];
    if (cov->typus == sub->typus) {
      COV(x, sub, z);
      for (m=0; m<vsq; m++) v[m] += z[m]; 
    }

    // if (cov->calling != NULL) printf("stat i=%d %f %f\n", i, v[m], z[m]);
    //  crash(cov);
    //  APMI(cov);
  }
  
  //  printf("plus x=%f %f\n", *x, *v);
}

void plusNonStat(double *x, double *y, cov_model *cov, double *v){
  cov_model *sub;
  int i, m,
    nsub=cov->nsub,
    vdim = cov->vdim2[0],
    vsq = vdim * vdim;
  assert(cov->vdim2[0] == cov->vdim2[1]);
  assert(cov->Sdollar != NULL);
  double *z = cov->Sdollar->z;
  if (z == NULL) z = cov->Sdollar->z = (double*) MALLOC(sizeof(double) * vsq);
  for (m=0; m<vsq; m++) v[m] = 0.0;
  for(i=0; i<nsub; i++) {
    sub = cov->sub[i];
    if (cov->typus == sub->typus) {
      NONSTATCOV(x, y, sub, z);
      for (m=0; m<vsq; m++) v[m] += z[m]; 
    }

    //printf("i=%d %f %f\n", i, v[0], z[0]);
  }
  // printf("plus nonstat x=%f %f\n", *x, *v);
}

void Dplus(double *x, cov_model *cov, double *v){
  cov_model *sub;
  double v1;
  int n = cov->nsub, i;
  *v = 0.0;
  for (i=0; i<n; i++) { 
    sub = cov->sub[i];
    if (cov->typus == sub->typus) {
      Abl1(x, sub, &v1);
      (*v) += v1;
    }
  }
}

void DDplus(double *x, cov_model *cov, double *v){
  cov_model *sub;
  double v1;
  int n = cov->nsub, i;
  *v = 0.0;
  for (i=0; i<n; i++) { 
    sub = cov->sub[i];
    if (cov->typus == sub->typus) {
      Abl2(x, sub, &v1);
      (*v) += v1;
    }
  }
}


int checkplus(cov_model *cov) {
  int err, i;
  if ((err = checkplusmal(cov)) != NOERROR) {
    return err;
  }
  
  if (cov->domown == STAT_MISMATCH) return ERRORNOVARIOGRAM;
  if (cov->nsub == 0) cov->pref[SpectralTBM] = PREF_NONE;

  if (isPosDef(cov) && cov->domown == XONLY) cov->logspeed = 0.0;
  else if (isNegDef(cov) && cov->domown == XONLY) {
    cov->logspeed = 0.0;
    for (i=0; i<cov->nsub; i++) {
      cov_model *sub = cov->sub[i];
      if (cov->typus == sub->typus) {
	if (ISNAN(sub->logspeed)) {
	  cov->logspeed = RF_NA;
	  break;
	} else cov->logspeed += sub->logspeed;
      }
    }
  } else cov->logspeed = RF_NA;

 if (cov->Sdollar != NULL && cov->Sdollar->z != NULL)
    DOLLAR_DELETE(&(cov->Sdollar));
  if (cov->Sdollar == NULL) {
    cov->Sdollar = (dollar_storage*) MALLOC(sizeof(dollar_storage));
    DOLLAR_NULL(cov->Sdollar);
  } 
  assert(cov->Sdollar->z == NULL);
  return NOERROR;

  // spectral mit "+" funktioniert, falls alle varianzen gleich sind,
  // d.h. nachfolgend DOLLAR die Varianzen gleich sind oder DOLLAR nicht
  // auftritt; dann zufaellige Auswahl unter "+"
}


bool Typeplus(Types required, cov_model *cov) {
  // assert(false);
  bool allowed = TypeConsistency(ShapeType, required) || required==TrendType;
    //||required==ProcessType ||required==GaussMethodType; not yet allowed;to do

  // printf("allowed %d\n", allowed);
  if (!allowed) return false;
  int i;
  for (i=0; i<cov->nsub; i++) {
    //printf("type plus %s %s\n",TYPENAMES[required], NICK(cov->sub[i]));
    if (TypeConsistency(required, cov->sub[i])) return true;
  }  
  // printf("none\n");
  
  return false;
}

void spectralplus(cov_model *cov, storage *s, double *e){
  int nr;
  double dummy;
  spec_properties *cs = &(s->spec);
  double *sd_cum = cs->sub_sd_cum;

  nr = cov->nsub - 1;
  dummy = UNIFORM_RANDOM * sd_cum[nr];
  if (ISNAN(dummy)) BUG;
  while (nr > 0 && dummy <= sd_cum[nr - 1]) nr--;
  cov_model *sub = cov->sub[nr];
  SPECTRAL(sub, s, e);  // nicht gatternr
}


int structplus(cov_model *cov, cov_model VARIABLE_IS_NOT_USED **newmodel){
  int m, err;  
  switch(cov->role) {
  case ROLE_COV :  return NOERROR;
  case ROLE_GAUSS : 
    if (isProcess(cov->typus)) {
      //assert(cov->calling != NULL && (isInterface(cov->calling->typus) ||
      ///				      isProcess(cov->calling->typus)));
      assert(cov->nr != PLUS_PROC);
      assert(cov->nr == PLUS);
      //cov->nr = PLUS_PROC;
      BUG;
      //return structplusproc(cov, newmodel); // kein S-TRUCT !!
    }
    if (cov->Splus != NULL) BUG;
    for (m=0; m<cov->nsub; m++) {
      cov_model *sub = cov->sub[m];
      if ((err = STRUCT(sub, newmodel))  > NOERROR) {
      //	PMI(cov->Splus->keys[m]);
	//	assert(false);
	//printf("end plus\n");
	return err;
      }
    }
    break;
  default :
    SERR2("role '%s' not allowed for '%s'", ROLENAMES[cov->role],
	  NICK(cov));
  }
  return NOERROR;
}


int initplus(cov_model *cov, storage *s){
  int i, err,
    vdim = cov->vdim2[0];
  if (cov->vdim2[0] != cov->vdim2[1]) BUG;

  for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = RF_NA;

  if (cov->role == ROLE_GAUSS) {
    spec_properties *cs = &(s->spec);
    double *sd_cum = cs->sub_sd_cum;
 
    for (i=0; i<cov->nsub; i++) {
      cov_model *sub = cov->Splus == NULL ? cov->sub[i] : cov->Splus->keys[i];

      //print("initplus %d %d initialised=%d\n",i,cov->nsub, sub->initialised);
      //PMI(sub);
      
      if (sub->pref[Nothing] > PREF_NONE) { // to do ??
	// for spectral plus only
	COV(ZERO, sub, sd_cum + i);
	if (i>0) sd_cum[i] += sd_cum[i-1];
      }
      cov->sub[i]->stor = (storage *) MALLOC(sizeof(storage));
      if ((err = INIT(sub, cov->mpp.moments, s)) != NOERROR) {
	//  AERR(err);
	return err;
      }
      sub->simu.active = true;
    }

    // assert(false);
  
    cov->fieldreturn = cov->Splus != NULL;
    cov->origrf = false;
    if (cov->Splus != NULL) cov->rf = cov->Splus->keys[0]->rf;
     
    return NOERROR;
  }

  /*
    pref_type pref;

    for (m=0; m<Forbidden; m++) pref[m] = PREF_BEST;
    if (meth->cov->user[0] == 0 || meth->cov->user[1] == 0) {
 
    // i.e. user defined
    pref_type pref;
    pref[CircEmbed] = pref[TBM] = pref[Direct] = pref[Sequential] 
    = PREF_NONE;
    for (m=0; m<Nothing; m++) { // korrekt auch fuer MaxStable?
    //	  print("%d %d\n", m, pref[m]);
    //	  print("%d %d\n", meth->cov->user[m]);
    if (pref[m] > 0 &&  meth->cov->user[m] > 0) {
    break;
    }
    }
    if (m == Nothing) return ERRORSUBMETHODFAILED;
    }
  */
     
  else if (cov->role == ROLE_COV) {    
    return NOERROR;
  }

  return ERRORFAILED;
}


void doplus(cov_model *cov, storage *s) {
  int i;
  
  if (cov->role == ROLE_GAUSS && cov->method==SpectralTBM) {
    ERR("error in doplus with spectral");
  }
  
  for (i=0; i<cov->nsub; i++) {
    cov_model *sub = cov->Splus==NULL ? cov->sub[i] : cov->Splus->keys[i];
    DO(sub, s);
  }
}




void covmatrix_plus(cov_model *cov, double *v) {
  location_type *loc = Loc(cov);
  //  cov_fct *C = CovList + cov->nr; // nicht gatternr
  int i, 
    totalpoints = loc->totalpoints,
    vdimtot = totalpoints * cov->vdim2[0],
    vdimtotSq = vdimtot * vdimtot,
    nsub = cov->nsub;
  bool is = iscovmatrix_plus(cov) >= 2;
  double *mem = NULL;
  if (is && nsub>1) {
    mem = cov->Sdollar->y;
    if (mem == NULL) 
      mem = cov->Sdollar->y = (double*) MALLOC(sizeof(double) * vdimtotSq);
    is = mem != NULL;
  }
  
  if (is) {
    // APMI(cov);

    //cov_model *sub = cov->sub[0];
    int j;
    if (PisNULL(SELECT_SUBNR)) PALLOC(SELECT_SUBNR, 1, 1);
    P(SELECT_SUBNR)[0] = 0;
    CovList[SELECT].covmatrix(cov, v);
    for (i=1; i<nsub; i++) {
      if (Loc(cov->sub[i])->totalpoints != totalpoints) BUG;
      P(SELECT_SUBNR)[0] = i;
      CovList[SELECT].covmatrix(cov, mem);
      for (j=0; j<vdimtotSq; j++) v[j] += mem[j];
    }
  } else StandardCovMatrix(cov, v);  
}

char iscovmatrix_plus(cov_model *cov) {
  char max=0, is;
  int i,
    nsub = cov->nsub;
  for (i=0; i<nsub; i++) {
    cov_model *sub = cov->sub[i];
    is = CovList[sub->nr].is_covmatrix(sub);
    if (is > max) max = is;
  }
  return max;
}


void malStat(double *x, cov_model *cov, double *v){
  cov_model *sub;
  int i, m,
    nsub=cov->nsub,
    vdim = cov->vdim2[0],
    vsq = vdim * vdim;
  assert(cov->Sdollar != NULL);
  double *z = cov->Sdollar->z;
  if (z == NULL) z = cov->Sdollar->z =(double*) MALLOC(sizeof(double) * vsq);
  
  assert(x[0] >= 0.0 || cov->xdimown > 1);
  for (m=0; m<vsq; m++) v[m] = 1.0;
  for(i=0; i<nsub; i++) {
    sub = cov->sub[i];
    COV(x, sub, z);
    for (m=0; m<vsq; m++) v[m] *= z[m]; 
  }
}

void logmalStat(double *x, cov_model *cov, double *v, double *sign){
  cov_model *sub;
  int i, m,
    nsub=cov->nsub,
    vdim = cov->vdim2[0],
    vsq = vdim * vdim;
  double *z = cov->Sdollar->z,
    *zsign = cov->Sdollar->z2;
  assert(cov->vdim2[0] == cov->vdim2[1]);
  if (z == NULL) z = cov->Sdollar->z = (double*) MALLOC(sizeof(double) * vsq);
  if (zsign == NULL) 
    zsign = cov->Sdollar->z2 = (double*) MALLOC(sizeof(double) * vsq);
  
  assert(x[0] >= 0.0 || cov->xdimown > 1);
  for (m=0; m<vsq; m++) {v[m] = 0.0; sign[m]=1.0;}
  for(i=0; i<nsub; i++) {
    sub = cov->sub[i];
    LOGCOV(x, sub, z, zsign);
    for (m=0; m<vsq; m++) {
      v[m] += z[m]; 
      sign[m] *= zsign[m];
    }
  }
}

void malNonStat(double *x, double *y, cov_model *cov, double *v){
  cov_model *sub;
  int i, m, nsub=cov->nsub,
    vdim = cov->vdim2[0],
    vsq = vdim * vdim;

  assert(cov->vdim2[0] == cov->vdim2[1]);

  assert(cov->Sdollar != NULL);
  double *z = cov->Sdollar->z;
  assert(cov->vdim2[0] == cov->vdim2[1]);
  if (z == NULL) z = cov->Sdollar->z =(double*) MALLOC(sizeof(double) * vsq);
  for (m=0; m<vsq; m++) v[m] = 1.0;
  for(i=0; i<nsub; i++) {
    sub = cov->sub[i];
    NONSTATCOV(x, y, sub, z);
    for (m=0; m<vsq; m++) v[m] *= z[m]; 
  }
}

void logmalNonStat(double *x, double *y, cov_model *cov, double *v, 
		   double *sign){
  cov_model *sub;
  int i, m, nsub=cov->nsub,
    vdim = cov->vdim2[0],
    vsq = vdim * vdim;
  assert(cov->Sdollar != NULL);
  assert(cov->vdim2[0] == cov->vdim2[1]);
  double *z = cov->Sdollar->z,
    *zsign = cov->Sdollar->z2;
  if (z == NULL) z = cov->Sdollar->z = (double*) MALLOC(sizeof(double) * vsq);
  if (zsign == NULL) 
    zsign = cov->Sdollar->z2 = (double*) MALLOC(sizeof(double) * vsq);
  for (m=0; m<vsq; m++) {v[m] = 0.0; sign[m]=1.0;}
  for(i=0; i<nsub; i++) {
    sub = cov->sub[i];
    LOGNONSTATCOV(x, y, sub, z, zsign);
    for (m=0; m<vsq; m++) {
      v[m] += z[m]; 
      sign[m] *= zsign[m];
    }
  }
}

void Dmal(double *x, cov_model *cov, double *v){
  cov_model *sub;
  double c[MAXSUB], d[MAXSUB];
  int n = cov->nsub, i;
  for (i=0; i<n; i++) {
    sub = cov->sub[i];
    COV(x, sub, c + i);
    Abl1(x, sub, d + i);
  }
  *v = 0.0;
  for (i=0; i<n; i++) {
    double zw = d[i];
    int j;
    for (j=0; j<n; j++) if (j!=i) zw *= c[j]; 
    (*v) += zw; 
  }
}
int checkmal(cov_model *cov) {
  cov_model *next1 = cov->sub[0];
  cov_model *next2 = cov->sub[1];
  int err;

  if (next2 == NULL) next2 = next1;
  
  if ((err = checkplusmal(cov)) != NOERROR) return err;

  if (cov->domown == STAT_MISMATCH || !isPosDef(cov)) {
    return ERRORNOVARIOGRAM;
  }
  cov->logspeed = cov->domown == XONLY ? 0 : RF_NA;
  
  if (cov->xdimown >= 2) cov->pref[TBM] = PREF_NONE;
  if (cov->xdimown==2 && cov->nsub == 2 && 
      isAnyDollar(next1) && isAnyDollar(next2)) {
    double *aniso1 = PARAM(next1, DANISO),
      *aniso2 = PARAM(next2, DANISO);
    if (aniso1 != NULL && aniso2 != NULL) {
      if (aniso1[0] == 0.0 && next1->ncol[DANISO] == 1) {
	cov->pref[TBM] = next2->pref[TBM];
      } else if (aniso2[0] == 0.0 && next2->ncol[DANISO] == 1) {
	cov->pref[TBM] = next1->pref[TBM];
      }
    }
  }
 if (cov->Sdollar != NULL && cov->Sdollar->z != NULL)
    DOLLAR_DELETE(&(cov->Sdollar));
  if (cov->Sdollar == NULL) {
    cov->Sdollar = (dollar_storage*) MALLOC(sizeof(dollar_storage));
    DOLLAR_NULL(cov->Sdollar);
  } 
  assert(cov->Sdollar->z == NULL);
  return NOERROR;
}

bool Typemal(Types required, cov_model *cov) {
  bool allowed = required==PosDefType || required==ShapeType;
  //||required==NegDefType ||required==TcfType; to do; ist erlaubt unter
  // gewissen Bedingungen
  if (!allowed) return false;
  int i;
  for (i=0; i<cov->nsub; i++) {
    if (!TypeConsistency(required, cov->sub[i])) return false;
  }
  return true;
}


int initmal(cov_model *cov, storage VARIABLE_IS_NOT_USED *s){
//  int err;
//  return err;
  return ERRORFAILED;
  int i, 
    vdim = cov->vdim2[0];
  if (cov->vdim2[0] != cov->vdim2[1]) BUG;

  for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = RF_NA;
}
void domal(cov_model VARIABLE_IS_NOT_USED *cov, storage VARIABLE_IS_NOT_USED *s){
  BUG;
}


//////////////////////////////////////////////////////////////////////
// mpp-plus


#define PLUS_P 0 // parameter
int  CheckAndSetP(cov_model *cov){   
  int n,
    nsub = cov->nsub;
  double 
    cump = 0.0;
  if (PisNULL(PLUS_P)) {
    assert(cov->nrow[PLUS_P] == 0 && cov->ncol[PLUS_P] == 0);
    PALLOC(PLUS_P, nsub, 1);
    for (n=0; n<nsub; n++) P(PLUS_P)[n] = 1.0 / (double) nsub;    
  } else {
    cump = 0.0;
    for(n = 0; n<nsub; n++) {
      cump += P(PLUS_P)[n];
      //printf("cump %f %f\n",  cump , P(PLUS_P)[n]);
      if (cump > 1.0 && n+1<nsub) return ERRORATOMP; 
    }
    if (cump != 1.0) {
      if (nsub == 1) {
	warning("the p-values do not sum up to 1.\nHere only one p-value is given which must be 1.0");
	P(PLUS_P)[0] = 1.0;
      } else {
	//printf("%e\n", 1-P(PLUS_P)[nsub-1] );
	if (cump < 1.0 && P(PLUS_P)[nsub-1] == 0)
	  warning("The value of the last component of 'p' is increased."); 
	else SERR("The components of 'p' do not sum up to 1.");
	P(PLUS_P)[nsub-1] = 1.0 - (cump - P(PLUS_P)[nsub-1]);
      }  
    }
  }
  return NOERROR;
}

void kappamppplus(int i, cov_model *cov, int *nr, int *nc){
  *nr = cov->nsub;
  *nc = i < CovList[cov->nr].kappas ? 1 : -1;
}

void mppplus(double *x, cov_model *cov, double *v) { 
  int i, n,
    vdim = cov->vdim2[0],
    vdimSq = vdim * vdim;
  double *z = cov->Sdollar->z;
  if (z == NULL) z = cov->Sdollar->z =(double*) MALLOC(sizeof(double) * vdimSq);
  cov_model *sub;

  if (cov->role == ROLE_COV) {  
    for (i=0; i<vdimSq; i++) v[i] = 0.0;
    for (n=0; n<cov->nsub; n++, sub++) {
      sub = cov->sub[n];
      COV(x, sub, z); // urspruenglich : covlist[sub].cov(x, cov, z); ?!
      for (i=0; i<vdimSq; i++) v[i] += P(PLUS_P)[n] * z[i];
    }
  } else {
    assert(hasPoissonRole(cov));
    BUG;
  }
}

int checkmppplus(cov_model *cov) { 
  int err, 
    size = 1;
  cov->maxdim = MAXMPPDIM;
  
  if ((err = checkplusmal(cov)) != NOERROR) {
    // printf("checkmppplus error %d %s\n", err, ERRORSTRING);
    return err;
  }

  if ((err = CheckAndSetP(cov)) != NOERROR) return(err);

  if (cov->q == NULL) {
    if ((cov->q  = (double*) CALLOC(sizeof(double), size)) == NULL)
      return ERRORMEMORYALLOCATION;
    cov->qlen = size;
  }
 if (cov->Sdollar != NULL && cov->Sdollar->z != NULL)
    DOLLAR_DELETE(&(cov->Sdollar));
  if (cov->Sdollar == NULL) {
    cov->Sdollar = (dollar_storage*) MALLOC(sizeof(dollar_storage));
    DOLLAR_NULL(cov->Sdollar);
  } 
  assert(cov->Sdollar->z == NULL);
  
  return NOERROR;
}



void rangempplus(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){ 
  range->min[PLUS_P] = 0.0;
  range->max[PLUS_P] = 1.0;
  range->pmin[PLUS_P] = 0.0;
  range->pmax[PLUS_P] = 1.0;
  range->openmin[PLUS_P] = false;
  range->openmax[PLUS_P] = false;
}

int struct_mppplus(cov_model *cov, cov_model **newmodel){
  int m,
    //nsub = cov->nsub,
    err = NOERROR;

  //if (cov->calling == NULL || cov->calling->ownloc == NULL) 
  // SERR("mppplus does not seem to be used in the right context");
  
  if (!hasMaxStableRole(cov) && !hasPoissonRole(cov)) {
    SERR("method is not based on Poisson point process");
  }

  SERR("'mppplus not programmed yet");
  // Ausnahme: mppplus wird separat behandelt:
  // if (nr == MPPPLUS) return S TRUCT(shape, NULL);
 
  if (newmodel == NULL) BUG;  
  if (cov->Splus == NULL) {
    cov->Splus = (plus_storage*) MALLOC(sizeof(plus_storage));
    PLUS_NULL(cov->Splus);
  }

  for (m=0; m<cov->nsub; m++) {
    cov_model *sub = cov->sub[m];          
    if (cov->Splus->keys[m] != NULL) COV_DELETE(cov->Splus->keys + m);    
    if ((err = covcpy(cov->Splus->keys + m, sub)) != NOERROR) return err;
    if ((err = addShapeFct(cov->Splus->keys + m)) != NOERROR) return err;
    cov->Splus->keys[m]->calling = cov;
  }
  return NOERROR;
}


int init_mppplus(cov_model *cov, storage *S) {
  cov_model  *sub;
  double M2[MAXMPPVDIM], M2plus[MAXMPPVDIM], Eplus[MAXMPPVDIM], 
    maxheight[MAXMPPVDIM];
  int i,n, err,
    vdim = cov->vdim2[0];
  if (cov->vdim2[0] != cov->vdim2[1]) BUG;
  if (vdim > MAXMPPVDIM) BUG;
  ext_bool 
    loggiven = SUBMODEL_DEP,
    fieldreturn = SUBMODEL_DEP;
  pgs_storage *pgs = NULL;
  for (i=0; i<vdim; i++) {
    maxheight[i] = RF_NEGINF;
    M2[i] = M2plus[i] = Eplus[i] = 0.0;
  }
  
  if (cov->Spgs != NULL) PGS_DELETE(&(cov->Spgs));
  if ((pgs = cov->Spgs = (pgs_storage*) MALLOC(sizeof(pgs_storage))) == NULL) 
    return ERRORMEMORYALLOCATION;
  PGS_NULL(pgs);
  pgs->totalmass = 0.0;
  
  for (n=0; n<cov->nsub; n++) {
    sub = cov->sub[n];
    //if (!sub->mpp.loc_done) 
    //SERR1("submodel %d of '++': the location of the shapes is not defined", 
    // n);
    if ((err = INIT(sub, cov->mpp.moments, S)) != NOERROR) return err;
    //if (!sub->mpp.loc_done) SERR("locations are not initialised");
    if (n==0) {
      loggiven = sub->loggiven;
      fieldreturn = sub->fieldreturn;
    } else {
      if (loggiven != sub->loggiven) loggiven = SUBMODEL_DEP;
      if (fieldreturn != sub->loggiven) fieldreturn = SUBMODEL_DEP;
    }
    pgs->totalmass += sub->Spgs->totalmass * P(PLUS_P)[n];
    for (i=0; i<vdim; i++)   
      if (cov->mpp.maxheights[i] > maxheight[i]) 
	maxheight[i] = cov->mpp.maxheights[i];
    loggiven &= cov->loggiven;

    // Achtung cov->mpp.mM2 und Eplus koennten nicht direkt gesetzt
    // werden, da sie vom CovList[sub->nr].Init ueberschrieben werden !!
    
    if (cov->mpp.moments >= 1) {
      int nmP1 = sub->mpp.moments + 1;
      for (i=0; i<vdim; i++) {
	int idx = i * nmP1;
	Eplus[i] += PARAM0(sub, PLUS_P) * sub->mpp.mMplus[idx + 1]; 
      }
      if (cov->mpp.moments >= 2) {
	for (i=0; i<vdim; i++) {
	  int idx = i * nmP1;
	  M2[i] += PARAM0(sub, PLUS_P)  * sub->mpp.mM[idx + 2];
	  M2plus[i] += PARAM0(sub, PLUS_P) * sub->mpp.mM[idx + 2];
	}
      }
    }
    //assert(sub->mpp.loc_done);
  }
  for (i=0; i<vdim; i++) cov->mpp.maxheights[i] = maxheight[i];
  //cov->mpp.refsd = RF_NA;
  //  cov->mpp.refradius = RF_NA;

  
  if (cov->mpp.moments >= 1) {
    int nmP1 = cov->mpp.moments + 1;
    for (i=0; i<vdim; i++) {
      int idx = i * nmP1;
      cov->mpp.mMplus[idx + 1] = Eplus[i];
      cov->mpp.mM[idx + 1] = RF_NA;
    }
    if (cov->mpp.moments >= 2) {
      for (i=0; i<vdim; i++) {
	 int idx = i * nmP1;
	 cov->mpp.mM[idx + 2] = M2[i];
	 cov->mpp.mMplus[idx + 2] = M2plus[i];
      }
    }
  }
  
  cov->loggiven = loggiven;
  cov->fieldreturn = fieldreturn;
  //cov->mpp.loc_done = true;       
  cov->origrf = false;
  cov->rf = NULL;

  return NOERROR;
}

void do_mppplus(cov_model *cov, storage *s) {
  cov_model *sub;
  double subselect = UNIFORM_RANDOM;
  int i, subnr,
    vdim = cov->vdim2[0];
  assert(cov->vdim2[0] == cov->vdim2[1]);
  for (subnr=0; (subselect -= PARAM0(cov->sub[subnr], PLUS_P)) > 0; subnr++);
  cov->q[0] = (double) subnr;
  sub = cov->sub[subnr];
  
  CovList[sub->nr].Do(sub, s);  // nicht gatternr
  for (i=0; i<vdim; i++)   
    cov->mpp.maxheights[i] = sub->mpp.maxheights[i];
  cov->fieldreturn = sub->fieldreturn;
  cov->loggiven = sub->loggiven;
  // PrintMPPInfo(cov, s);
}

//////////////////////////////////////////////////////////////////////
// PROCESSES
//////////////////////////////////////////////////////////////////////


int structSproc(cov_model *cov, cov_model **newmodel) {
  cov_model
    *next = cov->sub[DOLLAR_SUB],
    *Aniso = cov->kappasub[DALEFT];
  int dim, err; 
  // cov_model *sub;

  //   printf("%d\n", cov->role); assert(cov->role == ROLE_GAUSS);
  
  //   assert(false);

  //    assert(false);
 
   assert(isDollarProc(cov));
   //printf("her %d %d\n", cov->role, ROLE_GAUSS);

  if (Aniso != NULL && !Aniso->deterministic) {
     SERR("complicated models including arbitrary functions for 'Aniso' cannot be simulated yet");
  }



  switch (cov->role) {
  case ROLE_GAUSS :
    if (newmodel != NULL) 
      SERR1("unexpected call to structure of '%s'", NICK(cov));
    if (cov->key != NULL) COV_DELETE(&(cov->key));

    if (cov->prevloc->distances) 
      SERR("distances do not allow for more sophisticated simulation methods");
    
    if (Aniso!= NULL) {
      //A
      // crash();
      Transform2NoGrid(cov, false, true); 
      
      location_type *loc = Loc(cov);
      dim = loc->timespacedim;
      int i,
	bytes = dim * sizeof(double),
	total = loc->totalpoints;
      if (dim != Aniso->vdim2[0]) BUG;
      double *v = NULL,
	*x = loc->x;
      assert(x != NULL);
      assert(!loc->grid);
      if ((v = (double*) MALLOC(bytes)) == NULL) return ERRORMEMORYALLOCATION;
      for (i=0; i<total; i++, x+=dim) {
	FCTN(x, Aniso, v);
	//printf("x = %f %f  v=%f %f %d\n", x[0], x[1], v[0], v[1], bytes);
	MEMCOPY(x, v, bytes);
      }
      free(v);
      v = NULL;
    } else {
      Transform2NoGrid(cov, true, false); 
    }

    //assert(false);
    // printf("%f %f %f\n",
	   //	   Loc(cov)->xgr[0][0], Loc(cov)->xgr[0][1], Loc(cov)->xgr[0][2]);
    // assert(false);
 
    if ((err = covcpy(&(cov->key), next)) != NOERROR) return err;

    if (!isGaussProcess(next)) addModel(&(cov->key), GAUSSPROC);
    SetLoc2NewLoc(cov->key, Loc(cov));
    //   APMI(cov);assert(false);
    
    cov_model *key;
    key = cov->key;
    assert(key->calling == cov);    
    
    dim = key->prevloc->timespacedim;
    if ((err = CHECK(key, dim, dim, ProcessType, XONLY, CARTESIAN_COORD,
		     cov->vdim2, cov->role)) != NOERROR) return err;
    err = STRUCT(cov->key, NULL);
    //    cov->initialised = err==NOERROR && key->initialised;

    //    APMI(cov);

    return err;
  default :
    //  PMI(cov, "structS");
    SERR1("changes in scale/variance not programmed yet for '%s'", 
	  ROLENAMES[cov->role]);      
  }
     
  return NOERROR;
}




int initSproc(cov_model *cov, storage *s){
  // am liebsten wuerde ich hier die Koordinaten transformieren;
  // zu grosser Nachteil ist dass GetDiameter nach trafo 
  // grid def nicht mehr ausnutzen kann -- umgehbar?!

  //cov_model *next = cov->sub[DOLLAR_SUB];
  //mppinfotype *info = &(s->mppinfo);
  //  location_type *loc = cov->prevloc;
  int 
    err = NOERROR;

  //  assert(false);
  
  cov_model 
    *key = cov->key;
    //*sub = key == NULL ? next : key;
  location_type *prevloc = cov->prevloc;

  assert(key != NULL);
  
  if ((err = INIT(key, 0, s)) != NOERROR) {
    return err;
  }
  
  key->simu.active = true; 
  assert(s != NULL);

  cov->fieldreturn = true;

  if ((cov->origrf = cov->ownloc != NULL &&
       cov->ownloc->totalpoints != prevloc->totalpoints)) {
    assert(prevloc->grid);
    int dim = prevloc->timespacedim;
    if (cov->vdim2[0] != cov->vdim2[1]) BUG;
    cov->rf = (res_type*) MALLOC(sizeof(res_type) *
				 cov->vdim2[0] * 
				 prevloc->totalpoints);
    if (cov->Sdollar != NULL) DOLLAR_DELETE(&(cov->Sdollar));
    cov->Sdollar = (dollar_storage*) MALLOC(sizeof(dollar_storage));
    DOLLAR_NULL(cov->Sdollar);
 
    int d,
      *proj = PINT(DPROJ),
      bytes = dim * sizeof(int),
      *cumsum = cov->Sdollar->cumsum = (int*) MALLOC(bytes),
      *total = cov->Sdollar->total = (int*) MALLOC(bytes),
      *len = cov->Sdollar->len = (int*) MALLOC(bytes);     
    cov->Sdollar->nx = (int*) MALLOC(bytes); 
    
    for (d=0; d<dim; d++) {
      cumsum[d] = 0;
      len[d] = prevloc->xgr[d][XLENGTH];
    }
    if (proj != NULL) {
      int 
	nproj = cov->nrow[DPROJ];
      d = 0;
      cumsum[proj[d] - 1] = 1;
      for (d = 1; d < nproj; d++) {
	cumsum[proj[d] - 1] =
	  cumsum[proj[d - 1] - 1] * prevloc->xgr[d - 1][XLENGTH];
      }
      for (d=0; d<dim;d++) 
	total[d] = cumsum[d] * prevloc->xgr[d][XLENGTH];
    } else {
      int i,
	iold = 0,
	nrow = cov->nrow[DANISO],
	ncol = cov->ncol[DANISO];
      double *A = P(DANISO);
      for (d=0; d<ncol; d++, A += nrow) {
	for (i = 0; i < nrow && A[i] == 0.0; i++);
	if (i == nrow) i = nrow - 1;
	if (d > 0) {
	  cumsum[i] = cumsum[iold] * prevloc->xgr[d - 1][XLENGTH];
	} else { // d ==0
	  cumsum[i] = 1;
	}
	iold = i;
	for (i++; i < nrow; i++) if (A[i] != 0.0) BUG;  // just a check
      }
    }
  } else {
    cov->rf = cov->key->rf;
  }
 
  //  PMI(cov,-1); assert(false);
 
  return NOERROR;
}


void doSproc(cov_model *cov, storage *s){

  if (hasMaxStableRole(cov) || hasPoissonRole(cov)) {
    cov_model *next = cov->sub[DOLLAR_SUB];
    
    cov_model
      *varM = cov->kappasub[DVAR],
      *scaleM = cov->kappasub[DSCALE];
 
    int i,
      vdim = cov->vdim2[0];
    assert(cov->vdim2[0] == cov->vdim2[1]);

    if (varM != NULL && !varM->deterministic) {
      assert(!PisNULL(DVAR));
      VTLG_R(NULL, varM, P(DVAR));
    }

    if (scaleM != NULL && !scaleM->deterministic) {
      assert(!PisNULL(DSCALE));
      VTLG_R(NULL, scaleM, P(DSCALE));
    }
    
    DO(next, s);// nicht gatternr
    for (i=0; i<vdim; i++)   
      cov->mpp.maxheights[i] = next->mpp.maxheights[i] * P0(DVAR);

  }

  else if (cov->role == ROLE_GAUSS) {    
    assert(cov->key != NULL);
    double 
      *res = cov->key->rf,
      sd = sqrt(P0(DVAR));
    int i,
      totalpoints = Gettotalpoints(cov);

    DO(cov->key, s); 

    if (sd != 1.0) 
      for (i=0; i<totalpoints; i++) {
	res[i] *= (res_type) sd;
    }

  }
  
  else ERR("unknown option in 'doSproc' "); 

 
  if (cov->origrf) {
    assert(cov->prevloc->grid);
    int i, zaehler, d,
      dim = cov->prevloc->timespacedim,
      *cumsum = cov->Sdollar->cumsum,
      *nx = cov->Sdollar->nx,
      *len = cov->Sdollar->len,
      *total = cov->Sdollar->total;
    assert(cov->key != NULL);
    res_type
      *res = cov->rf,
      *rf = cov->key->rf;
    
    assert(nx != NULL && total != NULL && cumsum != NULL);
    for (d=0; d<dim; d++) {
      nx[d] = 0;
    }
    zaehler = 0;
    i = 0;      
    
    while (true) {
      res[i++] = rf[zaehler];
      // printf("%f %d %d", res[i], zaehler,i);
      d = 0;			
      nx[d]++;			
      zaehler += cumsum[d];	
      while (nx[d] >= len[d]) {	
	nx[d] = 0;		
	zaehler -= total[d];
	if (++d >= dim) break;	
	nx[d]++;			
	zaehler += cumsum[d];					
      }
      if (d >= dim) break;			
    }
  }

}


int checkplusmalproc(cov_model *cov) {
  cov_model *sub;
  int i, err, 
    dim = cov->tsdim, 
    xdim = cov->xdimown,
    role = cov->role;
  Types type = ProcessType; 
  domain_type dom = cov->domown;
  isotropy_type iso = cov->isoown;

  //PMI(cov);
  assert(cov->Splus != NULL);

  for (i=0; i<cov->nsub; i++) {

    sub = cov->Splus->keys[i];
     
    if (sub == NULL) 
      SERR("named submodels are not given in a sequence.");


    if (!TypeConsistency(type, sub)) return ERRORTYPECONSISTENCY;
    if ((err= CHECK(sub, dim, xdim, type, dom, iso, SUBMODEL_DEP, role))
	!= NOERROR) return err;

   if (i==0) {
      cov->vdim2[0]=sub->vdim2[0];  // to do: inkonsistent mit vorigen Zeilen !!
      cov->vdim2[1]=sub->vdim2[1];  // to do: inkonsistent mit vorigen Zeilen !!
    } else {
      if (cov->vdim2[0] != sub->vdim2[0] || cov->vdim2[1] != sub->vdim2[1]) {
	SERR("multivariate dimensionality must be equal in the submodels");
      }
    }

    //    printf("OK plusprocess\n");
  }

  return NOERROR;
}



int checkplusproc(cov_model *cov) {
  int err;
  if ((err = checkplusmalproc(cov)) != NOERROR) {
    return err;
  }
  return NOERROR;
}


int structplusmalproc(cov_model *cov, cov_model VARIABLE_IS_NOT_USED**newmodel){
  int m, err;

  switch(cov->role) {
  case ROLE_GAUSS : 
    {
      location_type *loc = Loc(cov);
      
      if (cov->Splus == NULL) {
	cov->Splus = (plus_storage*) MALLOC(sizeof(plus_storage));
	PLUS_NULL(cov->Splus);
      }
      
      for (m=0; m<cov->nsub; m++) {

	cov_model *sub = cov->sub[m];

	if (cov->Splus->keys[m] != NULL) COV_DELETE(cov->Splus->keys + m);
	if ((err =  covcpy(cov->Splus->keys + m, sub)) != NOERROR) {
	  return err;
	}
	assert(cov->Splus->keys[m] != NULL);
	assert(cov->Splus->keys[m]->calling == cov);
	
	if (PL >= PL_STRUCTURE) {
	  LPRINT("plus: trying initialisation of submodel #%d (%s).\n", m+1, 
		 NICK(sub));
	}
	
	addModel(cov->Splus->keys + m, GAUSSPROC);
	cov->Splus->keys[m]->calling = cov;
	//	cov_model *fst = cov; while (fst->calling != NULL) fst = fst->calling; 

	//	assert(false);

	err = CHECK(cov->Splus->keys[m], loc->timespacedim, loc->timespacedim,
		    ProcessType, XONLY, CARTESIAN_COORD, cov->vdim2, ROLE_GAUSS);
	if (err != NOERROR) {
	  //	  
	  return err;
	}
	//APMI(cov->Splus->keys[m]);
	
	//if (m==1) APMI(cov->Splus->keys[m]);
	if ((cov->Splus->struct_err[m] =
	     err = STRUCT(cov->Splus->keys[m], NULL))  > NOERROR) {
	  //	PMI(cov->Splus->keys[m]);
	  //	assert(false);
	  //printf("end plus\n");
	  //	  	  AERR(err);
	  return err;
	}
	//AERR(err);
	
	//     printf("structplusmal %d %d\n", m, cov->nsub);
	// PMI(cov->Splus->keys[m]);
      }
  
    
      //assert(false);
      return NOERROR;
    }
    
  default :
    SERR2("role '%s' not allowed for '%s'", ROLENAMES[cov->role],
	  NICK(cov));
  }

  return NOERROR;
}


int structplusproc(cov_model *cov, cov_model **newmodel){
  assert(cov->nr == PLUS_PROC);
  return structplusmalproc(cov, newmodel);
}


int structmultproc(cov_model *cov, cov_model **newmodel){
  assert(CovList[cov->nr].check == checkmultproc);
  return structplusmalproc(cov, newmodel);
}

int initplusmalproc(cov_model *cov, storage VARIABLE_IS_NOT_USED *s){
  int i, err,
    vdim = cov->vdim2[0];
 assert(cov->vdim2[0] == cov->vdim2[1]);

  for (i=0; i<vdim; i++)   
    cov->mpp.maxheights[i] = RF_NA;
  if (cov->Splus == NULL) BUG;

  if (cov->role == ROLE_GAUSS) {
 
    for (i=0; i<cov->nsub; i++) {
      //printf("i=%d\n", i);
      cov_model *sub = cov->Splus == NULL ? cov->sub[i] : cov->Splus->keys[i];
      assert(cov->sub[i]->stor==NULL);
      cov->sub[i]->stor = (storage *) MALLOC(sizeof(storage));
      if ((err = INIT(sub, 0, cov->sub[i]->stor)) != NOERROR) {
	return err;
      }
      sub->simu.active = true;
    }
    cov->simu.active = true;
    return NOERROR;
  }
    
  else {
    BUG;
  }

    return ERRORFAILED;

    // assert(false);
}

 
int initplusproc(cov_model *cov, storage VARIABLE_IS_NOT_USED *s){
  int err;
  if ((err = initplusmalproc(cov, s)) != NOERROR) return err;

  if (cov->role == ROLE_GAUSS) {
    cov->fieldreturn = cov->Splus != NULL;
    cov->origrf = false;
    if (cov->Splus != NULL) cov->rf = cov->Splus->keys[0]->rf;
     
    return NOERROR;
  }
     
  else {
    BUG;
  }

  return ERRORFAILED;
}


void doplusproc(cov_model *cov, storage VARIABLE_IS_NOT_USED *s) {
  int m, i,
    total = cov->prevloc->totalpoints * cov->vdim2[0];
  double *res = cov->rf;
 assert(cov->vdim2[0] == cov->vdim2[1]);

  if (cov->role == ROLE_GAUSS && cov->method==SpectralTBM) {
    ERR("error in doplus with spectral");
  }
  assert(cov->Splus != NULL);
  
  for (m=0; m<cov->nsub; m++) {
    cov_model *key = cov->Splus->keys[m],
      *sub = cov->sub[m];
    double *keyrf = key->rf;
    DO(key, sub->stor);
    if (m > 0)
      for(i=0; i<total; i++) res[i] += keyrf[i];
  }
  return;
}



#define MULTPROC_COPIES 0
int checkmultproc(cov_model *cov) {
  int err;
  kdefault(cov, MULTPROC_COPIES, GLOBAL.special.multcopies);
  if ((err = checkplusmalproc(cov)) != NOERROR) {
    return err;
  }
  return NOERROR;
}


int initmultproc(cov_model *cov, storage VARIABLE_IS_NOT_USED *s){
  int  err;

  if ((err = initplusmalproc(cov, s)) != NOERROR) {
    BUG;
   return err;
  }

  if (cov->role == ROLE_GAUSS) {
    FieldReturn(cov);
    return NOERROR;
  }
     
  else {
    BUG;
  }

  return ERRORFAILED;
}



void domultproc(cov_model *cov, storage VARIABLE_IS_NOT_USED *s) {
  int m, i,
    total = cov->prevloc->totalpoints * cov->vdim2[0];
  double *res = cov->rf;
 assert(cov->vdim2[0] == cov->vdim2[1]);

  if (cov->role == ROLE_GAUSS && cov->method==SpectralTBM) {
    ERR("error in do_mult with spectral");
  }
  assert(cov->Splus != NULL);

  for(i=0; i<total; res[i++] = 0.0);

  
  for (m=0; m<cov->nsub; m++) {
    cov_model *key = cov->Splus->keys[m],
      *sub = cov->sub[m];
    double *keyrf = key->rf;
    DO(key, sub->stor);
      for(i=0; i<total; i++) res[i] += keyrf[i];
  }
  return;
}



void rangemultproc(cov_model VARIABLE_IS_NOT_USED *cov, range_type* range){
  range->min[MULTPROC_COPIES] = 1.0;
  range->max[MULTPROC_COPIES] = RF_INF;
  range->pmin[MULTPROC_COPIES] = 1.0;
  range->pmax[MULTPROC_COPIES] = 1000;
  range->openmin[MULTPROC_COPIES] = false;
  range->openmax[MULTPROC_COPIES] = true;
}


// $power

void PowSstat(double *x, cov_model *cov, double *v){
  logPowSstat(x, cov, v, NULL);

  //printf("powscale %f %f\n", x[0], v);
  
}

void logPowSstat(double *x, cov_model *cov, double *v, double *sign){
  cov_model *next = cov->sub[POW_SUB];
  double 
    factor,
    var = P0(POWVAR),
    scale =P0(POWSCALE), 
    p = P0(POWPOWER),
    *z = cov->Sdollar->z,
    invscale = 1.0 / scale;
  int i,
    vdim = cov->vdim2[0],
    vdimSq = vdim *vdim,
    xdimown = cov->xdimown;
 assert(cov->vdim2[0] == cov->vdim2[1]);

  // if (cov->calling->nr != RECTANGULAR)
  //   printf("%s\n",  NICK((cov->calling->calling->sub[PGS_LOC])));
  //  assert(cov->calling->nr == RECTANGULAR || 
  //	 (cov->calling->calling->sub[PGS_LOC]->nr == LOC &&
  //	  scale == PARAM0(cov->calling->calling->sub[PGS_LOC], LOC_SCALE)));

  if (z == NULL) 
    z = cov->Sdollar->z =(double*) MALLOC(xdimown * sizeof(double)); 
  for (i=0; i < xdimown; i++) z[i] = invscale * x[i];
  if (sign==NULL) {
    COV(z, next, v);
    factor = var * pow(scale, p);
    
    //printf("pows=%f %f %f\n", scale, var, factor);

    for (i=0; i<vdimSq; i++) v[i] *= factor; 
  } else {
    LOGCOV(z, next, v, sign);
    factor = log(var) + p * log(scale);
    for (i=0; i<vdimSq; i++) v[i] += factor; 
  }
}

void PowSnonstat(double *x, double *y, cov_model *cov, double *v){
  logPowSnonstat(x, y, cov, v, NULL);
}

void logPowSnonstat(double *x, double *y, cov_model *cov, double *v, 
		 double *sign){
  cov_model *next = cov->sub[POW_SUB];
  double 
    factor,
    var = P0(POWVAR),
    scale =P0(POWSCALE), 
    p = P0(POWPOWER),
    *z1 = cov->Sdollar->z,
    *z2 = cov->Sdollar->z2,
    invscale = 1.0 / scale;
  int i,
    vdim = cov->vdim2[0],
    vdimSq = vdim * vdim,
    xdimown = cov->xdimown;
  assert(cov->vdim2[0] == cov->vdim2[1]);
 
  if (z1 == NULL) 
    z1 = cov->Sdollar->z = (double*) MALLOC(xdimown * sizeof(double));
  if (z2 == NULL) 
    z2 = cov->Sdollar->z2 = (double*) MALLOC(xdimown * sizeof(double));

  for (i=0; i<xdimown; i++) {
    z1[i] = invscale * x[i];
    z2[i] = invscale * y[i];
  }

  if (sign==NULL) {
    NONSTATCOV(z1, z2, next, v);
    factor = var * pow(scale, p);
    for (i=0; i<vdimSq; i++) v[i] *= factor; 
  } else {
    LOGNONSTATCOV(z1, z2, next, v, sign);
    factor = log(var) + p * log(scale);
    for (i=0; i<vdimSq; i++) v[i] += factor; 
  }
}

 
void inversePowS(double *x, cov_model *cov, double *v) {
  cov_model *next = cov->sub[POW_SUB];
  int i,
    vdim = cov->vdim2[0],
    vdimSq = vdim * vdim;
  double y, 
    scale =P0(POWSCALE),
    p = P0(POWPOWER),
    var = P0(POWVAR);
 assert(cov->vdim2[0] == cov->vdim2[1]);

  y= *x / (var * pow(scale, p)); // inversion, so variance becomes scale
  if (CovList[next->nr].inverse == ErrCov) BUG;
  INVERSE(&y, next, v);
 
  for (i=0; i<vdimSq; i++) v[i] *= scale; 
}


int TaylorPowS(cov_model *cov) {
  cov_model 
    *next = cov->sub[POW_SUB];
  int i;
  double scale = PisNULL(POWSCALE) ? 1.0 : P0(POWSCALE);
  cov->taylorN = next->taylorN;  
  for (i=0; i<cov->taylorN; i++) {
    cov->taylor[i][TaylorPow] = next->taylor[i][TaylorPow];
    cov->taylor[i][TaylorConst] = next->taylor[i][TaylorConst] *
      P0(POWVAR) * pow(scale, P0(POWPOWER) - next->taylor[i][TaylorPow]);   

    //printf("TC %f %f\n",  cov->taylor[i][TaylorConst], next->taylor[i][TaylorConst]);

  }
  
  cov->tailN = next->tailN;  
  for (i=0; i<cov->tailN; i++) {
    cov->tail[i][TaylorPow] = next->tail[i][TaylorPow];
    cov->tail[i][TaylorExpPow] = next->tail[i][TaylorExpPow];
    cov->tail[i][TaylorConst] = next->tail[i][TaylorConst] *
      P0(POWVAR) * pow(scale, P0(POWPOWER) - next->tail[i][TaylorPow]);   
    cov->tail[i][TaylorExpConst] = next->tail[i][TaylorExpConst] *
      pow(scale, -next->tail[i][TaylorExpPow]);
  }
  return NOERROR;
}

int checkPowS(cov_model *cov) {
  // hier kommt unerwartet  ein scale == nan rein ?!!
  cov_model 
    *next = cov->sub[POW_SUB];
  int err, 
    tsdim = cov->tsdim,
    xdimown = cov->xdimown,
    xdimNeu = xdimown;
    
  kdefault(cov, POWVAR, 1.0);
  kdefault(cov, POWSCALE, 1.0);
  kdefault(cov, POWPOWER, 0.0);
  if ((err = checkkappas(cov)) != NOERROR) {
    return err;
  }
  
  if ((err = CHECK(next, tsdim, xdimNeu, cov->typus, cov->domown,
		   cov->isoown, SUBMODEL_DEP, cov->role)) != NOERROR) {
    return err;
  }

  setbackward(cov, next);
  if ((err = TaylorPowS(cov)) != NOERROR) return err;

  if (cov->Sdollar != NULL && cov->Sdollar->z != NULL)
    DOLLAR_DELETE(&(cov->Sdollar));
  if (cov->Sdollar == NULL) {
    cov->Sdollar = (dollar_storage*) MALLOC(sizeof(dollar_storage));
    DOLLAR_NULL(cov->Sdollar);
  }
  assert(cov->Sdollar->z == NULL);
  
  return NOERROR;
}



bool TypePowS(Types required, cov_model *cov) {
  cov_model *next = cov->sub[0];

  // printf("\n\n\nxxxxxxxxxxxx %d %s %s\n\n", TypeConsistency(required, next), TYPENAMES[required], NICK(next));
  
  // if (required == ProcessType) crash(); //assert(false);

  return (required==TcfType || required==PosDefType || required==NegDefType
	  || required==ShapeType || required==TrendType 
	  || required==ProcessType || required==GaussMethodType)
    && TypeConsistency(required, next);
}


void rangePowS(cov_model *cov, range_type* range){
  range->min[POWVAR] = 0.0;
  range->max[POWVAR] = RF_INF;
  range->pmin[POWVAR] = 0.0;
  range->pmax[POWVAR] = 100000;
  range->openmin[POWVAR] = false;
  range->openmax[POWVAR] = true;

  range->min[POWSCALE] = 0.0;
  range->max[POWSCALE] = RF_INF;
  range->pmin[POWSCALE] = 0.0001;
  range->pmax[POWSCALE] = 10000;
  range->openmin[POWSCALE] = true;
  range->openmax[POWSCALE] = true;

  range->min[POWPOWER] = RF_NEGINF;
  range->max[POWPOWER] = RF_INF;
  range->pmin[POWPOWER] = -cov->tsdim;
  range->pmax[POWPOWER] = +cov->tsdim;
  range->openmin[POWPOWER] = true;
  range->openmax[POWPOWER] = true;
 }



void PowScaleToLoc(cov_model *to, cov_model *from, int VARIABLE_IS_NOT_USED depth) {
  assert(!PARAMisNULL(to, LOC_SCALE) && !PARAMisNULL(from, POWSCALE));
  PARAM(to, LOC_SCALE)[0] = PARAM0(from, POWSCALE);
}

int structPowS(cov_model *cov, cov_model **newmodel) {
  cov_model
    *next = cov->sub[POW_SUB],
    *scale = cov->kappasub[POWSCALE];
  int err; 
  // cov_model *next;



  //   printf("%s %d %s\n", ROLENAMES[cov->role], cov->role, TYPENAMES[cov->typus]);
  //  if (cov->role != ROLE_GAUSS) {
    //  APMI(cov->calling);
    //crash();
  //  }
  if (!next->deterministic) SERR("random shapes not programmed yet");

  switch (cov->role) {
  case ROLE_SMITH :  case ROLE_GAUSS :
    ASSERT_NEWMODEL_NOT_NULL;
    
    if ((err = STRUCT(next, newmodel)) > NOERROR) return err;
    
    addModel(newmodel, POWER_DOLLAR);
    if (!PisNULL(POWVAR)) kdefault(*newmodel, POWVAR, P0(POWVAR));
    if (!PisNULL(POWSCALE)) kdefault(*newmodel, POWSCALE, P0(POWSCALE));
    if (!PisNULL(POWPOWER)) kdefault(*newmodel, POWPOWER, P0(POWPOWER));
    
    break;
  case ROLE_MAXSTABLE : {
    ASSERT_NEWMODEL_NOT_NULL;
    
    if ((err = STRUCT(next, newmodel)) > NOERROR) return err;
    
    if (!isRandom(scale)) SERR("unstationary scale not allowed yet");
    addModel(newmodel, LOC);
    addSetDistr(newmodel, scale, PowScaleToLoc, true, MAXINT);
  }
    break;
  default :
    //  PMI(cov, "structS");
    SERR1("changes in scale/variance not programmed yet for '%s'", 
	  ROLENAMES[cov->role]);      
  }  
   
  return NOERROR;
}




int initPowS(cov_model *cov, storage *s){
  // am liebsten wuerde ich hier die Koordinaten transformieren;
  // zu grosser Nachteil ist dass GetDiameter nach trafo 
  // grid def nicht mehr ausnutzen kann -- umgehbar?!
  cov_model *next = cov->sub[POW_SUB],
    *varM = cov->kappasub[POWVAR],
    *scaleM = cov->kappasub[POWSCALE];
  int 
    vdim = cov->vdim2[0],
    nm = cov->mpp.moments,
    nmvdim = (nm + 1) * vdim,
    err = NOERROR;
  bool 
    maxstable = hasExactMaxStableRole(cov);// Realisationsweise 
  assert(cov->vdim2[0] == cov->vdim2[1]);


  assert(cov->key == NULL || ({PMI(cov);false;}));//   
  
  if (hasAnyShapeRole(cov)) { // !! ohne maxstable selbst !!
    double
      var[MAXMPPVDIM],  
      p = P0(POWPOWER),
      scale = P0(POWSCALE);
    int i,
      intp = (int) p,
      dim = cov->tsdim;

    

    // Achtung I-NIT_RANDOM ueberschreibt mpp.* !!
    if (varM != NULL) {
      int nm_neu = nm == 0 && !maxstable ? 1 : nm;
      if ((err = INIT_RANDOM(varM, nm_neu, s, P(POWVAR))) != NOERROR) 
	return err;
      int nmP1 = varM->mpp.moments + 1;
      for (i=0; i<vdim; i++) {
	int idx = i * nmP1;
	var[i] = maxstable ? P0(DVAR) : varM->mpp.mM[idx + 1];      
      }
    } else for (i=0; i<vdim; var[i++] = P0(POWVAR));

    if (scaleM != NULL) {
      if (p != intp)
	SERR1("random scale can be initialised only for integer values of '%s'",
	     KNAME(POWPOWER));
      if (dim + intp < 0) SERR("negative power cannot be calculated yet");
      int idx_s = maxstable ? nm : dim + nm + intp < 1 ? 1 : dim + nm + intp;
      if ((err = INIT_RANDOM(scaleM, idx_s, s, P(POWSCALE))) != NOERROR) return err;
      assert(scaleM->mpp.moments == 1);
      scale = maxstable ? P0(DSCALE) : scaleM->mpp.mM[1];      
    }
    if ((err = INIT(next, nm, s)) != NOERROR) return err;


    for (i=0; i < nmvdim; i++) {
      cov->mpp.mM[i] = next->mpp.mM[i];
      cov->mpp.mMplus[i] = next->mpp.mMplus[i];
      //printf("%s I=%d %f %f\n", NICK(cov->calling), i,  cov->mpp.mMplus[i], cov->mpp.mM[i]);
   }


    if (varM != NULL && !maxstable) {
      int j,
	nmP1 = varM->mpp.moments + 1;
      for (j=0; j<vdim; j++) {
	int idx = j * nmP1;
	for (i=0; i <= nm; i++) {
	  cov->mpp.mM[i] *= varM->mpp.mM[idx + i];
	  cov->mpp.mMplus[i] *= varM->mpp.mMplus[idx + i];
	}
      }
    } else {
      int j, k;
      double pow_var;
      for (k=j=0; j<vdim; j++) { 
	pow_var = 1.0;
	for (i=0; i<=nm; i++, pow_var *= var[j], k++) {	
	  cov->mpp.mM[k] *= pow_var;
	  cov->mpp.mMplus[k] *= pow_var;
	  //printf("%s i=%d %f p=%f %f\n", NICK(cov->calling), i, var, pow_var, cov->mpp.mM[i]);
	}
      }
    }

    if (scaleM != NULL && !maxstable) {
      if (dim + nm * intp < 0 || dim + intp * nm > scaleM->mpp.moments) 
	SERR("moments cannot be calculated");
      assert(scaleM->vdim2[0] == 1 && scaleM->vdim2[1] == 1 );
      for (i=0; i <= nm; i++) {
	int idx = dim + i * intp;
	cov->mpp.mM[i] *= scaleM->mpp.mM[idx];
	cov->mpp.mMplus[i] *= scaleM->mpp.mMplus[idx];
      }
    } else {
      int j,k;
      double 
	pow_scale,
	pow_s = pow(scale, dim),
	pow_p = pow(scale, p);
      for (k=j=0; j<vdim; j++) { 
	pow_scale = pow_s;
	for (i=0; i <= nm; i++, pow_scale *= pow_p, k++) {
	  cov->mpp.mM[k] *= pow_scale;
	  cov->mpp.mMplus[k] *= pow_scale;
	}
    	//printf("%s i=%d s=%e %e\n", NICK(cov->calling), i, pow_scale, cov->mpp.mM[i]);
      }
    }
 
    if (R_FINITE(cov->mpp.unnormedmass)) {
      if (vdim > 1) BUG;
      cov->mpp.unnormedmass = next->mpp.unnormedmass * var[0] / pow(scale, p);
    } else {
      double pp = pow(scale, -p);
      for (i=0; i<vdim; i++)   
	cov->mpp.maxheights[i] = next->mpp.maxheights[i] * var[i] * pp;
    }
 
    //    printf("ini pows %f %f %d %f\n",   
    //	   cov->mpp.maxheight , next->mpp.maxheight ,
    //	   varM == NULL, varM == NULL ? P0(POWVAR) : 1.0);
 
    //    cov->mpp.refradius *= scale;
    //cov->mpp.refsd *= scale;
  }


  else if (cov->role == ROLE_GAUSS) {  
    if ((err=INIT(next, 0, s)) != NOERROR) return err;
  }

  else if (cov->role == ROLE_BASE) {
    if ((err=INIT(next, 0, s)) != NOERROR) return err;
    
  }

  else SERR("initiation of scale and/or variance failed");

 
  if ((err = TaylorPowS(cov)) != NOERROR) return err;

  return NOERROR;
}


void doPowS(cov_model *cov, storage *s){
 
  if (hasAnyShapeRole(cov)) {
    cov_model *next = cov->sub[POW_SUB];
         
    DO(next, s);// nicht gatternr
    double factor = P0(POWVAR) / pow(P0(POWSCALE), P0(POWPOWER));
    int i,
      vdim = cov->vdim2[0];
    assert(cov->vdim2[0] == cov->vdim2[1]);
    for (i=0; i<vdim; i++)   
      cov->mpp.maxheights[i] = next->mpp.maxheights[i] * factor;
    return;
  }
  
  ERR("unknown option in 'doPowS' "); 
}
