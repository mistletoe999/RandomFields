\name{Internal functions}
\alias{rfGenerateModels}
\alias{rfGenerateConstants}
\alias{rfGenerateTest}
\alias{rfGenerateMaths}
\alias{checkExamples}
\alias{StartExample}
\alias{FinalizeExample}
\alias{Dependencies}
\alias{ScreenDevice}
\alias{plotWithCircles}
\alias{showManpages}
\title{Internal functions}
\description{
 These functions are internal and should not be used.
}
\usage{
rfGenerateModels(assigning,
 RFpath = "~/R/RF/svn/RandomFields",
 RMmodels.file = paste(RFpath, "RandomFields/R/RMmodels.R", sep="/")
 )
rfGenerateConstants(
           RFpath = "~/R/RF/svn/RandomFields",
           RCauto.file = paste(RFpath, "RandomFields/R/RCauto.R", sep="/")
           )
rfGenerateTest(files = NULL, RFpath = "~/R/RF/svn/RandomFields")
rfGenerateMaths(files = "/usr/include/tgmath.h", Cfile = "QMath",
                        RFpath = "~/R/RF/svn/RandomFields")
checkExamples(exclude = NULL, include=1:length(.fct.list),
               ask=FALSE, echo=TRUE, halt=FALSE, ignore.all = FALSE,
               path=package, package = "RandomFields", 
               read.rd.files=TRUE, libpath = NULL, single.runs = FALSE)

ScreenDevice(height, width)

FinalizeExample()
StartExample(reduced = TRUE)

Dependencies(pkgs = all.pkgs, dir = "Dependencies",
             install = FALSE, check=TRUE, reverse=FALSE, package =
                 "RandomFields")

showManpages(path="/home/schlather/svn/RandomFields/RandomFields/man")

plotWithCircles(data, factor=1.0,
                xlim=range(data[,1])+c(-maxr,maxr),
                ylim=range(data[,2])+c(-maxr,maxr),
                col=1, fill=0, ...)
}
\arguments{
  \item{assigning, RFpath, RMmodels.file,RCauto.file,files,Cfile}{internal}
  \item{exclude, include, ask, echo, halt, ignore.all, path, package,
    read.rd.files, libpath, single.runs
  }{internal; 
    ignore.all referes to the \sQuote{all} export statement in the
    namespace -- whether this should be ignored.
    if \code{read.rf.files} is \code{TRUE} or a path to the Rd files, then
    the man pages are analysed to get all examples; \code{ignore.all} is
    then ignored. If \code{FALSE} only examples of functions (which are
    search in the environments) are run.
  }
  \item{pkgs, dir,install, check, reverse}{internal}
  \item{height,width}{window sizes}
  \item{data, factor, xlim, ylim, col, fill, ...}{internal}
  \item{reduced}{internal}
 % \item{REGISTER, COVREGISTER, RELAX}{internal}
}

\author{Martin Schlather, \email{schlather@math.uni-mannheim.de}
 \url{http://ms.math.uni-mannheim.de/de/publications/software}
}
\keyword{spatial}

\examples{
RFoptions(seed=0) ## *ANY* simulation will have the random seed 0; set
##                   RFoptions(seed=NA) to make them all random again

## internal functions: no examples given
\dontshow{\dontrun{ ## OK
## check own examples
checkExamples(ignore.all=TRUE, halt=!TRUE)


### Check the examples of the other packages:
dep.packages <- c( #"DSpat","lgcp", "constrainedKriging", "MarkedPointProcess",
                  "Geneland", "glmmBUGS", 
                  "ProbForecastGOP","geoR",
                  "CompRandFld", ## RFsim does not work in version 2.1.18
                  "fractaldim", "rpanel", "spatstat")
#for (i in dep.packages) library(i, character.only=TRUE)
#for (i in dep.packages) install.packages(i)


not.working <- list()
for (.i in 1:length(dep.packages)) {
  not.working[[.i]] <-
    checkExamples(path=paste("~/TMP/dep.packages", dep.packages[.i], sep="/"),
                   package=dep.packages[.i])
  Print(.i, not.working); repeat{ if (readline()=="e") break}
}

Print(not.working)
}}

# for (i in dep.packages) cat(maintainer(i), "\n") 
\dontshow{FinalizeExample()}
}
