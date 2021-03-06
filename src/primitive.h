


/*
 Authors 
 Martin Schlather, schlather@math.uni-mannheim.de


 Copyright (C) 2015 -- 2017 Martin Schlather

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



#ifndef Primitives_H
#define Primitives_H 1

#define UNIT_EPSILON 1E-13
#include "Coordinate_systems.h"


void AngleMatrix(cov_model *cov, double *A);
void kappa_Angle(int i, cov_model *cov, int *nr, int *nc);
void Angle(double *x, cov_model *cov, double *v);
int checkAngle(cov_model *cov);
void rangeAngle(cov_model *cov, range_type* ra);
void invAngle(double *x, cov_model *cov, double *v);


void Bessel(double *x, cov_model *cov, double *v);
int initBessel(cov_model *cov, gen_storage *s);
void spectralBessel(cov_model *cov, gen_storage *s, double *e); 
int checkBessel(cov_model *cov);
void rangeBessel(cov_model *cov, range_type *ra);


/* Cauchy models */
void Cauchy(double *x, cov_model *cov, double *v);
void logCauchy(double *x, cov_model *cov, double *v, double *Sign);
void TBM2Cauchy(double *x, cov_model *cov, double *v);
void DCauchy(double *x, cov_model *cov, double *v);
void DDCauchy(double *x, cov_model *cov, double *v);
void InverseCauchy(double *x, cov_model *cov, double *v);
int checkCauchy(cov_model *cov);
void rangeCauchy(cov_model *cov, range_type *ra);
void coinitCauchy(cov_model *cov, localinfotype *li);
void DrawMixCauchy(cov_model *cov, double *random); 
double LogMixDensCauchy(double *x, double logV, cov_model *cov);


/* another Cauchy model */
void Cauchytbm(double *x, cov_model *cov, double *v);
void DCauchytbm(double *x, cov_model *cov, double *v);
void rangeCauchytbm(cov_model *cov, range_type *ra);


/* circular model */
void circular(double *x, cov_model *cov, double *v);
void Inversecircular(double *x, cov_model *cov, double *v);
void Dcircular(double *x, cov_model *cov, double *v);
int structcircular(cov_model *cov,  cov_model **);



void kappaconstant(int i, cov_model *cov, int *nr, int *nc);
void constant(double *x, cov_model *cov, double *v);
void nonstatconstant(double *x, double *y, cov_model *cov, double *v);
void rangeconstant(cov_model *cov, range_type* ra);
int checkconstant(cov_model *cov) ;


void covariate(double *x, cov_model *cov, double *v);
int checkcovariate(cov_model *cov);
void rangecovariate(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range);
void kappa_covariate(int i, cov_model *cov, int *nr, int *nc);

void fixStat(double *x, cov_model *cov, double *v);
void fix(double *x, double *y, cov_model *cov, double *v);
int checkfix(cov_model *cov);
void rangefix(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range);
void kappa_fix(int i, cov_model *cov, int *nr, int *nc);


/* coxgauss, cmp with nsst1 !! */
// see NonIsoCovFct.cc

/* cubic */
void cubic(double *x, cov_model *cov, double *v);
void Dcubic(double *x, cov_model *cov, double *v); 
void Inversecubic(double *x, cov_model *cov, double *v);


/* cutoff */
// see Hypermodel.cc

/* dagum */
void dagum(double *x, cov_model *cov, double *v);
void Inversedagum(double *x, cov_model *cov, double *v); 
void Ddagum(double *x, cov_model *cov, double *v);
void rangedagum(cov_model *cov, range_type* ra);
int checkdagum(cov_model *cov);
int initdagum(cov_model *cov, gen_storage *stor);


/*  damped cosine -- derivative of exponential:*/
void dampedcosine(double *x, cov_model *cov, double *v);
void logdampedcosine(double *x, cov_model *cov, double *v, double *Sign);
void Inversedampedcosine(double *x, cov_model *cov, double *v); 
void Ddampedcosine(double *x, cov_model *cov, double *v);
void rangedampedcosine(cov_model *cov, range_type* ra);
int checkdampedcosine(cov_model *cov);


