#pragma once
#include "mcrenderer.h"
#include <omp.h>
#include <stack>
#include <utility>
#include <queue>
#include "PointKDTree.h"
#include "CountHashGrid.h"
#include "macros.h"
#include "SceneVPMObject.h"
#include "visualizeUtils.h"

static FILE *fp2 = fopen("debug_ipt_gather_y.txt" , "w");
static FILE *fp3 = fopen("debug_ipt_gather_var.txt" , "w");

struct IptPathState
{
	vec3f throughput , indirContrib;
	//vec3f contribs[4]; 
	Ray *ray , *lastRay , *originRay;
	int pathLen;
	vec3f pos;
	int index;
	double mergedPath;
	//double accuProb;
};

class IptTracer : public MCRenderer
{
protected:
	unsigned spp;

	int mergeIterations;
	int checkCycleIters;

	vector<vector<int> > partPathMergeIndex;

	vector<float> weights;

	int *revIndex;

	vector<IptPathState> partialSubPathList;

	vector<bool> vis;
	vector<bool> inStack;
	vector<bool> cannotBeCycle;

	queue<int> q;
	stack<int> cycle;
	vector<pair<int , int> > edgeToRemove;

	vector<bool> volMask;

	vector<double> vars;
	vector<vec3f> varColors;

	bool dfs(int depth , int cur);

	void movePaths(omp_lock_t& cmdLock , vector<Path>& , vector<Path*>&);

	void genLightPaths(omp_lock_t& cmdLock , vector<Path*>& , bool);

	void genIntermediatePaths(omp_lock_t& cmdLock , vector<Path*>&);

	void mergePartialPaths(omp_lock_t& cmdLock);

	Ray genIntermediateSamples(Scene& scene);
	Ray genIntermediateSamplesByPhotons(vector<IptPathState>& partialSubPathList ,
        PointKDTree<IptPathState>& tree , Scene& scene , int *index);

	void mergePartialPaths(vector<vec3f>& contribs , vector<double>& mergedPath , const IptPathState& interState);

	vec3f colorByMergingPaths(IptPathState& cameraState, PointKDTree<IptPathState>& partialSubPaths);

	vec3f colorByRayMarching(Path& eyeMergePath , PointKDTree<IptPathState>& partialSubPaths , int pixelID);

	vec3f colorByConnectingLights(Ray lastRay , Ray ray , bool dirIlluWeight = true);

	void sampleMergePath(Path &path, Ray &prevRay, uint depth);

	void renderPhotonMap(std::vector<vec3f>& positions , const char* fileName);

public:
	Real mergeRadius;
	Real gatherRadius;
	Real pathRatio;
	Real lightMergeKernel , interMergeKernel;
	Real lightGatherKernel , interGatherKernel;
	Real alpha;
	Real totArea , totVol;
	Real mergeRatio;
	bool useWeight , usePPM , useDirIllu , useRayMarching , checkCycle;
	bool useUniformInterSampler , useUniformSur , useUniformVol , useUniformDir;
	bool useConstantKernel;
	bool showLightPhotons , showInterPhotons;

	int pixelNum , lightPathNum , cameraPathNum , interPathNum , partialPathNum;
	int totPathNum;
	int lightPhotonNum , partialPhotonNum;

	IptTracer(Renderer* renderer) : MCRenderer(renderer)
	{ 
		alpha = 2.f / 3.f;
		spp = -1; 
		mergeRatio = 1.f;
		gatherRadius = 0.f;
		pathRatio = 0.5f;
		outputIter = 5;

		pixelNum = renderer->camera.height * renderer->camera.width;
		totPathNum = pixelNum;

		usePPM = false;
		useDirIllu = false;
		useRayMarching = true;

		useUniformSur = true;
		useUniformVol = true;
		useUniformDir = false;

		useConstantKernel = true;

		showLightPhotons = false;
		showInterPhotons = false;

		checkCycle = true;
		checkCycleIters = 100;

		isDebug = true;
	}
	void setRadius(const Real& r) { mergeRadius = r; }
	virtual vector<vec3f> renderPixels(const Camera& camera);
	
