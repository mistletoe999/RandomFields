/*
 Authors 
 Yindeng, Jiang, jiangyindeng@gmail.com
 Martin Schlather, schlath@hsu-hh.de

 Simulation of a random field by circulant embedding
 (see Wood and Chan, or Dietrich and Newsam for the theory )

 Copyright (C) 2001 -- 2003 Martin Schlather
 Copyright (C) 2004 -- 2005 Yindeng Jiang & Martin Schlather

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
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
#include <stdlib.h>
#include "RFsimu.h"
#include <assert.h>
#include <R_ext/Applic.h>

ce_param CIRCEMBED={false, true, TRIVIALSTRATEGY, -1e-7, 1e-3, 3, 20000000, 
		    0, 0, 0, 0};
ce_param LOCAL_CE={false, true, TRIVIALSTRATEGY, -1e-9, 1e-7, 1, 20000000, 
		    0, 0, 0, 0};

void FFT_destruct(FFT_storage *FFT)
{
  if (FFT->iwork!=NULL) {free(FFT->iwork); FFT->iwork=NULL;}
  if (FFT->work!=NULL) {free(FFT->work); FFT->work=NULL;} //?
}

void FFT_NULL(FFT_storage *FFT) 
{
  FFT->work = NULL;
  FFT->iwork = NULL;
}

void CE_destruct(void **S) 
{
  if (*S!=NULL) {
    CE_storage *x;
    x = *((CE_storage**)S);
    if (x->c!=NULL) free(x->c);
    if (x->d!=NULL) free(x->d);
    FFT_destruct(&(x->FFT));
    free(*S);
    *S = NULL;
  }
}
 
void localCE_destruct(void **S) 
{
  if (*S!=NULL) {
    localCE_storage* x;
    x = *((localCE_storage**) S);
    DeleteKeyNotTrend(&(x->key)); 
    free(*S);
    *S = NULL;
  }
}


/*********************************************************************/
/*           CIRCULANT EMBEDDING METHOD (1994) ALGORITHM             */
/*  (it will always be refered to the paper of Wood & Chan 1994)     */
/*********************************************************************/

void SetParamCircEmbed( int *action, int *force, double *tolRe, double *tolIm,
			int *trials, double *mmin, int *useprimes, int *strategy,
		        double *maxmem) 
{
  SetParamCE(action, force, tolRe, tolIm, trials, mmin, useprimes, strategy,
	     maxmem, &CIRCEMBED, "CIRCEMBED");
}

void SetParamLocal( int *action, int *force, double *tolRe, double *tolIm,
			int *trials, double *mmin, int *useprimes, int *strategy,
		        double *maxmem) 
{
  SetParamCE(action, force, tolRe, tolIm, trials, mmin, useprimes, strategy,
	     maxmem, &LOCAL_CE, "LOCAL");
}

int fastfourier(double *data, int *m, int dim, bool first, bool inverse,
		FFT_storage *FFT)
  /* this function is taken from the fft function by Robert Gentleman 
     and Ross Ihaka, in R */
{
    long int inv, nseg, n,nspn,i;
  int maxf, maxp, Xerror;
  if (first) {
   int maxmaxf,maxmaxp;
   nseg = maxmaxf =  maxmaxp = 1;
   /* do whole loop just for Xerror checking and maxmax[fp] .. */

   for (i = 0; i<dim; i++) {
     if (m[i] > 1) {
       fft_factor(m[i], &maxf, &maxp);
       if (maxf == 0) {Xerror=ERRORFOURIER; goto ErrorHandling;}	
       if (maxf > maxmaxf) maxmaxf = maxf;
       if (maxp > maxmaxp) maxmaxp = maxp;
       nseg *= m[i];
     }
   }
 
   assert(FFT->work==NULL);
   if ((FFT->work = (double*) malloc(4 * maxmaxf * sizeof(double)))==NULL) { 
     Xerror=ERRORMEMORYALLOCATION; goto ErrorHandling; 
   }
   assert(FFT->iwork==NULL);
   if ((FFT->iwork = (int*) malloc(maxmaxp  * sizeof(int)))==NULL) { 
     Xerror=ERRORMEMORYALLOCATION; goto ErrorHandling;
   }
   FFT->nseg = nseg;
   //  nseg = LENGTH(z); see loop above
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
      fft_work(&(data[0]), &(data[1]), nseg, n, nspn, inv, FFT->work,FFT->iwork);
    }
  }
  return NOERROR;
 ErrorHandling:
  FFT_destruct(FFT);
  return Xerror;
}

