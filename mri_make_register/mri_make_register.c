#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include "machine.h"
#include "fio.h"
#include "utils.h"
#include "mri.h"
#include "volume_io.h"
#include "analyze.h"
#include "mri_identify.h"
#include "matrix.h"
#include "error.h"

float fct_c_r, fct_c_a, fct_c_s;
float fct_n_r, fct_n_a, fct_n_s;
float fct_tl_r, fct_tl_a, fct_tl_s;
float fct_tr_r, fct_tr_a, fct_tr_s;
float fct_br_r, fct_br_a, fct_br_s;
float fct_slice_thickness;
float fct_in_plane_resolution;
int fct_rows, fct_cols, fct_slices;

char *Progname;

void usage(void);
void read_functional_header(char *fct_stem);
MATRIX *make_register_matrix(MRI *high_res_vol, MRI *fct_run_vol);
void set_matrix(MATRIX *m, float e11, float e12, float e13, float e14, 
                           float e21, float e22, float e23, float e24,
                           float e31, float e32, float e33, float e34,
                           float e41, float e42, float e43, float e44);

void usage(void)
{

  fprintf(stderr, "usage: %s <subject name> <fct stem> [<structural dir>]\n", Progname);

} /* end usage() */

int main(int argc, char *argv[])
{

  MRI *high_res_vol = NULL, *fct_run_vol = NULL;
  char *subjects_dir;
  MATRIX *register_matrix;
  char *subject_name, *fct_stem;
  char file_name[STRLEN];
  FILE *fp;
  char *s;
  int n_time_points;

  Progname = strrchr(argv[0], '/');
  Progname = (Progname == NULL ? argv[0] : Progname + 1);

  if(argc == 3 || argc == 4)
  {
    fct_stem = argv[2];
    read_functional_header(fct_stem);
    subject_name = argv[1];
    if(argc == 3)
    {
      if((subjects_dir = getenv("SUBJECTS_DIR")) == NULL)
        ErrorExit(ERROR_BAD_PARM, "%s: can't get environment variable SUBJECTS_DIR", Progname);
      sprintf(file_name, "%s/%s/mri/T1", subjects_dir, subject_name);
      if((high_res_vol = MRIreadInfo(file_name)) == NULL)
        ErrorExit(ERROR_NO_FILE, "%s: couldn't open volume %s", Progname, file_name);
    }
    else
    {
      if((high_res_vol = MRIreadInfo(argv[3])) == NULL)
        ErrorExit(ERROR_NO_FILE, "%s: couldn't open volume %s", Progname, file_name);
    }
  }
  else
  {
    usage();
    exit(1);
  }

  register_matrix = make_register_matrix(high_res_vol, fct_run_vol);

  strcpy(file_name, fct_stem);
  s = strrchr(file_name, '/');
  s = (s == NULL ? file_name : s+1);
  sprintf(s, "register.dat");
  if((fp = fopen(file_name, "w")) == NULL)
    ErrorExit(ERROR_BAD_FILE, "%s: couldn't open file %s for writing", Progname, file_name);
  fprintf(fp, "%s\n", subject_name);
  fprintf(fp, "%g\n", fct_in_plane_resolution);
  fprintf(fp, "%g\n", fct_slice_thickness);
  fprintf(fp, "%g\n", 0.1);
  fprintf(fp, "%10.5g %10.5g %10.5g %10.5g\n", *MATRIX_RELT(register_matrix, 1, 1), *MATRIX_RELT(register_matrix, 1, 2), *MATRIX_RELT(register_matrix, 1, 3), *MATRIX_RELT(register_matrix, 1, 4));
  fprintf(fp, "%10.5g %10.5g %10.5g %10.5g\n", *MATRIX_RELT(register_matrix, 2, 1), *MATRIX_RELT(register_matrix, 2, 2), *MATRIX_RELT(register_matrix, 2, 3), *MATRIX_RELT(register_matrix, 2, 4));
  fprintf(fp, "%10.5g %10.5g %10.5g %10.5g\n", *MATRIX_RELT(register_matrix, 3, 1), *MATRIX_RELT(register_matrix, 3, 2), *MATRIX_RELT(register_matrix, 3, 3), *MATRIX_RELT(register_matrix, 3, 4));
  fprintf(fp, "%10.5g %10.5g %10.5g %10.5g\n", *MATRIX_RELT(register_matrix, 4, 1), *MATRIX_RELT(register_matrix, 4, 2), *MATRIX_RELT(register_matrix, 4, 3), *MATRIX_RELT(register_matrix, 4, 4));
  fclose(fp);

  sprintf(file_name, "%s_000.hdr", fct_stem);
  if((fp = fopen(file_name, "r")) == NULL)
    ErrorExit(ERROR_NO_FILE, "%s: couldn't open file %s", Progname, file_name);
  fscanf(fp, "%*d %*d %d %*d", &n_time_points);
  fclose(fp);

  strcpy(file_name, fct_stem);
  s = strrchr(file_name, '/');
  s = (s == NULL ? file_name : s+1);
  sprintf(s, "analyse.dat");
  if((fp = fopen(file_name, "w")) == NULL)
    ErrorExit(ERROR_BAD_FILE, "%s: couldn't open file %s for writing", Progname, file_name);
  fprintf(fp, ".\n");
  s = strrchr(fct_stem, '/');
  s = (s == NULL ? fct_stem : s + 1);
  fprintf(fp, "%s_%%03d.bshort\n", s);
  fprintf(fp, "%d %d\n", fct_slices, n_time_points);
  fprintf(fp, "%d %d\n", fct_cols, fct_rows);
  fclose(fp);

  exit(0);

} /* end main() */

