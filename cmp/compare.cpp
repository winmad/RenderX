#ifdef __APPLE__
    #include <sys/uio.h>
#else
    #include <io.h>
#endif

#include "stdafx.h"
#include "nvVector.h"
#include "smallFuncs.h"
#include "visualizeUtils.h"
#include <string>
#include <iostream>
#include <fstream>
#include <opencv/cv.h>
#include <opencv/highgui.h>
using namespace std;

using namespace nv;

static vector<bool> volMask;

void Compare(IplImage *img1, IplImage *img2, IplImage *ref){
	int width = ref->width, height = ref->height;
	std::vector<double> var1(width*height), var2(width*height);
	std::cout << "run1 " << std::endl;
	IplImage *rst1 = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 3);
	IplImage *rst2 = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 3);
	std::cout << "run2 " << std::endl;

	double TotalVar1 = 0, TotalVar2 = 0;
	int count = 0;
	for(int x = 0; x < width; x++){
		for(int y = 0; y < height; y++){
			if (!volMask[y * width + x])
				continue;

			vec3f bgr_ref = ((vec3f*)ref->imageData)[y*width + x];
			vec3f bgr_img1 = ((vec3f*)img1->imageData)[y*width + x];
			vec3f bgr_img2 = ((vec3f*)img2->imageData)[y*width + x];
			
			var1[y*width+x] = pow((bgr_ref - bgr_img1).length(),2); 
			var2[y*width+x] = pow((bgr_ref - bgr_img2).length(),2);
            
			if (isnan(var1[y*width+x]))
				printf("(%d,%d): (%.6f,%.6f,%.6f), (%.6f,%.6f,%.6f)\n" , y , x , bgr_ref[0] , bgr_ref[1] , bgr_ref[2] ,
					bgr_img1[0] , bgr_img1[1] , bgr_img1[2]);
			if (isnan(var2[y*width+x]))
				printf("(%d,%d): (%.6f,%.6f,%.6f), (%.6f,%.6f,%.6f)\n" , y , x , bgr_ref[0] , bgr_ref[1] , bgr_ref[2] ,
					bgr_img2[0] , bgr_img2[1] , bgr_img2[2]);
            
            int index = width * y + x;
			
			TotalVar1 += pow((bgr_img1-bgr_ref).length(),2);
			TotalVar2 += pow((bgr_img2-bgr_ref).length(),2);

			count++;
		}
	}

	for(int x = 0; x < width; x++){
		for(int y = 0; y < height; y++){
			vec3f &var1C = ((vec3f*)rst1->imageData)[y*width + x];
			vec3f &var2C = ((vec3f*)rst2->imageData)[y*width + x];

			vec3f RGB = convertLuminanceToRGB((double)var1[y*width+x]*10.f);
			var1C = vec3f(255*RGB.z, 255*RGB.y, 255*RGB.x);
			RGB = convertLuminanceToRGB((double)var2[y*width+x]*10.f);
			var2C = vec3f(255*RGB.z, 255*RGB.y, 255*RGB.x);
			
		}
	}
	
	std::cout << "run3 " << std::endl;

	cvSaveImage("rst1.jpg" , rst1);
	std::cout << "run4 " << std::endl;
	cvSaveImage("rst2.jpg" , rst2);

	double Var1 = sqrt(TotalVar1 / (count)), Var2 = sqrt(TotalVar2 / (count));
	std::cout << "Var1 = " << Var1 << " Var2 = " << Var2 << std::endl;
}

double CalculateRMSE(IplImage *img1, IplImage *ref){
	int width = ref->width, height = ref->height;
	std::vector<double> var1(width*height);
	IplImage *rst1 = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 3);

	double TotalVar1 = 0;
	int count = 0;
	for(int x = 0; x < width; x++){
		for(int y = 0; y < height; y++){
			if (!volMask[y * width + x])
				continue;

			vec3f bgr_ref = ((vec3f*)ref->imageData)[y*width + x];
			vec3f bgr_img1 = ((vec3f*)img1->imageData)[y*width + x];

			var1[y*width+x] = pow((bgr_ref - bgr_img1).length(),2); 
            
			if (isnan(var1[y*width+x]))
				printf("(%d,%d): (%.6f,%.6f,%.6f), (%.6f,%.6f,%.6f)\n" , y , x , bgr_ref[0] , bgr_ref[1] , bgr_ref[2] ,
					bgr_img1[0] , bgr_img1[1] , bgr_img1[2]);

			int index = width * y + x;

			TotalVar1 += pow((bgr_img1-bgr_ref).length(),2);

			count++;
		}
	}

	for(int x = 0; x < width; x++){
		for(int y = 0; y < height; y++){
			vec3f &var1C = ((vec3f*)rst1->imageData)[y*width + x];

			vec3f RGB = convertLuminanceToRGB((double)var1[y*width+x]);
			var1C = vec3f(255*RGB.z, 255*RGB.y, 255*RGB.x);
		}
	}
	cvSaveImage("rst1.jpg" , rst1);

	double Var1 = sqrt(TotalVar1 / double(count));
	return Var1;
}

void cmpTwoImages(int argc, char* argv[])
{
	std::string file1, file2, fileRef, fileMask;
	//std::cin >> file1 >> file2 >> fileRef;
	
	IplImage *img1 = convert_to_float32(readImagePFM(argv[1]));
	IplImage *img2 = convert_to_float32(readImagePFM(argv[2]));
	IplImage *imgRef = convert_to_float32(readImagePFM(argv[3]));
	
	volMask.resize(img1->height * img1->width);
	for (int i = 0; i < img1->height * img1->width; i++)
		volMask[i] = 1;

	if (argc >= 5)
	{
		FILE *fm = fopen(argv[4] , "r");
		for (int y = img1->height - 1; y >= 0; y--)
		{
			for (int x = 0; x < img1->width; x++)
			{
				int f;
				fscanf(fm , "%d " , &f);
				volMask[y * img1->width + x] = (f == 1);
			}
		}
		fclose(fm);
	}

	Compare(img1, img2, imgRef);
}

