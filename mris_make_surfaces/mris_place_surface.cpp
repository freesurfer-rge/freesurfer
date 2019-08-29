/**
 * @file  mri_place_surface.c
 * @brief Places surface based on an intensity input image. This is meant to provide
 * replacement functionality for mris_make_surfaces in a form that is easier to 
 * maintain.
 */
/*
 * Original Author: Douglas N Greve (but basically a rewrite of mris_make_surfaces by BF)
 * CVS Revision Info:
 *    $Author: greve $
 *    $Date: 2017/02/15 21:04:18 $
 *    $Revision: 1.246 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */

#define BRIGHT_LABEL         130
#define BRIGHT_BORDER_LABEL  100

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
double round(double x);
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <float.h>
#include <errno.h>

#include "utils.h"
#include "mrisurf.h"
#include "mrisutils.h"
#include "error.h"
#include "diag.h"
#include "mri.h"
#include "mri2.h"
#include "fio.h"
#include "version.h"
#include "label.h"
#include "annotation.h"
#include "cmdargs.h"
#include "romp_support.h"

static int  parse_commandline(int argc, char **argv);
static void check_options(void);
static void print_usage(void) ;
static void usage_exit(void);
static void print_help(void) ;
static void print_version(void) ;
static void dump_options(FILE *fp);

struct utsname uts;
char *cmdline, cwd[2000];
int debug = 0, checkoptsonly = 0;

int main(int argc, char *argv[]) ;

static char vcid[] =
"$Id: mri_glmfit.c,v 1.246 2017/02/15 21:04:18 greve Exp $";
const char *Progname = "mri_glmfit";

INTEGRATION_PARMS parms, old_parms ;
int lh_label = LH_LABEL ;
int rh_label = RH_LABEL ;

int max_pial_averages = 16 ;
int min_pial_averages = 2 ;
int max_white_averages = 4 ;
int min_white_averages = 0 ;
float pial_sigma = 2.0f ;
float white_sigma = 2.0f ;
float max_thickness = 5.0 ;
int vavgs = 5 ;
int nthreads = 1;
int nbrs = 2;
static int smooth = 5 ;

char *SUBJECTS_DIR;
char *insurfpath = NULL;
char *outsurfpath = NULL;
char *involpath=NULL;
char *segvolpath=NULL;
char *wmvolpath=NULL;

char *subject = NULL,*hemi = NULL,*insurfname = NULL, *outsurfname = NULL;
char *involname="brain.finalsurfs.mgz", *segvolname="aseg.presurf.mgz",*wmvolname="wm.mgz";

char tmpstr[2000];
int err=0;

