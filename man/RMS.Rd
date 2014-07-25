\name{RMS}
\alias{RMS}
\title{Scaling operator}
\description{
 \command{\link{RMS}} is an operator that modifies the variance and the
 coordinates or distances of a submodel \eqn{\phi} by 
 \deqn{C(h) = v * phi(A*h/s).}
 
 \bold{Most users will never call \command{\link{RMS}} directly, see the
 details.}
}
\usage{
RMS(phi, var, scale, Aniso, proj, anisoT)
}
\arguments{
 \item{phi}{submodel}
 \item{var}{is the optional variance parameter \eqn{v},}
 \item{scale}{scaling parameter \eqn{s} which is positive}
 \item{Aniso}{the optional anisotropy matrix \eqn{A}, multiplied from
 the left, or an arbitrary function
 } 
 \item{proj}{is the optional projection vector which defines a diagonal
 matrix of zeros and ones and \code{proj} gives the 
 positions of the ones (integer values between 1 and teh dimension of
 \eqn{x}).}
 \item{anisoT}{the transpose of the anisotropy matrix \eqn{A}, multiplied from the right
 }
}
\value{
 \command{\link{RMS}} returns an object of class \code{\link[=RMmodel-class]{RMmodel}}.
}
\details{
 The call in the usage section is equivalent to 
 \code{phi(..., var, scale, anisoT, Aniso, proj)}, where \code{phi} has
 to be replaced by a valid \command{\link{RMmodel}}

 Most user will never call \command{\link{RMS}} directly.
}
\note{At most one of the arguments,
 \code{Aniso} and \code{proj} may be given at the same time.
}

\author{Martin Schlather, \email{schlather@math.uni-mannheim.de}
}
\seealso{
 \command{\link{RMmodel}},
}

\keyword{spatial}
\keyword{models}




\examples{
RFoptions(seed=0) ## *ANY* simulation will have the random seed 0; set
##                   RFoptions(seed=NA) to make them all random again
model1 <- RMS(RMexp(), scale=2)
model2 <- RMexp(scale=2)
x <- seq(0, 10, if (interactive()) 0.02 else 1)
print(all(RFcov(model1, x) == RFcov(model2, x))) # TRUE
\dontshow{FinalizeExample()}
}