int fastfourier(double *data, int *m, int dim, bool first, FFT_storage *FFT){
  return fastfourier(data, m, dim, first, !first, FFT);
}


int init_circ_embed(key_type *key, int m)
{
  methodvalue_type *meth;  
  int Xerror=NOERROR, d, actcov;
  long twoRealmtot;
  double steps[MAXDIM], *c;
  CE_storage *s;
  ce_param* cepar;
  bool // Critical[MAXDIM],
      critical;

  c = NULL;
  meth = &(key->meth[m]);
  cepar = &CIRCEMBED;
  if (!key->grid) {Xerror=ERRORMETHODNOTALLOWED;goto ErrorHandling;}
  SET_DESTRUCT(CE_destruct, m);

  if ((meth->S = malloc(sizeof(CE_storage)))==0){
    Xerror=ERRORMEMORYALLOCATION; goto ErrorHandling;
  } 
  s =  (CE_storage*) meth->S;
  s->c =NULL;
  s->d =NULL;
  FFT_NULL(&(s->FFT));
  

  if ((Xerror = FirstCheck_Cov(key, m, true)) != NOERROR)  goto ErrorHandling;
  actcov = meth->actcov;
  critical = false; 
//  for (k=0; k<dim; k++) Critical[k]=false; // critical odd not used yet
//    if (cov->type==ANISOTROPIC) { // if Hypermodels are
//	// included it becomes much more difficult!
//     if (!cov->even) {
//        for(k=0; k<dim; k++) {
//	  Critical[k] |= cov->odd[k];
//        }
//      }
//     }


  for (d=0; d<key->timespacedim; d++) {
    s->nn[d]=key->length[d]; 
    steps[d]=key->x[d][XSTEP];
  }

  int *mm, *cumm, *halfm, dim;
  double hx[MAXDIM], totalm;
  int  trials, index[MAXDIM], dummy;
  long mtot,i,k,twoi;
  bool positivedefinite, cur_crit;
  
  mtot=-1;
  mm = s->m;
  halfm= s->halfm;
  cumm = s->cumm;
  dim = key->timespacedim;
  c=NULL;
  if (GENERAL_PRINTLEVEL>=5) PRINTF("calculating the Fourier transform\n");

  /* cumm[i+1]=\prod_{j=0}^i m[j] 
     cumm is used for fast transforming the matrix indices into an  
       index of the vector (the way the matrix is stored) corresponding 
       to the matrix                   */                               
  /* calculate the dimensions of the matrix C, eq. (2.2) in W&C */       
                                                                         
 // ("CE missing strategy for matrix entension in case of anisotropic fields %d\n
  totalm = 1.0;                      
  for (i=0;i<dim;i++){ // size of matrix at the beginning  
    mm[i] = s->nn[i];
    if (cepar->mmin[i]>0.0) {
	if (mm[i] > ((int) ceil(cepar->mmin[i])) / 2) {
	    sprintf(ERRORSTRING_OK, "Minimum size in direction %d is %d", 
		    (int) i, mm[i]);
	    sprintf(ERRORSTRING_WRONG,"%d", (int) ceil(cepar->mmin[i]));
	    return ERRORMSG;
	}
	mm[i] = ((int) ceil(cepar->mmin[i])) / 2;
    } else if (cepar->mmin[i] < 0.0) {
	assert(cepar->mmin[i] <= -1.0);
	mm[i] = (int) ceil((double) mm[i] * -cepar->mmin[i]);
    }
    if (cepar->useprimes) {
      // Note! algorithm fails if mm[i] is not a multiple of 2
      //       2 may not be put into NiceFFTNumber since it does not
      //       guarantee that the result is even even if the input is even !
      mm[i] = 2 * NiceFFTNumber((unsigned long) mm[i]);
    } else {
      mm[i] = (1 << 1 + (int) ceil(log((double) mm[i]) * INVLOG2 - EPSILON1000));
    }
    totalm *= (double) mm[i];                          
  }                             
  if (totalm > cepar->maxmem) {
    sprintf(ERRORSTRING_OK, "%f", cepar->maxmem);
    sprintf(ERRORSTRING_WRONG,"%f", totalm);
    return ERRORMAXMEMORY;
  }

  positivedefinite = false;     
    /* Eq. (3.12) shows that only j\in I(m) [cf. (3.2)] is needed,
       so only the first two rows of (3.9) (without the taking the
       modulus of h in the first row)
       The following variable `index' corresponds to h(l) in the following
       way: index[l]=h[l]        if 0<=h[l]<=mm[l]/2
       index[l]=h[l]-mm[l]   if mm[l]/2+1<=h[l]<=mm[l]-1     
       Then h[l]=(index[l]+mm[l]) mod mm[l] !!
    */

 /* The algorithm below:
  while (!positivedefinite && (trials<cepar->trials)){
    trials++;
    calculate the covariance values "c" according to the given "m"
    fastfourier(c)
    if (!cepar->force || (trials<cepar->trials)) {
      check if positive definite
      if (!positivedefinite && (trials<cepar->trials)) {
        enlarge "m"
      }
    } else 
      print "forced"
  }
*/
  
  trials=0;
  while (!positivedefinite && (trials<cepar->trials)){ 
    trials++;
    cumm[0]=1; 
    for(i=0;i<dim;i++){
      halfm[i]=mm[i]/2; 
      index[i]=1-halfm[i]; 
      cumm[i+1]=cumm[i] * mm[i]; // only relevant up to i<dim-1 !!
    }

    mtot=cumm[dim-1] * mm[dim-1]; 
    if (GENERAL_PRINTLEVEL>=2) {
      for (i=0;i<dim;i++) PRINTF("mm[%d]=%d, ",i,mm[i]);
      PRINTF("mtot=%d\n ",mtot);
    }
    twoRealmtot = 2 * sizeof(double) * mtot;

    // for the following, see the paper by Wood and Chan!
    // meaning of following variable c, see eq. (3.8)
    if ((c = (double*) malloc(twoRealmtot)) == 0) {
      Xerror=ERRORMEMORYALLOCATION; goto ErrorHandling;
    }

    for(i=0; i<dim; i++){index[i]=0;}
  
    for (i=0; i<mtot; i++){
      cur_crit = false;
      for (k=0; k<dim; k++) {
	hx[k] = steps[k] * 
	  (double) ((index[k] <= halfm[k]) ? index[k] : index[k] - mm[k]);
	cur_crit |= (index[k]==halfm[k]);
      }	
      dummy=i << 1;

      c[dummy] = (critical && cur_crit) ?  0.0 :      
	 key->covFct(hx, dim, key->cov, meth->covlist, actcov, key->anisotropy); 
      // statt critical hier besser andere CovFct addieren!
          
      c[dummy+1] = 0.0;
      k=0; while( (k<dim) && (++(index[k]) >= mm[k])) {
	index[k]=0;
	k++;
      }
      assert( (k<dim) || (i==mtot-1));
    }

    if (GENERAL_PRINTLEVEL>6) PRINTF("FFT...");
    if ((Xerror=fastfourier(c, mm, dim, true, &(s->FFT)))!=0) 
	goto ErrorHandling;  
    if (GENERAL_PRINTLEVEL>6) PRINTF("finished\n");
   
    // check if positive definite. If not: enlarge and restart 
    if (!cepar->force || (trials<cepar->trials)) { 
      long int mtot2;
      mtot2 = mtot * 2; 
      twoi=0;
      // 16.9. < cepar.tol.im  changed to <=
      while ((twoi<mtot2) && (positivedefinite=(c[twoi]>=cepar->tol_re) && 
					  (fabs(c[twoi+1])<=cepar->tol_im)))
	{twoi+=2;}      

      if ( !positivedefinite) {
        if (GENERAL_PRINTLEVEL>=2)
	  // 1.1.71: %f changed to %e because c[twoi+1] is usually very small
	  PRINTF("non-positive eigenvalue: c[%d])=%e + %e i.\n",
		 (int) (twoi/2), c[twoi], c[twoi+1]);
	if (GENERAL_PRINTLEVEL>=4) { // just for printing the smallest 
	  //                            eigenvalue (min(c))
	  long int index=twoi, sum=0;
	  double smallest=c[twoi];
	  char percent[]="%";
	  for (twoi=0; twoi<mtot2; twoi += 2) {
	    if (c[twoi] < 0) {
	      sum++;
	      if (c[twoi]<smallest) { index=twoi; smallest=c[index]; }
	    } 
	  }
	  PRINTF("   The smallest eigenvalue is c[%d] =  %e.\n",
		 index / 2, smallest);
	  PRINTF("   The percentage of negative eigenvalues is %5.3f%s.\n", 
		 100.0 * (double) sum / (double) mtot, percent);
	}
      }

      if (!positivedefinite && (trials<cepar->trials)) { 
	FFT_destruct(&(s->FFT));
	free(c); c=NULL;

	totalm = 1.0;
	switch (cepar->strategy) {
	case 0 :
	  for (i=0;i<dim;i++) { /* enlarge uniformly in each direction, maybe 
				   this should be modified if the grid has 
				   different sizes in different directions */
	    mm[i] <<= 1;
	    totalm *= (double) mm[i];
	  }
	  break;
	case 1 :  
	  double cc, maxcc, hx[MAXDIM];
	  int maxi;
	  maxi = -1;
	  maxcc = RF_NEGINF;
	  for (i=0; i<dim; i++) hx[i] = 0.0;
	  for (i=0; i<dim; i++) {
	    hx[i] = steps[i] * mm[i]; 
	    cc = fabs(key->covFct(hx, dim, key->cov, meth->covlist, actcov,
			     key->anisotropy)); 
	    if (GENERAL_PRINTLEVEL>2) PRINTF("%d cc=%e (%e)",i,cc,hx[i]);
	    if (cc>maxcc) {
	      maxcc = cc;
	      maxi = i; 
	    }
	    hx[i] = 0.0;
	  }
	  assert(maxi>=0);
	  mm[maxi] <<= 1;
	  for (i=0;i<dim; i++) totalm *= (double) mm[i];
	  break;
	default:
	  assert(false);
	}
	if (totalm>cepar->maxmem) {    
	  sprintf(ERRORSTRING_OK, "%f", cepar->maxmem);
	  sprintf(ERRORSTRING_WRONG,"%f", totalm);
	  Xerror=ERRORMAXMEMORY;
	  goto ErrorHandling;
	}
	//	assert(false);
      }
    } else {if (GENERAL_PRINTLEVEL>=2) PRINTF("forced\n");}
  }
  assert(mtot>0);
 

  if (positivedefinite || cepar->force) { 
    // correct theoretically impossible values, that are still within 
    // tolerance CIRCEMBED.tol_re/CIRCEMBED.tol_im 
    double r, imag;
    r = imag = 0.0;    
    for(i=0,twoi=0;i<mtot;i++) {
      if (c[twoi] > 0.0) {
	c[twoi] = sqrt(c[twoi]);
      } else {
	if (c[twoi] < r) r = c[twoi];
	c[twoi] = 0.0;
      }
      {
	register double a;
	if ((a=fabs(c[twoi+1])) > imag) imag = a;
      }
      c[twoi+1] = 0.0;
      twoi+=2;
    }
    if (GENERAL_PRINTLEVEL>1) {
      if (r < -GENERAL_PRECISION || imag > GENERAL_PRECISION) {
	PRINTF("using approximating circulant embedding:\n");
	if (r < -GENERAL_PRECISION) 
	  PRINTF("\tsmallest real part has been %e \n", r);
	if (imag > GENERAL_PRECISION) 
	  PRINTF("\tlargest modulus of the imaginary part has been %e \n",imag);
      }
    }
  } else {Xerror=ERRORFAILED;goto ErrorHandling;}
  if (GENERAL_PRINTLEVEL>=10) {
    for (i=0;i<2*mtot;i++) {PRINTF("%f ",c[i]);} PRINTF("\n");
  }  
//  s->c = c;
//  return NOERROR;
  
  if (key->storing) {
    if ((s->d=(double *) malloc(twoRealmtot))==0){
      Xerror=ERRORMEMORYALLOCATION;goto ErrorHandling;} //d
  }
  s->c = c;

  return NOERROR;
  
 ErrorHandling:
    if (c!=NULL) {free(c);}
  return Xerror;
}