/* De Wijsian */
void dewijsian(double *x, cov_model *cov, double *v);
void Ddewijsian(double *x, cov_model *cov, double *v);
void DDdewijsian(double *x, cov_model *cov, double *v);
void D3dewijsian(double *x, cov_model *cov, double *v);
void D4dewijsian(double *x, cov_model *cov, double *v);
void rangedewijsian(cov_model *cov, range_type* ra);
int checkdewijsian(cov_model *cov);
void Inversedewijsian(double *x, cov_model *cov, double *v); 
void coinitdewijsian(cov_model *cov, localinfotype *li);

/* De Wijsian */
void DeWijsian(double *x, cov_model *cov, double *v);
void rangeDeWijsian(cov_model *cov, range_type* ra);
void InverseDeWijsian(double *x, cov_model *cov, double *v); 


/* exponential model */
void exponential(double *x, cov_model *cov, double *v);
void logexponential(double *x, cov_model *cov, double *v, double *Sign);
void TBM2exponential(double *x, cov_model *cov, double *v);
void Dexponential(double *x, cov_model *cov, double *v);
void DDexponential(double *x, cov_model *cov, double *v);
void Inverseexponential(double *x, cov_model *cov, double *v);
void nonstatLogInvExp(double *x, cov_model *cov, double *left, double *right);
int initexponential(cov_model *cov, gen_storage *s);
void spectralexponential(cov_model *cov, gen_storage *s, double *e);
int checkexponential(cov_model *cov);
int hyperexponential(double radius, double *center, double *rx,
		     cov_model *cov, bool simulate, 
		     double ** Hx, double ** Hy, double ** Hr);
void coinitExp(cov_model *cov, localinfotype *li);
void ieinitExp(cov_model *cov, localinfotype *li);
void DrawMixExp(cov_model *cov, double *random);
double LogMixDensExp(double *x, double logV, cov_model *cov);
int init_exp(cov_model *cov, gen_storage *s); 
void do_exp(cov_model *cov, gen_storage *s);



int checklsfbm(cov_model *cov);
void rangelsfbm(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range);
void lsfbm(double *x, cov_model *cov, double *v);
void Dlsfbm(double *x, cov_model *cov, double *v);
void DDlsfbm(double *x, cov_model *cov, double *v);
void D3lsfbm(double *x, cov_model *cov, double *v); 
void D4lsfbm(double *x, cov_model *cov, double *v); 
void Inverselsfbm(double *x, cov_model *cov, double *v);


// Brownian motion 
void fractalBrownian(double *x, cov_model *cov, double *v);
void fractalBrownian(double *x, cov_model *cov, double *v, double *Sign);
void logfractalBrownian(double *x, cov_model *cov, double *v, double *Sign);
void DfractalBrownian(double *x, cov_model *cov, double *v);
void DDfractalBrownian(double *x, cov_model *cov, double *v);
void D3fractalBrownian(double *x, cov_model *cov, double *v);
void D4fractalBrownian(double *x, cov_model *cov, double *v);
void rangefractalBrownian(cov_model *cov, range_type* ra);
void ieinitBrownian(cov_model *cov, localinfotype *li);
void InversefractalBrownian(double *x, cov_model *cov, double *v);
int checkfractalBrownian(cov_model *cov);
int initfractalBrownian(cov_model *cov, gen_storage *s); 



/* FD model */
void FD(double *x, cov_model *cov, double *v);
void rangeFD(cov_model *cov, range_type* ra);



/* fractgauss */
void fractGauss(double *x, cov_model *cov, double *v);
void rangefractGauss(cov_model *cov, range_type* ra);

/*
void fix(double *x, cov_model *cov, double *v);
void fix_nonstat(double *x, double *y, cov_model *cov, double *v);
void covmatrix_fix(cov_model *cov, double *v);
char iscovmatrix_fix(cov_model *cov);
int checkfix(cov_model *cov) ;
*/
void rangefix(cov_model *cov, range_type* ra);

