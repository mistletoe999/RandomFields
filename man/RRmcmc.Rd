\name{RRmcmc}
\alias{RRmcmc}
\title{RRmcmc}
\description{
  \command{RRmcmc} draws a random sample from the
  modulus of any given function (provided the integral is finite)
}
\usage{
RRmcmc(phi, mcmc_n, sigma, normed, maxdensity, rand.loc, gibbs)
}
\arguments{
  \item{phi}{an arbitrary integrable function
  }
  \item{mcmc_n}{
    positve integer. Every \code{mcmc_n}th element of the MCMC chain
    is returned.
  }
  \item{sigma}{positive real number.
    The MCMC update is done by adding a normal variable with
    standard deviation \code{sigma}.
  }
  
  \item{normed}{logical.
    Only used if the value of the density is calculated.
    If \code{FALSE} the unnormed value given by \code{phi} is returned.

    Default: \code{FALSE}
  }
  \item{maxdensity}{positive real number.
    the given density is truncated at the \code{maxdensity}.

    Default: 1000
  }
  
  \item{rand.loc}{logical. Internal. Do not change the value.
    
   Default: \code{FALSE}
   }
 \item{gibbs}{logical. If \code{TRUE} only one component is updated at a
   time.
   
   Default: \code{FALSE}
  }
}


\details{
 \command{\link{RRmcmc}} returns an object of class \code{\link[=RMmodel-class]{RMmodel}}.
}
\note{
 The use of \command{RRmcmc} is completely on the risk of the user. There is no
 way to check whether the integral of the modulus is finite.
}

\author{Martin Schlather, \email{schlather@math.uni-mannheim.de} \url{http://ms.math.uni-mannheim.de/de/publications/software}
}
\seealso{
  \command{\link{RMmodel}},
  \link{RR},
  \command{\link{RRdistr}}
   \command{\link{RMuser}}
}

\examples{
RFoptions(seed=0) ## *ANY* simulation will have the random seed 0; set
##                   RFoptions(seed=NA) to make them all random again
## here a model with random scale parameter
\dontshow{StartExample()}
## not exponential, but the Laplace dstribution as symmetry is assumed
z <- RFrdistr(RRmcmc(RMexp(), sigma=1), n=10000)
hist(z, 100, freq=FALSE)
curve(0.5 * exp(-abs(x)), add=TRUE, col="blue") ## Laplace distribution

\dontshow{FinalizeExample()}
}

\keyword{spatial}
\keyword{models}