void covcpy(covinfo_type *dest, covinfo_type *source){
  assert(source->x==NULL);
  memcpy(dest, source, sizeof(covinfo_type));
}

double GetScaledDiameter(key_type *key, covinfo_type *kc)
{
// SCALE and ANISO is considered as space trafo and envolved here
  double diameter; 
  diameter = 0.0;
  if (key->anisotropy) { // code can be reduced to the anisotropic case
      //      with the obvious loss of time in the isotropic case...
    int i, j, k, dim, ncorner_dim;
    double distsq;
    double sx[ZWEIHOCHMAXDIM * MAXDIM];
    dim = kc->truetimespacedim;
    ncorner_dim = (1 << key->timespacedim) * dim;
    GetCornersOfGrid(key, dim, kc->aniso, sx);
    for (i=1; i<ncorner_dim; i+=dim) {
      for (j=0; j<i; j+=dim) {
        distsq = 0.0;
	for (k=0; k<dim; k++) { 
	  register double dummy;
	  dummy = sx[i + k] - sx[j + k];
	  distsq += dummy * dummy;
	}
	if (distsq > diameter) diameter = distsq;
      }
    }
  } else { // see above
    int d;
    for (d=0; d<key->timespacedim; d++) {
      register double dummy;
      dummy = key->x[d][XSTEP] * (double) (key->length[d] - 1);
      diameter += dummy * dummy; 
    }
    diameter *= kc->aniso[0] * kc->aniso[0];
  }
//  printf("diameter=%f", sqrt(diameter));
  return sqrt(diameter);
}

