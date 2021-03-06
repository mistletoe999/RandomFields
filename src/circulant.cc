
/*
 Authors 
 Martin Schlather, schlather@math.uni-mannheim.de

 Simulation of a random field by circulant embedding
 (see Wood and Chan, or Dietrich and Newsam for the theory)
 and variants by Stein and by Gneiting

 Copyright (C) 2001 -- 2003 Martin Schlather
 Copyright (C) 2004 -- 2005 Yindeng Jiang & Martin Schlather
 Copyright (C) 2006 -- 2011 Martin Schlather
 Copyright (C) 2011 -- 2015 Peter Menck & Martin Schlather

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
#include <stdio.h>  
//#include <stdlib.h>
#include "RF.h"
#include "shape_processes.h"
#include "Coordinate_systems.h"

  
#include <R_ext/Linpack.h>
#include <R_ext/Utils.h>     
#include <R_ext/Lapack.h> // MULT 



/*********************************************************************/
/*           CIRCULANT EMBEDDING METHOD (1994) ALGORITHM             */
/*  (it will always be refered to the paper of Wood & Chan 1994)     */
/*********************************************************************/


int fastfourier(double *data, int *m, int dim, bool first, bool inverse,
		FFT_storage *FFT)
  /* this function is taken from the fft function by Robert Gentleman 
     and Ross Ihaka, in R */
{
  long int inv, nseg, n,nspn,i;
  int maxf, maxp, err;
  bool ok;
  if (first) {
   int maxmaxf,maxmaxp;
   nseg = maxmaxf =  maxmaxp = 1;
   /* do whole loop just for err checking and maxmax[fp] .. */

   for (i = 0; i<dim; i++) {
     if (m[i] > 1) {
       fft_factor(m[i], &maxf, &maxp);
       if (maxf == 0) {GERR("fft factorization failed")}	
       if (maxf > maxmaxf) maxmaxf = maxf;
       if (maxp > maxmaxp) maxmaxp = maxp;
       nseg *= m[i];
     }
   }
  
   FREE(FFT->work);
   FREE(FFT->iwork);
   if ((FFT->work = (double*) MALLOC(4 * maxmaxf * sizeof(double)))==NULL || 
       (FFT->iwork = (int*) MALLOC(maxmaxp  * sizeof(int)))==NULL) { 
     err=ERRORMEMORYALLOCATION; goto ErrorHandling;
   }
   FFT->nseg = nseg;
   //  nseg = leng th(z); see loop above
  }
  inv = (inverse) ? 2 : -2;
  n = 1;
  nspn = 1;  
  nseg = FFT->nseg;
  for (i = 0; i < dim; i++) {
    if (m[i] > 1) {
      nspn *= n;
      n = m[i];
      nseg /= n;
      fft_factor(n, &maxf, &maxp);
      ok = (bool) fft_work(&(data[0]), &(data[1]), nseg, n, nspn, inv, 
		      FFT->work, FFT->iwork);
      if (!ok) GERR("error within Fourier transform");
    }
  }
  return NOERROR;
 ErrorHandling:
  FFT_destruct(FFT);
  return err;
}

int fastfourier(double *data, int *m, int dim, bool first, FFT_storage *FFT){
  return fastfourier(data, m, dim, first, !first, FFT);
}


#define LOCPROC_DIAM  (CE_LAST + 1)// parameter p
#define LOCPROC_A  (CE_LAST + 2) // always last of LOCPROC_*
#define LOCPROC_R LOCPROC_A // always last of LOCPROC_*


