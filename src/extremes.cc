 /* 
 Authors
 Martin Schlather, schlather@math.uni-mannheim.de

 simulation of max-stable random fields

 Copyright (C) 2001 -- 2014 Martin Schlather, 

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include <math.h>
#include <stdio.h>
#include "RF.h"
#include "Covariance.h"

 
#define POISSON_INTENSITY 0
#define RANDOMCOIN_INTENSITY (COMMON_GAUSS + 1) 

/* not used 
double GetTotal(cov_model* cov) {
  cov_fct *C = CovList + cov->nr;
  double v = 1.0;
  int i,
    nsub = cov->nsub,
    kappas = C->kappas;							

  if (C->Type == RandomType) {
    if (cov->total_n >= 0) {
      int VARIABLE_IS_NOT_USED old_n = cov->total_n;
      if (cov->total_n < GLOBAL.distr.repetitions) {
	double *r = (double*) MALLOC(cov->xdimown * sizeof(double));
	while(cov->total_n < GLOBAL.distr.repetitions) {
	  DORANDOM(cov, r);
	  assert(cov->total_n > cov->total_n);
	}
	free(r);
      }
      v *= cov->total_sum / (double) cov->total_n;
    }
  } else {
    for (i=0; i<nsub; i++) {
      v *= GetTotal(cov->sub[i]);
    }    
    for (i=0; i<kappas; i++) {						
      if (cov->kappasub[i] != NULL) 
	v *= GetTotal(cov->kappasub[i]);  
    }
  }
  return v;
}
*/


int addStandard(cov_model **Cov) {
  cov_model *cov = (*Cov)->calling,
    *next = *Cov;
  int i, err,
    dim = next->xdimprev,
    vdim = next->vdim2[0],
    role = next->role;     
  assert(cov->vdim2[0] == cov->vdim2[1]);

  addModel(Cov, STANDARD_SHAPE);
  cov_model *key = *Cov;
  SetLoc2NewLoc(key, Loc(cov));
  assert(key->calling == cov);
  assert(key->sub[PGS_LOC] == NULL && key->sub[PGS_FCT] != NULL);

  for (i=0; i<2; i++) {
    if ((err = CHECK(key, dim, dim, PointShapeType, XONLY, CARTESIAN_COORD,
		     vdim, role)) != NOERROR) return err;   
    if (i==0) {
      if (hasPoissonRole(cov)) {
	addModel(key->sub + PGS_LOC, UNIF);
	key->sub[PGS_LOC]->calling = cov;
      } else {
	if ((err = STRUCT(key, key->sub + PGS_LOC)) != NOERROR) return err;
      }

    }

  }
  return NOERROR;
}


int addPGS(cov_model **Cov) {
  // for m3 & random coin
  cov_model *shape = *Cov;
  assert(shape->calling != NULL);
  //domain_type dom = shape->domprev;
  //  isotropy_type iso = shape->isoprev;
  int err,
    dim = shape->xdimprev,
    vdim = shape->vdim2[0],
    role = shape->role;
  assert(shape->vdim2[0] == shape->vdim2[1]);


  //PMI(shape);
  
  assert(dim == shape->tsdim);
  assert(vdim == 1);
    

  // most models split into a shape function and location distribution
  // given the shape function, see oesting, schlather, chen
  //
  // for anisotropic models also the reverse modelling could be of interest:
  // first the location_type aere modelled. Then conditional on the locations
  // the shape functions are modelled. Um diese Option zu ermoeglichen muesste
  // noch ein Schalter programmiert werden, oder es ist implizit dadurch
  // gegeben, dass das Modell eben als ganzen gegeben ist.
  //
  // Or the model is given as a whole. This is the first if-condition:
  //      if (isPointShape(shape)) return NOERROR;
  // else: model is given by the shape and then conditional on the 
  // location

  
  addModel(Cov, PTS_GIVEN_SHAPE);
  cov_model *cov = *Cov;
  assert(cov->sub[PGS_LOC] == NULL && cov->sub[PGS_FCT] != NULL);
  if ((err = CHECK(cov, dim, dim, PointShapeType, XONLY, CARTESIAN_COORD,
		     vdim, role)) != NOERROR) return err; 
  if ((err = STRUCT(cov, cov->sub + PGS_FCT)) != NOERROR) return err;
  if ((err = CHECK(cov, dim, dim, PointShapeType, XONLY, CARTESIAN_COORD, 
		     vdim, role)) != NOERROR) return err;

  //   APMI(*Cov);

  return NOERROR;
}


  void range_mpp(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range) {
    //   assert(cov != NULL);
  range->min[GEV_XI] = RF_NEGINF;
  range->max[GEV_XI] = RF_INF;
  range->pmin[GEV_XI] = -10;
  range->pmax[GEV_XI] = 10;
  range->openmin[GEV_XI] = true;
  range->openmax[GEV_XI] = true; 

  range->min[GEV_MU] = RF_NEGINF;
  range->max[GEV_MU] = RF_INF;
  range->pmin[GEV_MU] = -1000;
  range->pmax[GEV_MU] = 1000;
  range->openmin[GEV_MU] = true;
  range->openmax[GEV_MU] = true; 

  range->min[GEV_S] = 0;
  range->max[GEV_S] = RF_INF;
  range->pmin[GEV_S] = 0.001;
  range->pmax[GEV_S] = 1000;
  range->openmin[GEV_S] = true;
  range->openmax[GEV_S] = true; 
}


int init_mpp(cov_model *cov, storage *S) {
  cov_model *sub = cov->key != NULL ? cov->key :
    cov->sub[0] != NULL ? cov->sub[0] : cov->sub[1];
  if (sub == NULL) SERR("substructure could be detected");
  location_type *loc = Loc(cov);
  //window_info *w = &(S->window);
  int err,
    //    dim = cov->tsdim,
    role = cov->role, 
    maxstable = hasMaxStableRole(cov);
  pgs_storage *pgs;


  if (!maxstable && !hasPoissonRole(cov)) ILLEGAL_ROLE;
  if (!isPointShape(sub)) SERR1("%s is not shape/point function", NICK(sub));
  if (loc->distances) return ERRORFAILED;
  
  //   printf("%d \n", 1 + (role == ROLE_POISSON_GAUSS));APMI(sub->calling);
  //  print("nr =%d gatter=%d \n", sub->nr, sub->gatternr);
  //  assert(sub->gatternr >= 0);
  //  print("Init=%ld %ld\n", CovList[sub->gatternr].Init, init2);
  //  print("S=%ld\n", S);
  //
  // print("end list\n");
 
  if ((err = INIT(sub, maxstable ? 1 : role == ROLE_POISSON ? 0 : 2, S)) 
      != NOERROR)  return err;
  pgs = sub->Spgs;

  //assert(false);

  //PMI(cov);


  assert(pgs != NULL);
   
  GetDiameter(loc, pgs->supportmin, pgs->supportmax, pgs->supportcentre); 
  
  if (maxstable) {      
    if (!R_FINITE(sub->mpp.mMplus[1]) || sub->mpp.mMplus[1] <= 0.0) {
      // APMI(sub);
      // printf("mplus %s %f\n",  NICK(sub), sub->mpp.mMplus[1]);
      //      crash();
      SERR("integral of positive part of submodel unkown");
    }

    //PMI(sub, "sub");

    if (!R_FINITE(pgs->zhou_c) && !R_FINITE(pgs->sum_zhou_c))
      SERR("maximal height of submodel unkown or the set of locations exceeds possibilities of the programme");
  } else if (role == ROLE_POISSON_GAUSS) {
    if (ISNAN(sub->mpp.mM[2]) || !R_FINITE(sub->mpp.mM[2] || sub->mpp.mM[2] <=0.0)){
      SERR("second moment unkown");
    }
  } // else role == ROLE_POISSON -- nothing to do
 
  if ((err = FieldReturn(cov)) != NOERROR) { // must be later than INIT !
    return err;  
  }
  
  cov->simu.active = err == NOERROR;

  return err;
}

#define SET_SUB	{							\
  sub = (cov->key != NULL) ? cov->key : /* if changed, change do_max_pgs! */ \
    cov->sub[0] != NULL ? cov->sub[0] : cov->sub[1];			\
  if (sub == NULL) ERR("substructure could be detected");		\
  pgs = sub->Spgs;							\
  assert(pgs != NULL);							\
  gridlen = pgs->gridlen;						\
  end = pgs->end;							\
  start = pgs->start;							\
  delta = pgs->delta;							\
  nx = pgs->nx;							\
  xstart = pgs->xstart; /* nach DO gesetzt */				\
  x = pgs->x;           /* nach DO gesetzt */				\
  inc = pgs->inc;}

 

