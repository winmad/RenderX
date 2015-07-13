#pragma once
#include <algorithm>
#include <string>
#include <iostream>
#include <fstream>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include "macros.h"
#include "nvVector.h"
#include "nvMatrix.h"

using namespace nv;

using namespace std;

inline vec3f clampVector(const vec3f &v){
	vec3f ans;
	for(int i = 0; i < 3; i++)
		ans[i] = clampf(v[i], 0, 255);
	return ans;
}

inline IplImage *readImagePFM(const string& fileName)
{
	FILE* file;
	int height, width, f;
	file = fopen(fileName.c_str(), "rb");

	fscanf(file, "PF\n%d %d\n%d\n" , &width , &height, &f);

	IplImage *img = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 3);
	float* data = (float*)img->imageData;
	for (int j=img->height-1; j>=0; j--)
	{
		for (unsigned i=0; i<img->width; i++)
		{
			fread((float*)&data[3*(j*img->width+i)+2], sizeof(float), 1, file);
			fread((float*)&data[3*(j*img->width+i)+1], sizeof(float), 1, file);
			fread((float*)&data[3*(j*img->width+i)], sizeof(float), 1, file);
			data[3*(j*img->width+i)+2] = clampf(data[3*(j*img->width+i)+2] , 0.f , 4.f);
			data[3*(j*img->width+i)+1] = clampf(data[3*(j*img->width+i)+1] , 0.f , 4.f);
			data[3*(j*img->width+i)] = clampf(data[3*(j*img->width+i)] , 0.f , 4.f);
		}
	}

	fclose(file);
	return img;
}

inline void saveImagePFM(const string& fileName, const IplImage* image)
{
	FILE* file;
	file = fopen(fileName.c_str(), "wb");

	fprintf(file, "PF\n%d %d\n-1.000000\n", image->width, image->height);

	const float* data = (float*)image->imageData;
	for(int j=image->height-1; j>=0; j--)
	{
		for(unsigned i=0; i<image->width; i++)
		{
			fwrite(&data[3*(j*image->width+i)+2], sizeof(float), 1, file);
			fwrite(&data[3*(j*image->width+i)+1], sizeof(float), 1, file);
			fwrite(&data[3*(j*image->width+i)], sizeof(float), 1, file);
		}
	}
	fclose(file);
}

inline IplImage *increaseImageBrightness(IplImage *img, int delta){
	int width = img->width, height = img->height;
	IplImage *img_new = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 3);
	for(int x = 0; x < width; x++){
		for(int y = 0; y < height; y++){
			vec3f bgr = ((vec3f*)img->imageData)[y*width + x];
			vec3f &bgr_new = ((vec3f*)img_new->imageData)[y*width + x];
			bgr_new = bgr + vec3f(delta);
			bgr_new = clampVector(bgr_new);
		}
	}
	return img_new;
}

inline IplImage *decreaseImageBrightness(IplImage *img, int delta){
	int width = img->width, height = img->height;
	IplImage *img_new = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 3);
	for(int x = 0; x < width; x++){
		for(int y = 0; y < height; y++){
			vec3f bgr = ((vec3f*)img->imageData)[y*width + x];
			vec3f &bgr_new = ((vec3f*)img_new->imageData)[y*width + x];
			bgr_new = bgr - vec3f(delta);
			bgr_new = clampVector(bgr_new);
		}
	}
	return img_new;
}

inline IplImage *IncreaseImageContrast(IplImage *img, float scopeA){
	if(scopeA < 1.f){
		std::cerr << "assert a > 1.0 " << std::endl;
		return img;
	}
	int width = img->width, height = img->height;
	IplImage *img_new = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 3);
	for(int x = 0; x < width; x++){
		for(int y = 0; y < height; y++){
			vec3f bgr = ((vec3f*)img->imageData)[y*width + x];
			vec3f &bgr_new = ((vec3f*)img_new->imageData)[y*width + x];
			bgr_new = scopeA * (bgr - vec3f(127)) + vec3f(127);
			bgr_new = clampVector(bgr_new);
		}
	}
	return img_new;
}

inline IplImage *DecreaseImageContrast(IplImage *img, float scopeA){
	if(scopeA > 1.f){
		std::cerr << "assert a < 1.0 " << std::endl;
		return img;
	}
	int width = img->width, height = img->height;
	IplImage *img_new = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 3);
	for(int x = 0; x < width; x++){
		for(int y = 0; y < height; y++){
			vec3f bgr = ((vec3f*)img->imageData)[y*width + x];
			vec3f &bgr_new = ((vec3f*)img_new->imageData)[y*width + x];
			bgr_new = scopeA * (bgr - vec3f(127)) + vec3f(127);
			bgr_new = clampVector(bgr_new);
		}
	}
	return img_new;
}

inline IplImage *GammaCorrection(IplImage *img, float gamma){
	int width = img->width, height = img->height;
	IplImage *img_new = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 3);
	for(int x = 0; x < width; x++){
		for(int y = 0; y < height; y++){
			vec3f bgr = ((vec3f*)img->imageData)[y*width + x];
			vec3f &bgr_new = ((vec3f*)img_new->imageData)[y*width + x];
			for(int i = 0; i < 3; i++){
				float invGamma = 1.0 / gamma;
				bgr_new[i] = 255.0 * std::powf(bgr[i]/255.0, invGamma);
			}
			bgr_new = clampVector(bgr_new);
		}
	}
	return img_new;
}

inline IplImage* convert_to_float32(IplImage* img)
{
	IplImage* img32f = cvCreateImage(cvGetSize(img),IPL_DEPTH_32F,img->nChannels);

	for(int i=0; i<img->height; i++)
	{
		for(int j=0; j<img->width; j++)
		{
			cvSet2D(img32f,i,j,cvGet2D(img,i,j));
		}
	}
	return img32f;
}

inline vec3f convertLuminanceToRGB(double luminance){
	if(luminance < 0.25){
		float g = luminance / 0.25;
		return vec3f(0, g, 1);
	}
	else if(luminance < 0.5){
		float b = 1 - (luminance - 0.25) / 0.25;
		return vec3f(0, 1, b);
	}
	else if(luminance < 0.75){
		float r = (luminance - 0.5) / 0.25;
		return vec3f(r, 1, 0);
	}
	else{
		float g = max(0.0 , 1 - (luminance - 0.75) / 0.25);
		return vec3f(1, g, 0);
	}

}