int init_circ_embed_local(key_type *key, int m)
{
  methodvalue_type *meth;  
  int Xerror=NOERROR, v, store_msg[MAXDIM], msg, simuactcov, instance,
      actcov, nc, store_nr;
  localCE_storage *s;
  cov_fct *hyper;
  covinfo_type *sc;
  bool selectlocal[Forbidden + 1];
  param_type store_param;
  double rawRmax;

  if (key->covFct != CovFct) { Xerror=ERRORNOTPROGRAMMED; goto ErrorHandling;}
  SET_DESTRUCT(localCE_destruct, m);
  if (!key->grid) { Xerror=ERRORMETHODNOTALLOWED; goto ErrorHandling;}
  meth = &(key->meth[m]);
  assert(meth->S==NULL);
   if ((meth->S=malloc(sizeof(localCE_storage)))==0){
    Xerror=ERRORMEMORYALLOCATION; goto ErrorHandling;
  } 
  s = (localCE_storage*) meth->S;
  KEY_NULL(&(s->key));

  // prepared for more sophisticated selections if init_circ_embed_local
  // and init_circ_embed are merged
  for (v=0; v < (int) Forbidden; v++) selectlocal[v]=false;
  selectlocal[meth->unimeth] = true;

  if ((Xerror = FirstCheck_Cov(key, m, false)) != NOERROR) goto ErrorHandling;
  actcov = meth->actcov;
  if (2 * actcov > MAXCOV) {Xerror=ERRORNCOVOUTOFRANGE; goto ErrorHandling;}

  rawRmax = 0.0;
  for (simuactcov=v=0; v<actcov; v++) {
    covinfo_type *kc;
    store_nr = -1;
    kc = &(key->cov[meth->covlist[v]]);
//    printf("** %d\n", kc->nr);
    sc = &(s->key.cov[simuactcov]);
    sc->method = CircEmbed;
    sc->param[VARIANCE] = 1.0; 
    sc->param[HYPERNR] = 1; // as only addition between models allowed
    sc->param[DIAMETER] = GetScaledDiameter(key, kc); // SCALE is considered 
    //                                      as space trafo and envolved here
    if (GENERAL_PRINTLEVEL>7) PRINTF("diameter %f\n", sc->param[DIAMETER]);
    sc->op = 2;
    covcpy(sc + 1, kc);

    instance = -1;
    store_param[LOCAL_R] =  R_PosInf;
    store_msg[simuactcov] = MSGLOCAL_JUSTTRY; // lowest preference
    for (nc=0; nc < nlocal; nc++) {// in case more cutoff methods are found
      instance = 0;
      sc->nr = LocalCovList[nc];
      hyper = &(CovList[sc->nr]);
      if (GENERAL_PRINTLEVEL>7) PRINTF("%d %s:\n", nc, hyper->name);
      if (!selectlocal[hyper->localtype]) continue;
      // for future use if more hyper functions for cutoff and intrinsic
     // are available
      // if (PREFERENCE[hyper->localtype] >= 0 && 
      //	PREFERENCE[hyper->localtype]!=sc->nr) continue; 
      while ((msg = hyper->getparam(sc + 1, sc->param, instance++))
	     != MSGLOCAL_ENDOFLIST) {
//	printf("%d %s %d \n", instance, hyper->name, msg);

	if ((Xerror=
	     hyper->checkNinit(sc, COVLISTALL, key->ncov - meth->covlist[v], 
			       CircEmbed)) == NOERROR
	    && (
		(int) msg < (int) store_msg[simuactcov] 
		||
		((int) msg == (int) store_msg[simuactcov] &&
		 sc->param[LOCAL_R] < store_param[LOCAL_R])
		)
	    )
	{	   
	    memcpy(store_param, sc->param, sizeof(param_type));
	    store_msg[simuactcov] = msg;
	    store_nr = sc->nr;
	} // if

//	printf("%d %d %f \n",  Xerror, store_msg[simuactcov], store_param[LOCAL_R] );

        if (GENERAL_PRINTLEVEL>7) {
	    PRINTF("v=%d nc=%d inst=%d err=%d hyp.kappa=%f, #=%d, diam=%f\n r=%f curmin_r=%f\n", 
		   v, nc, instance, Xerror, sc->param[HYPERKAPPAI],
		   (int) sc->param[HYPERNR],
		   sc->param[DIAMETER],  sc->param[LOCAL_R],
		   store_param[LOCAL_R]);
	    if (hyper->implemented[CircEmbedIntrinsic] == HYPERIMPLEMENTED)
		PRINTF("userr=%f, a0=%f, a2=%f, b=%f\n",
		       sc->param[INTRINSIC_RAWR],
		       sc->param[INTRINSIC_A0], sc->param[INTRINSIC_A2],
		       sc->param[INTRINSIC_B]
		    );
	    else {
//	      printf("%s %d\n", 
//		     hyper->name, hyper->implemented[CircEmbedCutoff]);
	      assert(hyper->implemented[CircEmbedCutoff] == HYPERIMPLEMENTED);
	      PRINTF("a=%f, sqrt.a=%f, b=%f\n",
		     sc->param[CUTOFF_A],
		     sc->param[CUTOFF_ASQRTR], sc->param[CUTOFF_B]
		);
	    }
	}
      } // while
      if (rawRmax < sc->param[LOCAL_R] / sc->param[DIAMETER]) 
	  rawRmax = sc->param[LOCAL_R] /sc->param[DIAMETER];
    } // nc
    if (!R_FINITE(store_param[LOCAL_R])) {
      PRINTF("v=%d nc=%d, inst=%d err=%d hyp.kappa=%f, #=%d, diam=%f\nr=%f curmin_r=%f\n", 
	     v, nc, instance, Xerror, sc->param[HYPERKAPPAI],
	     (int) sc->param[HYPERNR],
	     sc->param[DIAMETER],  sc->param[LOCAL_R],
	     store_param[LOCAL_R]);
      assert(Xerror!=NOERROR);
      goto ErrorHandling;
    }
    sc->nr = store_nr;
    assert(sc->nr >= 0);
    memcpy(sc->param, store_param, sizeof(param_type));
    simuactcov += (short int) sc->param[HYPERNR] + 1; 
    sc += (int) sc->param[HYPERNR];  
  } // v

  // prepare for call of internal_InitSimulateRF
  int covnr[MAXCOV], op[MAXCOV], CEMethod[MAXCOV], cum_nParam, 
      aniso, n_aniso, i;
  double ParamList[MAXCOV * TOTAL_PARAM];
  char errorloc_save[nErrorLoc];
  if (key->anisotropy) {
      n_aniso = key->timespacedim * key->timespacedim ;
      aniso = ANISO;
  } else {
      n_aniso = 1;
      aniso = SCALE;
  }
  cum_nParam = 0;
  for (v=0; v<simuactcov; v++) {
    sc = &(s->key.cov[v]);
    covnr[v] = sc->nr;
    op[v] = sc->op;
    CEMethod[v] = (int) CircEmbed;
    ParamList[cum_nParam++] = sc->param[VARIANCE];
    for (i=0; i<CovList[sc->nr].kappas; i++)
	ParamList[cum_nParam++] = sc->param[KAPPA + i];
    for (i=0; i<n_aniso; i++)
	ParamList[cum_nParam++] = sc->param[aniso + i];
  }

  //for (i=0; i<cum_nParam; printf("%d %f \n", i, ParamList[i++]));
  // assert(false);

  strcpy(errorloc_save, ERROR_LOC);
  sprintf(ERROR_LOC, "%s%s ", errorloc_save, "cutoff: ");
  ce_param ce_save;
  memcpy(&ce_save, &CIRCEMBED, sizeof(ce_param));
  memcpy(&CIRCEMBED, &LOCAL_CE, sizeof(ce_param));
 
  rawRmax *= sqrt(key->timespacedim);
  for (i=0; i<key->timespacedim; i++) {
    if (CIRCEMBED.mmin[i]==0.0)  CIRCEMBED.mmin[i] = - rawRmax;
  }

  instance = 0;
  for (;;) {
    bool anychangings;
    Xerror = internal_InitSimulateRF(key->x[0], key->T, key->spatialdim,
				     3, key->grid, key->Time,
				     covnr, ParamList, cum_nParam,
				     simuactcov, key->anisotropy, op,
				     CEMethod, DISTR_GAUSS, &(s->key), 
				     GENERAL_STORING, 
				     0 /* natural scaling */, CovFct);
    anychangings = false;
    if (Xerror == NOERROR || Xerror == USEOLDINIT) {
      break;
    } else {
      for (v=0; v<simuactcov; v++, instance++) {
	sc = &(s->key.cov[v]);
	hyper = &(CovList[sc->nr]);
	if ((hyper->localtype == CircEmbedCutoff || 
	     hyper->localtype == CircEmbedIntrinsic)) {
	  if (store_msg[v] != MSGLOCAL_OK) {
	    bool improved;
	    memcpy(store_param, sc->param, sizeof(param_type));
	    improved = hyper->alternative(sc, instance) && 
		(hyper->checkNinit(sc, COVLISTALL, (int) sc->param[HYPERNR], 
				   CircEmbed) != NOERROR);
	    if (improved) memcpy(sc->param, store_param, sizeof(param_type));
	    anychangings |=  improved;
	  }
	} else {
	  assert((v % 2) == 1); // if the method is generalised to
	  // multiplicative submodels this assert reminds that
	  // this part must be changed.
	}
      }
      if (!anychangings) break;
    }
  }

  memcpy(&CIRCEMBED, &ce_save, sizeof(ce_param));
  strcpy(ERROR_LOC, errorloc_save);
  if (Xerror != NOERROR) goto ErrorHandling;

  s->factor = 0.0;
  for (v=0; v<simuactcov; v++) {
    sc = &(s->key.cov[v]);
    hyper = &(CovList[sc->nr]);
    if (hyper->localtype == CircEmbedIntrinsic) {
      static double zero=0.0;
      // these steps are necessary, see the two cases in 3dBrownian!
      s->factor += sc->param[INTRINSIC_A2] * 
	  key->covFct(&zero, 1, sc, COVLISTALL, (int) sc->param[HYPERNR], 
		      false) / (sc->param[DIAMETER] * sc->param[DIAMETER]);
    }
  }
  // 2.0 : see Stein (2002)
  s->factor = sqrt(2.0 * s->factor);  // standard deviation of the Gaussian 
  //                                        variables in do_...

  return NOERROR;
 
 ErrorHandling:
  return Xerror;
}