void dompp(cov_model *cov, storage *s, double *simuxi) {

  // printf("here\n");

  assert(cov->simu.active);
  location_type *loc = Loc(cov);	
  if (loc->caniso != NULL) BUG;

  cov_model *sub = NULL;
  pgs_storage *pgs = NULL;
  int i, k, d, err,
    *gridlen = NULL,
    *end = NULL,
    *start = NULL,
    *delta = NULL,
    *nx = NULL,
    dim = cov->tsdim,
    spatial = loc->totalpoints,
    every = GLOBAL.general.every, //
    nthreshold = (every>0) ? every : MAXINT,	
    //    covrole = cov->role,
    subrole = ROLE_FAILED,
    deltathresh = nthreshold;			

  double logdens,  factor, summand, sign[2], value[2], logthreshold, 
    *xstart = NULL, // nach DO gesetzt
    *x = NULL,           // nach DO gesetzt
    *inc = NULL,
    *rf = NULL, 
    //M1 = RF_NA,
    logM2 = RF_NA,
    gumbel = RF_NA,
    threshold = RF_NA,
    poisson=0.0;
  res_type *res = cov->rf;
  assert(res != NULL);
  ext_bool fieldreturn;
  bool
    compoundpoisson, poissongauss,
    maxstable = hasMaxStableRole(cov),
    simugrid = loc->grid,
    partialfield = false,
    // AVERAGE braucht Sonderbehandlung:
     randomcoin = cov->method == RandomCoin //only for ROLE_POISSON_GAUSS
    ;
  long zaehler, n, cumgridlen[MAXMPPDIM +1], Total_n,			
    total_pts = loc->totalpoints, 
    vdim = cov->vdim2[0],
    vdimtot = total_pts * vdim,
    control = 0;
 assert(cov->vdim2[0] == cov->vdim2[1]);

  cov_model *key = cov->key;
  ext_bool loggiven;
  double Minimum = RF_NEGINF; // -10 // TO DO

  if (vdim != 1)
    error("Poisson point process based methods only work in the univariate case");

  SET_SUB;
  if (!isPointShape(sub)) BUG;					       
  subrole = sub->role;
  compoundpoisson = subrole == ROLE_POISSON;
  poissongauss = subrole == ROLE_POISSON_GAUSS;
  Total_n = maxstable ? -1 : rpois(pgs->intensity);
  assert(maxstable || Total_n > 0);
  loggiven = key == NULL ? cov->sub[0]->loggiven : key->loggiven;
  if (!loggiven) Minimum = exp(Minimum);


  //  assert(covrole != ROLE_POISSON_GAUSS || subrole == ROLE_POISSON_GAUSS);
  //  assert(loc->caniso == NULL);
  // assert(covrole != ROLE_POISSON_GAUSS || sub->mpp.moments >= 2);
  // assert(covrole == ROLE_POISSON_GAUSS || covrole == ROLE_POISSON || 
  //	 sub->mpp.moments >= 1);
  
  if (simugrid) {
    cumgridlen[0] = 1;
    for (d=0; d<dim; d++) {
      inc[d] = loc->xgr[d][XSTEP];
      gridlen[d] =  loc->length[d]; 
      cumgridlen[d+1] = loc->length[d] * cumgridlen[d];

      // printf("set d=%d inc=%f gridlen=%d cum=%d\n", d, inc[d], gridlen[d],
      //     (int) cumgridlen[d]);
    }
  }

  if (maxstable) {
    if (loggiven) {
      for (i=0; i<vdimtot; i++) res[i] = R_NegInf;
    }
    else for (i=0; i<vdimtot; i++) res[i] = 0.0;
    if (sub->mpp.moments < 1 || !R_FINITE(sub->mpp.mMplus[1]) || 
      sub->mpp.mMplus[1] == 0.0)
      ERR("integral of positive part of the submodel is unknown or not finite");
    //    M1 = sub->mpp.mMplus[1];
    threshold = logthreshold = RF_INF;
    pgs->globalmin = Minimum;
    pgs->currentthreshold =
      loggiven ? pgs->globalmin - threshold : pgs->globalmin / threshold;
    if (simuxi != NULL) {
      // to do : RPstudent, extremal t process
    }
  } else if (compoundpoisson){
    assert(simuxi == NULL);
    for (i=0; i<vdimtot; i++) res[i] = 0.0;
  } else {
    assert(simuxi == NULL);
     for (i=0; i<vdimtot; i++) res[i] = 0.0;
    logM2 = log(sub->mpp.mM[2]);
    pgs->currentthreshold = 1e-8;
   }

  //  APMI(sub);
 
 
  for(n=0; ; n++) {    
    //       if (n % 1000 == 0) printf("n=%d tot=%d mh=%f, contr=%d: %f < %f; res[0]=%f\n", (int) n, (int) Total_n,  sub->mpp.maxheight, (int) control, res[control], threshold, res[0]);// assert(n <= 10);

    //    printf("n=%d\n", n);
    
    //    assert(n < 1200);

    //    assert(res[0] < 1e-4);
    //    assert(res[0] < 80);

    if (!maxstable && (n >= Total_n)) break; 
    
    // for (d=0; d<dim; d++) info->min[d] = info->max[d] = 0.0;

    //assert(n < 2   );

    //      printf("XX.\n");  APMI(sub);
    if (!sub->deterministic && 
	(err = REINIT(sub, sub->mpp.moments, s)) != NOERROR) BUG;
    DO(sub, s);   

   //printf("PL =%d\n", PL);

    // APMI(sub);

    if (!sub->deterministic) SET_SUB;

    //       printf("here!!!\n");

    // PMI(sub);

    fieldreturn = sub->fieldreturn;
    // MARCO: hattest Du dies reingenommen?
    //   if (subrole == ROLE_BROWNRESNICK) {
    //      assert(sub->SBR != NULL);
    //      n += sub->SBR->hatnumber;
    //    }
    
    //    PMI(sub, -1);
    //printf("log_den %f\n",  pgs->log_density);
    logdens = pgs->log_density;

    //    PMI(sub, -1);

    if (!R_FINITE(logdens)) {
      // PMI(sub);
      BUG;
    //    get_shape = CovList[sub->gatternr].cov;
    //   get_logshape = CovList[sub->gatternr].log;
    }
   
    if (compoundpoisson) {
      summand = 0.0;
    } else if (poissongauss) {
      summand = -0.5 * (logdens + logM2);
    } else { // max-stable field          
      assert(subrole == ROLE_SMITH || subrole == ROLE_SCHLATHER ||
	     subrole == ROLE_BROWNRESNICK);

      if (n >= GLOBAL.extreme.maxpoints) {
	PRINTF("'maxpoints' (= %d) exceeded. Simulation is likely to be a rough approximation only. Consider increasing 'maxpoints'.\n", 
	       GLOBAL.extreme.maxpoints
		);
	//	printf("break1 %d %d\n",  n, ((int*) cov->p[MAXSTABLE_POINTS])[0]);
	break;
      }
      poisson += rexp(1.0); // Poisson, not Frechet
      gumbel = -log(poisson);
      // MARCO, habe ich am 17.11.13 geaendert

      // pgs->log_zhou_c = 0.0; // wrong! to do

      summand = gumbel - logdens;
     
      
      //   threshold = sub->loggiven ? GumbelThreshold : FrechetThreshold;
       threshold = logthreshold = 
	 (res_type) (gumbel + log(sub->mpp.maxheights[0])); //Gumbel
    //   
       if (!loggiven) {
	threshold = exp(threshold); // Frechet
      }

      //  
       //  APMI(sub);

       //      printf("Max = %f gumbel=%f %f %d logdens=%f\n", sub->mpp.maxheights[0], gumbel, threshold, loggiven, logdens);


      //printf("for thres=%e %d res=%e zhou=%f(%f) logden=%f  gumbel=%f loggiven=%d summand=%f\n", threshold, (int) control, res[control], pgs->zhou_c, sub->mpp.maxheights[0], logdens, gumbel, loggiven, summand);// assert(false);

      //        assert(R_FINITE(threshold ));

// {   int i; for (i=0; i<total_pts; i++) printf("%e ", res[i]); }

      //APMI(cov);
      while ((control<total_pts) && (res[control]>=threshold)) {
	// 	print("control: %d %f %f\n", control, res[control], threshold); assert(false);
	control++;
      }
     if (control >= total_pts) {
	//printf("break2\n");
	break;
      }
    
     //printf("n=%d every =%d %d %d\n", n, GLOBAL.extreme.check_every, PL, PL_STRUCTURE);
     
      if (n % GLOBAL.extreme.check_every == 0) {
	pgs->globalmin = res[control];
	
	for (i=control + 1; i<total_pts; i++) 
	  if (res[i] < pgs->globalmin) pgs->globalmin = res[i];

	if (pgs->globalmin < Minimum) pgs->globalmin = Minimum;	

	//	APMI(cov);

	if (PL >= PL_STRUCTURE) 
	  PRINTF("control: %d %f %f global=%f n=%d every=%d %f log.max=%f\n", 
		 control, res[control], threshold, pgs->globalmin,
		 n, GLOBAL.extreme.check_every, sub->mpp.maxheights[0],
		 log(sub->mpp.maxheights[0]));    
      }
 
 
      // Marco:
      pgs->currentthreshold = loggiven
	? pgs->globalmin - gumbel
	: pgs->globalmin * poisson;  // Frechet :  / exp(-gumbel)
     

      //printf("loggiven =%d %e %f cur.thr=%e\n",
      //     loggiven, pgs->globalmin, gumbel,  pgs->currentthreshold);

    }
    factor = exp(summand);
    //    APMI(cov);

  //
  //printf("factor %4.4f sumd=%4.4f logdens=%4.4f logM2=%4.4f th=%4.4f %d<%d \n", factor, summand, logdens, logM2, threshold, control, total_pts);  //  assert(false);
    
  //   printf("dim=%d loggiven=%d maxstable=%d simugrid=%d randomcoin=%d\n",
  //	 dim, loggiven, maxstable, simugrid, randomcoin);// assert(false);

    //printf("A=%d\n", n);
    zaehler = 0;
    if (simugrid) {
      partialfield = false;
      for (d=0; d<dim; d++) {	
	double
	  step = inc[d] == 0.0 ? 1.0 : inc[d], 
	  dummy = ceil((pgs->supportmin[d] - loc->xgr[d][XSTART]) / step);	
	start[d] = dummy < 0 ? 0 : dummy > gridlen[d] ? gridlen[d] : (int)dummy;
	partialfield |= start[d] > 0;
	dummy = 1.0 + ((pgs->supportmax[d] - loc->xgr[d][XSTART]) / step);
	end[d] = dummy < 0 ? 0 : dummy > gridlen[d] ? gridlen[d] : (int) dummy;
	partialfield |= end[d] < gridlen[d];

	//printf("dummy %f %d %d len=%d\n", dummy, gridlen[d], end[d], loc->length[d]);
	// 	printf("window [%f %f] [%d %d] %d %d\n", pgs->supportmin[d], pgs->supportmax[d], start[d], end[d], gridlen[d], partialfield);  //assert(n < 150); //APMI(cov);


	if (start[d] >= end[d]) { // 
	  //PMI(cov);
	  //  printf("broken %d %d %d supp=%f %f, loc.start=%f %f\n", d,
	  //	 start[d], end[d],
	  //	 pgs->supportmin[d], pgs->supportmax[d],
	  //	 loc->xgr[d][XSTART], inc[d]); 
	  break;
	}
	delta[d] = (end[d] - start[d]) * cumgridlen[d];
	nx[d] = start[d];
	zaehler += start[d] * cumgridlen[d];
	x[d] = xstart[d] = loc->xgr[d][XSTART] + (double) start[d] * inc[d]; 

	if (false || zaehler < 0) {
	  PRINTF("d=%d start=%d, end=%d xstart=%f %f pgs=[%4.3f, %4.3f] xgr=%f %f %f inc=%3.2f\n", //
	  	 d, start[d], end[d], xstart[d], pgs->xstart[d], pgs->supportmin[d], pgs->supportmax[d],
	 	 loc->xgr[d][XSTART], loc->xgr[d][XSTEP], loc->xgr[d][XLENGTH],
	  	 inc[d]);
	   // 	  assert(false);
	}
      }

      //          APMI(cov);
      if (d < dim) {
	//printf("cont: d=%d\n", d);
	continue;
      }
    }

    //  
    //printf("simugrid %d\n", simugrid);
    //  assert(false);
    //    APMI(cov);
 

#define STANDARDINKREMENT_ZAEHLER			\
    d = 0;						\
    nx[d]++;						\
    x[d] += inc[d];					\
    zaehler += cumgridlen[d];				\
    while (nx[d] >= end[d]) {				\
    nx[d] = start[d];					\
    x[d] = xstart[d];					\
    zaehler -= delta[d]; /* delta is positive */	\
    if (++d >= dim) break;				\
    nx[d]++;						\
    x[d] += inc[d];					\
    zaehler += cumgridlen[d];				\
    }							\
    if (d >= dim) { break;}	
    // end define StandardInkrement

 
#define GET SHAPE(x, sub, value); /*printf("%f : %f fctr=%f %ld \n", x[0], *value, factor, zaehler); // */
#define LOGGET LOGSHAPE(x, sub, value, sign);
#define LOGGETPOS LOGGET if (sign[0] > 0)
#define GETRF *value = (res_type) (rf[zaehler]);

    //   printf("%d\n", total_pts); assert(false);

#define RFMAX(OP) {				\
      rf = sub->rf;				\
      for (i=0; i<total_pts; i++) {		\
	*value = (res_type) (OP);		\
	if (res[i] < *value) res[i]=*value;	\
      }						\
    }

#define RFSUM(OP) {				\
      rf = sub->rf;				\
      for (i=0; i<total_pts; i++) {		\
	*value = (res_type) (OP);		\
	if (res[i] < *value) res[i] += OP;	\
      }						\
    }