/* Gausian model */
void Gauss(double *x, cov_model *cov, double *v);
void logGauss(double *x, cov_model *cov, double *v, double *Sign);
void DGauss(double *x, cov_model *cov, double *v);
void DDGauss(double *x, cov_model *cov, double *v);
void D3Gauss(double *x, cov_model *cov, double *v);
void D4Gauss(double *x, cov_model *cov, double *v);
void spectralGauss(cov_model *cov, gen_storage *s, double *e);   
void DrawMixGauss(cov_model *cov, double *random);
double LogMixDensGauss(double *x, double logV, cov_model *cov);
void InverseGauss(double *x, cov_model *cov, double *v);
void nonstatLogInvGauss(double *x, cov_model VARIABLE_IS_NOT_USED *cov, 
			double *left, double *right);
int struct_Gauss(cov_model *cov, cov_model **);
int initGauss(cov_model *cov, gen_storage *s);
void do_Gauss(cov_model *cov, gen_storage *s) ; 
//void getMassGauss(double *a, cov_model *cov, double *kappas, double *m);
//void densGauss(double *x, cov_model *cov, double *v);
//void simuGauss(cov_model *cov, int dim, double *v);

void bcw(double *x, cov_model *cov, double *v);
//void logbcw(double *x, cov_model *cov, double *v, double *Sign);
void Dbcw(double *x, cov_model *cov, double *v);
void DDbcw(double *x, cov_model *cov, double *v);
int checkbcw(cov_model *cov);
void rangebcw(cov_model *cov, range_type* ra);
void coinitbcw(cov_model *cov, localinfotype *li);
void ieinitbcw(cov_model *cov, localinfotype *li);
void Inversebcw(double *x, cov_model *cov, double *v);


/* generalised fractal Brownian motion */
void genBrownian(double *x, cov_model *cov, double *v);
void loggenBrownian(double *x, cov_model *cov, double *v, double *Sign);
void InversegenBrownian(double *x, cov_model *cov, double *v) ;
int checkgenBrownian(cov_model *cov);
void rangegenBrownian(cov_model *cov, range_type* ra);


/* epsC */
void epsC(double *x, cov_model *cov, double *v);
void logepsC(double *x, cov_model *cov, double *v, double *Sign);
void DepsC(double *x, cov_model *cov, double *v);
void DDepsC(double *x, cov_model *cov, double *v);
int checkepsC(cov_model *cov);
void rangeepsC(cov_model *cov, range_type* ra);
void inverseepsC(double *x, cov_model *cov, double *v);


/* gencauchy */
void generalisedCauchy(double *x, cov_model *cov, double *v);
void loggeneralisedCauchy(double *x, cov_model *cov, double *v, double *Sign);
void DgeneralisedCauchy(double *x, cov_model *cov, double *v);
void DDgeneralisedCauchy(double *x, cov_model *cov, double *v);
int checkgeneralisedCauchy(cov_model *cov);
void rangegeneralisedCauchy(cov_model *cov, range_type* ra);
void coinitgenCauchy(cov_model *cov, localinfotype *li);
void ieinitgenCauchy(cov_model *cov, localinfotype *li);
void InversegeneralisedCauchy(double *x, cov_model *cov, double *v);


/* bivariate Cauchy bivariate genCauchy bivariate generalized Cauchy */

void biCauchy (double *x, cov_model *cov, double *v);
void DbiCauchy(double *x, cov_model *cov, double *v);
void DDbiCauchy(double *x, cov_model *cov, double *v);
void D3biCauchy(double *x, cov_model *cov, double *v);
void D4biCauchy(double *x, cov_model *cov, double *v);
void kappa_biCauchy(int i, cov_model *cov, int *nr, int *nc);
int checkbiCauchy(cov_model *cov);
void rangebiCauchy(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range);
void coinitbiCauchy(cov_model *cov, localinfotype *li);

/* gengneiting */
void genGneiting(double *x, cov_model *cov, double *v);
void DgenGneiting(double *x, cov_model *cov, double *v);
void DDgenGneiting(double *x, cov_model *cov, double *v);
void rangegenGneiting(cov_model *cov, range_type* ra);
int checkgenGneiting(cov_model *cov);