int init_circ_embed(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED  *S){
  int err=NOERROR;
  if (!Getgrid(cov)) SERR("circ embed requires a grid");

  //  cov_model *prev = cov;
  //  while (prev->calling != NULL) prev = prev->calling; PMI(prev);

  int d,
	*sum = NULL,
	index[MAXCEDIM], dummy, j, l, 
	*mm=NULL,
	*cumm=NULL, 
	*halfm=NULL; // MULT added vars j,l;
    double hx[MAXCEDIM], realmtot, steps[MAXCEDIM], 
      **c = NULL, // For multivariate *c->**c
      **Lambda = NULL,
      *tmp = NULL,
      *rwork=NULL, 
      *tmpLambda=NULL,
      *smallest=NULL;
    complex optim_lwork, 
      *R = NULL,
      *work=NULL;
  long int i,k,twoi,twoi_plus1, mtot, hilfsm[MAXCEDIM],
      *idx = NULL;
  bool cur_crit;
  ce_storage *s = NULL;
  cov_model *next = cov->sub[0];
  location_type *loc = Loc(cov);
  double
    maxGB = P0(CE_MAXGB),
    tolRe = P0(CE_TOLRE),
    tolIm = P0(CE_TOLIM),
    *caniso = loc->caniso,
    *mmin = P(CE_MMIN);
  int dim = cov->tsdim,
    vdim   = cov->vdim[0],
    cani_ncol = loc->cani_ncol,
    cani_nrow = loc->cani_nrow,
    lenmmin = cov->nrow[CE_MMIN],
    vdimSQ = vdim * vdim, // PM 12/12/2008
    maxmem = P0INT(CE_MAXMEM),
    trials = GLOBAL.internal.examples_reduced ? 1 : P0INT(CE_TRIALS),
    strategy = P0INT(CE_STRATEGY);
  bool 
    isaniso = loc->caniso != NULL,
    force = (bool) P0INT(CE_FORCE),
    useprimes = (bool) P0INT(CE_USEPRIMES),
    dependent = (bool) P0INT(CE_DEPENDENT);
 
  ROLE_ASSERT_GAUSS;
  assert(cov->vdim[0] == cov->vdim[1]);

  cov->method = CircEmbed;
  
  if (loc->distances) return ERRORFAILED;

  NEW_STORAGE(ce);
  s = cov->Sce;

  if (dim > MAXCEDIM) {
    err=ERRORMAXDIMMETH ; goto ErrorHandling;
  } 
  for (d=0; d<dim; d++) {
    s->nn[d]=(int) loc->xgr[d][XLENGTH]; 
    steps[d]=loc->xgr[d][XSTEP];
  }
  s->vdim = vdim;

  // PM 12/12/2008
  // in case of multiple calls: possible to use imaginary part of
  // field generated in preceding run. If s->dependent is set, fields
  // are read off in the following order:
  //  1. real field 1st square
  //  2. imag field 1st square
  //  3. real field 2nd square
  //  4. imag field 2nd square
  //  ...
  //  2k+1. real field (2k+1)th square
  //  2k+2. imag field (2k+1)th square
  //
  //  i.e. read off imag fields in even calls. Generate new ones in
  //  odd calls resp. read off real squares if s->dependent==1

  s->cur_call_odd = 0;  // first one is set to be odd at beginning of
                        // do_circ_embed

  if( (tmp = (double *) MALLOC(vdimSQ * sizeof(double))) == NULL) {
      err=ERRORMEMORYALLOCATION; goto ErrorHandling;
  }

  mtot=0;
  mm = s->m;
  halfm= s->halfm;
  cumm = s->cumm;
  if (PL>=PL_STRUCTURE) { LPRINT("calculating the Fourier transform\n"); }

  /* cumm[i+1]=\prod_{j=0}^i m[j] 
     cumm is used for fast transforming the matrix indices into an  
     index of the vector (the way the matrix is stored) corresponding 
     to the matrix                   */                               
  /* calculate the dimensions of the matrix C, eq. (2.2) in W&C */       

  //  ("CE missing strategy for matrix entension in case of anisotropic fields %d\n)
  //  for (i=0;i<dim;i++) { // size of matrix at the beginning  
  //	print("%d %f\n", i, mmin[i]);
  //  }

  bool multivariate;
  multivariate = cov->vdim[0] > 1;
  assert(cov->vdim[0] == cov->vdim[1]);

  // multivariate = !true; // checken !!

  for (i=0;i<dim;i++) { // size of matrix at the beginning  
    double hilfsm_d;
    if (FABS(mmin[i % lenmmin]) > 10000.0) {     
      GERR1("maximimal modulus of mmin is 10000. Got %f", mmin[i % lenmmin]);
    }
    hilfsm_d = (double) s->nn[i];
    if (mmin[i % lenmmin]>0.0) {
      if (hilfsm_d > (1 + (int) CEIL(mmin[i % lenmmin])) / 2) { // plus 1 since 
	// mmin might be odd; so the next even number should be used
        GERR3("Minimum size in direction %d is %f. Got %f\n",
	      (int) i, hilfsm_d, CEIL(mmin[i % lenmmin]));
      }
      hilfsm_d = (double) ( (1 + (int) CEIL(mmin[i % lenmmin])) / 2);
    } else if (mmin[i % lenmmin] < 0.0) {
      assert(mmin[i % lenmmin] <= -1.0);
      hilfsm_d = CEIL((double) hilfsm_d * - mmin[i % lenmmin]);
    }
    if (hilfsm_d >=  MAXINT) return ERRORMEMORYALLOCATION;
    if (useprimes) {
      // Note! algorithm fails if hilfsm_d is not a multiple of 2
      //       2 may not be put into NiceFFTNumber since it does not
      //       guarantee that the result is even even if the input is even !
	 hilfsm_d = 2.0 * NiceFFTNumber((int) hilfsm_d);
    } else {
      hilfsm_d =ROUND(multivariate
		      ? POW(3.0, 1.0 + CEIL(LOG(hilfsm_d) / LOG3 - EPSILON1000))
		      : POW(2.0, 1.0 + CEIL(LOG(hilfsm_d) *INVLOG2-EPSILON1000))
		      );
    }
    if (hilfsm_d >=  MAXINT) return ERRORMEMORYALLOCATION;
    hilfsm[i] = (int) hilfsm_d;
  }
   


  s->positivedefinite = false;     
  /* Eq. (3.12) shows that only j\in I(m) [cf. (3.2)] is needed,
     so only the first two rows of (3.9) (without the taking the
     modulus of h in the first row)
     The following variable 'index' corresponds to h(l) in the following
way: index[l]=h[l]        if 0<=h[l]<=mm[l]/2
index[l]=h[l]-mm[l]   if mm[l]/2+1<=h[l]<=mm[l]-1     
Then h[l]=(index[l]+mm[l]) % mm[l] !!
   */


  /* The algorithm below is as follows:
     while (!s->positivedefinite && (s->trials<trials)){
     (s->trials)++;
     calculate the covariance values "c" according to the given "m"
     fastfourier(c)
     if (!force || (s->trials<trials)) {
     check if positive definite
     if (!s->positivedefinite && (s->trials<trials)) {
     enlarge "m"
     }
     } else 
     print "forced" //
     }
   */

  s->trials=0;
  
  // print("trials %d strat=%d force=%d prim=%d dep=%d  max=%f re=%f im=%f\n", 
  //	 trials, strategy, force, useprimes, dependent, 
  //	 maxmem, tol_re, tol_im);

  while (!s->positivedefinite && (s->trials < trials)) {
    // print("trials %d %d\n", trials, s->trials);

    (s->trials)++;
    cumm[0]=1; 
    realmtot = 1.0;
    for(i=0; i<dim; i++) {
      if (hilfsm[i] < MAXINT) {
	mm[i] = hilfsm[i];
      } else {
	GERR3("Each direction allows for at most %d points. Got %ld in the %ld-th direction", MAXINT, hilfsm[i], i);
      }
      halfm[i]=mm[i] / 2; 
      index[i]=1-halfm[i]; 
      cumm[i+1]=cumm[i] * mm[i]; // only relevant up to i<dim-1 !!
      realmtot *= (double) mm[i];
    }
    s->mtot = mtot = cumm[dim];

    if (PL>=PL_BRANCHING) {
      LPRINT("Memory need for ");
      for (i=0;i<dim;i++) { 
	if (i > 0) PRINTF(" x "); 
 	PRINTF("%d", mm[i]); 
     }
      PRINTF(" %s is 2 x %ld complex numbers.\n", 
	     dim == 1 ? "locations" : "grid", mtot  * vdimSQ);
    }

    if ( (maxmem != NA_INTEGER && realmtot * vdimSQ > maxmem))
      GERR4("total real numbers needed=%.0f > '%s'=%d -- increase '%s'",
	    realmtot * vdimSQ, CE[CE_MAXMEM - COMMON_GAUSS - 1], 
	    maxmem, CE[CE_MAXMEM - COMMON_GAUSS - 1])
    else if (R_FINITE(maxGB) && realmtot * vdimSQ * 32e-9 > maxGB)
      GERR4("total memory needed=%4.4f GB > '%s'=%4.4f GB -- increase '%s'",
	    realmtot* vdimSQ * 32e-9, CE[CE_MAXGB - COMMON_GAUSS - 1], 
	    maxGB, CE[CE_MAXGB - COMMON_GAUSS - 1]);

    if (c != NULL) {
      for(l=0; l<vdimSQ; l++) FREE(c[l]);
      UNCONDFREE(c);
    }
    // for the following, see the paper by Wood and Chan!
    // meaning of following variable c, see eq. (3.8)
    if ((c = (double **) MALLOC(vdimSQ * sizeof(double *))) == NULL) {
      err=ERRORMEMORYALLOCATION; goto ErrorHandling;
    }
    for(l=0; l<vdimSQ; l++) {
      //     printf("%d maxmem=%d %d %f\n", 2 * mtot * sizeof(double), maxmem, mtot, realmtot);
      if( (c[l] = (double *) MALLOC(2 * mtot * sizeof(double))) == NULL) {
	err=ERRORMEMORYALLOCATION; goto ErrorHandling;
      }
    }
  
    for(i=0; i<dim; i++){index[i]=0;}

    for (i=0; i<mtot; i++) {
      cur_crit = false;
      for (k=0; k<dim; k++) {
	int idxk = index[k];
	hx[k] = -steps[k] *// with '-' consistent with x[d]-y[d] in nonstat2stat
	  (double) ((idxk <= halfm[k]) ? idxk : idxk - mm[k]);
	cur_crit |= (idxk==halfm[k]);
      }
      dummy = i << 1;

      //      print("ci %d %d %e %e \n", k, dim, hx[0], hx[1]);

      //COV(hx, next, c + dummy);
      if (isaniso) {
	double z[MAXCEDIM];
	xA(hx, caniso, cani_nrow, cani_ncol, z);
	COV(z, next, tmp);
      } else COV(hx, next, tmp);

      //     PMI(next);
      //print("hx %f %f tmp %f\n", hx[0], hx[1], *tmp);

  
      for(l=0; l<vdimSQ; l++) {
	  c[l][dummy] = tmp[l];
	  c[l][dummy+1] = 0.0;

	  if (ISNAN(c[l][dummy]) || ISNAN(c[l][dummy + 1])) {
	    //
	    //PMI(cov);
	    // PRINTF("i=%d %d tri=%d %d vdim=$d ani=%d %f %f c=%f %f \n",
	    //	   i, l, trials, dummy, vdimSQ, isaniso,
	    //	   hx[0], hx[1], c[l][dummy], c[l][dummy]);
	    ERR("fourier transform returns NAs");
	  }
	  //	  print("%d %d %f\n", i, mtot,  c[l][dummy]);
      }

      //    print("%f %f %f\n", hx[0], hx[1], c[dummy]);

      if (cur_crit) {
	for (k=0; k<dim; k++) {
	  if (index[k]==halfm[k]) hx[k] -= steps[k] * (double) mm[k];
	}
	COV(hx, next, tmp);
	for(l=0; l<vdimSQ; l++) {
	  c[l][dummy] = 0.5 *(c[l][dummy] + tmp[l]);
	}
      }
      //        print("%10f,%10f:%10f \n", hx[0], hx[1], c[dummy]);

      k=0; while( (k<dim) && (++(index[k]) >= mm[k])) {
	index[k]=0;
	k++;
      }
      assert( (k<dim) || (i==mtot-1));

    }


  
    bool first;
    for(j=0; j<vdim; j++) {
      for(k=0; k<vdim; k++) {
	l= j*vdim + k;
        assert({				\
	    int ii;				\
	    for (ii =0; ii<mtot * 2; ii++) {		\
	      if (ISNAN(c[l][ii])) {	\
		PRINTF("%d %d %f\n", l, ii, c[l][ii]);	\
		ERR("fourier transform returns NAs");	\
	      }						\
	    }; true;					\
	  });

	if(k>=j) { //// martin 29.12.
		   // just compute FFT for upper right triangle
	  first = (l==0);
	  if (PL>=PL_STRUCTURE) { LPRINT("FFT..."); }
	  if ((err=fastfourier(c[l], mm, dim, first, &(s->FFT))) != NOERROR) 
	    goto ErrorHandling;
	}

	if (false) {
	  int ii;
	  for (ii =0; ii<mtot * 2; ii++) {
	    if (ISNAN(c[l][ii])) {
	      ERR("fourier transform returns NAs");
	    }
	  }
	}
 

      }
    }
  
    if (PL>=PL_STRUCTURE) { LPRINT("finished\n"); }

    // here the A(k) have been constructed; A(k) =      c[0][k], c[1][k], ...,  c[p-1][k]
    //                                                  c[p][k], ...            c[2p-1][k]
    //                                                  ...                     ...
    //                                                  c[(p-1)*p][k], ...,     c[p*p-1][k]
    // NOTE: only the upper triangular part of A(k) has actually been filled; A is Hermitian.

    // Now:
    // A(k) is placed into R (cf W&C 1999)
    // application of zheev puts eigenvectors
    // of A(k) into R


    if(Lambda!=NULL) {
      for(l = 0; l<vdim; l++) FREE(Lambda[l]);
      UNCONDFREE(Lambda);
    }
    FREE(rwork);
    FREE(tmpLambda);
    FREE(R);

    if( (Lambda = (double **) MALLOC(vdim * sizeof(double *))) == NULL ||
	(rwork = (double *) MALLOC(sizeof(double) * (3*vdim - 2) )) == NULL ||
	(tmpLambda = (double *) MALLOC(sizeof(double) * vdim )) == NULL ||
	(R = (complex *) MALLOC(vdimSQ * sizeof(complex))) == NULL) {
      err=ERRORMEMORYALLOCATION; goto ErrorHandling;
    }

    for(l=0;l<vdim;l++) {
      if( (Lambda[l] = (double *) MALLOC(mtot * sizeof(double))) == NULL) {
	err=ERRORMEMORYALLOCATION; goto ErrorHandling;
      }
    }
  
    int lwork, info;
    s->positivedefinite = true;
    if (vdim==1) { // in case vdim==1, the eigenvalues we need are already contained in c[0]
      for(i = 0; i<mtot; i++) {
	Lambda[0][i] = c[0][2*i];
	s->positivedefinite &= FABS(c[0][2*i+1]) <= tolIm;
	c[0][2*i] = 1.0;
	c[0][2*i+1] = 0.0;
      }
    } else {
      int index1, index2, Sign;
      for(i = 0; i<mtot; i++) {
	twoi=2*i;
	twoi_plus1=twoi+1;

	// construct R (vdim x vdim matrix) (which is idential to A[i] (cf W&C))
	for(k=0; k<vdim; k++) {
	  for(l=0; l<vdim; l++) {
	      
	    index1 = vdim * k + l;
	    if(l>=k) { 
	      index2=index1; Sign=1;
	      s->positivedefinite &= k!=l || FABS(c[index2][twoi_plus1])<=tolIm;
	      //	      c[index2][twoi_plus1].r = 0.0; ?!!!
	    }  // obtain lower triangular bit of c as well with
	    else{ index2= vdim * l + k; Sign=-1;}  // (-1) times imaginary part of upper triangular bit

	    R[index1].r = c[index2][twoi];
	    R[index1].i = Sign*c[index2][twoi_plus1];

///	if (i < 5) 
	    //print("%d %f %f; vdim=%d\n", index1, R[index1].r, R[index1].i, vdim);
	  }
	}

//	if (i < 5) 
//	      print("2 : i=%d mtot=%d k=%d l=%d vdim=%d \n   ", i, mtot, k, l, vdim);

	//for(k=1; k<2; k++)  R[k].r = R[k].i = 7.0;

	// diagonalize R via Fortran call zheev
	// initialization
	if(i==0) {
	  lwork = -1; // for initialization call to get optimal size of array 'work'

	  F77_CALL(zheev)("V", "U", &vdim, R,  // complex
			  &vdim, // integer
			  tmpLambda,  // double
			  &optim_lwork, // complex
			  &lwork, rwork, &info); 



           //// martin 29.12.: nur einmal aufrufen]
	  ////                                ganz weit vorne, oder haengt
	  //// optim_lwork.r  von den Werten der Matrix ab ??

	  lwork = (int) optim_lwork.r;

	  FREE(work);
	  if( (work = (complex *) CALLOC(lwork, sizeof(complex))) == NULL ) {
	    err=ERRORMEMORYALLOCATION; goto ErrorHandling;
	  }
	}

	F77_CALL(zheev)("V", "U", &vdim, R, &vdim, tmpLambda, work, &lwork,
			rwork, &info);


	for(l=0;l<vdim;l++) Lambda[l][i] = tmpLambda[l];


	// martin.1.1.09 --- it only works technically!
//	F77_CALL(zpotf2)("Upper", &vdim, R, &vdim, &info);
 


	// now R contains the eigenvectors of A[i]
	// and Lambda[l][i], l=0,...,vdim-1, contains the eigenvalues of A[i]

	// the following step writes the vdim vdim-dimensional eigenvectors just computed into c


	for(j=k=0; k<vdim; k++) {
	  int endfor = vdimSQ + k;
	  for(l=k; l<endfor; l+=vdim, j++) {
	    c[j][twoi] = R[l].r;
	    c[j][twoi_plus1] = R[l].i;
	  }
	}

	// peter 28.12. doch wieder
	if(false)
	  for(l=0; l<vdimSQ; l++) {
	    c[l][twoi] = R[l].r;
	    c[l][twoi_plus1] = R[l].i;
	  }


	// next: check if all eigenvalues are >= 0 (non-negative definiteness)
	// and do check this within this loop... in case of some lambda <0 there is no point in going
	// on diagonalizing if there are free trial... or not? Hmm.
      }
    }
  
    FREE(R);
    FREE(rwork);
    FREE(work);
    FREE(tmpLambda);

    // check if positive definite. If not: enlarge and restart 
    if (!force || (s->trials<trials)) {

      if (isVariogram(next)) {
	for (l=0; l<vdim; l++) {
	  if (Lambda[l][0] < 0.0) Lambda[l][0]=0.0;
	}
      } 

      for(i=0; i<mtot; i++) { // MULT

	// Check if all eigenvalues of A(i) are > 0:
	l=0;
	while( (l<vdim) &&
	    ( s->positivedefinite &= Lambda[l][i]>=tolRe) )           
	  // eigenvalues ARE real: drop former line
	{l++;}                                                      // s->positivedefinite = (lambda.real>=cepar->tol_re) && (lambda.imag<=cepar->tol_im)


	if ( !s->positivedefinite) {
	  if (PL>=PL_BRANCHING) {
	    if (Lambda[l][i] < 0.0) {
	      LPRINT("There are complex eigenvalues\n");
	    } else if (vdim==1) {
	      LPRINT("non-positive eigenvalue: Lambda[%ld]=%e.\n",
		     i, Lambda[l][i]);
	    } else {
	      LPRINT("non-pos. eigenvalue in dim %d: Lambda[%ld][%d]=%e.\n",
		     l, i, l, Lambda[l][i]);
	    }
	  }
	  if (PL>=PL_ERRORS) { // just for printing the smallest 
	      //                            eigenvalue (min(c))
 	    if( (sum = (int *) CALLOC(vdim, sizeof(int))) == NULL ||
		(smallest = (double *)CALLOC(vdim, sizeof(double))) == NULL || 
		(idx = (long int *) CALLOC(vdim, sizeof(long int))) == NULL) {
	      err=ERRORMEMORYALLOCATION; goto ErrorHandling;
	    }
	    char percent[]="%";
	    for(j=i; j<mtot; j++) {
	      for(k=0; k<vdim; k++) {
		if(Lambda[k][j] < 0) {
		  sum[k]++;
		  if(Lambda[k][j] < smallest[k]) {
		    idx[k] = j;
		    smallest[k] = Lambda[k][j];}
		}
	      }
	    }
	    if(vdim==1) {
	      LPRINT("   There are %d negative eigenvalues (%5.3f%s).\n",
		  sum[0], 100.0 * (double) sum[0] / (double) mtot, percent);
	      LPRINT("   The smallest eigenvalue is Lambda[%d] =  %e\n",
		  idx[0], smallest[0]);
	    }
	    else {
	      for(l=0; l<vdim; l++) {
		LPRINT("   There are %d negative eigenvalues in dimension %d (%5.3f%s).\n",
		    sum[l], l, 100.0 * (double) sum[l] / (double) mtot, percent);
		LPRINT("   The smallest eigenvalue in dimension %d is Lambda[%d][%d] =  %e\n",
		    l, idx[l], l, smallest[l]);
	      }
	    }
	    FREE(sum);
	    FREE(smallest);
	    FREE(idx);
	  }
	  break; // break loop 'for(i=0 ...)'
	}
      }

      //print("%d %d %d %d\n", s->positivedefinite, s->trials, trials, force);
     
      if (!s->positivedefinite && (s->trials<trials)) { 
	FFT_destruct(&(s->FFT));
	switch (strategy) {
	  case 0 :
	    for (i=0; i<dim; i++) { /* enlarge uniformly in each direction, maybe 
				       this should be modified if the grid has 
				       different sizes in different directions */
		hilfsm[i] *= multivariate ? 3 : 2;
	    }
	    break;
	  case 1 :  
	    double cc, maxcc;

	    int maxi;
	    maxi = -1;
	    maxcc = RF_NEGINF;
	    for (i=0; i<dim; i++) hx[i] = 0.0;
	    for (i=0; i<dim; i++) {

	      // MUSS HIER ETWAS GEAENDERT WERDEN FUER MULTIVARIAT?

	      hx[i] = steps[i] * mm[i]; 

	      cc=0;
	      COV(hx, next, tmp);
	      for(l=0; l<vdimSQ; l++) cc += FABS(tmp[l]);
	      //if (PL>2) { LPRINT("%d cc=%e (%e)",i,cc,hx[i]); }
	      if (cc>maxcc) {
		maxcc = cc;
		maxi = i; 
	      }
	      hx[i] = 0.0;
	    }
	    assert(maxi>=0); 
	    hilfsm[maxi] *= multivariate ? 3 : 2;
	    break;
	  default:
	    ERR("unknown strategy for circulant embedding");
	}
      }
    } else {if (PL>=PL_BRANCHING) {LPRINT("forced.\n");} }
    R_CheckUserInterrupt();

    //   printf("trials=%d %d %d\n", s->trials, trials, s->positivedefinite);

  } // while (!s->positivedefinite && (s->trials<trials)) 354-706


  assert(mtot>0);
  if (s->positivedefinite || force) { 
    
 
    // correct theoretically impossible values, that are still within 
    // tolerance CIRCEMBED.tol_re/CIRCEMBED.tol_im 
    double r;
    r = Lambda[0][0]; // MULT

    for(i=0,twoi=0;i<mtot;i++) {
      twoi_plus1 = twoi+1;
      for(l=0;l<vdim;l++) {
	if(Lambda[l][i] > 0.0) Lambda[l][i] = SQRT(Lambda[l][i]);
	else {
	  if(Lambda[l][i] < r) r = Lambda[l][i];
	  Lambda[l][i] = 0;
	}
      }

      // now compute R[i]*Lambda^(1/2)[i]
      for(j=0;j<vdim;j++) {
	for(l=0; l<vdim; l++) {
	  c[j*vdim+l][twoi] *= Lambda[l][i];
	  c[j*vdim+l][twoi_plus1] *= Lambda[l][i];
	}
      }

      twoi+=2;

    }
    if (PL>=PL_BRANCHING) {
      if (r < -GENERAL_PRECISION) {
	LPRINT("using approximating circulant embedding:\n");
	LPRINT("\tsmallest real part has been %e \n", r);
      }
    }
    s->smallestRe = (r > 0.0) ? RF_NA : r;
    s->largestAbsIm = 0.0; // REMOVE THIS
  } else {
    //printf("FEHLER\n");
    GERR("embedded matrix has (repeatedly) negative eigenvalues and approximation is not allowed (force=FALSE)");
    }
  if (PL >= PL_SUBDETAILS) {
    for (i=0; i < 2 * mtot; i++)
      for (l=0; l<vdimSQ; l++)
	{LPRINT("%f ",c[l][i]);} 
    LPRINT("\n");
  }


  s->dependent = dependent;
  s->new_simulation_next = true;
  for(i=0; i<dim; i++) { // set anyway -- does not cost too much
    s->cur_square[i] = 0;
    s->max_squares[i] = hilfsm[i] / s->nn[i];
    s->square_seg[i] = cumm[i] * 
      (s->nn[i] + (hilfsm[i] - s->max_squares[i] * s->nn[i]) / 
       s->max_squares[i]);
  }

  if ( (s->d=(double **) CALLOC(vdim, sizeof(double *))) == NULL ) {
      err=ERRORMEMORYALLOCATION; goto ErrorHandling;
  }
  for(l=0; l<vdim;l++) {
      if( (s->d[l] = (double *) CALLOC(2 * mtot, sizeof(double))) == NULL) {
	  err=ERRORMEMORYALLOCATION;goto ErrorHandling;
      }
  }

  if ((s->gauss1 = (complex *) MALLOC(sizeof(complex) * vdim)) == NULL) {
    err=ERRORMEMORYALLOCATION; goto ErrorHandling;
  }
  if ((s->gauss2 = (complex *) MALLOC(sizeof(complex) * vdim)) == NULL) {
    err=ERRORMEMORYALLOCATION; goto ErrorHandling;
  }

  s->c = c;

  err = FieldReturn(cov);

ErrorHandling:
  if (Lambda != NULL) {
    for(l = 0; l<vdim; l++) FREE(Lambda[l]);
    UNCONDFREE(Lambda);
  }

  FREE(R);
  FREE(rwork);
  FREE(work);
  FREE(tmpLambda);
  FREE(sum);
  FREE(smallest);
  FREE(idx);
  FREE(tmp);
  
  if (err != NOERROR) {
      if (s != NULL) s->c = c;
      else if (c!=NULL) {
	  for(l=0;l<vdimSQ;l++) FREE(c[l]);
	  UNCONDFREE(c);
      }
  }

  cov->simu.active = err == NOERROR;

  //  printf("error =%d\n", err);
  return err;
}


