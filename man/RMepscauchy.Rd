
\name{RMepscauchy}
\alias{RMepscauchy}
\title{Generalized Cauchy Family Covariance Model}
\description{
 \command{\link{RMepscauchy}} is a stationary isotropic covariance model
 belonging to the generalized Cauchy family. \bold{In contrast to most
   other models it is not a correlation function.}
 The corresponding covariance function only depends on the distance \eqn{r \ge 0}{r \ge 0} between
 two points and is given by
 \deqn{C(r) = (\varepsilon + r^\alpha)^(-\beta/\alpha)}{C(r) = (\epsilon + r^\alpha)^(-\beta/\alpha)}
 where \eqn{\epsilon > 0},
 \eqn{\alpha \in (0,2]}{0 < \alpha \le 2} and \eqn{\beta > 0}{\beta > 0}.
 See also \command{\link{RMcauchy}}.
}
\usage{
RMepscauchy(alpha, beta, eps, var, scale, Aniso, proj)
}
\arguments{
 \item{alpha}{a numerical value; should be in the interval (0,2]
 to provide a valid covariance function for a random field of any dimension.}
 \item{beta}{a numerical value; should be positive to provide a valid
   covariance function for a random field of any dimension.}
 \item{eps}{a positive value}
 \item{var,scale,Aniso,proj}{optional arguments; same meaning for any
 \command{\link{RMmodel}}. If not passed, the above
 covariance function remains unmodified.}
}
\details{
 This model has a smoothness parameter \eqn{\alpha}{\alpha} and a
 paramater \eqn{\beta}{\beta} which determines the asymptotic power law.
 More precisely, this model admits simulating random fields where fractal dimension
 \emph{D} of the Gaussian sample and Hurst coefficient \emph{H}
 can be chosen independently (compare also \command{\link{RMlgd}}.): Here, we have
 \deqn{ D = d + 1 - \alpha/2, \alpha \in (0,2]}{ D = d + 1 - \alpha/2, 0
 < \alpha \le 2}
 and
 \deqn{ H = 1 - \beta/2, \beta > 0.}{ H = 1 - \beta/2, \beta > 0.}
 
 I. e. the smaller \eqn{\beta}{\beta}, the longer the long-range
 dependence.
 
 The covariance function is very regular near the origin, because its
 Taylor expansion only contains even terms and reaches its sill slowly.
 
 Each covariance function of the Cauchy family is a normal scale mixture.

 Note that the Cauchy Family (see \command{\link{RMcauchy}}) is included
 in this family for the choice \eqn{\alpha = 2}{\alpha = 2} and
 \eqn{\beta = 2 \gamma}{\beta = 2 \gamma}. 
}
\value{
 \command{\link{RMepscauchy}} returns an object of class \code{\link[=RMmodel-class]{RMmodel}}
}
\references{
 \itemize{
 % \item Gneiting, T. (2002) Compactly supported correlation
 % functions. \emph{J. Multivariate Anal.} \bold{83} 493--508.
 
 \item Gneiting, T. and Schlather, M. (2004)
 Stochastic models which separate fractal dimension and Hurst effect.
 \emph{SIAM review} \bold{46}, 269--282.
 }
}

\author{Martin Schlather, \email{schlather@math.uni-mannheim.de}
}
\seealso{
 \command{\link{RMcauchy}},
 \command{\link{RMcauchytbm}},
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
model <- RMepscauchy(alpha=1.5, beta=1.5, scale=0.3, eps=0.5)
x <- seq(0, 10, 0.02)
plot(model)
plot(RFsimulate(model, x=x))
\dontshow{FinalizeExample()}
}