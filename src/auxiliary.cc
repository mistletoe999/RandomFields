//#define DEBUG 1

/* 
 Authors
 Martin Schlather, schlather@math.uni-mannheim.de

 Collection of auxiliary functions

 Copyright (C) 2001 -- 2017 Martin Schlather, 

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


#include <Rmath.h>
#include <unistd.h>
 
//#include <curses.h>
#include <R_ext/Lapack.h>
#include <R_ext/Linpack.h>
#include <Rdefines.h>
#include "RF.h"

// important check !!
#ifndef SCHLATHERS_MACHINE
#ifdef SHOW_ADDRESSES
SHOW_ADRESSES IS NOT ALLOWED
#endif
#ifdef RANDOMFIELDS_DEBUGGING
RANDOMFIELDS_DEBUGGING IS NOT ALLOWED
#endif
#endif



double intpow(double x, int p) {
  double res = 1.0;
  if (p < 0) {
    p = -p;
    x = 1.0 / x;
  } 
  while (p != 0) {
    if (p % 2 == 1) res *= x;
    x *= x;
    p /= 2;
  }
  return res;
}



void AxResType(double *A, double *x, int nrow, int ncol, double *y) {
  int i,j,k;
  for (i=0; i<nrow; i++) y[i]=0.0;
  for (k=i=0; i<ncol; i++) { 
    for (j=0; j<nrow; j++) {
      y[j] += A[k++] * x[i];
    }
  }
}



double getMinimalAbsEigenValue(double *Aniso, int dim) {
  double dummy, 
    min = RF_INF, 
    *SICH = NULL, 
    *D = NULL, 
    *work = NULL;
  int dd, Err,
    err = NOERROR,
    *iwork = NULL,
     dimSq = dim * dim,
    optim_work = 12 * dim;

  if ((D =(double *) MALLOC(sizeof(double) * dim))==NULL ||
      (work = (double *) MALLOC(sizeof(double) * optim_work))==NULL ||
      (iwork = (int *) MALLOC(sizeof(int) * 8 * dim))==NULL ||
      (SICH =(double *) MALLOC(sizeof(double) * dimSq))==NULL) {
    err=ERRORMEMORYALLOCATION; goto ErrorHandling;
  }
  
  MEMCOPY(SICH, Aniso, sizeof(double) * dimSq);
  F77_CALL(dgesdd)("N", &dim, &dim, SICH, &dim, D, NULL, &dim, NULL,
		   &dim, work, &optim_work, iwork, &Err);
  if (Err != 0) GERR("SVD for anisotropy matrix failed.");
  for (dd = 0; dd < dim; dd++) {
    dummy = FABS(D[dd]);
    if (dummy < min) min = dummy;
  } 

 ErrorHandling:
  FREE(D);
  FREE(SICH);
  FREE(work);
  FREE(iwork);
  if (err != NOERROR) XERR(err);

  return min;
}


double getDet(double *Aniso, int dim) {
  double  
    det = 1.0, 
    *SICH = NULL, 
    *D = NULL, 
    *work = NULL;
  int dd, Err,
    err = NOERROR,
    *iwork = NULL,
     dimSq = dim * dim,
    optim_work = 12 * dim;

  if ((D =(double *) MALLOC(sizeof(double) * dim))==NULL ||
      (work = (double *) MALLOC(sizeof(double) * optim_work))==NULL ||
      (iwork = (int *) MALLOC(sizeof(int) * 8 * dim))==NULL ||
      (SICH =(double *) MALLOC(sizeof(double) * dimSq))==NULL) {
    err=ERRORMEMORYALLOCATION; goto ErrorHandling;
  }
  
  MEMCOPY(SICH, Aniso, sizeof(double) * dimSq);
  F77_CALL(dgesdd)("N", &dim, &dim, SICH, &dim, D, NULL, &dim, NULL,
		   &dim, work, &optim_work, iwork, &Err);
  if (Err != 0) GERR("SVD for anisotropy matrix failed.");
  for (dd = 0; dd < dim; det *= D[dd++]);

 ErrorHandling:
  FREE(D);
  FREE(SICH);
  FREE(work);
  FREE(iwork);
  if (err != NOERROR) XERR(err);

  return det;
}

double detU(double *C, int dim) {
  /* ACHTUNG!! detU zerstoert !!! */
  int i, info, 
