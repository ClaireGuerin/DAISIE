# Update your R version

When trying to install DAISIE, you may come across errors that prevent you to install the package and its depencies. The errors might look something like:

```r
Error: package ‘phangorn’ is not available (for R version 3.6.3) and could not be installed.
Error: package ‘phytools’ is not available (for R version 3.6.3) and could not be installed.
Error: package ‘DAISIE’ is not available (for R version 3.6.3) and could not be installed.
```

Update your R version from `3.x` to `4.x` to be able to install DAISIE and all it dependencies.

## Step 1: Update R

### Windows
From R or RStudio, use `installr`, the R package which helps install and update software on Windows.

```r
install.packages("installr") # assuming it is not already installed
library(installr)
updateR()
```

### Mac OS
From R or RStudio, use `updateR`, the R package which helps install and update software on Mac OS.

```r
install.packages('devtools') # assuming it is not already installed
library(devtools)
install_github('andreacirilloac/updateR')
library(updateR)
updateR(admin_password = 'Admin user password')
```

### Ubuntu

From the terminal:

1. Look for the line `deb https://cloud.r-project.org/bin/linux/ubuntu focal-cran40/` in your `/etc/apt/sources.list’` file. If present, move to the next step. If absent, run 
```
$ sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E298A3A825C0D65DFD57CBB651716619E084DAB9
$ sudo add-apt-repository 'deb https://cloud.r-project.org/bin/linux/ubuntu focal-cran40/'
```
2. Install R with `sudo apt install r-base`. Checking the R version with `R --version` should return an updated version of R.

## Step 2: Update R packages

From R or RStudio:

```r
update.packages(ask = FALSE, checkBuilt = TRUE) # update already installed packages

old_packages <- installed.packages(lib.loc = "/home/$USER/R/x86_64-pc-linux-gnu-library/3.6/") # list packages installed in old R version. Change the path according to your software installation location and the old version number
new_packages <- installed.packages() # list packages installed in current, updated R version
missing_packages <- as.data.frame(
  old_packages[!old_packages[, "Package"] %in% new_packages[, "Package"],]) # list missing packages in new R version.

install.packages(missing_packages$Package) # install missing packages
```