/*--------------------------------------------------*/
int main(int argc, char **argv) 
{
  int nargs, i, msec;
  double        current_sigma, spring_scale = 1;
  MRIS *surf;
  MRI *invol, *seg, *mri_smooth, *wm;
  Timer timer ;
  char *cmdline2, cwd[2000];

  /* rkt: check for and handle version tag */
  nargs = handle_version_option (argc, argv, vcid, "$Name:  $");
  if (nargs && argc - nargs == 1) exit (0);
  argc -= nargs;
  cmdline = argv2cmdline(argc,argv);
  uname(&uts);
  getcwd(cwd,2000);
  cmdline2 = argv2cmdline(argc,argv);

  Progname = argv[0] ;
  argc --;
  argv++;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;
  Gdiag |= DIAG_SHOW ;

  memset(&parms, 0, sizeof(parms)) ;
  // don't let gradient use exterior information (slows things down)
  parms.fill_interior = 0 ;
  parms.projection = NO_PROJECTION ;
  parms.tol = 1e-4 ;
  parms.dt = 0.5f ;
  parms.base_dt = parms.dt ;

  parms.l_curv = 1.0 ;
  parms.l_intensity = 0.2 ;
  parms.l_tspring = 1.0f ;
  parms.l_nspring = 0.5f ;
  parms.l_spring = 0.0f ;
  parms.l_surf_repulse = 0.0 ;
  parms.l_repulse = 5.0 ;
  parms.l_spring_nzr = 0.0 ;
  parms.l_spring_nzr_len = 0.0 ;
  parms.l_hinge = 0;

  parms.niterations = 0 ;
  parms.write_iterations = 0 /*WRITE_ITERATIONS */;
  parms.integration_type = INTEGRATE_MOMENTUM ;
  parms.momentum = 0.0 /*0.8*/ ;
  parms.dt_increase = 1.0 /* DT_INCREASE */;
  parms.dt_decrease = 0.50 /* DT_DECREASE*/ ;
  parms.error_ratio = 50.0 /*ERROR_RATIO */;
  parms.niterations = 100;
  if(parms.momentum < 0.0) parms.momentum = 0.0 ;
    
  if(argc == 0) usage_exit();
  parse_commandline(argc, argv);
  check_options();
  if(checkoptsonly) return(0);

  // print out version of this program and mrisurf.c
  printf("%s\n",vcid);
  printf("%s\n",MRISurfSrcVersion());
  printf("\n");
  printf("cd %s\n",cwd);
  printf("setenv SUBJECTS_DIR %s\n",getenv("SUBJECTS_DIR"));
  printf("%s\n",cmdline2);
  printf("\n");
  fflush(stdout);

  printf("Reading in input surface %s\n",insurfpath);
  surf = MRISread(insurfpath);
  if(surf==NULL) exit(1);
  MRIScomputeMetricProperties(surf);
  if(nbrs > 1) MRISsetNeighborhoodSizeAndDist(surf, nbrs) ;
  if(smooth > 0) {
    MRISaverageVertexPositions(surf, smooth) ;
    MRIScomputeMetricProperties(surf);
  }
  MRISstoreMetricProperties(surf) ;
  MRISsaveVertexPositions(surf, ORIGINAL_VERTICES) ;
  MRISsaveVertexPositions(surf, WHITE_VERTICES) ;
  MRISsetVals(surf,-1) ;  /* clear white matter intensities */
  //MRISprettyPrintSurfQualityStats(stdout, surf);

  printf("Reading in input volume %s\n",involpath);
  invol = MRIread(involpath);
  if(invol==NULL) exit(1);
  mri_smooth = MRIcopy(invol, NULL) ;

  printf("Reading in wm volume %s\n",wmvolpath);
  wm = MRIread(wmvolpath);
  if(wm==NULL) exit(1);
  MRIclipBrightWM(invol, wm) ;
  MRI *mri_labeled = MRIfindBrightNonWM(invol, wm) ;
  MRImask(invol, mri_labeled, invol, BRIGHT_LABEL, 0) ;
  MRImask(invol, mri_labeled, invol, BRIGHT_BORDER_LABEL, 0) ;
  MRIfree(&wm);
  MRIfree(&mri_labeled);

  printf("Reading in seg volume %s\n",segvolpath);
  seg = MRIread(segvolpath);
  if(seg==NULL) exit(1);

  AutoDetGWStats adgws;
  err = adgws.AutoDetectStats(subject, hemi);
  if(err) exit(1);
  double inside_hi = adgws.white_inside_hi;
  double border_hi = adgws.white_border_hi;
  double border_low = adgws.white_border_low;
  double outside_low = adgws.white_outside_low;
  double outside_hi = adgws.white_outside_hi;


  timer.reset() ;

  current_sigma = white_sigma ;
  int n_averages = max_white_averages;
  printf("n_averages %d\n",n_averages);
  for (i = 0 ;  n_averages >= min_white_averages ; n_averages /= 2, current_sigma /= 2, i++) {

    printf("Iteration %d =========================================\n",i);
    printf("n_averages=%d, current_sigma=%g\n",n_averages,current_sigma); fflush(stdout);

    if(seg){
      printf("Freezing midline and others\n");  fflush(stdout);
      // This rips the midline vertices so that they do not move (thus
      // "fix"). It may also rip some vertices near the putamen and
      // maybe near lesions. It may set v->marked2 which will
      // influence the cortex.label. At onset, it will set all marked
      // and marked2 to 0. At then end it will set all marked=0.
      // It does not unrip any vertices, so, unless they are unripped
      // at some other point, the number of ripped vertices will 
      // increase.
      MRISripMidline(surf, seg, invol, hemi, GRAY_WHITE, 0) ;
    }

    parms.sigma = current_sigma ;
    parms.n_averages = n_averages ;
    
    printf("Computing border values \n");
    MRIScomputeBorderValues(surf, invol, mri_smooth, inside_hi,border_hi,border_low,outside_low,outside_hi,
			    current_sigma, 2*max_thickness, parms.fp, GRAY_WHITE, NULL, 0, parms.flags,seg,-1,-1) ;
    printf("Finding expansion regions\n"); fflush(stdout);
    MRISfindExpansionRegions(surf) ;
    
    if(vavgs > 0) {
      printf("Averaging target values for %d iterations...\n",vavgs) ;
      // MRIScomputeBorderValues() sets v->marked=1 for all unripped
      MRISaverageMarkedVals(surf, vavgs) ;
    }

    /*There are frequently regions of gray whose intensity is fairly
      flat. We want to make sure the surface settles at the innermost
      edge of this region, so on the first pass, set the target
      intensities artificially high so that the surface will move
      all the way to white matter before moving outwards to seek the
      border (I know it's a hack, but it improves the surface in
      a few areas. The alternative is to explicitly put a gradient-seeking
      term in the cost functional instead of just using one to find
      the target intensities).
    */

    // Everything up to now has been leading up to this. This is where
    // the surfaces get placed.
    INTEGRATION_PARMS_copy(&old_parms, &parms) ;

    // This appears to adjust the cost weights based on the iteration but in
    // practice, they never change because of the PARMS_copy above and below
    parms.l_nspring *= spring_scale ; 
    parms.l_spring *= spring_scale ; 
    // This line with tspring being ajusted twice was probably originally a typo
    // but it has existed this way for a long time. It was changed after 
    // version 6, but I just changed it back for consistency. 
    parms.l_tspring *= spring_scale ;  parms.l_tspring *= spring_scale ;

    //parms.l_tspring = MIN(1.0,parms.l_tspring) ; // This had a bad effect on highres and no effect on 1mm
    parms.l_nspring = MIN(1.0, parms.l_nspring) ;
    parms.l_spring = MIN(1.0, parms.l_spring) ;
    printf("Positioning White Surface: tspring = %g, nspring = %g, spring = %g, niters = %d ",
	   parms.l_tspring,parms.l_nspring,parms.l_spring,parms.niterations); 
    printf("l_repulse = %g, checktol = %d\n",parms.l_repulse,parms.check_tol);fflush(stdout);
    MRISpositionSurface(surf, invol, mri_smooth, &parms);

    old_parms.start_t = parms.start_t ;
    INTEGRATION_PARMS_copy(&parms, &old_parms) ;

    if(!n_averages)
      break ; 

  } // end major loop placing the white surface using

  printf("\n\n");
  printf("Writing output to %s\n",outsurfpath);
  err = MRISwrite(surf,outsurfpath);
  if(err){
    printf("ERROR: writing to %s\n",outsurfpath);
    exit(1);
  }

  msec = timer.milliseconds() ;
  printf("#ET# mris_place_surface %5.2f minutes\n", (float)msec/(60*1000.0f));
  printf("#VMPC# mris_make_surfaces VmPeak  %d\n",GetVmPeak());
  printf("mris_place_surface done\n");

  return(0);

}
/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

