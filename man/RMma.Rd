\name{RMma}
\alias{RMma}
\title{Ma operator}
\description{
 \command{\link{RMma}} is a univariate stationary covariance model
 depending on a univariate stationary covariance model.
 The corresponding covariance function only depends on the difference
 \eqn{h}{h} between two points and is given by
 \deqn{C(h) = (\theta / (1 - (1-\theta) \phi(h)))^\alpha}{C(h) = (\theta
 / (1 - (1-\theta) * \phi(h)))^\alpha}
}
\usage{
RMma(phi, alpha, theta, var, scale, Aniso, proj)
}
\arguments{
 \item{phi}{a stationary covariance \command{\link{RMmodel}}.}
 \item{alpha}{a numerical value; positive}
 \item{theta}{a numerical value; in the interval \eqn{(0,1)}{(0,1)}}
 \item{var,scale,Aniso,proj}{optional arguments; same meaning for any
 \command{\link{RMmodel}}. If not passed, the above
 covariance function remains unmodified.}
}
%\details{}
\value{
 \command{\link{RMma}} returns an object of class \code{\link[=RMmodel-class]{RMmodel}}
}
\references{
 \itemize{
 \item Ma, C. (2003)
 Spatio-temporal covariance functions generated by mixtures.
 \emph{Math. Geol.}, \bold{34}, 965-975.
 }
}

\author{Martin Schlather, \email{schlather@math.uni-mannheim.de}
}
\seealso{
 \command{\link{RMmodel}},
 \command{\link{RFsimulate}},
 \command{\link{RFfit}}.
}


\keyword{spatial}
\keyword{models}





\examples{
RFoptions(seed=0) ## *ANY* simulation will have the random seed 0; set
##                   RFoptions(seed=NA) to make them all random again
\dontshow{StartExample()}
model <- RMma(RMgauss(), alpha=4, theta=0.5)
x <- seq(0, 10, 0.02)
plot(model)
plot(RFsimulate(model, x=x))
\dontshow{FinalizeExample()}
}