	Real connectFactor(Real pdf)
	{
		return pdf;
	}

	Real mergeFactor(Real *volScale , Real *initProb , Real *dirProb , int *pathNum)
	{
		Real s = 1.0;
		if (volScale)
			s = *volScale;
		if (initProb)
			s *= *initProb;
		if (dirProb)
			s *= *dirProb;
		if (pathNum)
			s *= (float)*pathNum;
		Real res = M_PI * mergeRadius * mergeRadius * s;
		return res * mergeRatio;
	}
	
	Real kernel(Real distSqr , Real radiusSqr)
	{
		Real s = max(0.f , 1.f - distSqr / radiusSqr);
		return 3.f * s * s / M_PI;
	}
};

struct InterSampleQuery
{
    float pdf;
    vec3f pos;

    InterSampleQuery(const vec3f& p)
    {
        pos = p;
        pdf = 0.f;
    }

    void process(IptPathState& lightState)
    {
        vec3f dir = pos - lightState.pos;
        if (std::abs(lightState.ray->getContactNormal().dot(dir)) > 1e-6) return;
        pdf += 1.f;
    }
};

struct GatherQuery
{
	vec3f color;
	std::vector<vec3f> dirColors , indirColors;
	IptTracer *tracer;
	IptPathState* cameraState;
	bool constKernel;
	double mergeNum;

	GatherQuery(IptTracer* tracer) 
	{
		color = vec3f(0.f);
		this->tracer = tracer; mergeNum = 0; constKernel = true; 
		dirColors.clear(); indirColors.clear();
	}