void kappa_ce(int i, cov_model *cov, int *nr, int *nc){
  //  printf("kappa_ce %d %d %d\n", i , CE_MMIN, cov->tsdim);
  kappaGProc(i, cov, nr, nc);
  if (i == CE_MMIN) *nr = 0;
}


int check_ce_basic(cov_model *cov) { 
  // auf co/stein ebene !
  //  cov_model *next=cov->sub[0];
  //  location_type *loc = Loc(cov);
  int i, //err,
    dim = cov->tsdim
    ; // taken[MAX DIM],
  ce_param *gp  = &(GLOBAL.ce); // ok
  
  ROLE_ASSERT(ROLE_GAUSS);
  ASSERT_CARTESIAN;

  if(cov->tsdim != cov->xdimprev) {
    SERR2("time-space dimension (%d) differs from dimension of locations (%d)", 
	  cov->tsdim, cov->xdimown);
  }
  kdefault(cov, CE_FORCE, (int) gp->force);
  if (PisNULL(CE_MMIN)) {
    PALLOC(CE_MMIN, dim, 1);
    for (i=0; i<dim; i++) {
      P(CE_MMIN)[i] = gp->mmin[i];
    }
  } 
  kdefault(cov, CE_STRATEGY, (int) gp->strategy);
  kdefault(cov, CE_MAXGB, gp->maxGB);
  kdefault(cov, CE_MAXMEM, (int) gp->maxmem);
  kdefault(cov, CE_TOLIM, gp->tol_im);
  kdefault(cov, CE_TOLRE, gp->tol_re);
  kdefault(cov, CE_TRIALS, gp->trials);
  kdefault(cov, CE_USEPRIMES, (int) gp->useprimes);
  kdefault(cov, CE_DEPENDENT, (int) gp->dependent);
  kdefault(cov, CE_APPROXSTEP, gp->approx_grid_step);
  kdefault(cov, CE_APPROXMAXGRID, gp->maxgridsize);

  return NOERROR;
}