#define MAXAPPLY(GET, OP)						\
      assert(zaehler >=0 && zaehler<total_pts);				\
      /* if (zaehler<0 || zaehler>=total_pts) { PMI(cov);  printf("//z=%d n=%d\n", zaehler, n); } */ \
      GET {								\
	OP;								\
	/*  assert( {if (*value > sub->mpp.maxheights[0] * exp(gumbel) * (1+1e-15)) {  printf("//r=%f %f delta=%e %d %e\n", *value / exp(gumbel), sub->mpp.maxheights[0], *value / exp(gumbel) - sub->mpp.maxheights[0], loggiven, factor); };true;}); */ \
	/*assert(loggiven || *value <= sub->mpp.maxheights[0] * exp(gumbel) * (1+1e-15)); */ \
	if (res[zaehler] < *value) res[zaehler]=*value;			\
      }								       
   
#define SUMAPPLY(GET,OP) GET; res[zaehler] += OP;

#define APPLY(GET, OP, WHAT)	{				\
      if (simugrid) {						\
	while(true) {						\
	  WHAT(GET, OP);					\
	  STANDARDINKREMENT_ZAEHLER;				\
	}							\
      } else {							\
	for(x=loc->x; zaehler<spatial; zaehler++, x += dim) {	\
	  WHAT(GET, OP);					\
	}							\
      }								\
    }
 

#define OG_BASIC(OP)						\
    assert(zaehler >=0 && zaehler < total_pts);			\
    int jj, idx, index=0;					\
    for (jj=0; jj<dim; jj++) {					\
      idx = round((x[jj] - ogstart[jj]) / ogstep[jj]);		\
      if (idx < 0 || idx >= pgs->own_grid_length[jj]) break;	\
      index += pgs->own_grid_cumsum[jj] * idx;			\
    }								\
    if (jj >= dim) {  OP;  } else if (false) printf("%d %f %f %f len=%f %d %d\n", jj, x[jj],  ogstart[jj], ogstep[jj], pgs->own_grid_length[jj], pgs->own_grid_cumsum[jj], idx);  /* // */  \
   
  				

#define OG_MAXAPPLY(NONE, OP)						\
    rf = sub->rf;							\
    OG_BASIC(*value = rf[index];					\
	     OP; if (res[zaehler] < *value) res[zaehler]=*value;)
    
#define OG_SUMAPPLY(NONE, OP)				\
    rf = sub->rf;					\
    OG_BASIC(*value = rf[index]; res[zaehler] += OP;) 
    
    //   OWNGRIDAPPLY(OP, OG_SUMAPPLY_BASIC)
#define OWNGRIDAPPLY(NONE, OP, WHAT) {		\
      double *ogstart = pgs->own_grid_start,	\
	*ogstep = pgs->own_grid_step;		\
      APPLY(NONE, OP, WHAT);			\
    }


#define	ALL_APPLY(APPLY, MGET, MAPPLY, SGET, SAPPLY, NONLOGGET)		\
    if (loggiven == true) {						\
      if (maxstable) APPLY(MGET, (*value) += summand, MAPPLY)		\
	else APPLY(SGET, sign[0] * exp(*value + summand), SAPPLY);	\
    } else { /* not loggiven */						\
      if (sub->loggiven){						\
	if (maxstable)							\
	  APPLY(MGET, *value=sign[0] * exp(*value+summand), MAPPLY)	\
	else APPLY(SGET, sign[0] * exp(*value + summand), SAPPLY);	\
      } else {								\
	assert(R_FINITE(factor));					\
	if (maxstable) APPLY(NONLOGGET, (*value) *= factor, MAPPLY)	\
	  else APPLY(NONLOGGET, *value * factor, SAPPLY);		\
      }									\
    }