	void process(IptPathState& lightState)
	{
		Real volMergeScale = 1;
		if (cameraState->ray->insideObject && !cameraState->ray->contactObject)
			volMergeScale = 4.f / 3.f * tracer->gatherRadius;
		
		Real originProb = 1.f / tracer->totArea;
		Real dirProb;
		if (cameraState->ray->insideObject && cameraState->ray->contactObject == NULL)
		{
			if (lightState.ray->insideObject == NULL || 
				lightState.ray->contactObject != NULL ||
				cameraState->ray->insideObject != lightState.ray->insideObject ||
				!cameraState->ray->insideObject->canMerge ||
				!lightState.ray->insideObject->canMerge)
			{
				return;
			}
			volMergeScale = 4.f / 3.f * tracer->gatherRadius;
			originProb = 1.f / tracer->totVol;
		}
		else if (cameraState->ray->contactObject)
		{
			if (lightState.ray->contactObject != cameraState->ray->contactObject ||
				!lightState.ray->contactObject->canMerge ||
				!cameraState->ray->contactObject->canMerge)
			{
				return;
			}
		}
		else 
		{
			return;
		}
		
		Ray outRay;
		vec3f bsdfFactor;
		
		outRay = *lightState.ray;
		outRay.direction = -(cameraState->lastRay->direction);
		bsdfFactor = lightState.lastRay->getBSDF(outRay);
		if (y(bsdfFactor) < 1e-7f)
			return;

		vec3f totContrib(0.f);
		if (lightState.index < tracer->lightPhotonNum)
			totContrib = lightState.throughput;
		else
			totContrib = lightState.indirContrib;

		// !!! for experiment !!!
		//totContrib = lightState.indirContrib;
		//if (lightState.pathLen <= 1)
		//	totContrib = vec3f(0.f);

		vec3f tmp = totContrib * bsdfFactor * cameraState->throughput; 

		/*
		if (cameraState->ray->contactObject && cameraState->ray->contactObject->hasCosineTerm())
			dirProb = cameraState->ray->getContactNormal().dot(-cameraState->lastRay->direction) / M_PI;
		else
			dirProb = 0.25f / M_PI;

		Real lastPdf;
		lastPdf = lightState.lastRay->getDirectionSampleProbDensity(outRay);

		Real weightFactor = (tracer->mergeFactor(&volMergeScale , &originProb , &dirProb)) /
			(tracer->connectFactor(lastPdf) + tracer->mergeFactor(&volMergeScale , &originProb , &dirProb));
		*/

		//fprintf(fp , "weight = %.8f , cameraAccuProb = %.8f , hashOriginProb = %.8f\n" , weightFactor , cameraState->accuProb , originProb);

		//mergeNum++;

		vec3f res;
		float mergeKernel , pathNum;
		if (lightState.index < tracer->lightPhotonNum)
		{
			mergeKernel = tracer->lightGatherKernel / volMergeScale;
			pathNum = tracer->lightPathNum;
		}
		else
		{
			mergeKernel = tracer->interGatherKernel / volMergeScale;
			pathNum = tracer->partialPathNum;
		}

		if (constKernel)
		{	
			res = tmp * mergeKernel;
		}
		else
		{
			float distSqr = (cameraState->pos - lightState.pos).length();
			distSqr = distSqr * distSqr;
			mergeKernel = tracer->kernel(distSqr , tracer->gatherRadius * tracer->gatherRadius);
			if (abs(volMergeScale - 1.f) > 1e-6f)
				mergeKernel /= pathNum * tracer->gatherRadius * tracer->gatherRadius * tracer->gatherRadius;
			else 
				mergeKernel /= pathNum * tracer->gatherRadius * tracer->gatherRadius;

			res = tmp * mergeKernel;
		}
		
		//res *= weightFactor;

		if (!tracer->usePPM && tracer->useDirIllu && lightState.index < tracer->lightPhotonNum && 
			lightState.pathLen == 1 && lightState.ray->contactObject && !lightState.ray->contactObject->isVolumetric())
		{
			float dist = (lightState.lastRay->origin - lightState.ray->origin).length();
			float cosToLight = clampf(lightState.ray->getContactNormal().dot(-lightState.lastRay->direction) , 0.f , 1.f);
			float p1 = lightState.lastRay->originProb;
			float p2 = lightState.lastRay->originProb * lightState.lastRay->directionProb * cosToLight / (dist * dist) *
				M_PI * tracer->gatherRadius * tracer->gatherRadius * tracer->partialPathNum;
			float weightFactor = p2 / (p1 + p2);

			//printf("merge weight = %.8f\n" , weightFactor);

			res *= weightFactor;
		}

		if (lightState.index < tracer->lightPhotonNum)
		{
			//fprintf(fp2 , "dir , contrib = (%.4f,%.4f,%.4f) , thr = (%.4f,%.4f,%.4f)\n" , res.x , res.y , res.z ,
			//	lightState.throughput.x , lightState.throughput.y , lightState.throughput.z);
			dirColors.push_back(res * 1e3);
		}
		else
		{
			//fprintf(fp2 , "indir , contrib = (%.4f,%.4f,%.4f) , thr = (%.4f,%.4f,%.4f)\n" , res.x , res.y , res.z ,
			//	lightState.indirContrib.x , lightState.indirContrib.y , lightState.indirContrib.z);
			indirColors.push_back(res * 1e3);
		}

		color += res;
		/*
		if (cameraState->ray->insideObject)
		{
			fprintf(fp , "=====================\n");
			if (volMergeScale == 1)
				fprintf(fp , "surface\n");
			else 
				fprintf(fp , "volume\n");
			fprintf(fp , "res = (%.8f,%.8f,%.8f) \ntotContrib = (%.8f,%.8f,%.8f), bsdf = (%.8f,%.8f,%.8f),\n" ,
				res[0] , res[1] , res[2] ,
				totContrib[0] , totContrib[1] , totContrib[2] , bsdfFactor[0] , bsdfFactor[1] , bsdfFactor[2]);
		}
		*/
	}
};