//    job = 10,
    dimP1 = dim + 1,
    dimsq = dim * dim;
  double det = 1.0;

  F77_CALL(dpofa)(C, &dim, &dim, &info); // C i s now cholesky
  if (info != 0) {
    ERR("detU: matrix does not seem to be strictly positive definite");
  }
  for (i=0; i<dimsq; i+=dimP1) det *= C[i];
  return det * det;
}

void det_UpperInv(double *C, double *det, int dim) {
  int i, info, 
    job = 01,
    dimP1 = dim + 1,
    dimsq = dim * dim;
  F77_CALL(dpofa)(C, &dim, &dim, &info); // C i s now cholesky
  if (info != 0) {
    ERR("det_UpperInv: dpofa failed -- is matrix positive definite?");
  }

  double Det = 1.0;
  for (i=0; i<dimsq; i+=dimP1) Det *= C[i];
  *det = Det * Det;

  F77_CALL(dpodi)(C, &dim, &dim, det, &job); // C is now Cinv
}



/*
void InvChol(double *C, int dim) {
  int i, info, ii, endfor,
    job = 01,
    dimP1 = dim + 1,
    dimsq = dim * dim;
  long ve, ho;
  double Det = 1.0;
  F77_CALL(dpofa)(C, &dim, &dim, &info); // C is now cholesky
  if (info != 0) ERR("InvChol: Inversion failed, bad functions\n");
  for (i=0; i<dimsq; i+=dimP1) Det *= C[i];
  Det = Det * Det;
  F77_CALL(dpodi)(C, &dim, &dim, &Det, &job); // C is now Cinv
  for (ii=dim, i=0; ii>0; i+=dimP1, ii--) {  // filling lower half
      endfor = i + ii;
      for (ve = i + 1, ho = i + dim; ve < endfor;
	   ve++, ho += dim) {
	C[ve] = C[ho];
      }
  }
}
*/
 
 



void memory_copy(void *dest, void *src, int bytes) {
  int i, 
    len = bytes / sizeof(int),
    *d = (int*) dest,
    *s = (int *) src;
  if ((len * (int) sizeof(int)) != bytes) {
    ERR("size not a multiple of int");
  }
  for (i=0; i<len; i++) d[i] = s[i];
}


SEXP distInt(SEXP XX, SEXP N, SEXP Genes) {
  int i,j, k, di, diff, *x, *y, ve, ho, endfor,
    *X = INTEGER(XX),
    n = INTEGER(N)[0],
    nP1 = n + 1,
    genes = INTEGER(Genes)[0];
 
  SEXP Dist;
  PROTECT(Dist = allocMatrix(REALSXP, n, n));
  double *dist = REAL(Dist);

  x = y = X;
  for (j=0, i=0;  j<n;  i += nP1, j++, y += genes) {
    dist[i] = 0.0;
    endfor = i + (n - j);
    for (ve = i + 1, ho = i + n, x = y + genes; 
         ve < endfor; 
	 ve++, ho += n) {
      for (di=0.0, k=0; k<genes; k++, x++) {
	diff = *x - y[k];
	di += diff * diff;
      }
      dist[ve] = dist[ho] = SQRT((double) di);
    }
  }
  UNPROTECT(1);
  return Dist;
}