#define AVEAPPLY(G,OP0,OP1) {						\
      warning(" * timescale ?!");					\
      if (simugrid) {							\
	while (true) {							\
	  int zt;  double zw,inct, segt, ct, st, A, cit, sit;		\
	  inct = sub->q[AVERAGE_YFREQ] * loc->xgr[dim][XSTEP];		\
	  cit = cos(inct);						\
	  sit = sin(inct);						\
	  G;								\
	  segt = OP1;							\
	  A = OP0;							\
	  ct = A * cos(segt);						\
	  st = A * sin(segt);						\
	  for (zt = zaehler; zt < total_pts; zt += spatial) {		\
	    res[zt] += (res_type) ct;					\
	    zw = ct * cit - st * sit;					\
	    st = ct * sit + st * cit;					\
	    ct = zw;							\
	    res[zaehler] += zw;						\
	  }								\
	  STANDARDINKREMENT_ZAEHLER;					\
	}								\
      } else {  /* not a grid */					\
        x=loc->x;							\
	/* the following algorithm can greatly be improved ! */		\
	/* but for ease, just the stupid algorithm	        */	\
	for (; zaehler<spatial; zaehler++, x += dim) {			\
	  G; res[zaehler] += OP0 * cos(OP1);				\
	}								\
      }									\
    }	

    //printf("%d %d %d\n", fieldreturn, loggiven, sub->loggiven);

    // *atomdependent rfbased/not; (( recurrent/dissipative: min/max gesteuert))
    if (poissongauss  && !randomcoin) { // AVERAGE
      if (sub->loggiven) {
	AVEAPPLY(LOGGET, sign[0] * exp(value[0] + summand), 
		 sign[1] * exp(value[1]));
      } else AVEAPPLY(GET, value[0] * factor, value[1]);
    } else {
      assert(subrole == ROLE_SMITH || subrole == ROLE_SCHLATHER ||
	     subrole ==ROLE_BROWNRESNICK || subrole==ROLE_POISSON || 
	     subrole==ROLE_POISSON_GAUSS);

      if (fieldreturn == HUETCHEN_OWN_GRIDSIZE) { // atomdependent rfbased/not;
	ALL_APPLY(OWNGRIDAPPLY, NULL, OG_MAXAPPLY, NULL, OG_SUMAPPLY, NULL);	
      } else if (fieldreturn == true) { // extended boolean
	// todo : !simugrid! && Bereiche einengen!!     
	// note ! no sign may be given currently when fieldreturn and loggiven!!
	if (partialfield) {
	  if (!simugrid) BUG;
	  ALL_APPLY(APPLY, GETRF, MAXAPPLY, GETRF, SUMAPPLY, GETRF);
	} else { // !partialfield
	  if (sub->loggiven) {
	    if (maxstable) RFMAX(rf[i] + summand)
	    else RFSUM(exp(rf[i] + summand));	  
	  } else {
	    if (maxstable) RFMAX(rf[i] * factor)
	    else RFSUM(rf[i] * factor);
	  }
	}
      } else if (fieldreturn == false) { // not field return
	ALL_APPLY(APPLY, LOGGETPOS, MAXAPPLY, LOGGET, SUMAPPLY, GET);
      } else BUG; // neither HUETCHEN_OWN_GRIDSIZE, nor true nor false
    }
  
    // printf("B=%d\n", n);
    if (n >= nthreshold) {
      if (maxstable) {
	LPRINT("%d-th pos.: value=%1.3f threshold=%1.3f gu=%1.3f %1.3f %d (%d %d)\n",
	       control, (double) res[control], (double) threshold,
	       gumbel, logdens, loggiven,
	       n, nthreshold); //, deltathresh); 
   	nthreshold += deltathresh;
      } else {
	PRINTF("n=%d Total=%d\n", (int) (n / 1000), (int)(Total_n));
      }
    }

    R_CheckUserInterrupt();
    
    //printf("n=%d %f %f %f 0:%f\n", n, res[0], res[1], res[2], res[3]);

    //    assert(res[0] < 80);

  }// for(n,...    --     while control < total_pts

  //printf("n=%d %f %f %f 3::%f\n", n, res[0], res[1], res[2], res[3]);

  // Trafo to margins

  // printf("%d frechet %d \n", sub->loggiven, ev_p->Frechet);
 
 
  if (maxstable){
    double meansq, var, 
      mean = RF_NA, 
      n_min = RF_INF,      
      eps = GLOBAL.extreme.eps_zhou;

    //    APMI(cov);

    if (pgs->estimated_zhou_c) {
      if (PL > 5) PRINTF("zhou_c: %ld %d",pgs->n_zhou_c, GLOBAL.extreme.min_n_zhou);
      while (pgs->n_zhou_c < GLOBAL.extreme.min_n_zhou) {
	if ((err = REINIT(sub, sub->mpp.moments, s)) != NOERROR) BUG;
	DO(sub, s);   
	SET_SUB;
      }

      while (pgs->n_zhou_c < n_min) {
	for (k=0; k<10; k++) {
	  if ((err = REINIT(sub, sub->mpp.moments, s)) != NOERROR) BUG;
	  DO(sub, s);   
	  SET_SUB; 
	}
	mean = (double) (pgs->sum_zhou_c / (long double) pgs->n_zhou_c);
	meansq = mean * mean;
	var = (double) (pgs->sq_zhou_c / (long double) pgs->n_zhou_c 
			- meansq);
	n_min = var / (meansq * eps * eps);
	
	if (n_min > GLOBAL.extreme.max_n_zhou) n_min =GLOBAL.extreme.max_n_zhou;
 

	//
	if (PL > 5) PRINTF(" n=%ld, min=%f %f mean=%f zhou=%f eps=%f\n", pgs->n_zhou_c, n_min, (double) pgs->sum_zhou_c, mean, pgs->zhou_c, eps);
  
      }
      //
    } else {
      mean = pgs->zhou_c;
    }



    // mean /= M1;

    //printf(" n=%ld, min=%f %f mean=%f zhou=%f eps=%f %f max=%f\n", pgs->n_zhou_c, n_min, (double) pgs->sum_zhou_c, mean, pgs->zhou_c, eps, M1, sub->mpp.maxheights[0]);

    

    // printf("loggiven = %d %f %f\n", loggiven, mean, pgs->zhou_c);
    //    APMI(cov);

    //     mean = 250; // 2 statt 11

    double xi = P0(GEV_XI);

    //    printf("xi=%f %d logmean=%d %f\n", xi, loggiven, pgs->logmean, mean);

    if (loggiven) { // 3.6.13 sub->loggiven geaendert zu loggiven
      if (!pgs->logmean) mean = log(mean);
      for (k=0; k<total_pts; k++) res[k] += mean;
      if (xi != 0.0) 
	for (k=0; k<total_pts; k++) res[k] = exp(xi * res[k]) - 1.0;
    } else {
      for (k=0; k<total_pts; k++) res[k] *= mean;  
      if (xi==0.0) for (k=0; k<total_pts; k++) res[k] = log(res[k]);
      else if (xi == 1.0) for (k=0; k<total_pts; k++) res[k] -= 1.0;
      else for (k=0; k<total_pts; k++) res[k] = pow(res[k], xi) - 1.0;
    }
    
    

    // muss das vorletzte sein !
    if (cov->kappasub[GEV_S] != NULL) {
      error("'s' cannot be chosen as a function yet.");
      // check whether s is positive !!
    } else {
      double ss = xi == 0 ? P0(GEV_S) : (P0(GEV_S) / xi);
      //printf("s=%f\n", ss);

      if (ss != 1.0) for (k=0; k<total_pts; k++) res[k] *= ss;
    }


    // muss das letzte sein !
    if (cov->kappasub[GEV_MU] != NULL) {
      error("mu cannot be chosen as a function yet.");
    } else {
      double mu = P0(GEV_MU);
      //   printf("mu=%f xi=%f %f\n", mu, xi, s);
      //printf("mu=%f\n", mu);
    if (mu != 0.0) for (k=0; k<total_pts; k++) res[k] += mu;
    }
  }
  //printf("n=%d %f %f %f 0:%f\n", n, res[0], res[1], res[2], res[3]);
  // for (k=0; k<total_pts; k++) printf("%f ", res[k]); printf("\n");
  if (PL >= PL_SUBIMPORTANT) {
    PRINTF("number of shape functions used: %d\n", n);
  }

  return;
} // end dompp


void dompp(cov_model *cov, storage *s) {
  dompp(cov, s, NULL);
}

//void calculate_integral(int d, double R, cov_model *cov,
//			integral_type *integral) {
// 
//}




////////////////////////////////////////////////////////////////////
// Poisson