struct Data
{
	int iter , time;
	double rmse;

	Data(int _iter , double _rmse) : iter(_iter) , rmse(_rmse) {}
	Data(int _iter , int _time , double _rmse) : iter(_iter) , time(_time) , rmse(_rmse) {}

	bool operator <(const Data& rhs)
	{
		//return iter < rhs.iter;
		return time < rhs.time;
		//return rmse < rhs.rmse;
	}
};

vector<Data> data;

void calcRMSE_cb()
{
	_finddata_t file;
	long flag;
	//string root = "D:\\Winmad\\RenderX\\x64\\results\\vol_scene\\";
	//flag = _findfirst("D:\\Winmad\\RenderX\\x64\\results\\vol_scene\\*.pfm" , &file);
	//FILE *fp = fopen("D:\\Winmad\\RenderX\\x64\\results\\vol_scene_rmse.txt" , "w");

	//IplImage *ref = convert_to_float32(readImagePFM("D:\\Winmad\\mitsuba\\VolScene\\vol_ref_10w.pfm"));

	string root = "D:\\Winmad\\RenderX\\Release\\results\\cornell_box\\";
	flag = _findfirst("D:\\Winmad\\RenderX\\Release\\results\\cornell_box\\*.pfm" , &file);
	FILE *fp = fopen("D:\\Winmad\\RenderX\\x64\\results\\cb_rmse.txt" , "w");

	IplImage *ref = convert_to_float32(readImagePFM("D:\\Winmad\\RenderX\\Release\\results\\cb_PT_ref.pfm"));

	volMask.resize(ref->height * ref->width);
	for (int i = 0; i < ref->height * ref->width; i++)
		volMask[i] = 1;

	for (;;)
	{
		printf("%s\n", file.name);
		IplImage *img = convert_to_float32(readImagePFM(root + file.name));

		double rmse = CalculateRMSE(img , ref);

		string name = file.name;
		string str = "";

		int index = -1;

		for (int i = 0; i < name.length(); i++)
		{
			if (name[i] == '_')
			{
				str = "";
				continue;
			}
			if (name[i] == '.')
			{
				index = atoi(str.c_str());
				break;
			}
			str += name[i];
		}

		cvReleaseImage(&img);

		if (index != -1)
		{
			data.push_back(Data(index , rmse));
		}

		if (_findnext(flag , &file) == -1)
			break;
	}

	sort(data.begin() , data.end());
	for (int i = 0; i < data.size(); i++)
	{
		fprintf(fp , "%d %.8lf\n" , data[i].iter , data[i].rmse);
	}
	fclose(fp);
	_findclose(flag);
}

void calcRMSEs(int argc, char* argv[])
{
	printf("%s\n%s\n%s\n" , argv[1] , argv[2] , argv[3]); 
	_finddata_t file;
	long flag;
	//string root = "D:\\Winmad\\RendererGPU\\Release\\Data\\results\\vol_ipt_6_29\\";
	string root(argv[1]);
	string filenames = root + "*.pfm";
	//flag = _findfirst("D:\\Winmad\\RendererGPU\\Release\\Data\\results\\vol_ipt_6_29\\*.pfm" , &file);
	flag = _findfirst(filenames.c_str() , &file);
	FILE *fp = fopen(argv[3] , "w");

	IplImage *ref = convert_to_float32(readImagePFM(argv[2]));

	volMask.resize(ref->height * ref->width);
	for (int i = 0; i < ref->height * ref->width; i++)
		volMask[i] = 1;

	if (argc >= 5)
	{
		string mask_filename(argv[4]);
		FILE *fm = fopen(mask_filename.c_str() , "r");
		for (int y = ref->height - 1; y >= 0; y--)
		{
			for (int x = 0; x < ref->width; x++)
			{
				int f;
				fscanf(fm , "%d " , &f);
				volMask[y * ref->width + x] = (f == 1);
			}
		}
		fclose(fm);
	}

	for (;;)
	{
		printf("%s\n", file.name);
		IplImage *img = convert_to_float32(readImagePFM(root + file.name));
		
		double rmse = CalculateRMSE(img , ref);

		string name = file.name;
		string str = "";

		int iter = -1 , time = -1;
		bool iterReady = 0 , timeReady = 0;

		for (int i = 0; i <= name.length(); i++)
		{
			if (i == name.length() || name[i] == '_' || name[i] == '.')
			{
				if (iterReady)
				{
					iter = atoi(str.c_str());
					iterReady = 0;
				}
				if (timeReady)
				{
					time = atoi(str.c_str());
					timeReady = 0;
				}
				if (str == "iter")
					iterReady = 1;
				if (str == "time")
					timeReady = 1;
				str = "";
				continue;
			}
			str += name[i];
		}

		cvReleaseImage(&img);

		if (iter != -1 && time != -1)
		{
			data.push_back(Data(iter , time , rmse));
		}

		if (_findnext(flag , &file) == -1)
			break;
	}

	sort(data.begin() , data.end());
	for (int i = 0; i < data.size(); i++)
	{
		fprintf(fp , "%d %.8lf\n" , data[i].time , data[i].rmse);
	}
	fclose(fp);
	_findclose(flag);
}

int main(int argc, char* argv[])
{
	// cmpTwoImages(argc , argv);

	// calc_RMSE <input_dir> <ref_img> <output_file> [<mask_file>]
	calcRMSEs(argc , argv); 
	
	// calcRMSE_cb();
	return 0;
}