void do_circ_embed_local(key_type *key, int m, double *res )
{  
  double x[MAXDIM], dx[MAXDIM];
  long index[MAXDIM], k, r;
  localCE_storage *s;

  assert(key->active);
  s = (localCE_storage*) key->meth[m].S;
  assert(s->factor >= 0.0);
  internal_DoSimulateRF(&(s->key), 1, res);

  if (s->factor>0) {
    for (k=0; k<key->timespacedim; k++) {
      index[k]=0;
      dx[k]= GAUSS_RANDOM(s->factor * key->x[k][XSTEP]);
      x[k]= 0.0;
    }
    for(r=0;;) { 
      for (k=0; k<key->timespacedim; k++)  res[r] += x[k]; 
      r++;
      k=0;
      while( (k<key->timespacedim) && (++index[k]>=key->length[k])) {
	index[k]=0;
	x[k] = 0.0;
	k++;
      }
      if (k>=key->timespacedim) break;
      x[k] += dx[k];
    }
  }
}

void do_circ_embed(key_type *key, int m, double *res ){
  double *d;
  CE_storage *s;
  s = (CE_storage*)key->meth[m].S;
  assert(key->active);
  if (s->d==NULL) { /* overwrite the intermediate 
		       result directly (algorithm  allows for that) */
    d=s->c;
    assert(!key->storing);
  }
  else {
    assert(key->storing);
    d=s->d;
  }

/* 
     implemented here only for rotationsinvariant covariance functions
     for arbitrary dimensions;
     (so it works only for even covariance functions in the sense of 
     Wood and Chan,p. 415, although they have suggested a more general 
     algorithm;) 
     
     Warning! If key->storing==false when calling init_circ_embed
     and key->storing==true when calling do_circ_embed, the
     complete programme will fail, since the initialization depends on
     the value of key->storing
  */  
  int  i, j, k, HalfMp1[MAXDIM], HalfMaM[2][MAXDIM], index[MAXDIM], dim,
      *nn, *mm, *cumm, *halfm;
  double XX,YY,invsqrtmtot, *c;
  bool first, free[MAXDIM+1], noexception;
  long mtot;
  
  dim = key->timespacedim;
  nn = s->nn;
  mm = s->m;
  cumm = s->cumm;
  halfm = s->halfm;
  c = s->c;

  mtot=cumm[dim-1] * mm[dim-1]; 
  for (i=0; i<dim; i++) {
    HalfMp1[i] =  ((mm[i] % 2)==1) ? -1 : halfm[i];
    HalfMaM[0][i] = halfm[i];
    HalfMaM[1][i] = mm[i] - 1;
  }

  //if there are doubts that the algorithm below reaches all elements 
  //of the matrix, uncomment the lines containing the variable xx
  //
  //bool *xx; xx = (bool*) malloc(sizeof(bool) * mtot);
  //for (i=0; i<mtot;i++) xx[i]=true;
  
  invsqrtmtot = 1/sqrt((double) mtot);

  if (GENERAL_PRINTLEVEL>=10) PRINTF("Creating Gaussian variables... \n");
  /* now the Gaussian r.v. have to defined and multiplied with sqrt(FFT(c))*/

  for (i=0; i<dim; i++) {
    index[i]=0; 
    free[i] = false;
  }
  free[MAXDIM] = false;

//  if MaxStableRF calls Cutofff, c and look uninitialised

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

//    if (index[1] ==0) printf("A %d %d %d %d %d %f %f\n", 
//	   index[0], index[1], i, j, noexception, d[i], d[i+1]);
    if (GENERAL_PRINTLEVEL>=10) PRINTF("cumm...");
    i <<= 1; // since we have to index imaginary numbers
    j <<= 1;
    if (noexception) { // case 3 in prop 3 of W&C
      XX = GAUSS_RANDOM(INVSQRTTWO);
      YY = GAUSS_RANDOM(INVSQRTTWO);
      d[i] = d[i+1] = c[i];   d[i] *= XX;  d[i+1] *= YY;   
      d[j] = d[j+1] = c[j];   d[j] *= XX;  d[j+1] *= -YY; 
    } else { // case 2 in prop 3 of W&C
      d[i]   = c[i] * GAUSS_RANDOM(1.0);
      d[i+1] = 0.0;
    }

//    if (i==224|| i==226 || i==228) {
//printf("B %d %d %d %d %d %f %f\n", 
//	   index[0], index[1], i, j, noexception, d[i], d[i+1]);
// assert(false); 
//    }

    if (GENERAL_PRINTLEVEL>=10) PRINTF("k=%d ", k);
    
//    if (index[1] == 0) printf("B %d %d %d %d %d %f %f\n", 
//	   index[0], index[1], i, j, noexception, d[i], d[i+1]);
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
       
	 free==true   <=>   endfor==mm[]	 
    */
    k=0; 
    if (++index[k]>HalfMaM[free[k]][k]) {
      // in case k increases the number of indices that run over 0..m increases
      free[k] = true;
      index[k]= 0; 
      k++;
      while((k<dim) && (++index[k]>HalfMaM[free[k]][k])) {
	free[k] = true;
	index[k]= 0; 
	k++;
      }
      if (k>=dim) break;
      // except the very last (new) number is halfm and the next index is
      // restricted to 0..halfm
      // then k decreases as long as the index[k] is 0 or halfm
      if (!free[k] && (index[k]==halfm[k])){//index restricted to 0..halfm?
	// first: index[k] is halfm? (test on ==0 is superfluent)
	k--;
 	while ( (k>=0) && ((index[k]==0) || (index[k]==halfm[k]))) {
	  // second and following: index[k] is 0 or halfm?
	  free[k] = false;
	  k--;
	}
      }
    }
  }