int check_poisson(cov_model *cov) {
  cov_model *next=cov->sub[0],
    *key = cov->key,
    *sub = key == NULL ? next : key;
  int err,
    dim = cov->tsdim; // taken[MAX DIM],
  mpp_param *gp  = &(GLOBAL.mpp);
  Types type = sub == key ? PointShapeType : ShapeType; 

  //print("eee\n");
  
  cov->role = ROLE_POISSON;

  kdefault(cov, POISSON_INTENSITY, gp->intensity[dim]);
  if ((err = checkkappas(cov, true)) != NOERROR) return err;

  if (cov->tsdim != cov->xdimprev || cov->tsdim != cov->xdimown)
    return ERRORDIM;
  
  if ((err = CHECK(sub, dim, dim, type, XONLY, CARTESIAN_COORD,
		     SUBMODEL_DEP, cov->role)) != NOERROR) return err;
  setbackward(cov, sub);
 
  return NOERROR;
}


void range_poisson(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range) {
  range->min[POISSON_INTENSITY] = RF_NEGINF;
  range->max[POISSON_INTENSITY] = RF_INF;
  range->pmin[POISSON_INTENSITY] = -10;
  range->pmax[POISSON_INTENSITY] = 10;
  range->openmin[POISSON_INTENSITY] = true;
  range->openmax[POISSON_INTENSITY] = true; 
}


int struct_poisson(cov_model *cov, cov_model **newmodel){
  cov_model *next=cov->sub[0];
  location_type *loc = Loc(cov);
 
  if (newmodel != NULL) SERR("unexpected call of struct_poisson"); /// ?????
  if (cov->role != ROLE_POISSON)
    SERR1("'%s' not called as random coin", NICK(cov));  
  if (cov->key != NULL) COV_DELETE(&(cov->key));

  if (loc->Time || (loc->grid && loc->caniso != NULL)) {
    Transform2NoGrid(cov, false, GRIDEXPAND_AVOID);
  }

  if (!isPointShape(next)) {
    int err;
    if ((err = covcpy(&(cov->key), next)) != NOERROR) return err;
    if ((err = addStandard(&(cov->key))) != NOERROR) return err;
  }
 
  return NOERROR;
}


int init_poisson(cov_model *cov, storage *S) {
  //  location_type *loc = Loc(cov);
  // mpp_storage *s = &(S->mpp);
  cov_model *key=cov->key;
  int err;
 
  if ((err = init_mpp(cov, S)) != NOERROR) {
    return err;    
  }

  //APMI(cov);
  if (!isPointShape(key)) SERR("no definition of a shape function found");
  
  key->Spgs->intensity = key->Spgs->totalmass * P0(POISSON_INTENSITY);

  return err;
}




//////////////////////////////////////////////////////////////////////
//           Random Coin
//////////////////////////////////////////////////////////////////////


/*
  void coin(double *x, cov_model *cov, double *v){
  cov_model
  *shape = cov->sub[COIN_SHAPE],
  *next = cov->sub[COIN_COV];
  if (shape == NULL) COV(x, next, v);
  else {
  int i, d,
  vdim = cov->vdim;
  for (i=0; i<vdim; i++) v[i] = RF_NA;
  for (d=0; d<cov->xdimown; d++) {
  if (x[d] != 0.0) {
  return;
  }
  }
  v[0] = 1.0; // only in the univariate case well defined
  }
  }


  void coinInverse(double *v, cov_model *cov, double *x){
  cov_model
  *shape = cov->sub[COIN_SHAPE],
  *next = cov->sub[COIN_COV];
  if (shape == NULL) INVERSE(v, next, x);
  else {
  int i, 
  vdim = cov->vdim;
  for (i=0; i<vdim; i++) x[i] = RF_NA;
  }
  }
*/

int check_randomcoin(cov_model *cov) {
  cov_model 
    *next = cov->sub[COIN_COV],
    *shape = cov->sub[COIN_SHAPE],
    *key = cov->key;
  int err,
    dim = cov->tsdim; // taken[MAX DIM],
  mpp_param *gp  = &(GLOBAL.mpp);
  //extremes_param *ep = &(GLOBAL.extreme);

  //PMI(cov->calling, "check_randomcoin!");
  
  //PMI(cov, "check random coins");
  return ERRORNOTPROGRAMMEDYET;
  NotProgrammedYet("'random coin'");
 
  ROLE_ASSERT(ROLE_POISSON_GAUSS || (cov->role==ROLE_GAUSS && cov->key!=NULL));
  ASSERT_ONE_SUBMODEL(cov);

  if (cov->sub[COIN_COV] != NULL && cov->sub[COIN_SHAPE]!=NULL)
    SERR("either 'tcf' or 'shape' must be given");

  kdefault(cov, RANDOMCOIN_INTENSITY, gp->intensity[dim]);
  if ((err = checkkappas(cov, true)) != NOERROR) {
    //AERR(err);
    return err;
  }
  if (cov->tsdim != cov->xdimprev || cov->tsdim != cov->xdimown)  {
    //AERR(1);
    return ERRORDIM;
  }


  if (key == NULL) {

    //printf("SHAPE %ld\n", shape);

    if (shape == NULL) {      
      if ((err = CHECK(next, dim, dim, PosDefType,
			 XONLY, SYMMETRIC,
			 SCALAR, ROLE_COV)) != NOERROR)  {
	return err;
      }      
      if ((next->pref[Average] == PREF_NONE) + 
	  (next->pref[RandomCoin] == PREF_NONE) !=1){

	//APMI(next);

	//assert(false);
	return ERRORPREFNONE;
      }
    } else { // shape != NULL
      if (next != NULL) SERR("phi and shape may not be given at the same time");
    
      if ((err = CHECK(shape, dim, dim, ShapeType, XONLY, CARTESIAN_COORD, 
			 SCALAR, ROLE_POISSON)) != NOERROR)  {
	return err;
      }     
      //   APMI(cov);
    }
    //print("ok\n");
  } else { // key != NULL
    // note: key calls first function to simulate the points
    // if this is true then AverageIntern/RandomCoinIntern calls Average/RandomCoin 
    cov_model *intern = cov;
    while (intern != NULL && intern->nr!=AVERAGE_INTERN  && isAnyDollar(intern))
      intern = intern->key != NULL ? intern->key : intern->sub[0];
   if (intern == NULL) {
      //APMI(key);
      BUG;
    } //else if (intern != cov) {
    //      printf("Here\n");
    //      APMI(cov);
    //      paramcpy(intern, cov, true, false);
    //    }
    
    if ((err = cov->role == ROLE_BASE || cov->role == ROLE_GAUSS
	 ? CHECK(key, dim, dim, ProcessType, XONLY, CARTESIAN_COORD, //err
    		   SUBMODEL_DEP, ROLE_POISSON_GAUSS)
	 : cov->role == ROLE_POISSON_GAUSS 
	 ? CHECK(key, dim,  dim, PointShapeType, XONLY, CARTESIAN_COORD, //err
		   SUBMODEL_DEP, ROLE_POISSON_GAUSS)
	 : ERRORFAILED
	 ) != NOERROR) {
      // APMI(cov)
      return err;
    }
    // APMI(cov)
  }
 
  cov_model *sub = key != NULL ? key : shape != NULL ? shape : next;
  setbackward(cov, sub);
 
  //  APMI(cov);

  return NOERROR;
}

void range_randomcoin(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range) {
  range->min[RANDOMCOIN_INTENSITY] = RF_NEGINF;
  range->max[RANDOMCOIN_INTENSITY] = RF_INF;
  range->pmin[RANDOMCOIN_INTENSITY] = -10;
  range->pmax[RANDOMCOIN_INTENSITY] = 10;
  range->openmin[RANDOMCOIN_INTENSITY] = true;
  range->openmax[RANDOMCOIN_INTENSITY] = true; 
}

 
int struct_randomcoin(cov_model *cov, cov_model **newmodel){
  cov_model 
    *next = cov->sub[COIN_COV],
    *shape = cov->sub[COIN_SHAPE];
  location_type *loc = Loc(cov);
  int err,
    dim = cov->tsdim; // taken[MAX DIM],

  // APMI(cov);

  ROLE_ASSERT(ROLE_POISSON_GAUSS);

  if (cov->key != NULL) COV_DELETE(&(cov->key));
  if (loc->Time || (loc->grid && loc->caniso != NULL)) {
    Transform2NoGrid(cov, true, GRIDEXPAND_AVOID);
    SetLoc2NewLoc(next == NULL ? shape : next, Loc(cov)); 
  }

  if (newmodel != NULL) {
    //    printf("error in struct random coin %ld\n", (long int) (newmodel));
    //    PMI(cov->calling);
    SERR("unexpected call of stuct_randomcoin"); /// ?????
  }

  if (shape != NULL) {
    if ((err = covcpy(&(cov->key), shape)) > NOERROR) {
      return err;
    }
    if ((err = addPGS(&(cov->key))) != NOERROR) return err;
   
    // APMI(cov);
    return NOERROR;
  } else { // shape == NULL, i.e. covariance given
    if (next == NULL) BUG;
    // next not NULL
    if (next->pref[Average] == PREF_NONE && next->pref[RandomCoin]==PREF_NONE) {
      // if (next->nr > LASTDOLLAR) AERR(ERRORPREFNONE);
      return ERRORPREFNONE;
    }

    //    printf("%d \n", dim);
    //    PMI(next, "randomcoins");
    
    if ((err = CHECK(next, dim,  dim, PosDefType, XONLY, ISOTROPIC, 
		       SCALAR, ROLE_POISSON_GAUSS))
	!= NOERROR) {
      // APMI(cov)
      return err;
    }

    if ((err = STRUCT(next, &(cov->key))) > NOERROR) return err;
    if (cov->key == NULL)
      SERR("no structural information for random coins given");
    
    cov->key->calling = cov;
    if ( cov->pref[Average] == PREF_NONE ) {
      if (cov->key->nr != RANDOMSIGN) addModel(&(cov->key), RANDOMSIGN);
      assert(!isPointShape(cov->key));
      if ((err = addPGS(&(cov->key))) != NOERROR) return err;
    }
    
    //APMI(cov);
    return NOERROR;
  }
}


