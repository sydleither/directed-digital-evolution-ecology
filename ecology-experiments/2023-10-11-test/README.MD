# 2023-10-11 - Repeat selection algorithm comparison

Try to run the experiment "2021-11-11-selection".

Steps taken:
- Run hpcc/job-gen.sh
- cd to scratch/jobs and manually sbatch all generated files
- Run analysis/run-agg.sb
- Rename dump to data (in future experiments change this in run-agg.sb)
- Run hpcc/build-max-coverage.sb (i added this file)
- Run analysis/fill_timeseries.py
- Create analysis/plots directory
- Make a conda R environment with ggplot2=3.3.0 and install required packages
- Run rmarkdown: Rscript -e "rmarkdown::render('2023-10-11-test.Rmd')"
    - First modify script to use discrete viridis instead of khroma bc conda doesnt have khroma
    - selection-figure-with-timeseries errors so i disabled eval
- DOT NOT Run analysis/gen-pop-profiles.R, it generates a million pdfs
    - comment out library(ggpcp) bc conda doesnt have ggpcp and script doesn't break with it commented out