/* Gneiting's functions -- alternative to Gaussian */
// #define Sqrt2TenD47 0.30089650263257344820 /* approcx 0.3 ?? */
void Gneiting(double *x, cov_model *cov, double *v); 
//void InverseGneiting(double *x, cov_model *cov, double *v);
void DGneiting(double *x, cov_model *cov, double *v); 
void DDGneiting(double *x, cov_model *cov, double *v); 
int checkGneiting(cov_model *cov);
void rangeGneiting(cov_model *cov, range_type *range);


/* hyperbolic */
void hyperbolic(double *x, cov_model *cov, double *v); 
void loghyperbolic(double *x, cov_model *cov, double *v, double *Sign); 
void Dhyperbolic(double *x, cov_model *cov, double *v); 
int checkhyperbolic(cov_model *cov);
void rangehyperbolic(cov_model *cov, range_type* ra);



/* iaco cesare model */
void IacoCesare(double *x, cov_model *cov, double *v);
//void InverseIacoCesare(cov_model *cov); return 1.0; } 
void rangeIacoCesare(cov_model *cov, range_type* ra);
//int checkIacoCesare(cov_model *cov); 

void Kolmogorov(double *x, cov_model *cov, double *v);
  int checkKolmogorov(cov_model *cov);


/* local-global distinguisher */
void lgd1(double *x, cov_model *cov, double *v);
// void Inverselgd1(cov_model *cov); // Fehler in der Progammierung?
void Dlgd1(double *x, cov_model *cov, double *v);
void rangelgd1(cov_model *cov, range_type* ra);
int checklgd1(cov_model *cov);
//void Inverselgd1(cov_model *cov, double u);


/* mastein */
// see Hypermodel.cc


/* Whittle-Matern or Whittle or Besset ---- rescaled form of Whittle-Matern,
    see also there */ 
void Matern(double *x, cov_model *cov, double *v);
void NonStMatern(double *x, double *y, cov_model *cov, double *v);
void logNonStMatern(double *x, double*y, 
		    cov_model *cov, double *v, double *Sign);
void logMatern(double *x, cov_model *cov, double *v, double *Sign);
void DMatern(double *x, cov_model *cov, double *v);
void DDMatern(double *x, cov_model *cov, double *v);
void D3Matern(double *x, cov_model *cov, double *v);
void D4Matern(double *x, cov_model *cov, double *v);
int checkMatern(cov_model *cov);
void rangeWM(cov_model *cov, range_type* ra);
void ieinitWM(cov_model *cov, localinfotype *li);
void coinitWM(cov_model *cov, localinfotype *li);
double densityMatern(double *x, cov_model *cov);
int initMatern(cov_model *cov, gen_storage *s);
void spectralMatern(cov_model *cov, gen_storage *s, double *e); 
void InverseMatern(double *x, cov_model *cov, double *v);


/* nugget effect model */
void nugget(double *x, cov_model *cov, double *v);
void covmatrix_nugget(cov_model *cov, double *v);
char iscovmatrix_nugget(cov_model *cov);
void Inversenugget(double *x, cov_model *cov, double *v); 
int check_nugget(cov_model *cov);
void range_nugget(cov_model *cov, range_type *range);


/* penta */
void penta(double *x, cov_model *cov, double *v);
void Dpenta(double *x, cov_model *cov, double *v); 
void Inversepenta(double *x, cov_model *cov, double *v);


/* power model */ 
void power(double *x, cov_model *cov, double *v);
void Inversepower(double *x, cov_model *cov, double *v); 
void TBM2power(double *x, cov_model *cov, double *v);
void Dpower(double *x, cov_model *cov, double *v);
int checkpower(cov_model *cov);
void rangepower(cov_model *cov, range_type* ra);


/* qexponential -- derivative of exponential */
void qexponential(double *x, cov_model *cov, double *v);
void Inverseqexponential(double *x, cov_model *cov, double *v);
void Dqexponential(double *x, cov_model *cov, double *v);
void rangeqexponential(cov_model *cov, range_type* ra);