int init_randomcoin(cov_model *cov, storage *S) {
  cov_model
    *covshape = cov->sub[ cov->sub[COIN_SHAPE] != NULL ? COIN_SHAPE : COIN_COV],
    *key = cov->key,
    *sub = key == NULL ? covshape : key; 
  char name[] = "Poisson-Gauss";
  location_type *loc = Loc(cov);
  //mpp_storage *s = &(S->mpp);  
  int 
    err = NOERROR;

  sprintf(ERROR_LOC, "%s process: ", name);

  if (cov->role != ROLE_POISSON_GAUSS)  {
    // PMI(cov->calling, "init_randomcoin");
    ILLEGAL_ROLE;
  }

  cov->method = covshape->pref[Average] == PREF_NONE ? RandomCoin : Average;
    
  if (cov->method == Average &&  loc->caniso != NULL) {
    bool diag, quasidiag, semiseparatelast, separatelast;
    int idx[MAXMPPDIM];
    assert(loc->timespacedim <= MAXMPPDIM);
    analyse_matrix(loc->caniso, loc->cani_nrow, loc->cani_ncol, &diag,
		   &quasidiag, idx, &semiseparatelast, &separatelast);
    if (!separatelast) SERR("not a model where time is separated");
  }
 
  if ((err = init_mpp(cov, S)) != NOERROR) {
    //PMI(cov);    AERR(err);
    return err;    
  }

  sub->Spgs->intensity = 
    sub->Spgs->totalmass * P0(RANDOMCOIN_INTENSITY); 

  //  PMI(cov, "randmcoin");
  assert(sub->mpp.moments >= 2);
  if (!R_FINITE(sub->Spgs->totalmass) || !R_FINITE(sub->mpp.mM[2]))
    SERR("Moments of submodels not known");

  sub->role = ROLE_POISSON_GAUSS;
  return NOERROR;
}



void do_randomcoin(cov_model *cov, storage *s) {   
  assert(cov->simu.active);
  bool loggauss = GLOBAL.gauss.loggauss;
  location_type *loc = Loc(cov);
  int i;
  double *res = cov->rf;

  dompp(cov, cov->stor==NULL ? s : cov->stor);// letzteres falls shape gegeben
  if (loggauss) {
    int vdimtot = loc->totalpoints * cov->vdim2[0];
    for (i=0; i<vdimtot; i++) res[i] = exp(res[i]);
  }
}


////////////////////////////////////////////////////////////////////
// Schlather

void extremalgaussian(double *x, cov_model *cov, double *v) {
  // schlather process
  cov_model *next = cov->sub[0];
  
  if (cov->role == ROLE_SCHLATHER) COV(x, next, v)
  else {
    COV(x, next, v);
    *v = 1.0 - sqrt(0.5 * (1.0 - *v));
  }
}

//#define MPP_SHAPE 0
//#define MPP_TCF 1

int SetGEVetc(cov_model *cov, int role) {
  int err;
  extremes_param *ep = &(GLOBAL.extreme);
  
  if (cov->role != ROLE_COV) cov->role = role;

  if (cov->sub[MPP_TCF] != NULL && cov->sub[MPP_SHAPE]!=NULL)
    SERR("either 'tcf' or a shape definition must be given");
  if ((err = checkkappas(cov, false)) != NOERROR) return err;
  kdefault(cov, GEV_XI, ep->GEV_xi);
  kdefault(cov, GEV_S, P0(GEV_XI) == 0.0 ? 1.0 : fabs(P0(GEV_XI)));
  kdefault(cov, GEV_MU, P0(GEV_XI) == 0.0 ? 0.0 : 1.0);

  // print("xi s mu %f %f %f\n",  P0(GEV_XI), P0(GEV_S), P0(GEV_MU)); assert(false);

  if ((err = checkkappas(cov, true)) != NOERROR) return err;
  if (cov->tsdim != cov->xdimprev || cov->tsdim != cov->xdimown) {
    return ERRORDIM;
  }
  return NOERROR;
}


int check_schlather(cov_model *cov) {

  //print("check schlather\n");

  cov_model
    *key = cov->key,
    *next = cov->sub[cov->sub[MPP_SHAPE] != NULL ? MPP_SHAPE:MPP_TCF];
  int err,
    dim = cov->tsdim; // taken[MAX DIM],
  // mpp_param *gp  = &(GLOBAL.mpp);
  double v;

  //  printf("A \n");
 
  ASSERT_ONE_SUBMODEL(cov);
  ///  printf("B A \n");
  if ((err = SetGEVetc(cov, ROLE_SCHLATHER)) != NOERROR) return err;
  //   printf("CA \n");

  cov_model *sub = cov->key==NULL ? next : key;
  //        printf("GA \n");
       
  if (key == NULL) {
    // printf("key=NULL\n");
    int  role = isNegDef(sub) ? ROLE_COV  //Marco, 29.5.13
      : isShape(sub) ? ROLE_SCHLATHER
      : isGaussProcess(sub) ? ROLE_GAUSS
      : isBernoulliProcess(sub) ? ROLE_BERNOULLI
      : ROLE_UNDEFINED;    
    ASSERT_ROLE_DEFINED(sub);

    if ((err = isPosDef(next)
	 ? CHECK(next, dim, dim, PosDefType, XONLY, ISOTROPIC, //err
		   SCALAR, role)
	 : CHECK(next, dim, dim, ProcessType, XONLY, CARTESIAN_COORD, //err, 
		 SCALAR, role)) != NOERROR)  return err;

    if (sub->vdim2[0] != 1) SERR("only univariate processes are allowed");
    assert(cov->vdim2[0] == cov->vdim2[1]);

    setbackward(cov, sub);

    if (role==ROLE_COV) {
      if (next->pref[Nothing] == PREF_NONE) return ERRORPREFNONE;
      COV(ZERO, next, &v);
      if (v != 1.0) 
	SERR("extremalgaussian requires a correlation function as submodel.");
    }
    
  } else { // key != NULL
    //printf("key!=NULL\n");
   if ( (err = CHECK(key, dim,  dim, PointShapeType, XONLY, CARTESIAN_COORD,  
		      SUBMODEL_DEP, ROLE_SCHLATHER)) != NOERROR) {
     //APMI(cov);
      return err;
    }
    setbackward(cov, sub);
  }
  //print("end check schlather\n");

  return NOERROR;
}


int struct_schlather(cov_model *cov, cov_model **newmodel){
  cov_model
    *sub = cov->sub[cov->sub[MPP_SHAPE] != NULL ? MPP_SHAPE:MPP_TCF];
  int err, role, ErrNoInit;

  if (cov->role != ROLE_SCHLATHER) BUG;
  if (newmodel != NULL)
    SERR1("unexpected structure request for '%s'", NICK(cov));
  if (cov->key != NULL) COV_DELETE(&(cov->key));
  if (cov->sub[MPP_TCF] != NULL) {
    if ((err = STRUCT(sub, &(cov->key))) >  NOERROR) return err;
    cov->key->calling = cov;
  } else {
    if ((err = covcpy(&(cov->key), sub)) != NOERROR) return err;
  }
  assert(cov->key->calling == cov);
 
  if (cov->key->nr != GAUSSPROC && !isBernoulliProcess(cov->key)) {
    if (isNegDef(cov->key)) addModel(&(cov->key), GAUSSPROC); 
    else {
      if (isGaussProcess(cov->key)) {
	SERR("invalid model specification");
      } else SERR("schlather process currently only allowed for gaussian processes and binary gaussian processes");
    }
  }
  assert(cov->key->calling == cov);
  
  role = cov->key->nr == GAUSSPROC ? ROLE_GAUSS   //Marco, 29.5.13
    : isBernoulliProcess(cov->key) ? ROLE_BERNOULLI
    : ROLE_UNDEFINED;

  //PMI(cov->key, "role ");
  ASSERT_ROLE_DEFINED(cov->key);

  if ((err = CHECK(cov->key, cov->tsdim, cov->xdimown, ProcessType, 
		     cov->domown,
		     cov->isoown, cov->vdim2, role)) != NOERROR) return err;  

  // APMI(cov->key);
 
  if ((ErrNoInit = STRUCT(cov->key, NULL)) > NOERROR) return ErrNoInit; //err
  
  addModel(&(cov->key), STATIONARY_SHAPE);
  
  if ((err = CHECK(cov->key, cov->tsdim, cov->xdimown, PointShapeType, 
		     cov->domown,
		     cov->isoown, cov->vdim2, ROLE_SCHLATHER)) != NOERROR)
    return err;
  
  return ErrNoInit;
}


