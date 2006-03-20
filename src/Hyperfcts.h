#ifndef Hyperfcts_H
#define Hyperfcts_H 1

double co(double *x, double *p);
int getcutoffparam_co(covinfo_type *kc,  param_type q, int instance);
bool alternativeparam_co(covinfo_type *kc, int instance);
void range_co(int dim, int *index, double* range);
void info_co(double *p, int *maxdim, int *CEbadlybehaved);
int checkNinit_co(covinfo_arraytype keycov, covlist_type covlist, 
		  int remaining_covlistlength, SimulationType method, 
		  bool anisotropy);

double Stein(double *x, double *p);
int getintrinsicparam_Stein(covinfo_type *kc, param_type q, int instance);
bool alternativeparam_Stein(covinfo_type *kc, int instance);
void range_Stein(int dim, int *index, double* range);
void info_Stein(double *p, int *maxdim, int *CEbadlybehaved);
int checkNinit_Stein(covinfo_arraytype keycov, covlist_type covlist, 
		     int remaining_covlistlength, SimulationType method, 
		     bool anisotropy);

double MaStein(double *x, double *p);
void range_MaStein(int dim, int *index, double* range);
void info_MaStein(double *p, int *maxdim, int *CEbadlybehaved);
int checkNinit_MaStein(covinfo_arraytype keycov, covlist_type covlist, 
		       int remaining_covlistlength, SimulationType metho, 
		       bool anisotropyd);

#endif /* Hyperfcts_H */
 