void kappa_rational(int i, cov_model *cov, int *nr, int *nc);
void rational(double *x, cov_model *cov, double *v);
int checkrational(cov_model *cov);
void rangerational(cov_model *cov, range_type* ra);

/* spherical model */ 
void spherical(double *x, cov_model *cov, double *v);
void Inversespherical(double *x, cov_model *cov, double *v);
void TBM2spherical(double *x, cov_model *cov, double *v);
void Dspherical(double *x, cov_model *cov, double *v);
void DDspherical(double *x, cov_model *cov, double *v);
int structspherical(cov_model *cov, cov_model **);
int initspherical(cov_model *cov, gen_storage *s);
void dospherical(cov_model *cov, gen_storage *s);


/* stable model */
void stable(double *x, cov_model *cov, double *v);
void logstable(double *x, cov_model *cov, double *v, double *Sign);
void Dstable(double *x, cov_model *cov, double *v);
void DDstable(double *x, cov_model *cov, double *v);
int checkstable(cov_model *cov);  
void rangestable(cov_model *cov, range_type* ra);
void coinitstable(cov_model *cov, localinfotype *li);
void ieinitstable(cov_model *cov, localinfotype *li);
void Inversestable(double *x, cov_model *cov, double *v);
void nonstatLogInversestable(double *x, cov_model *cov, double *L, double *R);


/* SPACEISOTROPIC stable model for testing purposes only */
void stableX(double *x, cov_model *cov, double *v);
void DstableX(double *x, cov_model *cov, double *v);


// bivariate exponential or bivariate stable model
void kappa_biStable(int i, cov_model *cov, int *nr, int *nc);
void biStable (double *x, cov_model *cov, double *v);
void DbiStable (double *x, cov_model *cov, double *v);
void DDbiStable (double *x, cov_model *cov, double *v);
void D3biStable (double *x, cov_model *cov, double *v);
void D4biStable (double *x, cov_model *cov, double *v);
int checkbiStable(cov_model *cov);
void rangebiStable(cov_model *cov, range_type *range);
int initbiStable(cov_model *cov, gen_storage *stor);
void coinitbiStable(cov_model *cov, localinfotype *li);

/* Stein */
// see Hypermodel.cc

/* stein space-time model */
void kappaSteinST1(int i, cov_model *cov, int *nr, int *nc);
void SteinST1(double *x, cov_model *cov, double *v);
int initSteinST1(cov_model *cov, gen_storage *s);
void spectralSteinST1(cov_model *cov, gen_storage *s, double *e);
void rangeSteinST1(cov_model *cov, range_type* ra);
int checkSteinST1(cov_model *cov);  


/* wave */
void wave(double *x, cov_model *cov, double *v);
void Inversewave(double *x, cov_model *cov, double *v);
int initwave(cov_model *cov, gen_storage *s);
void spectralwave(cov_model *cov, gen_storage *s, double *e); 

/* Whittle-Matern or Whittle or Besset */ 
void Whittle(double *x, cov_model *cov, double *v);
void NonStWhittle(double *x, double *y, cov_model *cov, double *v);
void logNonStWhittle(double *x, double*y, 
		    cov_model *cov, double *v, double *Sign);
void logWhittle(double *x, cov_model *cov, double *v, double *Sign);
void TBM2Whittle(double *x, cov_model *cov, double *v);
void DWhittle(double *x, cov_model *cov, double *v);
void DDWhittle(double *x, cov_model *cov, double *v);
void D3Whittle(double *x, cov_model *cov, double *v);
void D4Whittle(double *x, cov_model *cov, double *v);
int checkWM(cov_model *cov);
double densityWhittle(double *x, cov_model *cov);
int initWhittle(cov_model *cov, gen_storage *s);
void spectralWhittle(cov_model *cov, gen_storage *s, double *e); 
void DrawMixWM(cov_model *cov, double *random);
double LogMixDensW(double *x, double logV, cov_model *cov);
void InverseWhittle(double *x, cov_model *cov, double *v);

double WM(double x, double nu, double factor);
double logWM(double x, double nu1, double nu2, double factor);
double DWM(double x, double nu, double factor);