void read_functional_header(char *fct_stem)
{

  FILE *fp;
  char aws_header_name[STRLEN], aws_header_line[STRLEN];
  char *s;
  int good_fct_header_flag = 0;

  /* try .awshdr (from translate_aws), .bhdr, and .ras (from imgs2bshort) */
  sprintf(aws_header_name, "%s.awshdr", fct_stem);
  if((fp = fopen(aws_header_name, "r")) == NULL)
  {
    sprintf(aws_header_name, "%s.bhdr", fct_stem);
    if((fp = fopen(aws_header_name, "r")) == NULL)
    {
    sprintf(aws_header_name, "%s.ras", fct_stem);
    if((fp = fopen(aws_header_name, "r")) == NULL)
      ErrorExit(ERROR_NO_FILE, "%s: couldn't open file header file", Progname);
    }
  }

  while(fgets(aws_header_line, STRLEN, fp) != NULL)
  {
    if((s = strstr(aws_header_line, "normal_r")) != NULL)
    {
      s+=10;
      fct_n_r = (float)atof(s);
      good_fct_header_flag = 1;
    }
    if((s = strstr(aws_header_line, "normal_a")) != NULL)
    {
      s+=10;
      fct_n_a = (float)atof(s);
      good_fct_header_flag = 1;
    }
    if((s = strstr(aws_header_line, "normal_s")) != NULL)
    {
      s+=10;
      fct_n_s = (float)atof(s);
      good_fct_header_flag = 1;
    }
    if((s = strstr(aws_header_line, "top_left_r")) != NULL)
    {
      s+=12;
      fct_tl_r = (float)atof(s);
      good_fct_header_flag = 1;
    }
    if((s = strstr(aws_header_line, "top_left_a")) != NULL)
    {
      s+=12;
      fct_tl_a = (float)atof(s);
      good_fct_header_flag = 1;
    }
    if((s = strstr(aws_header_line, "top_left_s")) != NULL)
    {
      s+=12;
      fct_tl_s = (float)atof(s);
      good_fct_header_flag = 1;
    }
    if((s = strstr(aws_header_line, "top_right_r")) != NULL)
    {
      s+=13;
      fct_tr_r = (float)atof(s);
      good_fct_header_flag = 1;
    }
    if((s = strstr(aws_header_line, "top_right_a")) != NULL)
    {
      s+=13;
      fct_tr_a = (float)atof(s);
      good_fct_header_flag = 1;
    }
    if((s = strstr(aws_header_line, "top_right_s")) != NULL)
    {
      s+=13;
      fct_tr_s = (float)atof(s);
      good_fct_header_flag = 1;
    }
    if((s = strstr(aws_header_line, "bottom_right_r")) != NULL)
    {
      s+=16;
      fct_br_r = (float)atof(s);
      good_fct_header_flag = 1;
    }
    if((s = strstr(aws_header_line, "bottom_right_a")) != NULL)
    {
      s+=16;
      fct_br_a = (float)atof(s);
      good_fct_header_flag = 1;
    }
    if((s = strstr(aws_header_line, "bottom_right_s")) != NULL)
    {
      s+=16;
      fct_br_s = (float)atof(s);
      good_fct_header_flag = 1;
    }

    if((s = strstr(aws_header_line, "slice_thick")) != NULL)
    {
      s+=13;
      fct_slice_thickness = (float)atof(s);
    }
    if((s = strstr(aws_header_line, "cols")) != NULL)
    {
      s+=6;
      fct_cols = atoi(s);
    }
    if((s = strstr(aws_header_line, "rows")) != NULL)
    {
      s+=6;
      fct_rows = atoi(s);
    }
    if((s = strstr(aws_header_line, "nslices")) != NULL)
    {
      s+=9;
      fct_slices = atoi(s);
    }

  }

  fclose(fp);

  if(good_fct_header_flag == 0)
    ErrorExit(ERROR_BAD_FILE, "%s: %s does not contain necessary ras information", Progname, aws_header_name);

  fct_c_r = fct_tl_r + (fct_br_r - fct_tl_r) / 2 + fct_slice_thickness * fct_n_r * (fct_slices - 1) / 2;
  fct_c_a = fct_tl_a + (fct_br_a - fct_tl_a) / 2 + fct_slice_thickness * fct_n_a * (fct_slices - 1) / 2;
  fct_c_s = fct_tl_s + (fct_br_s - fct_tl_s) / 2 + fct_slice_thickness * fct_n_s * (fct_slices - 1) / 2;

printf("%g %g %g\n", fct_n_r, fct_n_a, fct_n_s);
printf("%g %g %g\n", fct_c_r, fct_c_a, fct_c_s);

  fct_in_plane_resolution = sqrt((fct_tl_r - fct_tr_r) * (fct_tl_r - fct_tr_r) +
                                 (fct_tl_a - fct_tr_a) * (fct_tl_a - fct_tr_a) +
                                 (fct_tl_s - fct_tr_s) * (fct_tl_s - fct_tr_s)) / (float)fct_cols;

  return;

}  /*  end read_functional_header()  */

