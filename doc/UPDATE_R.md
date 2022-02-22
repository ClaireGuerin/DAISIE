# Update your R version

When trying to install DAISIE, you may come across errors that prevent you to install the package and its depencies. The errors might look something like:

```r
Error: package ‘phangorn’ is not available (for R version 3.6.3) and could not be installed.
Error: package ‘phytools’ is not available (for R version 3.6.3) and could not be installed.
Error: package ‘DAISIE’ is not available (for R version 3.6.3) and could not be installed.
```

Update your R version from `3.x` to `4.x` to be able to install DAISIE and all it dependencies.

## Ubuntu

### Step 1: Update R (from the terminal)

1. Look for the line `deb https://cloud.r-project.org/bin/linux/ubuntu focal-cran40/` in your `/etc/apt/sources.list’` file. If present, move to the next step. If absent, run 
```
$ sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E298A3A825C0D65DFD57CBB651716619E084DAB9
$ sudo add-apt-repository 'deb https://cloud.r-project.org/bin/linux/ubuntu focal-cran40/'
```
2. Install R with `sudo apt install r-base`. Checking the R version with `R --version` should return an updated version of R.

### Step 2: Update R packages (from R or RStudio)

```r
update.packages(ask = FALSE, checkBuilt = TRUE) # update already installed packages

old_packages <- installed.packages(lib.loc = "/home/$USER/R/x86_64-pc-linux-gnu-library/3.6/") # list packages installed in old R version
new_packages <- installed.packages() # list packages installed in current, updated R version
missing_packages <- as.data.frame(
  old_packages[!old_packages[, "Package"] %in% new_packages[, "Package"],]) # list missing packages in new R version.

install.packages(missing_packages$Package) # install missing packages
```

