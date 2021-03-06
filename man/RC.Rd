\name{Constants}
\alias{constants}
\alias{RC}
\alias{RC_ISOTROPIC}
\alias{RC_ISONAMES}
\alias{RC_TYPENAMES}
\alias{RC_DOMAIN_NAMES}
\alias{RC_MONOTONE_NAMES}
\alias{RC_SPACEISOTROPIC}
\alias{RC_CARTESIAN_COORD}
\alias{RC_GNOMONIC_PROJ}
\alias{RC_ORTHOGRAPHIC_PROJ}
\alias{RC_EARTH_COORDS}
\alias{RC_SPHERICAL_COORDS}
\alias{RC_OPTIMISER_NAMES}
\alias{RC_NLOPTR_NAMES}
\alias{RC_LIKELIHOOD_NAMES}

\title{Constants used in RandomFields (RC constants)}
\description{
  Several constants are provided that might make
  the use of some functions easier, e.g. \command{\link{RFgetModelNames}}
}
\value{
  \code{RC_TYPENAMES = c("tail correlation function", "positive definite",
    "variogram", "negative definite", "process", 
    "method for Gauss process", "method for Brown-Resnick process",
    "shifted shape function",
    "distribution family", "shape function", "trend", "interface",
    "undefined", "other type")}

  \code{RC_DOMAIN_NAMES = c("single variable", "kernel", "framework dependent", "mismatch")
  }

  \code{RC_ISONAMES = c("isotropic", "space-isotropic", "zero-space-isotropic",
    "vector-isotropic", "symmetric", "cartesian system",
    "earth system", "spherical system", "cylinder system",
    "non-dimension-reducing", "parameter dependent",
    "<mismatch>")
  }

  \code{RC_MONOTONE_NAMES = c("mismatch in monotonicity",
                 "submodel dependent monotonicity",
                "previous model dependent monotonicity",
                "parameter dependent monotonicity",
                "not monotone", "monotone", "Gneiting-Schaback class", 
                "normal mixture", "completely monotone", "Bernstein")
  }
 
  \code{RC_ISOTROPIC} gives the numerical code for option
  \code{"isotropic"}

  
  \code{RC_SPACEISOTROPIC} gives the numerical code for option
  \code{"space-isotropic"}

  \code{RC_CARTESIAN_COORD} gives the numerical code for option
  \code{"cartesian system"}

  \code{RC_GNOMONIC_PROJ} gives the numerical code for
  the gnomonic projection, see also \code{zenit} in \code{\link{RFoptions}}.

  \code{RC_ORTHOGRAPHIC_PROJ} gives the numerical code for
  the  orthographic
  projection, see also \code{zenit} in \code{\link{RFoptions}}.

  \code{RC_EARTH_COORDS} gives the numerical code for option
  \code{"earth coordinates"}

  \code{RC_SPHERICAL_COORDS} gives the numerical code for option
  \code{"earth coordinates"}

  \code{RC_OPTIMISER_NAMES} and \code{RC_NLOPTR_NAMES} give the
  names for the options \code{optimiser} and \code{algorithm},
  respectively, \code{RFfitoptimiser}.

  \code{RC_LIKELIHOOD_NAMES = c("auto", "full", "composite",
    "tesselation")}
  gives the names of the ML variants: (i) internal
  choice according to the
  number of data, (ii) full likelihood, (iii) (pairwise) composite likelihood,
  and (iv) composite likelihood based on a tesselation of the space.
    
}


\seealso{
  \link{RF},   \link{RM},  \link{RP}, \link{RR}, \link{R.},
  \command{\link{RFgetModelNames}},
  \link{RMmodelgenerator-class},
  \command{\link{RMtrafo}}
}
\author{Martin Schlather, \email{schlather@math.uni-mannheim.de}
 \url{http://ms.math.uni-mannheim.de/de/publications/software}
}

\keyword{spatial}


\examples{
RFoptions(seed=0) ## *ANY* simulation will have the random seed 0; set
##                   RFoptions(seed=NA) to make them all random again

RC_ISONAMES
RC_ISONAMES[RC_ISOTROPIC:RC_CARTESIAN_COORD + 1]
RFgetModelNames(isotropy=RC_ISONAMES[RC_ISOTROPIC:RC_CARTESIAN_COORD + 1])
\dontshow{FinalizeExample()}
}

