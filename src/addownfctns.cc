#include <math.h>  
#include <stdio.h>  
#include <stdlib.h>
#include <sys/timeb.h>
#include <assert.h>
#include <string.h>
#include "RFsimu.h"
#include "RFCovFcts.h"
#include "MPPFcts.h"
#include <unistd.h>

#include "addownfctns.h"


/* In the following an example is given for an extension of 
   a current implementation of a covariance function.
 
   If stablelocal and stableS are coded correctly, then
   a stable random field can be generated by Stein's 
   (2002) local circulant embedding method using

   GaussRF(...., model="stable", method="local")

   Probably, newcheckstable should also be changed.

*/



void addownfunctions() {
/* compare with InitModelList() in RFinitNerror.cc!
   actually the definition of "stable" in InitModelList is overwritten,
   cf. GetModelNr(char **name, int *n, int *nr) in RFgetNset.cc

   Note that models with overwritten names may be called only by *full* name.
   If unhappy, choose any new name instead of overwritting an old one!

   See RFgetNset.cc or RFsimu.h for the definition of IncludeModel, addCov,
   addTBM, etc.
*/
  //  NEW !!!  the following line make the call of PrintModelList()
  // unnecessary, so 
  //         library(RandomFields); .C("addownfunctions")
  // is sufficient.
  if (currentNrCov==-1) InitModelList(); 

  // new stable
/*
  int nr;
  nr=IncludeModel("stable", // name that can also be chosen freely
		  1,
		  newcheckstable, // NEW
		  FULLISOTROPIC, false, 
		  infostable, rangestable);
  addCov(nr,stable, Dstable, Scalestable);
  // addLocal(...);
  addTBM(nr, NULL,TBM3stable, CircEmbed, NULL);
*/

/*
  int nr, i=1;
  char stable[]="stable";

  // new stable
  GetModelNr(&stable, &i, &nr); // implicit call of InitModelList
  ModifyModel(nr, newcheckstable, // NEW
              infostable, rangestable);
  addCov(nr,stable,Scalestable,
	 stablelocal, // NEW
	 stableS // NEW
    );
*/

}


