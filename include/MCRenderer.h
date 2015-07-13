#pragma once

#ifdef __APPLE__
    #include <sys/uio.h>
#else
    #include <io.h>
#endif

#include "Renderer.h"
#include "macros.h"
#include "TimeManager.h"
#include <omp.h>
#include <cmath>

typedef vector<Ray> Path;

class Renderer;

class MCRenderer
{
friend void mouseEvent (int evt, int x, int y, int flags, void* param);

public:

	enum {SHOW_PATH, SHOW_PIXEL_VALUE, NO_EVENT} showEvent;

	struct IntersectInfo
	{
		SceneObject *intersectObject;
		unsigned triangleID;
		float dist;
	};

	

protected:

	Renderer *renderer;

	void samplePathIter(Path& path, Ray& prevRay, unsigned depth, bool isLightPath, bool firstDiff) const;

	string savePath;

	vec4<float> connectColorProb(const Path& connectedPath, int connectIndex, bool merged = false);

protected:

	string response(const IplImage* currentImage = NULL);

	bool useMerge;

	unsigned maxDepth;

    int samplesPerPixel;
    
	unsigned pathPixelID;

	int outputIter;

	TimeManager timer;
    double runtime;
	unsigned timeStep , lastTime;

	double numEyePathsPerPixel;
	double numFullPathsPerPixel;
	omp_lock_t cmdLock;
    
	vector<Ray> showPath;

	void samplePath(Path& path, Ray& startRay, bool isLightPath) const;

	vector<Path> samplePathList(const vector<Ray>& startRayList, bool isLightPath) const;

	vector<Path> sampleMergePathList(const vector<Ray>& startRayList, bool isLightPath) const;

	void showCurrentResult(const vector<vec3f>& pixelColors , unsigned* time = NULL , unsigned* iter = NULL);

	void eliminateVignetting(vector<vec3f>& pixelColors);

	SceneObject* findInsideObject(const vec3f& origin, const vec3f& direction);

	IntersectInfo intersect(const vec3f& origin, const vec3f& direction, KDTree::Condition* condition = NULL);

	void preprocessEmissionSampler();

	void preprocessOtherSampler(bool isUniformOrigin);

	void preprocessVolumeSampler(bool isUniformOrigin , float mergeRadius);

	Ray genEmissiveSurfaceSample(bool isUniformOrigin , bool isUniformDir) const;

	Ray genOtherSurfaceSample(bool isUniformOrigin , bool isUniformDir) const;

	Ray genVolumeSample(bool isUniformDir = false) const;

	void resetInsideObject(Ray& ray);

	bool testVisibility(const Ray& startRay, const Ray& endRay);

	vector<vector<unsigned> > testPathListVisibility(const vector<Path>& startPathList, const vector<Path>& endPathList);

	void saveImagePFM(const string& fileName, const IplImage* image);

	bool connectRays(Path& path, int connectIndex, bool merged = false);

public:
	bool isDebug;

	void setMaxDepth(const int _maxDepth)
	{
		maxDepth = (unsigned)_maxDepth;
	}

    void setRuntime(const double _runtime)
    {
        runtime = _runtime;
    }

    void setSpp(const int _spp)
    {
        samplesPerPixel = _spp;
    }
    
	int getMaxDepth()
	{
		return maxDepth;
	}
    
    void setSavePath(const string& savePath);

	MCRenderer(Renderer *renderer)
	{
		this->renderer = renderer; 
		maxDepth = 20;
        samplesPerPixel = 1;
		pathPixelID = -1;
		useMerge = false;
		outputIter = 5;
		timeStep = 300;
		lastTime = 300;
	}

	virtual void preprocess(){}

	virtual vector<vec3f> renderPixels(const Camera& camera)
	{
		return vector<vec3f>(camera.generateRays().size(), vec3f(0, 0, 0));
	}

};