int check_ce(cov_model *cov) { 
  cov_model *next=cov->sub[0];
  location_type *loc = Loc(cov);
  int err,
    dim = cov->tsdim;
   // taken[MAX DIM],

  if ((err = check_ce_basic(cov)) != NOERROR) return err;
  if ((err = checkkappas(cov, false)) != NOERROR) return err;
  if (cov->tsdim != cov->xdimprev || cov->tsdim != cov->xdimown) 
    return ERRORDIM;
  if ((loc->timespacedim > MAXCEDIM) | (cov->xdimown > MAXCEDIM))
    return ERRORDIM;
  //  printf("here X\n");
   
  if (cov->key != NULL) {
    if ((err = CHECK(cov->key, loc->timespacedim, cov->xdimown, ProcessType,
		     XONLY, CARTESIAN_COORD, cov->vdim, ROLE_GAUSS)) 
	 != NOERROR) {
       //PMI(cov->key); XERR(err);
       //printf("specific ok\n");
       // crash();
       return err;
     }
  } else {
    if ((err = CHECK(next, dim,  dim, PosDefType, XONLY, SYMMETRIC,
		     SUBMODEL_DEP, ROLE_COV)) != NOERROR) {
      //      APMI(cov);
      //    XERR(err); APMI(cov);
      if ((err = CHECK(next, dim,  dim, VariogramType, XONLY, SYMMETRIC, 
		       SUBMODEL_DEP, ROLE_COV)) != NOERROR) {
	return err;
      }
    }
    if (next->pref[CircEmbed] == PREF_NONE) {
      //PMI(cov);
      //    assert(false);
      return ERRORPREFNONE;
    }
    if (!isPosDef(next->typus)) SERR("only covariance functions allowed.");
  }
  setbackward(cov, next);
  if ((err = kappaBoxCoxParam(cov, GAUSS_BOXCOX)) != NOERROR) return err;
  if ((err = checkkappas(cov, true)) != NOERROR) return err;
  return NOERROR;
}


void range_ce(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range){  
  GAUSS_COMMON_RANGE;

  range->min[CE_FORCE] = 0; 
  range->max[CE_FORCE] = 1;
  range->pmin[CE_FORCE] = 0;
  range->pmax[CE_FORCE] = 1;
  range->openmin[CE_FORCE] = false;
  range->openmax[CE_FORCE] = false;
  
  range->min[CE_MMIN] = RF_NEGINF; 
  range->max[CE_MMIN] = RF_INF;
  range->pmin[CE_MMIN] = -3;
  range->pmax[CE_MMIN] = 10000;
  range->openmin[CE_MMIN] = true;
  range->openmax[CE_MMIN] = true;
  
  range->min[CE_STRATEGY] = 0; 
  range->max[CE_STRATEGY] = 1;
  range->pmin[CE_STRATEGY] = 0;
  range->pmax[CE_STRATEGY] = 1;
  range->openmin[CE_STRATEGY] = false;
  range->openmax[CE_STRATEGY] = false;
  
  range->min[CE_MAXGB] = 0; 
  range->max[CE_MAXGB] = RF_INF;
  range->pmin[CE_MAXGB] = 0;
  range->pmax[CE_MAXGB] = 10;
  range->openmin[CE_MAXGB] = false;
  range->openmax[CE_MAXGB] = false;

  range->min[CE_MAXMEM] = 0; 
  range->max[CE_MAXMEM] = MAXINT;
  range->pmin[CE_MAXMEM] = 0;
  range->pmax[CE_MAXMEM] = 50000000;
  range->openmin[CE_MAXMEM] = false;
  range->openmax[CE_MAXMEM] = false;
  
  range->min[CE_TOLIM] = 0.0; 
  range->max[CE_TOLIM] = RF_INF;
  range->pmin[CE_TOLIM] = 0.0;
  range->pmax[CE_TOLIM] = 1e-2;
  range->openmin[CE_TOLIM] = false;
  range->openmax[CE_TOLIM] = false;
  
  range->min[CE_TOLRE] = RF_NEGINF; 
  range->max[CE_TOLRE] = RF_INF;
  range->pmin[CE_TOLRE] = 1e-2;
  range->pmax[CE_TOLRE] = 0;
  range->openmin[CE_TOLRE] = false;
  range->openmax[CE_TOLRE] = false;
  
  range->min[CE_TRIALS] = 1; 
  range->max[CE_TRIALS] = 10;
  range->pmin[CE_TRIALS] = 1;
  range->pmax[CE_TRIALS] = 3;
  range->openmin[CE_TRIALS] = false;
  range->openmax[CE_TRIALS] = true;
  
  range->min[CE_USEPRIMES] = 0; 
  range->max[CE_USEPRIMES] = 1;
  range->pmin[CE_USEPRIMES] = 0;
  range->pmax[CE_USEPRIMES] = 1;
  range->openmin[CE_USEPRIMES] = false;
  range->openmax[CE_USEPRIMES] = false;
  
  range->min[CE_DEPENDENT] = 0; 
  range->max[CE_DEPENDENT] = 1;
  range->pmin[CE_DEPENDENT] = 0;
  range->pmax[CE_DEPENDENT] = 1;
  range->openmin[CE_DEPENDENT] = false;
  range->openmax[CE_DEPENDENT] = false;

 
  range->min[CE_APPROXSTEP] = 0.0; 
  range->max[CE_APPROXSTEP] = RF_INF;
  range->pmin[CE_APPROXSTEP] = 1e-4;
  range->pmax[CE_APPROXSTEP] = 1e4;
  range->openmin[CE_APPROXSTEP] = true;
  range->openmax[CE_APPROXSTEP] = true;

 
  range->min[CE_APPROXMAXGRID] = 1; 
  range->max[CE_APPROXMAXGRID] = RF_INF;
  range->pmin[CE_APPROXMAXGRID] = 100;
  range->pmax[CE_APPROXMAXGRID] = 5e7;
  range->openmin[CE_APPROXMAXGRID] = false;
  range->openmax[CE_APPROXMAXGRID] = false;
}

 