SEXP vectordist(SEXP V, SEXP DIAG){
  bool notdiag = !LOGICAL(DIAG)[0];
  int 
    d, dr, 
    rows = nrows(V), 
    cols = ncols(V),
    rescols = (int) (cols * (cols - 1 + 2 * (int) !notdiag) / 2);
  double *v1, *v2, *end, *Dist,
    *v = REAL(V);
  
  end = v + cols * rows; 
  SEXP DIST;
  
  PROTECT(DIST = allocMatrix(REALSXP, rows, rescols));
  Dist = REAL(DIST);

  for (dr=0, v1=v; v1<end; v1+=rows) { // loop is one to large??
    v2 = v1;
    if (notdiag) {
       v2 += rows;
    }
    for (; v2<end; ) {
      for (d=0; d<rows; v2++) {
	Dist[dr++] = v1[d++] - *v2;
      }
    }
  }
  UNPROTECT(1);
  return DIST;
}

int is_positive_definite(double *C, int dim) {
    int err,
	bytes = sizeof(double) * dim * dim;
  double *test;
  test = (double*) MALLOC(bytes);
  MEMCOPY(test, C, bytes);
  F77_CALL(dpofa)(test, &dim, &dim, &err); 
  UNCONDFREE(test);
  return(err == 0);
}


int xMatch(char *name, char **list, unsigned int llen)  {
  unsigned int ln, nr;
  // == -1 if no matching function is found
  // == -2 if multiple matching fctns are found, without one matching exactly
  // if more than one match exactly, the last one is taken (enables overwriting 
  // standard functions)
  // see also GetModelNr !

  nr=0;
  ln=(unsigned int) STRLEN(name);
  while ((nr < llen) && STRNCMP(name, list[nr], ln)) {
    nr++;
  }
  if (nr < llen) { 
    // a matching method is found. Are there other methods that match?
    unsigned int j; 
    if (ln == (unsigned int) STRLEN(list[nr])) return nr; // exact matching
    j = nr + 1; // if two or more methods have the same name the last one is
    //          taken; stupid here, but nice in GetCovNr
    while (j < llen && STRNCMP(name, list[j], ln)) j++;
    if (j < llen) {
      if (ln == (unsigned int) STRLEN(list[j])) return j; //exact matching 
      else return MULTIPLEMATCHING; // multiple matching
    }
    return nr;
  } else return NOMATCHING; // unmatched
}


int CeilIndex(double x, double *cum, int size) {  
   // der kleinste index i so das cum[i] >= x --- sollte das gleiche sein
  // wie searchFirstGreater

  int mitte,
    min = 0,
    max = size - 1;
  while (min < max) {
    mitte = 0.5 * (min + max);
    if (cum[mitte] >= x) max = mitte;
    else min = mitte + 1;
  }
  //  printf("%f < %f <= %f\n", cum[min-1], x, cum[min]);
  assert((min==0) || x > cum[min-1]);
  assert(x <= cum[min] && (min==0 || x > cum[min-1]));


  return min;
} 

/*
int searchFirstGreater(double *v, int len, double z) {
  // assumes the list has non decreasing values
  
  int 
    firsti = 0,
    lasti = len - 1,
    i = len / 2;
  
  if (z > v[lasti]) {
    BUG;
  }
  if (z <= v[firsti]) return firsti;
  
  while (firsti < lasti) {
    if (v[i] < z) {
      firsti = i + 1;
      i = i + (int) ((lasti - i + 1) / 2);
    } else { // v[i] <= z
      lasti = i;
      i = i - (int) ((i - firsti + 1) / 2);
    }
  }
    
  assert((i==0) || z > v[i-1]);
  assert(z <= v[i]);

  //  printf("%f < %f <= %f\n", v[i-1], z, v[i]);//
}
*/


/*
double searchInverse(isofct fct, double start, double *value,
		     double releps) {	       
  while (fct(start, cov) > value) start *= 2.0;    
  while (fct(start, cov) < value) start *= 0.5;
 
  double x = start,
    step = start;
  releps *= start;
  while (step > releps) {
    step *= 0.5;
    if (fct(step, cov) < value) x -= step; else x += step;
  }
}
*/