////////////////////////////////////////////////////////////////////
// Smith


int check_smith(cov_model *cov) {
  cov_model 
    *shape = cov->sub[MPP_SHAPE],
    *TCF =  cov->sub[MPP_TCF],
    *next = shape != NULL ? shape : TCF,
    *key = cov->key,
    *sub = (key==NULL) ? next : key;
  ;
  int err, role,
    dim = cov->tsdim; // taken[MAX DIM],
  //Types type;

  ASSERT_ONE_SUBMODEL(cov);
  
  if ((err = SetGEVetc(cov, ROLE_SMITH)) != NOERROR) return err;

  if (key != NULL) {
    if ((err = CHECK(key, dim,  dim, PointShapeType, XONLY, CARTESIAN_COORD,
		       SUBMODEL_DEP, ROLE_SMITH)) != NOERROR) return err;
  } else { // key == NULL
    if (next == cov->sub[MPP_TCF]) {

      //      PMI(cov);

      if ((err = CHECK(next, dim, dim, TcfType, XONLY, ISOTROPIC, 
			 SCALAR, ROLE_SMITH)) != NOERROR) {
	return err;
      }

      if ((dim == 1 && next->rese_derivs < 1) ||
	  (dim >= 2 && dim <= 3 && next->rese_derivs < 2) ||
	  dim > 3) {

	//printf("dim = %d\n", dim);
        //APMI(next);
	SERR("submodel does not have enough derivatives (programmed).");
      }

    } else { // shape
      role = // (isNegDef(sub) && !isPosDef(sub)) ?  ROLE_COV  //Marco, 29.5.13
	isShape(sub) ? ROLE_MAXSTABLE // 23.4.14
	: isPointShape(sub) ? ROLE_SMITH
	: isGaussProcess(sub) ? ROLE_GAUSS
	: isBernoulliProcess(sub) ? ROLE_BERNOULLI
	: ROLE_UNDEFINED; 
      ASSERT_ROLE_DEFINED(sub);
 
      if ((err = CHECK(next, dim, dim, ShapeType, XONLY, CARTESIAN_COORD, 
		       SCALAR, role)) != NOERROR) {
	return err;
      }
      if (next->full_derivs < 0) 
	SERR1("'%s' requires an explicit submodel.", NICK(cov));
    }
  } 
  
  setbackward(cov, next);

  return NOERROR;
}

 


void param_set_identical(cov_model *to, cov_model *from, int depth) {
  assert(depth >= 0);
  assert(to != NULL && from != NULL);
  assert(strcmp(NICK(from), NICK(to)) == 0); // minimal check

  int i;

  assert(from->qlen == to->qlen);
  if (from->q != NULL) MEMCOPY(to->q, from->q, sizeof(double) * from->qlen);

  for (i=0; i<MAXPARAM; i++) { PCOPY(to, from, i); }

  if (depth > 0) {
    for (i=0; i<MAXSUB; i++) {
      if (from->sub[i] != NULL) 
	param_set_identical(to->sub[i], from->sub[i], depth - 1);
    }
  }
}


int PointShapeLocations(cov_model *key, cov_model *shape) {
  assert(key != NULL);
 
  //  printf("fx \n");
  int err,
    nr = key->nr;
  assert(isPointShape(key));

  if (key->sub[PGS_LOC] != NULL) return NOERROR;
  if ((err = covcpy(key->sub + PGS_FCT, shape)) != NOERROR) return err;
 
  if (nr == PTS_GIVEN_SHAPE) {   
    //PMI(shape, -1);
    //printf("here %ld %d %d %d\n", key->sub[PGS_LOC], ScaleOnly(shape),
    //	   !shape->deterministic, shape->sub[0]->deterministic);
    if (key->sub[PGS_LOC] == NULL) {        
      if (ScaleOnly(shape) && !shape->deterministic &&
	  shape->sub[0]->deterministic) { // pure scale mixture
	if ((err = covcpyWithoutRandomParam(key->sub + PGS_LOC, shape->sub[0]))
	    != NOERROR) return err;
	addModel(key->sub + PGS_LOC, RECTANGULAR);	
	addModel(key->sub + PGS_LOC, LOC);
	addSetDistr(key->sub + PGS_LOC, shape, ScaleDollarToLoc, true, 0);

	key->sub[PGS_LOC]->calling = key;
     } else { 
	if ((err = covcpyWithoutRandomParam(key->sub + PGS_LOC, shape)) 
	    != NOERROR) return err;
	if (!shape->deterministic) {	
	  //APMI(key);
	  addSetDistr(key->sub + PGS_LOC, key->sub[PGS_FCT], 
		      param_set_identical, true, MAXINT);
	  

	}
	addModel(key->sub + PGS_LOC, RECTANGULAR);
 	key->sub[PGS_LOC]->calling = key;
     }
    }
  } else if (nr == STANDARD_SHAPE) {
    assert(key != NULL && shape != NULL);
    if ((err = STRUCT(shape, key->sub + PGS_LOC)) != NOERROR) return err;
    key->sub[PGS_LOC]->calling = key;
    if (key->sub[PGS_LOC] == NULL)
      SERR("simple enlarging of the simulation window does not work");
  } else BUG;

  //  APMI(key);

  return NOERROR;
}

int addPointShape(cov_model **Key, cov_model *shape, cov_model *pts, 
		  cov_model *local_pts,
		  int dim, int vdim) {
#define PGS_N 2
  int i,
    err = NOERROR,
    pgs[PGS_N] = {PTS_GIVEN_SHAPE, STANDARD_SHAPE};
  char msg[PGS_N][200];
  assert(shape != NULL);
  assert(*Key == NULL);

  // to do: strokorbball: raeumlich waere Standard/Uniform besser;
  // praeferenzen programmieren?
  for (i=0; i<PGS_N; i++) { 
    if (i > 0) {
      errorMSG(err, msg[i-1]);
      // 
      XERR(err);
    }
    //    if (i > 0) XERR(err); assert(i ==0);
    if (*Key != NULL) {
      COV_DELETE(Key);
      //      (*Key)->sub + PGS_LOC, pts
    }
    addModel(Key, pgs[i]);
    (*Key)->calling = shape->calling;
    assert(shape->calling != NULL);
    assert((*Key)->sub[PGS_LOC] == NULL && (*Key)->sub[PGS_FCT] == NULL); 

    //   PMI(shape); 
    //   PMI(pts);
 
    if (pts != NULL) {
      if ((err = covcpy((*Key)->sub + PGS_FCT, shape)) != NOERROR ||
	  (err = covcpy((*Key)->sub + PGS_LOC, pts)) != NOERROR
 	  ) return err;
      Ssetcpy((*Key)->sub[PGS_FCT], (*Key)->sub[PGS_LOC], shape, pts);
      Ssetcpy((*Key)->sub[PGS_LOC], (*Key)->sub[PGS_FCT], pts, shape);
      
      //     PMI((*Key)->sub[PGS_FCT]); 
      //    APMI((*Key)->sub[PGS_LOC]);
 

    } else {
      //
      if ((err = PointShapeLocations(*Key, shape)) != NOERROR) continue;
      if (local_pts != NULL) { // only in plusmalS.cc
	if ((*Key)->nr != PTS_GIVEN_SHAPE) continue;
	cov_model *local, *dummy;
	if ((err = covcpy(&local, false, local_pts, shape->prevloc, NULL, 
			  true)) != NOERROR) return err;
	local->calling = (*Key)->calling;
	dummy = local;
	while (dummy->sub[DOLLAR_SUB] != NULL) dummy = dummy->sub[DOLLAR_SUB];
	if (dummy->nr != LOC) BUG;
	dummy->sub[DOLLAR_SUB] = *Key;
	(*Key)->calling = dummy;
      }
    }

    (*Key)->calling = shape->calling;
    (*Key)->sub[PGS_FCT]->calling = (*Key)->sub[PGS_LOC]->calling = *Key;

     //PMI((*Key)->calling);
    assert((*Key)->sub[PGS_LOC] != NULL && (*Key)->sub[PGS_FCT] != NULL);

    //    APMI(*Key);

    //    printf(">>>>>>> KEY start %d %s \n", i, NICK((*Key)));
    if ((err = CHECK(*Key, dim, dim, PointShapeType, XONLY, CARTESIAN_COORD,
		     vdim, ROLE_MAXSTABLE)) != NOERROR) {
      //        printf(">>>>>>> KEY break %d %s %d\n", i, Nick(*Key), dim);  
      //PMI(*Key);
    
      XERR(err);
      continue; 
    }
    //printf(">>>>>>> KEY done %d \n", i);
    (*Key)->stor = (storage *) MALLOC(sizeof(storage)); 
    STORAGE_NULL((*Key)->stor);


    //  APMI(*Key);

     if ((err = INIT(*Key, 1, (*Key)->stor)) == NOERROR) break;
  } // for i_pgs
  if (err != NOERROR) {
    errorMSG(err, msg[i-1]);
    sprintf(ERRORSTRING, 
	    "no method found to simulate the given model:\n\tpgs: %s\n\tstandard: %s)",
	   msg[0], msg[1]);
    return ERRORM;
  }
  return NOERROR;
}