void do_circ_embed(cov_model *cov, gen_storage VARIABLE_IS_NOT_USED *S){
  assert(cov != NULL);
  assert(Getgrid(cov));
  SAVE_GAUSS_TRAFO;
      
  int  i, j, k, l, pos,HalfMp1[MAXCEDIM], HalfMaM[2][MAXCEDIM], index[MAXCEDIM], 
    dim, *mm, *cumm, *halfm, // MULT added vars l, pos
    vdim;
  double invsqrtmtot,
    **c=NULL,
    **d=NULL,
    *dd=NULL,
    *res = cov->rf; // MULT *c->**c
  bool vfree[MAXCEDIM+1], noexception; // MULT varname free->vfree
  long mtot, start[MAXCEDIM], end[MAXCEDIM];
  ce_storage *s = cov->Sce;
  location_type *loc = Loc(cov);
  
  complex *gauss1 = s->gauss1, *gauss2 = s->gauss2;

  if (s->stop) XERR(ERRORNOTINITIALIZED);
 
  /* 
     implemented here only for rotationsinvariant covariance functions
     for arbitrary dimensions;
     (so it works only for even covariance functions in the sense of 
     Wood and Chan,p. 415, although they have suggested a more general 
     algorithm;) 
     
  */  
  
  dim = cov->tsdim;
  mm = s->m;
  cumm = s->cumm;
  halfm = s->halfm;
  c = s->c;
  d = s->d;
  mtot= s->mtot;
  vdim = s->vdim; // so p=vdim

  for (i=0; i<dim; i++) {
    HalfMp1[i] =  ((mm[i] % 2)==1) ? -1 : halfm[i];
    HalfMaM[0][i] = halfm[i];
    HalfMaM[1][i] = mm[i] - 1;
  }


  //if there are doubts that the algorithm below reaches all elements 
  //of the matrix, uncomment the lines containing the variable xx
  //
  //bool *xx; xx = (bool*) MALLOC(sizeof(bool) * mtot);
  //for (i=0; i<mtot;i++) xx[i]=true;
  
  invsqrtmtot = 1.0 / SQRT((double) mtot);

  if (PL>=PL_STRUCTURE) { LPRINT("Creating Gaussian variables... \n"); }
  /* now the Gaussian r.v. have to defined and multiplied with SQRT(FFT(c))*/

  for (i=0; i<dim; i++) {
    index[i]=0; 
    vfree[i] = false;
  }
  vfree[MAXCEDIM] = false;

  // PM 12/12/2008
  // don't simulate newly in case this is an even call
  s->cur_call_odd = !(s->cur_call_odd);

  //printf("%d \n", (int) s->new_simulation_next);
  //  printf("%d\n", (int) s->cur_call_odd);
  s->new_simulation_next = (s->new_simulation_next && s->cur_call_odd);

  if (s->new_simulation_next) {

    // MULT
 
    
    // martin: 13.12. check
    //for(k=0; k<vdim; k++) {
    //      dd = d[k];
    //  for (j=0; j<mtot; j++) dd[j] = RF_NA;
    //    }

    
    for(;;) {      
      i = j = 0;
      noexception = false;
      for (k=0; k<dim; k++) {
	i += cumm[k] * index[k];
	if ((index[k]==0) || (index[k]==HalfMp1[k])) {
	  j += cumm[k] * index[k];
	} else {
	  noexception = true; // not case 2 in prop 3 of W&C
	  j += cumm[k] * (mm[k] - index[k]);
	}
      }
      
      //A 0 0 0 0 0 0.000000 0.000000
      //B 0 0 0 0 0 -26.340334 0.000000

      //    if (index[1] ==0) print("A %d %d %d %d %d %f %f\n", 
      //	   index[0], index[1], i, j, noexception, d[i], d[i+1]);
      //      if (PL>=10) { LPRINT("cumm..."); }
      i <<= 1; // since we have to index imaginary numbers
      j <<= 1;

      //      print("ij %d %d (%d)\n", i, j, noexception);

      if (noexception) { // case 3 in prop 3 of W&C

        for(k=0; k<vdim; k++) {
	  gauss1[k].r = GAUSS_RANDOM(1.0);
	  gauss1[k].i = GAUSS_RANDOM(1.0);
	  gauss2[k].r = GAUSS_RANDOM(1.0);
	  gauss2[k].i = GAUSS_RANDOM(1.0);
        }

	
        for(k=0; k<vdim; k++) {
	  dd = d[k];
	  dd[j] = dd[i] = dd[j+1] = dd[i+1] = 0.0;// martin:13.12. neu eingefuegt
	  for(l=0; l<vdim; l++) {
	    pos = k * vdim + l;
	    dd[i] += c[pos][i] * gauss1[l].r - c[pos][i+1] * gauss1[l].i;   
	    // real part
	    dd[i+1] += c[pos][i+1] * gauss1[l].r + c[pos][i] * gauss1[l].i; 
	    // imaginary part 
	    
	    dd[j] += c[pos][j] * gauss2[l].r - c[pos][j+1] * gauss2[l].i;  
	    // real part    // martin: 13.12. geaendert von = + zu +=
	    dd[j+1] += c[pos][j+1] * gauss2[l].r + c[pos][j] * gauss2[l].i;  
	    // imaginary part
	  }
        }

      } else { // case 2 in prop 3 of W&C

        for(k=0; k<vdim; k++) {
	  gauss1[k].r = GAUSS_RANDOM(1.0);
	  gauss1[k].i = GAUSS_RANDOM(1.0);
        }

        for(k=0; k<vdim; k++) {
	  dd = d[k];
	  dd[i+1] = dd[i] = 0.0; //  martin: 13.12. neu eingefuegt
	  for(l=0; l<vdim; l++) {
	    pos = k * vdim + l;
	    dd[i] += c[pos][i] * gauss1[l].r - c[pos][i+1] * gauss1[l].i;   
	    // real part
	    dd[i+1] += c[pos][i+1] * gauss1[l].r + c[pos][i] * gauss1[l].i;  
	    // imaginary part
	  }
        }	
      }

      /* 
	 this is the difficult part.
	 We have to run over roughly half the points, but we should
	 not run over variables twice (time lost)
	 Due to case 2, we must include halfm.
	 
	 idea is:
	 for (i1=0 to halfm[dim-1])
	 if (i1==0) or (i1==halfm[dim-1]) then endfor2=halfm[dim-2] 
	 else endfor2=mm[dim-2]
         for (i2=0 to endfor2)
	 if ((i1==0) or (i1==halfm[dim-1])) and
	 ((i2==0) or (i2==halfm[dim-2]))
	 then endfor3=halfm[dim-3] 
	 else endfor3=mm[dim-3]
	 for (i3=0 to endfor3)
	 ....
	 
	 i.e. the first one that is not 0 or halfm (regarded from dim-1 to 0)
	 runs over 0..halfm, all the others over 0..m
	 
	 this is realised in the following allowing for arbitrary value of dim
	 
	 vfree==true   <=>   endfor==mm[]	 
      */
      k=0; 
      if (++index[k]>HalfMaM[vfree[k]][k]) {
	// in case k increases the number of indices that run over 0..m increases
	vfree[k] = true;
	index[k]= 0; 
	k++;
	while((k<dim) && (++index[k]>HalfMaM[vfree[k]][k])) {
	  vfree[k] = true;
	  index[k]= 0; 
	  k++;
	}
	if (k>=dim) break;
	// except the very last (new) number is halfm and the next index is
	// restricted to 0..halfm
	// then k decreases as long as the index[k] is 0 or halfm
	if (!vfree[k] && (index[k]==halfm[k])){//index restricted to 0..halfm?
	  // first: index[k] is halfm? (test on ==0 is superfluent)
	  k--;
	  while ( (k>=0) && ((index[k]==0) || (index[k]==halfm[k]))) {
	    // second and following: index[k] is 0 or halfm?
	    vfree[k] = false;
	    k--;
	  }
	}
      }
    }
      
    // MULT
    for(k=0; k<vdim; k++) {
      fastfourier(d[k], mm, dim, false, &(s->FFT));
    }
  } // if ** simulate **



  /* now we correct the result of the fastfourier transformation
     by the factor 1/SQRT(mtot) and read the relevant matrix out of 
     the large vector c */
  for(i=0; i<dim; i++) {
    index[i] = start[i] = s->cur_square[i] * s->square_seg[i];
    end[i] = start[i] + cumm[i] * s->nn[i];
    //    print("%d start=%d end=%d cur=%d max=%d seq=%d cumm=%d, nn=%d\n",
    //	   i, (int) start[i], (int) end[i],
    //	   s->cur_square[i], s->max_squares[i],
    //	   (int) s->square_seg[i], cumm[i], s->nn[i]);
  }
  long L, totpts = loc->totalpoints; // MULT

  /*
    for(l=0; l<vdim; l++) { // MULT
    for(k=0; k<dim; k++) index[k] = start[k];

    for (i=l*totpts; i<(l+1)*totpts; i++){
    j=0; for (k=0; k<dim; k++) {j+=index[k];}
    //res[i] += d[l][2 * j + !(s->cur_call_odd)] * invsqrtmtot;
    res[i] += d[l][2 * j] * invsqrtmtot;
    for(k=0; (k<dim) && ((index[k] += cumm[k]) >= end[k]); k++) {
    index[k]=start[k];
    }
    }
    }
  */
  // 


  for(k=0; k<dim; k++) index[k] = start[k];

  bool vdim_close_together = GLOBAL.general.vdim_close_together;
  for (L=0; L<totpts; L++) {
    j=0; for (k=0; k<dim; k++) {j+=index[k];}
    for (l=0; l<vdim; l++) { // MULT
      res[vdim_close_together ? vdim * L + l : L + l * totpts] = 
	(double) (d[l][2 * j + !(s->cur_call_odd)] * invsqrtmtot);      
    }
    for(k=0; (k<dim) && ((index[k] += cumm[k]) >= end[k]); k++) {
      index[k]=start[k];
      
    }
  }

 

  s->new_simulation_next = true;
  if (s->dependent && !(s->cur_call_odd)) {
    //if (s->dependent) {
    //  if MaxStableRF calls Cutoff, c and look uninitialised
    k=0; 
    while(k<dim && (++(s->cur_square[k]) >= s->max_squares[k])) {
      s->cur_square[k++]=0;
    }    
    s->new_simulation_next = k == dim; 
  }
  //
  //  print("dep=%d odd=%d squ=%d %d\n", s->dependent, s->cur_call_odd,
  //	 s->cur_square[0],s->cur_square[1]);
  s->stop |= s->new_simulation_next && s->d == NULL;

  BOXCOX_INVERSE;

}