double searchInverse(covfct fct, cov_model *cov, 
		     double start, double value, double releps) {
  double v;
  fct(&start, cov, &v);
  while (v > value) {start *= 2.0; fct(&start, cov, &v);}
  while (v < value) {start *= 0.5; fct(&start, cov, &v);}
 
  double x = start,
    step = start;
  releps *= step;
  while (step > releps) {
    step *= 0.5;
    fct(&step, cov, &v);
    if (v < value) x -= step; else x += step;
  }
  return x;
}

double searchInverse(covfct fct, cov_model *cov, 
		     double start, double min, double value, double releps) {
  double v;
  assert(start > min);
  fct(&start, cov, &v);
  while (v > value) {start = 2.0 * (start - min) + min; fct(&start, cov, &v);}
  while (v < value) {start = 0.5 * (start - min) + min; fct(&start, cov, &v);}
 
  double x = start,
    step = start - min;
  releps *= step;
  while (step > releps) {
    step *= 0.5;
    fct(&step, cov, &v);
    if (v < value) x -= step; else x += step;
  }
  return x;
}

//double gamma(double x) { BUG; } // use gammafn instead


double incomplete_gamma(double start, double end, double s) {
  // int_start^end t^{s-1} e^{-t} \D t

  // print("incomplete IN s=%f e=%f s=%f\n", start, end, s);

  double
    v = 0.0, 
    w = 0.0;

  if (s <= 1.0) {
    if (start == 0.0) return RF_NA;
  }
  
  double 
    e_start = EXP(-start),
    e_end = EXP(-end),
    power_start = POW(start, s),      
    power_end = end < RF_INF ? POW(end, s) : 0,
    factor = 1.0; 
  
  
  while (s < 0.0) {
    factor /= s;    
    v +=  factor * (power_end * e_end - power_start * e_start);
    power_start *= start;
    if (end < RF_INF) power_end *= end;
    s += 1.0;
  }
  
  w = pgamma(start, s, 1.0, false, false);  // q, shape, scale, lower, log
  if (R_FINITE(end)) w -= pgamma(end, s, 1.0, false, false);

  //  print("incomplete s=%f e=%f s=%f v=%f g=%f w=%f\n", start, end, s, v, gammafn(s), w);

  return v + gammafn(s) * w * factor;
}


int addressbits(void VARIABLE_IS_NOT_USED *addr) {
#ifndef SCHLATHERS_MACHINE 
  return -1;
#else
  double x = (intptr_t) addr,
    cut = 1e9;
  x = x - TRUNC(x / cut) * cut;
  return (int) x;
#endif

}



bool LOCAL_DEBUG = false;
void start_debug() { LOCAL_DEBUG = true; }
void end_debug() { LOCAL_DEBUG = false; }


void Abbreviate(char *Old, char *abbr) {
  char *old = Old;
  if (old[0] == '.') old++;
  int 
    len = GLOBAL.fit.lengthshortname / 3,
    nold = STRLEN(old),
    nabbr = len - 1;

  if (nold <= len) {
    abbr[len] = '\0';
    STRCPY(abbr, old);
    //  printf(">%s**%s<\n", Old, abbr);
    return;
  }
  abbr[0] = old[0];
  abbr[len] = '\0';
  while (nabbr >= 1 && nabbr < nold) { 
    char b = old[nold];
    if (b=='a' || b=='A' || b=='e' || b=='E' || b=='i' || b=='I' ||
        b =='o' || b=='O' || b=='u' || b=='U') nold--;
    else abbr[nabbr--] = old[nold--];
  }
  if (nabbr > 1) {
    assert(nabbr==0 || nold == nabbr);
    for (int i=2; i<=nold; i++) abbr[i] = old[i];
  }
  
  //printf(">%s--%s<\n", Old, abbr);

}
  
  