//  printf("%f\n", d[0]);
//  printf("%f\n", d[1]);
//  printf("%f\n", d[2]);
//  printf("%f\n", d[3]);

//  double zz=0.0;
//  for (i =0; i<2 * mtot; i++) {
  //    if (true || i<=100) printf("%d %d\n", i, 2 * mtot);
  //    zz += d[i];
//     assert(i!=230);
//     // if (i>100); break;
//  }
// printf("%f\n", zz);

  fastfourier(d, mm, dim, false, &(s->FFT));   
  

  /* now we correct the result of the fastfourier transformation
     by the factor 1/sqrt(mtot) and read the relevant matrix out of 
     the large vector c */
  first = true;
  for(i=0; i<dim; i++){index[i]=0;}
  int totpts = key->totalpoints;
  for (i=0; i<totpts; i++){
    j=0; for (k=0; k<dim; k++) {j+=cumm[k] * index[k];} 
    res[i] += d[2*j]*invsqrtmtot; 
    if ((fabs(d[2*j+1])>CIRCEMBED.tol_im) && 
        ((GENERAL_PRINTLEVEL>=2 && first) || GENERAL_PRINTLEVEL>=6)){ 
      PRINTF("IMAGINARY PART <> 0, %e\n",d[2*j+1]);
      first=false; 
    } 
    k=0; while((k<dim) && (++index[k]>=nn[k])) {index[k++]=0;} 
  }  
}