int GetOrthogonalUnitExtensions(double * aniso, int dim, double *grid_ext) {
  int k,i,j,l,m, job=01, err, dimsq, ev0, jump, endfor;
  double *s=NULL, G[MAXCEDIM+1], e[MAXCEDIM], D[MAXCEDIM], *V=NULL;
  dimsq = dim * dim;

  assert(aniso != NULL);
  
  s = (double*) MALLOC(dimsq * sizeof(double));
  V = (double*) MALLOC(dimsq * sizeof(double));
  for (k=0; k<dim; k++) {
    // kte-Zeile von aniso wird auf null gesetzt/gestrichen -> aniso_k
    // s = aniso %*% aniso_k 
    // s hat somit mindestens Eigenwert der 0 ist.
    //der zugeheorige EVektor wird miit der k-ten Spalte von aniso multipliziert
    // und ergibt dann den Korrekturfaktor
    for (i=0; i<dim; i++) {
      for (l=j=0; j<dimsq; j+=dim) {
	s[j + i] = 0.0;
	jump = l + k;
	endfor = l + dim;
	for (m=i; l<endfor; l++, m += dim) {
	  if (l!=jump) s[i + j] +=  aniso[m] * aniso[l];
	}
      }
    }
    //    for (print("\n\n s\n"), j=0; j<dim; j++, print("\n")) 
    //	for (i=0; i<dimsq; i+=dim) print("%f ", s[i+j]);
    //    for (print("\n\n aniso\n"), j=0; j<dim; j++, print("\n")) 
    //	for (i=0; i<dimsq; i+=dim) print("%f ", aniso[i+j]);	
    
    // note! s will be distroyed by dsvdc!
    F77_NAME(dsvdc)(s, &dim, &dim, &dim, D, e, NULL /* U */,
		    &dim, V, &dim, G, &job, &err);
    if (err!=NOERROR) { err=-err;  goto ErrorHandling; }
    ev0 = -1;
    for (i=0; i<dim; i++) {
      if (FABS(D[i]) <= EIGENVALUE_EPS) {
	if (ev0==-1) ev0=i;
	else {
	  GERR("anisotropy matrix must have full rank")
	    } 
      }
    }
    
    //    for (print("\n\n V\n"), j=0; j<dim; j++, print("\n")) 
    //	for (i=0; i<dimsq; i+=dim) print("%f ", V[i+j]);

    grid_ext[k] = 0.0;
    ev0 *= dim;
    for (i=0; i<dim; i++) {
      //	print("%d : %f %f %f\n", k,
      //	       V[ev0 + i], aniso[k + i * dim], V[ev0 + i] / aniso[k + i * dim]);
      grid_ext[k] += V[ev0 + i] * aniso[k + i * dim];
    }
    grid_ext[k] = FABS(grid_ext[k]);
  }
  FREE(V);
  FREE(s);
  return NOERROR;

 ErrorHandling:
  if (err<0) {
    KPRINT("F77 error in GetOrthogonalExtensions: %d\n", -err);
    err = ERRORFAILED;
  }
  FREE(V);
  FREE(s);
  return err;
}






int check_local_proc(cov_model *cov) {
  int 
    dim = cov->tsdim,
    err=NOERROR;
  cov_model 
    *key = cov->key,
    *next = cov->sub[0],
    *sub = key != NULL ? key : next;
  location_type *loc = Loc(cov);
  bool cutoff = cov->nr == CE_CUTOFFPROC_USER || cov->nr==CE_CUTOFFPROC_INTERN;
  if (!cutoff && cov->nr!=CE_INTRINPROC_USER && cov->nr!=CE_INTRINPROC_INTERN)
    BUG;

  //  print("entering check local from %d:%s\n", cov->calling->nr,
  //	 CovList[cov->calling->nr].name);

  //  PMI(cov);
   
  if ((err = check_ce_basic(cov)) != NOERROR) return err;
  if (cov->tsdim != cov->xdimprev || cov->tsdim != cov->xdimown) 
    return ERRORDIM;
  if ((loc->timespacedim > MAXCEDIM) | (cov->xdimown > MAXCEDIM))
    return ERRORDIM;

  if (key != NULL) {
    // falls nicht intern muessen die parameter runter und rauf kopiert
    // werden, ansonsten sind die kdefaults leer.
    // falls hier gelandet, wird RP*INTERN von RP* aufgerufen.
    // oder RP*INTERN wird Circulant aufrufen
    cov_model *intern = cov,
      *RMintrinsic = key->sub[0];
    while (intern != NULL && intern->nr != CE_INTRINPROC_INTERN &&
	   intern->nr != CE_CUTOFFPROC_INTERN) {
      // normalerweise ->key, aber bei $ ist es ->sub[0]
      intern = intern->key == NULL ? intern->sub[0] : intern->key;
    }
    if (intern == NULL) {
      BUG;
    }
    else if (intern != cov) // cov not intern, ie user
      paramcpy(intern, cov, true, true, false, false, false);  // i.e. INTERN
    else if (key->nr == CE_INTRINPROC_INTERN || 
	     key->nr == CE_CUTOFFPROC_INTERN) { 
      // 2x intern hintereinander, d.h. es wird eine approximation durchgefuehrt
      paramcpy(key, cov, true, true, false, false, false);        
    } else {
      //if (RMintrinsic->nr == GAUSSPROC) RMintrinsic = RMintrinsic->sub[0];
      if (RMintrinsic->nr != CUTOFF && RMintrinsic->nr != STEIN) {
	BUG;
      }

      if (!PisNULL(LOCPROC_DIAM)) 
	kdefault(RMintrinsic, pLOC_DIAM, P0(LOCPROC_DIAM));
      if (!PisNULL(LOCPROC_R)) 
	kdefault(RMintrinsic, pLOC_DIAM, P0(LOCPROC_R));
    }
     
    if ((err = CHECK(sub, dim,  dim, ProcessType, KERNEL, CARTESIAN_COORD, 
		     SUBMODEL_DEP, ROLE_GAUSS)) != NOERROR) {
      return err;
    }
    if (intern == cov) { // all the other parameters are not needed on the 
      //                    upper levels
      if (PisNULL(LOCPROC_DIAM))
	kdefault(cov, LOCPROC_DIAM, PARAM0(RMintrinsic, pLOC_DIAM));  
    }
  } else {
    if ((err = CHECK(sub, dim,  1, cutoff ? PosDefType : VariogramType,
		     XONLY, ISOTROPIC, SUBMODEL_DEP, ROLE_COV))
	!= NOERROR) {
      if (isDollar(next) && PARAM(next, DANISO) != NULL) {
	// if aniso is given then xdimprev 1 does not make sense
	err = CHECK(sub, dim, dim, cutoff ? PosDefType : VariogramType,
		    XONLY, ISOTROPIC, SUBMODEL_DEP, ROLE_COV);
      }
      // 
      if (err != NOERROR) return err;
      // PMI(cov, "out"); 
      //assert(false);
    }
  }

  //printf("check intern end\n");

  // no setbackward ?!
  setbackward(cov, sub); 
  cov->vdim[0] = cov->vdim[1] = sub->vdim[0];
  if ((err = kappaBoxCoxParam(cov, GAUSS_BOXCOX)) != NOERROR) return err;
  
  return NOERROR;
}