MATRIX *make_register_matrix(MRI *high_res_vol, MRI *fct_run_vol)
{

  MATRIX *itoi;
  MATRIX *si2toras, *fi2toras;
  MATRIX *si1toi2, *fi1toi2;
  MATRIX *si1tor, *fi1tor;
  MATRIX *rm;
  float fctxmn, fctymn, fctzmn;

  float fct_x_r, fct_x_a, fct_x_s;
  float fct_y_r, fct_y_a, fct_y_s;
  float fct_z_r, fct_z_a, fct_z_s;

  fct_x_r = (fct_tr_r - fct_tl_r) / fct_cols;
  fct_x_a = (fct_tr_a - fct_tl_a) / fct_cols;
  fct_x_s = (fct_tr_s - fct_tl_s) / fct_cols;

  fct_y_r = (fct_br_r - fct_tr_r) / fct_rows;
  fct_y_a = (fct_br_a - fct_tr_a) / fct_rows;
  fct_y_s = (fct_br_s - fct_tr_s) / fct_rows;

  fct_z_r = fct_n_r;
  fct_z_a = fct_n_a * fct_slice_thickness;
  fct_z_s = fct_n_s * fct_slice_thickness;

/******************************************************************
 *
 * composing the registration matrix
 *
 * The registration matrix goes from structural x, y, z coordinates 
 * in mm (origin at the center) to functional x, y, z coordinates 
 * in mm (origin at the center of the volume).  With the header 
 * information, we can go from the volume indicies 
 * (struct[i1][i2][i3], fct[i1][i2][i3]) to RAS coordinates.  We 
 * then need to go from volume indicies to structural and functional 
 * x, y, z coordinates that the register program likes.
 *
 * The spaces are labelled as follows:
 *   r    Register program space.  The registration matrix goes from 
 *        structural x, y, z in this space to functional x, y z
 *   i1   Indicies to the volume.  A voxel is accessed as 
 *        structural[z][y][x] or functional[z][y][x] in this space.
 *   i2   Centered index space.  The center of the volume is 
 *        (0, 0, 0) -- x, y, and z are voxel indicies (as in the 
 *        i1 space) relative to the center.
 *   ras: Right, anterior, superior space.  This is the only space 
 *        that is common to the structural and functional volumes.
 *
 * The matricies are as follows:
 *   itoi: Structural i1 space to functional i1 space
 *   si2toras: Structural i2 to ras
 *   fi2toras: Functional i2 to ras
 *   si1toi2: Structural i1 to structural i2
 *   fi1toi2: Fucntional i1 to functional i2
 *   si1tor: Structural i1 to register program space
 *   fi1tor: Functional i1 to register program space
 *   rm: Rotation matrix to be written to register.dat
 *
 * itoi is an intermediate matrix and goes from structural i1
 *                                           to structural i2 
 *                                           to ras 
 *                                           to functional i1 
 *                                           to functional i1:
 *   itoi = inv(fi1toi2) * inv(fi2toras) * si2toras * si1toi2
 *
 * rm goes from structural r
 *           to structual i1
 *           to functional i1 (through itoi)
 *           to functional r:
 *   rm = fi1tor * itoi * inv(si1tor)
 *
 ******************************************************************/

  itoi     = MatrixAlloc(4, 4, MATRIX_REAL);
  si2toras = MatrixAlloc(4, 4, MATRIX_REAL);
  fi2toras = MatrixAlloc(4, 4, MATRIX_REAL);
  si1toi2  = MatrixAlloc(4, 4, MATRIX_REAL);
  fi1toi2  = MatrixAlloc(4, 4, MATRIX_REAL);
  si1tor   = MatrixAlloc(4, 4, MATRIX_REAL);
  fi1tor   = MatrixAlloc(4, 4, MATRIX_REAL);
  rm       = MatrixAlloc(4, 4, MATRIX_REAL);

  if(high_res_vol->ras_good_flag == 0)
  {
    printf("T1 volume contains no orientation information\n");
    printf("assuming correctly oriented, centered coronal slices...\n");

    /* +x -> left
       +y -> inferior
       +z -> anterior */

    set_matrix(si2toras, -1,  0, 0, 0, 
                          0,  0, 1, 0, 
                          0, -1, 0, 0, 
                          0,  0, 0, 1);

  }
  else
  {
    set_matrix(si2toras, high_res_vol->x_r, high_res_vol->y_r, high_res_vol->z_r, high_res_vol->c_r, 
                         high_res_vol->x_a, high_res_vol->y_a, high_res_vol->z_a, high_res_vol->c_a, 
                         high_res_vol->x_s, high_res_vol->y_s, high_res_vol->z_s, high_res_vol->c_s, 
                         0, 0, 0, 1);
  }

  set_matrix(si1toi2, 1, 0, 0, -high_res_vol->width/2,
                      0, 1, 0, -high_res_vol->height/2,
                      0, 0, 1, -high_res_vol->depth/2,
                      0, 0, 0, 1);

  set_matrix(fi2toras, fct_x_r, fct_y_r, fct_z_r, fct_c_r, 
                       fct_x_a, fct_y_a, fct_z_a, fct_c_a, 
                       fct_x_s, fct_y_s, fct_z_s, fct_c_s, 
                       0, 0, 0, 1);

  set_matrix(fi1toi2, 1, 0, 0, -fct_cols/2,
                      0, 1, 0, -fct_rows/2,
                      0, 0, 1, -fct_slices/2,
                      0, 0, 0, 1);

  fctxmn = -fct_cols * fct_in_plane_resolution / 2;
  fctymn = -fct_rows * fct_in_plane_resolution / 2;
  fctzmn = -fct_slices * fct_slice_thickness / 2;

  set_matrix(si1tor, -high_res_vol->xsize, 0, 0, high_res_vol->xend, 
                    0, 0, high_res_vol->zsize, high_res_vol->zstart, 
                    0, -high_res_vol->ysize, 0, high_res_vol->yend, 
                    0, 0, 0, 1);

  set_matrix(fi1tor, -fct_in_plane_resolution, 0, 0, -fctxmn, 
                    0, 0, fct_slice_thickness, fctzmn, 
                    0, -fct_in_plane_resolution, 0, -fctymn, 
                    0, 0, 0, 1);

  MatrixMultiply(si2toras, si1toi2, itoi);
  MatrixMultiply(MatrixInverse(fi2toras, NULL), itoi, itoi);
  MatrixMultiply(MatrixInverse(fi1toi2, NULL), itoi, itoi);

  MatrixMultiply(itoi, MatrixInverse(si1tor, NULL), rm);
  MatrixMultiply(fi1tor, rm, rm);

  MatrixFree(&si1toi2);
  MatrixFree(&si2toras);
  MatrixFree(&fi2toras);
  MatrixFree(&fi1toi2);
  MatrixFree(&itoi);
  MatrixFree(&si1tor);
  MatrixFree(&fi1tor);

  return(rm);

}  /*  end make_register_matrix()  */