void kappa_biGneiting(int i, cov_model *cov, int *nr, int *nc);
void biGneiting(double *x, cov_model *cov, double *v);
void DbiGneiting(double *x, cov_model *cov, double *v);
void DDbiGneiting(double *x, cov_model *cov, double *v);
int checkbiGneiting(cov_model *cov);
void rangebiGneiting(cov_model *cov, range_type* ra);
int initbiGneiting(cov_model *cov, gen_storage *s);


/* User defined model */
void kappaUser(int i, cov_model *cov, int *nr, int *nc);
void User(double *x, cov_model *cov, double *v);
void UserNonStat(double *x, double *y, cov_model *cov, double *v);
void DUser(double *x, cov_model *cov, double *v);
void DDUser(double *x, cov_model *cov, double *v);
int checkUser(cov_model *cov);
void rangeUser(cov_model *cov, range_type *ra);
bool TypeUser(Types required, cov_model *cov, int depth);

/*
void userMatrix(double *x, cov_model *cov, double *v);
int checkuserMatrix(cov_model *cov);
void rangeuserMatrix(cov_model *cov, range_type* ra);
void kappa_userMatrix(int i, cov_model *cov, int *nr, int *nc);
*/

//void Nonestat(double *x, cov_model *cov, double *v);
//void Nonenonstat(double *x, double *y, cov_model *cov, double *v);
//void rangeNone(cov_model *cov, range_type* ra);


void kappa_biWM(int i, cov_model *cov, int *nr, int *nc);
void biWM2(double *x, cov_model *cov, double *v);
void biWM2D(double *x, cov_model *cov, double *v);
void biWM2DD(double *x, cov_model *cov, double *v);
void biWM2D3(double *x, cov_model *cov, double *v);
void biWM2D4(double *x, cov_model *cov, double *v);
int checkbiWM2(cov_model *cov);
void rangebiWM2(cov_model *cov, range_type* ra);
int initbiWM2(cov_model *cov, gen_storage *s);
void coinitbiWM2(cov_model *cov, localinfotype *li);


void rangebiWM2(cov_model *cov, range_type* ra);


void kappa_parsWM(int i, cov_model *cov, int *nr, int *nc);
void parsWM(double *x, cov_model *cov, double *v);
void parsWMD(double *x, cov_model *cov, double *v);
int checkparsWM(cov_model *cov);
void rangeparsWM(cov_model *cov, range_type* ra);

 
void Mathminus(double *x, cov_model *cov, double *v);
void Mathplus(double *x, cov_model *cov, double *v);
void Mathmult(double *x, cov_model *cov, double *v);
void Mathdiv(double *x, cov_model *cov, double *v);
void Mathc(double *x, cov_model *cov, double *v);
void rangec(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range);
int check_c(cov_model *cov);
void Mathis(double *x, cov_model *cov, double *v);
void rangeMathIs(cov_model *cov, range_type *range);
void Mathbind(double *x, cov_model *cov, double *v);
int check_bind(cov_model *cov);
void proj(double *x, cov_model *cov, double *v);
int checkproj(cov_model *cov);
void rangeproj(cov_model VARIABLE_IS_NOT_USED *cov, range_type *range);


void kappa_math(int i, cov_model *cov, int *nr, int *nc);
int checkMath(cov_model *cov);
void rangeMath(cov_model *cov, range_type *range);

#define MATH_DEFAULT_0(VARIAB)						\
  int i,								\
    variables = VARIAB;							\
  double w[MAXPARAM];							\
    for (i=0; i<variables; i++) {					\
      cov_model *sub = cov->kappasub[i];				\
      if (sub != NULL) {						\
	COV(x, sub, w + i);						\
      } else {  w[i] = P0(i);	}					\
    }									\
  // printf("sin %f w=%f;%f %f %d %d nr = %d %d\n", x[0], w[0], w[1], P0(1), variables, p[1], cov->nrow[0] > 0, cov->nrow[1] > 0);

#define MATH_DEFAULT MATH_DEFAULT_0(CovList[cov->nr].kappas)

#endif /* Primitives_H*/