int addPointShape(cov_model **Key, cov_model *shape, cov_model *pts, 
		  int dim, int vdim) {
  return addPointShape(Key, shape, pts, NULL, dim, vdim);
}

int struct_smith_pts(cov_model **Key, cov_model *shape, 
		     cov_model *calling,
		     int tsdim, int vdim) {
  cov_model
    *dummy = NULL;
  int err = NOERROR;

  if ((err = STRUCT(shape, Key)) == NOERROR && *Key != NULL) {
    (*Key)->calling = calling;
    //   PMI(shape); 
    //APMI(*Key);
    Types type = TypeConsistency(PointShapeType, *Key) ? PointShapeType :
      TypeConsistency(RandomType, *Key) ? RandomType :
      TypeConsistency(ShapeType, *Key) ? ShapeType :
      OtherType;

    //PMI(*Key);  
  
    //PMI(*Key);
 
    if (type == RandomType) { // random locations given;
      // so, it must be of pgs type (or standard):
      if ((err = CHECK_R(*Key, shape->tsdim)) != NOERROR) {
	goto ErrorHandling;
      }
      dummy = *Key;
      *Key = NULL;     
      if ((err = addPointShape(Key, shape, dummy, tsdim, vdim)) != NOERROR) 
	goto ErrorHandling; 
      if (*Key == NULL) BUG;
      (*Key)->calling = calling;
      // APMI(cov);
    } else {
      if ((err = CHECK(*Key, shape->tsdim, shape->xdimprev, type, 
		       shape->domprev, shape->isoprev, shape->vdim2, 
		       ROLE_MAXSTABLE))
	  != NOERROR) {
	goto ErrorHandling;
      }
 
      if (type==PointShapeType) {
	if ((err = PointShapeLocations(*Key, shape)) != NOERROR) 
	  goto ErrorHandling;
      } else if (type == ShapeType) {
	dummy = *Key;
	*Key = NULL;
 	  // suche nach geeigneten locationen
	if ((err = addPointShape(Key, dummy, NULL, tsdim, vdim)) != NOERROR) 
	  goto ErrorHandling; 
 	//printf("e ddd\n");
      } else BUG;
    } // ! randomtype
  } else { // S-TRUCT does not return anything
    int err2;
    //assert(false);
    // XERR(err);
    //	APMI(*Key);
    if ((err2 = addPointShape(Key, shape, NULL, tsdim, vdim)) != NOERROR) {
      if (err == NOERROR) err = err2;
      goto ErrorHandling; 
    }
    err = NOERROR;
  }
 ErrorHandling:
  if (dummy != NULL) COV_DELETE(&dummy);
  return err;
}


int struct_smith(cov_model *cov,  cov_model **newmodel){
  cov_model
    *tmp_shape = NULL,
    *shape = cov->sub[MPP_SHAPE],
    *tcf =  cov->sub[MPP_TCF],
    *tcf_shape = NULL,
    *sub = shape != NULL ? shape : tcf;
  location_type *loc = Loc(cov);
  int err = NOERROR;
  if (newmodel != NULL) SERR("unexpected call of struct_smith");
  
  //APMI(shape);


  if (cov->role != ROLE_SMITH) BUG;  

  if (loc->Time || (loc->grid && loc->caniso != NULL)) {
    Transform2NoGrid(cov, false, GRIDEXPAND_AVOID);
    SetLoc2NewLoc(sub, Loc(cov));
  }

  if (cov->key != NULL) COV_DELETE(&(cov->key));
  assert(cov->key == NULL);
  
  if (tcf != NULL) {
    // to do: ausbauen 
    if ((err = covcpy(&tcf_shape, sub)) != NOERROR) return err;   
    addModel(&tcf_shape, STROKORB_MONO);

    if ((err = CHECK(tcf_shape, tcf->tsdim, tcf->xdimprev, ShapeType, 
		     tcf->domprev, tcf->isoprev, tcf->vdim2, 
		     ROLE_MAXSTABLE)) != NOERROR) goto ErrorHandling;
    tmp_shape = tcf_shape; 
  } else {
    tmp_shape = shape;
   }

  //  	APMI(tmp_shape);
  if ((err = struct_smith_pts(&(cov->key), tmp_shape, cov,
			      cov->tsdim, cov->vdim2[0]))
      != NOERROR) goto ErrorHandling;

  //   APMI(cov);

  err = NOERROR;

 ErrorHandling:
  if (tcf_shape != NULL) COV_DELETE(&tcf_shape);
  return err;
}




typedef double (*logDfct)(double *data, double gamma);

double HueslerReisslogD(double *data, double gamma) {  
  double 
    g = sqrt(2.0 * gamma),
    logy2y1 = log(data[1] / data[0]);
  return -pnorm(0.5 * g + logy2y1 / g, 0.0, 1.0, 1.0, false) / data[0] 
    -pnorm(0.5 * g - logy2y1 / g, 0.0, 1.0, 1.0, false) / data[1];
}


double schlatherlogD(double *data, double gamma) {  
  double 
    sum = data[0] + data[1],
    prod = data[0] * data[1];
  return 0.5 * sum / prod * 
    (1.0 + sqrt(1.0 - 2.0 * (2.0 - gamma) * prod / (sum * sum)));
}

#define LL_AUTO 0
#define LL_FULL 1
#define LL_COMPOSITE 2
#define LL_SELECTION 3
#define LL_UNKNOWN error("unknown value for 'likelihood' -- please contact author");


void loglikelihoodMaxstable(double *data, cov_model *cov, logDfct logD,
			    double *v) {

  cov_model *sub = cov;
  while (isProcess(sub))
    sub = sub->key != NULL ? sub->key : sub->sub[0];

  if (cov->q == NULL) {
    location_type *loc = Loc(cov);
    int len = loc->totalpoints;    
    cov->qlen = len;
   // transform everything towards standard Frechet
    if ((cov->q = (double*) MALLOC(sizeof(double) * len)) == NULL)
      error("memory allocation error");
    if (loc->grid || loc->Time) Transform2NoGrid(sub, false, true);   
  }

  location_type *loc = Loc(cov);
  double *x, *y;
  int i,j,
    dim = cov->xdimown,
    len = loc->totalpoints;    
    
  if (data != NULL) {
    double 
      xi = P0(GEV_XI),
      mu = P0(GEV_MU),
      s = P0(GEV_S);    
    if (xi == 0) {
      for (i=0; i<len; i++) cov->q[i] = exp((data[i] - mu) / s);
    } else {
      double 
	xiinv = 1.0 / xi,
	xis = xi / s;      
      for (i=0; i<len; i++) cov->q[i] = pow(1.0 + xis * (data[i] - mu), xiinv);
    }
  }

  switch(GLOBAL.fit.likelihood) {
  case LL_AUTO: case LL_COMPOSITE: 
    double z[2], gamma, gamma0;
    COV(ZERO, sub, &gamma0);
    x = loc->x;
    for (i=0; i<len; i++, x += dim) {
      y = x + dim;
      for (j=i+1; j<len; j++) {
	NONSTATCOV(x, y, sub, &gamma);
	z[0] = cov->q[i];
	z[1] = cov->q[j];
	*v += logD(z, gamma0 - gamma);
      }
    }
    break;
  case LL_FULL: error("full MLE not available for Brown Resnick");
    break;
  case LL_SELECTION: NotProgrammedYet("LL_SELECTION");
  default : LL_UNKNOWN;
  }
}


void loglikelihoodBR(double *data, cov_model *cov, double *v) {
  loglikelihoodMaxstable(data, cov, HueslerReisslogD, v);
}

void loglikelihoodSchlather(double *data, cov_model *cov, double *v) {
  loglikelihoodMaxstable(data, cov, schlatherlogD, v);
}