int init_circ_embed_local(cov_model *cov, gen_storage *S){
  // being here, leadin $ have already been put away
  // so a new $ is included below

  cov_model //*dummy = NULL,
    *key = cov->key;
  location_type *loc = Loc(cov);
  //  simu_type *simu = &(cov->simu);
  int instance, i, d, 
    timespacedim = loc->timespacedim,   
    cncol = cov->tsdim,
    rowcol = timespacedim * cncol,
    err = NOERROR;
  localCE_storage *s=NULL;
  double grid_ext[MAXCEDIM], old_mmin[MAXCEDIM];
 
  int first_instance;
  double
    *mmin = P(CE_MMIN);

  double sqrt2a2;
  bool is_cutoff = cov->nr == CE_CUTOFFPROC_INTERN;


  // APMI(cov);
  // printf("init_circ_embed_local\n");assert(false);

  //  static pref_type pref =
  //    {5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  // CE CO CI TBM Sp di sq Ma av n mpp Hy - any

  ROLE_ASSERT_GAUSS;

  cov->method = is_cutoff ? CircEmbedCutoff : CircEmbedIntrinsic;

  if (loc->distances) return ERRORFAILED;

  NEW_STORAGE(localCE);
  s = cov->SlocalCE;
  
  if (loc->caniso != NULL) {
    if (loc->cani_ncol != loc->cani_nrow || 
	loc->cani_ncol != timespacedim) return ERRORDIM;
    err = GetOrthogonalUnitExtensions(loc->caniso, timespacedim, grid_ext);
    if (err != NOERROR) goto ErrorHandling;
  } else {
    for (i=0; i<timespacedim; i++) grid_ext[i] = 1.0;
  }

  //printf("diameter %d \n",LOCPROC_DIAM); APMI(cov);

  if (!PARAMisNULL(key->sub[0], LOCPROC_R))
    kdefault(key, pLOC_DIAM, P0(LOCPROC_R));
 
  kdefault(key, CE_FORCE, P0INT(CE_FORCE));

  PARAMFREE(key, CE_MMIN);
  PARAMALLOC(key, CE_MMIN, cov->tsdim, 1);
  PCOPY(key, cov, CE_MMIN);
  
  kdefault(key, CE_STRATEGY, P0INT(CE_STRATEGY));
  kdefault(key, CE_MAXGB, P0(CE_MAXGB));
  kdefault(key, CE_MAXMEM, P0INT(CE_MAXMEM));
  kdefault(key, CE_TOLIM, P0(CE_TOLIM));
  kdefault(key, CE_TOLRE, P0(CE_TOLRE));
  kdefault(key, CE_TRIALS, P0INT(CE_TRIALS));
  kdefault(key, CE_USEPRIMES, P0INT(CE_USEPRIMES));
  kdefault(key, CE_DEPENDENT, P0INT(CE_DEPENDENT));

  //  APMI(key);
 
  assert(cov->vdim[0] == cov->vdim[1]);
  err = CHECK(key, cov->tsdim, cov->xdimprev,
	      GaussMethodType,
              cov->domown, cov->isoown, SUBMODEL_DEP, ROLE_GAUSS); 
  if ((err < MSGLOCAL_OK && err != NOERROR) 
      || err >=MSGLOCAL_ENDOFLIST // 30.5.13 : neu
      ) {
    // PMI(cov);AERR(err);
    goto ErrorHandling;
  }
 

  first_instance = err != NOERROR;
  for (d=0; d<timespacedim; d++) {
    old_mmin[d] = mmin[d];
  }

  double *q;
  cov_model 
    //model = local->sub[0],
    *local;
  local = key->sub[0];
  q = local->q;
  assert(err == NOERROR);
  for (instance = first_instance; instance < 2; instance++) {
    //printf("lcoal start err =%d %d\n", err, instance);
    //printf("model =%s instance %d %d err=%d %f %d\n", CovList[model->nr].name,
    //	   instance, first_instance, err, q[LOCAL_MSG], MSGLOCAL_OK);
    if (instance == 1) {
      if (q[LOCAL_MSG] != MSGLOCAL_OK) {
	if (!CovList[local->nr].alternative(local)) break;
      } else {
	if (first_instance == 0 && err != NOERROR) goto ErrorHandling;
	else BUG;
      }
    } else assert(instance == 0);
    
    for (d=0; d<timespacedim; d++) {
      if (old_mmin[d]==0.0) {
	
	mmin[d] = - q[LOCAL_R] / 
	  (grid_ext[d] * (loc->xgr[d][XLENGTH] - 1.0) * loc->xgr[d][XSTEP]);
	if (mmin[d] > -1.0) mmin[d] = -1.0;
	
      }
    }
    if ((err = init_circ_embed(key, S)) == NOERROR) break;
  }

  //  printf("local err = %d\n", err);
  
  if (err != NOERROR) goto ErrorHandling;
  //printf("A end local err %d\n", err);
  
  if (cov->nr == CE_INTRINPROC_INTERN) {
    sqrt2a2 = SQRT(2.0 * q[INTRINSIC_A2]); // see Stein (2002) timespacedim * cncol
    if (loc->caniso == NULL) {
      if ((s->correction = MALLOC(sizeof(double)))==NULL) {
	err = ERRORMEMORYALLOCATION;
	goto ErrorHandling;
      }
      ((double *) s->correction)[0] = sqrt2a2;
    } else {       
      if ((s->correction = MALLOC(sizeof(double) * rowcol))==NULL){
	err = ERRORMEMORYALLOCATION;
	goto ErrorHandling;
      }
      double *stein_aniso = (double*) s->correction;
      for (i=0; i<rowcol; i++) {
	// print("ii=%d\n", i); print("%f\n", loc->caniso[i]);
	stein_aniso[i] = sqrt2a2 * loc->caniso[i];
      }
    }
  } else {
    assert(cov->nr == CE_CUTOFFPROC_INTERN);
  }

  if ((err = kappaBoxCoxParam(cov, GAUSS_BOXCOX)) !=NOERROR) goto ErrorHandling;

  assert(err == NOERROR);
  
 ErrorHandling: 
  // printf("X end local err %d\n", err);
  for (d=0; d<timespacedim; d++) {
    mmin[d] = old_mmin[d];
  }

  cov->fieldreturn = true;
  cov->origrf = false;
  cov->rf = cov->key->rf;
  cov->simu.active = err == NOERROR;

   return err;
}


int struct_ce_local(cov_model *cov, cov_model VARIABLE_IS_NOT_USED **newmodel) {
  cov_model 
    *next = cov->sub[0];
  int err;
  bool cutoff = cov->nr ==  CE_CUTOFFPROC_INTERN;
  
  if (cov->role != ROLE_GAUSS) BUG;
  if (next->pref[cutoff ? CircEmbedCutoff : CircEmbedIntrinsic] == PREF_NONE) {
    return ERRORPREFNONE;
  }

  if (cov->key != NULL) COV_DELETE(&(cov->key));
  if ((err = covCpy(&(cov->key), next)) != NOERROR) return err;
  addModel(&(cov->key), cutoff ? CUTOFF : STEIN);
  addModel(&(cov->key), CIRCEMBED);
  
  // crash(cov);

  // da durch init die relevanten dinge gesetzt werden, sollte
  // erst danach bzw. innerhalb von init der check zum ersten Mal
  //APMI(cov);
  return NOERROR;
}


void do_circ_embed_cutoff(cov_model *cov, gen_storage *S) {  
    double   *res = cov->rf;
    cov_model *sub = cov;
    for (int i=0; i<2; i++) sub =  sub->key != NULL ? sub->key : sub->sub[0];
    //    long index[MAXCEDIM], r;
    location_type *loc = Loc(cov);
    int // k, row = loc->timespacedim,
      vdim =cov->vdim[0];
    long totpts = loc->totalpoints;


    // printf("--- \n \n do_circ_embed_cutoff \n \n  ---");
    //PMI(sub);
    localCE_storage *s = sub->SlocalCE;


    // PMI(sub);


    /*
    printf( "\n s->is_multivariate_cutoff = %d \n", s->is_multivariate_cutoff);//

    printf("\n -------------  s->q[0][CUTOFF_CONSTANT]  = %f, C12 = %f, C22 =%f  \n ",  s->q[0][CUTOFF_CONSTANT] , c12, c22);//
    printf("\n ------------- C11*C22 - C12*C12 = %f,   \n ", c11*c22 - c12*c12);//
    printf("\n ------------- x[0] = %f, x[1] =%f  \n ", x[0], x[1]);//
    printf("\n -------------  s->q[0][CUTOFF_R]  = %f  \n ",  s->q[0][CUTOFF_R] );//
*/

    do_circ_embed(cov->key, S);

    if (s->is_bivariate_cutoff) {

        double normal1 = GAUSS_RANDOM(1.0), normal2 = GAUSS_RANDOM(1.0),
                c11 = s->q[0][CUTOFF_CONSTANT], c12 = s->q[1][CUTOFF_CONSTANT],
                c22 = s->q[3][CUTOFF_CONSTANT],
                x[2];

        if (c22*c11 - c12*c12 < 0 ) ERR("\n Cannot simulate field with cutoff, matrix of constants is not pos def \n ");
        x[0] = SQRT( -c11)*normal1 ;
        x[1] = -c12/SQRT(-c11)*normal1 + SQRT(-c22 + c12*c12/c11 )*normal2;

        if (GLOBAL.general.vdim_close_together) {
            //one location two values
            for (int i = 0; i < totpts; i++)  {
                for (int j = 0; j < vdim; j++)  {
                    res[i*vdim + j] += x[j];
                }
            }
        } else {
            // the first field, the second field
            for (int j = 0; j < vdim; j++)  {
                for (int i = 0; i < totpts; i++)  {
                    res[i+j*totpts] += x[j];
                }
            }

        }
    }

}

void kappa_localproc(int i, cov_model *cov, int *nr, int *nc){
  kappaGProc(i, cov, nr, nc);
  if (i == CE_MMIN) *nr = 0;

  // $(proj=1) ->intrinsicinternal as $ copies the parameters
  // to intrinsicinternal and at the same time proj=1 reduces
  // the tsdim
}

void range_co_proc(cov_model *cov, range_type *range){  
  range_ce(cov, range);

  range->min[LOCPROC_DIAM] = 0.0; //  CUTOFF_DIAM
  range->max[LOCPROC_DIAM] = RF_INF;
  range->pmin[LOCPROC_DIAM] = 1e-10;
  range->pmax[LOCPROC_DIAM] = 1e10;
  range->openmin[LOCPROC_DIAM] = true;
  range->openmax[LOCPROC_DIAM] = true;

  range->min[LOCPROC_A] = 0.0; // cutoff_a
  range->max[LOCPROC_A] = RF_INF;
  range->pmin[LOCPROC_A] = 0.5;
  range->pmax[LOCPROC_A] = 2.0;
  range->openmin[LOCPROC_A] = true;
  range->openmax[LOCPROC_A] = true;
}



