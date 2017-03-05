/**
 * @file color-transfert
 * @brief transfert color from source image to target image.
 *        Method from Reinhard et al. : 
 *        Erik Reinhard, Michael Ashikhmin, Bruce Gooch and Peter Shirley, 
 *        'Color Transfer between Images', IEEE CGA special issue on 
 *        Applied Perception, Vol 21, No 5, pp 34-41, September - October 2001
 */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <bcl.h>

#define D 3

static float ID[D][D] = {
  {1, 0, 0}, 
  {0, 1, 0},  
  {0, 0, 1}
};


static float RGB2LMS[D][D] = {
  {0.3811, 0.5783, 0.0402}, 
  {0.1967, 0.7244, 0.0782},  
  {0.0241, 0.1288, 0.8444}
};

static float LMS2RGB[D][D] = {
  { 4.4679, -3.5873,  0.1193},
  {-1.2186,  2.3809, -0.1624},
  { 0.0497, -0.2439,  1.2045}
};

static float LMS2LAB[D][D] = {
  { 0.5773,  0.5773,  0.5773},
  { 0.4082,  0.4082, -0.8165},
  { 0.7071, -0.7071,       0}
};

static float LAB2LMS[D][D] = {
  { 0.5773,  0.4082,  0.7071},
  { 0.5773,  0.4082, -0.7071},
  { 0.5773, -0.8165,       0}
};


static void product_matrix_vector(float matrix[D][D], float vector1[D], float vector2[D])
{
  for (int i = 0; i < D; ++i) {
    vector2[i] = 0;
    for (int j = 0; j < D; ++j) {
      vector2[i] += matrix[i][j] * vector1[j];
    }
  }
}

static void delete_skew_RGB_to_LMS(int rows, int cols, float* values_lms)
{
  float value;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      for (int k = 0; k < 3; ++k) {
	if (values_lms == 0) {
	  value = 0;
	} else {
	  value = log10(*values_lms);
	}
	*values_lms++ = value;
      }
    }
  }
  values_lms -= rows * cols * 3;
}

static void delete_skew_LMS_to_RGB(int rows, int cols, float* values_lms)
{
  float value;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      for (int k = 0; k < 3; ++k) {
	value = pow(10, *values_lms);
	*values_lms++ = value;
      }
    }
  }
  values_lms -= rows * cols * 3;
}

static pnm float_to_pnm(int rows, int cols, float* values_ims)
{
  pnm ims = pnm_new(cols, rows, PnmRawPpm);
  unsigned short value;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      for (int k = 0; k < 3; ++k) {
	value = (unsigned short) *values_ims++;
	pnm_set_component(ims, i, j, k, value);
      }
    }
  }
  values_ims -= rows * cols * 3;

  return ims;
}

static float* pnm_to_float(pnm ims)
{
  int rows = pnm_get_height(ims);
  int cols = pnm_get_width(ims);

  float* values_ims = malloc(rows * cols * D* sizeof(float));
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      for (int k = 0; k < 3; ++k) {
	*values_ims++ = pnm_get_component(ims, i, j, k);
      }
    }
  }
  values_ims -= rows * cols * D;
  
  return values_ims;
}


static void A_to_B(int rows, int cols, float* ims, float* imd, float matrix[D][D])
{
  float* vectorA = malloc(D * sizeof(float));
  float* vectorB = malloc(D * sizeof(float));
  
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      for (int k = 0; k < 3; ++k) {
	*vectorA++ = *ims++;
      }
      vectorA -= 3;
      product_matrix_vector(matrix, vectorA, vectorB);
      for (int k = 0; k < 3; ++k) {
	*imd++ = *vectorB++;
      }
      vectorB -= 3;
    }
  }
  
  ims -= rows * cols * D;
  imd -= rows * cols * D;

  free(vectorA);
  free(vectorB);
}

static float* convertRGBtoLAB(int rows, int cols, pnm ims)
{
  float* values_RGB = pnm_to_float(ims);
  
  float* values_LMS = malloc(rows * cols * D * sizeof(float));
  A_to_B(rows, cols, values_RGB, values_LMS, RGB2LMS);
  delete_skew_RGB_to_LMS(rows, cols, values_LMS);
  
  float* values_LAB = malloc(rows * cols * D * sizeof(float));
  A_to_B(rows, cols, values_LMS, values_LAB, LMS2LAB);

  free(values_RGB);
  free(values_LMS);
  
  return values_LAB;
}

static pnm convertLABtoRGB(int rows, int cols, float* values_LAB)
{
  float* values_LMS = malloc(rows * cols * D * sizeof(float));
  A_to_B(rows, cols, values_LAB, values_LMS, LAB2LMS);
  delete_skew_LMS_to_RGB(rows, cols, values_LMS);

  float* values_RGB = malloc(rows * cols * D * sizeof(float));
  A_to_B(rows, cols, values_LMS, values_RGB, LMS2RGB);

  pnm imd = float_to_pnm(rows, cols, values_RGB);
  
  free(values_LMS);
  free(values_RGB);

  return imd;
}

static void compute_mean(int rows, int cols, float *values)
{
  float values_mean[3] = {0,0,0};
  
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      for (int k = 0; k < 3; ++k) {
	values_mean[k] += *values++;
      }
    }
  }
  values -= rows * cols * D;
}

static void process(char *name_ims, char *name_imt, char* name_imd)
{
  pnm ims = pnm_load(name_ims);
  int rows = pnm_get_height(ims);
  int cols = pnm_get_width(ims);
  //pnm imt = pnm_load(name_imt);

  float* values_LAB_ims = convertRGBtoLAB(rows, cols, ims);
  //float* values_LAB_imt = convertRGBtoLAB(rows, cols, imt);

  pnm imd = convertLABtoRGB(rows, cols, values_LAB_ims);
  pnm_save(imd, PnmRawPpm, name_imd);

  (void) ID;
  (void) name_imt;
}

void usage (char *s){
  fprintf(stderr, "Usage: %s <ims> <imt> <imd> \n", s);
  exit(EXIT_FAILURE);
}

#define param 3
int main(int argc, char *argv[]){
  if (argc != param+1) 
    usage(argv[0]);
  process(argv[1], argv[2], argv[3]);
  return EXIT_SUCCESS;
}