/* --------------------------------------------- */
static int parse_commandline(int argc, char **argv) {
  int  nargc , nargsused;
  char **pargv, *option ;

  if (argc < 1) usage_exit();

  nargc   = argc;
  pargv = argv;
  while (nargc > 0) {

    option = pargv[0];
    if (debug) printf("%d %s\n",nargc,option);
    nargc -= 1;
    pargv += 1;

    nargsused = 0;

    if (!strcasecmp(option, "--help"))  print_help() ;
    else if(!strcasecmp(option, "--version")) print_version() ;
    else if(!strcasecmp(option, "--debug"))   debug = 1;
    else if(!strcasecmp(option, "--checkopts"))   checkoptsonly = 1;
    else if(!strcasecmp(option, "--nocheckopts")) checkoptsonly = 0;
    else if(!strcmp(option, "--i")){
      insurfpath = pargv[0];
      nargsused = 1;
      checkoptsonly = 0;
    }
    else if(!strcmp(option, "--o")){
      outsurfpath = pargv[0];
      nargsused = 1;
      checkoptsonly = 0;
    }
    else if(!strcmp(option, "--sd")){
      if(nargc < 1) CMDargNErr(option,1);
      printf("using %s as SUBJECTS_DIR...\n", pargv[0]) ;
      setenv("SUBJECTS_DIR",pargv[0],1);
      nargsused = 1;
    }
    else if(!strcmp(option, "--s")){
      if(nargc < 4) CMDargNErr(option,4);
      subject = pargv[0];
      hemi = pargv[1];
      insurfname = pargv[2];
      outsurfname = pargv[3];
      nargsused = 4;
      checkoptsonly = 0;
    }
    else if(!strcasecmp(option, "--threads") || !strcasecmp(option, "--nthreads") ){
      if(nargc < 1) CMDargNErr(option,1);
      sscanf(pargv[0],"%d",&nthreads);
      #ifdef _OPENMP
      omp_set_num_threads(nthreads);
      #endif
      nargsused = 1;
    } 
    else if(!strcasecmp(option, "--max-threads")){
      nthreads = 1;
      #ifdef _OPENMP
      nthreads = omp_get_max_threads();
      omp_set_num_threads(nthreads);
      #endif
    } 
    else if(!strcasecmp(option, "--max-threads-1") || !strcasecmp(option, "--max-threads-minus-1")){
      nthreads = 1;
      #ifdef _OPENMP
      nthreads = omp_get_max_threads()-1;
      if(nthreads < 0) nthreads = 1;
      omp_set_num_threads(nthreads);
      #endif
    } 
    else {
      fprintf(stderr,"ERROR: Option %s unknown\n",option);
      if (CMDsingleDash(option))
        fprintf(stderr,"       Did you really mean -%s ?\n",option);
      exit(-1);
    }
    nargc -= nargsused;
    pargv += nargsused;
  }
  return(0);
}
/* --------------------------------------------- */
static void check_options(void) {
  if(insurfpath == NULL && subject == NULL){
    printf("ERROR: no input surface set\n");
    exit(1);
  }
  if(outsurfpath == NULL && subject == NULL){
    printf("ERROR: no output surface set\n");
    exit(1);
  }
  if(insurfpath != NULL && subject != NULL){
    printf("ERROR: cannot use both --i and --s\n");
    exit(1);
  }
  if(outsurfpath != NULL && subject != NULL){
    printf("ERROR: cannot use both --o and --s\n");
    exit(1);
  }
  SUBJECTS_DIR = getenv("SUBJECTS_DIR");
  if(insurfpath == NULL && subject != NULL){
    sprintf(tmpstr,"%s/%s/surf/%s.%s",SUBJECTS_DIR,subject,hemi,insurfname);
    insurfpath = strcpyalloc(tmpstr);
    sprintf(tmpstr,"%s/%s/surf/%s.%s",SUBJECTS_DIR,subject,hemi,outsurfname);
    outsurfpath = strcpyalloc(tmpstr);
    sprintf(tmpstr,"%s/%s/mri/%s",SUBJECTS_DIR,subject,involname);
    involpath = strcpyalloc(tmpstr);
    sprintf(tmpstr,"%s/%s/mri/%s",SUBJECTS_DIR,subject,segvolname);
    segvolpath = strcpyalloc(tmpstr);
    sprintf(tmpstr,"%s/%s/mri/%s",SUBJECTS_DIR,subject,wmvolname);
    wmvolpath = strcpyalloc(tmpstr);
  }

  return;
}



/* --------------------------------------------- */
static void print_usage(void) 
{
printf("\n");
printf("USAGE: ./mris_place_surface\n");
printf(" --s subject hemi insurfname outsurfname\n");
printf("\n");
}


/* --------------------------------------------- */
static void print_help(void) {
  print_usage() ;
  printf("\n");
  printf("\n");
  exit(1) ;
}

/* ------------------------------------------------------ */
static void usage_exit(void) {
  print_usage() ;
  exit(1) ;
}
/* --------------------------------------------- */
static void print_version(void) {
  printf("%s\n", vcid) ;
  exit(1) ;
}
/* --------------------------------------------- */
static void dump_options(FILE *fp) {
  fprintf(fp,"\n");
  fprintf(fp,"%s\n",vcid);
  fprintf(fp,"cwd %s\n",cwd);
  fprintf(fp,"cmdline %s\n",cmdline);
  fprintf(fp,"sysname  %s\n",uts.sysname);
  fprintf(fp,"hostname %s\n",uts.nodename);
  fprintf(fp,"machine  %s\n",uts.machine);
  fprintf(fp,"user     %s\n",VERuser());
  return;
}