void range_intrinCE(cov_model *cov, range_type *range) {
  range_ce(cov, range);

  range->min[LOCPROC_DIAM] = 0.0; //  CUTOFF_DIAM
  range->max[LOCPROC_DIAM] = RF_INF;
  range->pmin[LOCPROC_DIAM] = 0.01;
  range->pmax[LOCPROC_DIAM] = 100;
  range->openmin[LOCPROC_DIAM] = true;
  range->openmax[LOCPROC_DIAM] = true;

  range->min[LOCPROC_R] = 1.0; // stein_r
  range->max[LOCPROC_R] = RF_INF;
  range->pmin[LOCPROC_R] = 1.0;
  range->pmax[LOCPROC_R] = 20.0;
  range->openmin[LOCPROC_R] = false;
  range->openmax[LOCPROC_R] = true;
}


void do_circ_embed_intr(cov_model *cov, gen_storage *S) {  
  cov_model *key = cov->key;
  assert(key != NULL);
  location_type *loc = Loc(cov);
  double x[MAXCEDIM], dx[MAXCEDIM],  
    *res = cov->rf;
  long index[MAXCEDIM], r;
  int i, l, k,
    row = loc->timespacedim,
    col = cov->tsdim,
    rowcol =  row * col;
  localCE_storage *s = cov->SlocalCE;
  SAVE_GAUSS_TRAFO;

  do_circ_embed(key, S);

  for (k=0; k<row; k++) {
    index[k] = 0;
    dx[k] = x[k] = 0.0;
  }

  double *stein_aniso = (double*) s->correction;
  if (loc->caniso == NULL) {
    for (l=0; l<row; l++) {
      double normal = GAUSS_RANDOM(1.0);
      dx[l] += *stein_aniso * normal;
    }
  } else {
    for (i=0; i<rowcol; ) {
      double normal = GAUSS_RANDOM(1.0);
      for (l=0; l<row; l++)
	dx[l] += stein_aniso[i++] * normal;
    }
  }
  for (k=0; k<row; k++) dx[k] *= loc->xgr[k][XSTEP];
  for(r=0; ; ) { 
    for (k=0; k<row; k++) res[r] += (double) x[k]; 
    r++;
    k=0;
    while( (k<row) && (++index[k]>=loc->xgr[k][XLENGTH])) {
      index[k]=0;
      x[k] = 0.0;
      k++;
    }
    if (k>=row) break;
    x[k] += dx[k];
  }
  BOXCOX_INVERSE;
}




int struct_ce_approx(cov_model *cov, cov_model **newmodel) {
  assert(cov->nr==CE_CUTOFFPROC_INTERN || cov->nr==CE_INTRINPROC_INTERN 
	 || cov->nr==CIRCEMBED);
  cov_model *next = cov->sub[0];
  bool cutoff = cov->nr ==  CE_CUTOFFPROC_INTERN;
  if (next->pref[cov->nr==CIRCEMBED ? CircEmbed : 
		 cutoff ? CircEmbedCutoff : CircEmbedIntrinsic] == PREF_NONE) {
    return ERRORPREFNONE;
  }
 
  //PMI(cov);
  if (cov->role != ROLE_GAUSS) BUG;


  // assert(!Getgrid(cov));
 
  if (!Getgrid(cov)) {    
    ASSERT_NEWMODEL_NULL;
    location_type *loc = Loc(cov);  
    double max[MAXCEDIM], min[MAXCEDIM],  centre[MAXCEDIM], 
      x[3 * MAXCEDIM],
      approx_gridstep = P0(CE_APPROXSTEP);
    int k, d, len, err,
      maxgridsize = P0INT(CE_APPROXMAXGRID),
      spatialdim = loc->spatialdim;
     
    if (approx_gridstep < 0)
      SERR("approx_step < 0 forbids approximative circulant embedding");
    
    if (cov->xdimown != loc->timespacedim)
      SERR("the dimensions of the coordinates and those of the process differ");
    
    GetDiameter(loc, min, max, centre);
  
    if (loc->Time) {
      if (loc->T[XLENGTH] > maxgridsize) SERR("temporal grid too long");
      maxgridsize /= loc->T[XLENGTH];
    }

    if (ISNA(approx_gridstep)) {
      // hier assert(false);
      double size = loc->spatialtotalpoints * 3;
      if (size > maxgridsize) size = maxgridsize;
      for (k=d=0; d<spatialdim; d++, k+=3) {
	x[k+XSTART] = min[d];
	x[k+XLENGTH] = (int) POW(size, 1.0 / (double) spatialdim);
	x[k+XSTEP] = (max[d] - min[d]) / (x[k+XLENGTH] - 1.0);
      }
    } else {
     len = 1;
      for (k=d=0; d<spatialdim; d++, k+=3) {
	x[k+XSTART] = min[d];
	len *= 
	  x[k+XLENGTH] = (int) ((max[d] - min[d]) / approx_gridstep)+1;
	x[k+XSTEP] = approx_gridstep;
      }
      if (len > maxgridsize) SERR("userdefined, approximate grid is too large");
    }
  
    if (cov->key!=NULL) COV_DELETE(&(cov->key));
    err = covCpy(&(cov->key), cov, x, loc->T, spatialdim, spatialdim,
		 3, loc->Time, true, false);
    if (err != NOERROR) return err;    
    cov->key->calling = cov;

    if ((err = CHECK(cov, cov->tsdim, cov->xdimprev, cov->typus,
		     cov->domprev, cov->isoprev, cov->vdim[0], cov->role)  
	 ) != NOERROR) return err;
 
  }

  if (cov->nr == CIRCEMBED) return NOERROR;
  else return struct_ce_local(Getgrid(cov) ? cov : cov->key, newmodel);

}




int init_ce_approx(cov_model *cov, gen_storage *S) {  
  //  printf("\ninit_ce_approx : %d %d\n", Getgrid(cov), cov->nr==CIRCEMBED); 

  if (Getgrid(cov)) {
    int ans = cov->nr==CIRCEMBED ? init_circ_embed(cov, S)
      : init_circ_embed_local(cov, S); 
    // printf("ans = %d\n", ans); BUG;
    return ans;
  }

  ROLE_ASSERT_GAUSS;

  location_type *loc = Loc(cov),
    *keyloc = Loc(cov->key);  
  assert(cov->key != NULL && keyloc != NULL);
 long i, cumgridlen[MAXCEDIM], 
   totspatialpts = loc->spatialtotalpoints;
  int d, err,
    // maxgridsize = P0INT(CE_APPROXMAXGRID),
    spatialdim = loc->spatialdim,
    tsdim = loc->timespacedim;

  if (cov->xdimown != tsdim)
    SERR("dimensions of the coordinates and those of the process differ");
  cov->method = cov->nr==CIRCEMBED ? CircEmbed 
    : cov->nr== CE_CUTOFFPROC_INTERN ? CircEmbedCutoff : CircEmbedIntrinsic;

  if (loc->distances) return ERRORFAILED;
  NEW_STORAGE(approxCE);
  ALLOC_NEWINT(SapproxCE, idx, totspatialpts, idx);
  
  cumgridlen[0] = 1;
  for (d=1; d<tsdim; d++) {
    cumgridlen[d] =  cumgridlen[d-1] * (int) keyloc->xgr[d-1][XLENGTH];    
  }

  double *xx = loc->x;

  for (i=0; i<totspatialpts; i++) {
    int dummy;    
    for (dummy = d = 0; d<spatialdim; d++, xx++) {
      dummy += cumgridlen[d] *
	(int) ROUND((*xx - keyloc->xgr[d][XSTART]) / keyloc->xgr[d][XSTEP]);
    }    
    idx[i] = dummy;
    assert(dummy >= 0);
  }
 
  if ((err = cov->nr==CIRCEMBED ? init_circ_embed(cov->key, S)
       : init_circ_embed_local(cov->key, S)) != NOERROR) return err;
  
  if ((err = FieldReturn(cov)) != NOERROR) return err;
 
  cov->key->initialised = cov->simu.active = err == NOERROR;
  return NOERROR;
}

void do_ce_approx(cov_model *cov, gen_storage *S){
  if (Getgrid(cov)) {
    if (cov->nr==CIRCEMBED) do_circ_embed(cov, S);
    else if (cov->nr== CE_CUTOFFPROC_INTERN) do_circ_embed_cutoff(cov, S);
    else do_circ_embed_intr(cov, S);
    return;
  }

  
  //  PMI(cov);

  cov_model *key=cov->key;
  location_type *loc = Loc(cov);
  approxCE_storage *s = cov->SapproxCE;
  //cov_model *cov = meth->cov;
  long i;
  int
     vdim = cov->vdim[0],
    *idx = s->idx;
  double 
    *res = cov->rf,
     *internalres = key->rf;

  // APMI(key);

  DO(key, S);
  location_type *key_loc = Loc(key);
  if (key_loc->Time) { // time separately given   
    long  t,
      j = 0,
      instances = (long) loc->T[XLENGTH],
      totspatialpts = loc->spatialtotalpoints,
      gridpoints = Loc(key)->spatialtotalpoints;
    
    for (int v=0; v<vdim; v++){
      for (t=0; t<instances; t++, internalres += gridpoints) {
	for (i=0; i<totspatialpts; i++) {
	  res[j++] = internalres[idx[i]];
	}
      }
    }
  } else { // no time separately given
    long totpts = loc->totalpoints;
    int 
      j = 0,
      totalkey = Loc(key)->totalpoints;
    for (int v=0; v<vdim; v++, internalres += totalkey){
      for (i=0; i<totpts; i++) {
      //    printf("i=%d %f %d\n", i, loc->x[i], idx[i]);
      // print(" %d %d %f %f", i, idx[i], loc->x[i*2], loc->x[i*2 +1]);
      // print("    %f\n", internalres[idx[i]]);
	res[j++] = internalres[idx[i]];
      }
    }
  }

}