void set_matrix(MATRIX *m, float e11, float e12, float e13, float e14, 
                           float e21, float e22, float e23, float e24,
                           float e31, float e32, float e33, float e34,
                           float e41, float e42, float e43, float e44)
{

  *MATRIX_RELT(m, 1, 1) = e11;
  *MATRIX_RELT(m, 1, 2) = e12;
  *MATRIX_RELT(m, 1, 3) = e13;
  *MATRIX_RELT(m, 1, 4) = e14;

  *MATRIX_RELT(m, 2, 1) = e21;
  *MATRIX_RELT(m, 2, 2) = e22;
  *MATRIX_RELT(m, 2, 3) = e23;
  *MATRIX_RELT(m, 2, 4) = e24;

  *MATRIX_RELT(m, 3, 1) = e31;
  *MATRIX_RELT(m, 3, 2) = e32;
  *MATRIX_RELT(m, 3, 3) = e33;
  *MATRIX_RELT(m, 3, 4) = e34;

  *MATRIX_RELT(m, 4, 1) = e41;
  *MATRIX_RELT(m, 4, 2) = e42;
  *MATRIX_RELT(m, 4, 3) = e43;
  *MATRIX_RELT(m, 4, 4) = e44;

  return;

}  /*  end set_matrix()  */

/* EOF */
