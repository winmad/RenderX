#include "StdAfx.h"
#include "IptTracer.h"
#include "SceneEmissiveObject.h"
#include "UniformSphericalSampler.h"
#include "NoSelfIntersectionCondition.h"
#include "SceneVPMObject.h"
#include "SceneHeteroGeneousVolume.h"
#include "colormap.h"
#include <algorithm>

static FILE *fp = fopen("debug_ipt_inter.txt" , "w");
static FILE *fp1 = fopen("debug_ipt_light.txt" , "w");
static FILE *err = fopen("error_report.txt" , "w");
static FILE *fm = fopen("mask.txt" , "w");

float INV_2_PI = 0.5 / M_PI;

vector<vec3f> IptTracer::renderPixels(const Camera& camera)
{
	if (!usePPM)
	{
		lightPathNum = totPathNum * pathRatio;
		interPathNum = totPathNum * (1.f - pathRatio);
		partialPathNum = interPathNum;

		mergeIterations = maxDepth;
		useWeight = true;
	}
	else
	{
		lightPathNum = totPathNum;
		interPathNum = totPathNum;
		partialPathNum = interPathNum;

		mergeIterations = 0;
		useWeight = false;
	}

	//!!! for experiment !!!
	//useWeight = false;

	cameraPathNum = pixelNum;
	
	useUniformInterSampler = (useUniformSur && useUniformVol);

	vector<vec3f> pixelColors(camera.width * camera.height, vec3f(0, 0, 0));

	vars.resize(camera.width * camera.height , 0.0);
	varColors.resize(camera.width * camera.height , vec3f(0, 0, 0));

	vector<omp_lock_t> pixelLocks(pixelColors.size());
	volMask.resize(camera.width * camera.height , true);

	preprocessEmissionSampler();
	preprocessOtherSampler(useUniformSur);
	preprocessVolumeSampler(useUniformVol , mergeRadius * 0.1f);
	
	for(int i=0; i<pixelLocks.size(); i++)
	{
		omp_init_lock(&pixelLocks[i]);
	}

	omp_init_lock(&cmdLock);

	if (gatherRadius < 1e-6f)
		gatherRadius = mergeRadius;
	Real r0 = mergeRadius;
	Real gr0 = gatherRadius;

	totArea = renderer->scene.getTotalArea();
	totVol = renderer->scene.getTotalVolume();
	printf("scene: totArea = %.8f, totVol = %.8f\n" , totArea , totVol);

	// abandon surface
	//totArea = 0.f;

	if (totVol > 1e-6f && totArea > 1e-6f)
		partialPathNum = interPathNum / 2;

	//unsigned startTime = clock();
    timer.StartStopWatch();

	for(unsigned s=0; s<spp; s++)
	{
		partPathMergeIndex.resize(interPathNum);

		partialSubPathList.clear();
		/*
		float shrinkRatio;
		if (totVol > 1e-6f)
			shrinkRatio = powf(((float)s + alpha) / ((float)s + 1.f) , 1.f / 3.f);
		else
			shrinkRatio = powf(((float)s + alpha) / ((float)s + 1.f) , 1.f / 2.f);
		if (s > 0)
			mergeRadius *= shrinkRatio;
		*/

		float base;
		if (useUniformInterSampler)
			base = (float)s + 1.f;
		else 
			base = (float)s;

		if (totVol > 1e-6f)
		{
			mergeRadius = r0 * powf(powf(max(base , 1.f) , alpha - 1.f) , 1.f / 3.f);
			gatherRadius = gr0 * powf(powf(max(base , 1.f) , alpha - 1.f) , 1.f / 3.f);
		}
		else
		{
			mergeRadius = r0 * powf(powf(max(base , 1.f) , alpha - 1.f) , 1.f / 2.f);
			gatherRadius = gr0 * powf(powf(max(base , 1.f) , alpha - 1.f) , 1.f / 2.f);
		}
        
		//mergeRadius = r0; // not reduce radius
		mergeRadius = std::max(mergeRadius , 1e-7f);

        //gatherRadius = gr0; // not reduce radius
		gatherRadius = std::max(gatherRadius , 1e-7f);

		printf("mergeRadius = %.8f, gatherRadius = %.8f\n" , mergeRadius , gatherRadius);

		vector<vec3f> singleImageColors(pixelColors.size(), vec3f(0, 0, 0));

		string cmd;

		//unsigned t = clock();
        timer.PushCurrentTime();

		vector<Path*> lightPathList(lightPathNum , NULL);
		vector<Path*> interPathList(interPathNum, NULL);

		interMergeKernel = 1.f / (M_PI * mergeRadius * 
			mergeRadius * (Real)partialPathNum);
		lightMergeKernel = 1.f / (M_PI * mergeRadius *
			mergeRadius * (Real)lightPathNum);

		interGatherKernel = 1.f / (M_PI * gatherRadius * 
			gatherRadius * (Real)partialPathNum);
		lightGatherKernel = 1.f / (M_PI * gatherRadius *
			gatherRadius * (Real)lightPathNum);

		if (!renderer->scene.usingGPU())
		{
			genLightPaths(cmdLock , lightPathList , (s == 0));

			if (!useUniformInterSampler)
			{
				if (!useUniformSur)
					renderer->scene.beginUpdateOtherSampler(s);
				if (!useUniformVol)
					renderer->scene.beginUpdateVolumeSampler(s);
				for (int i = 0; i < partialSubPathList.size(); i++)
				{
					IptPathState& lightState = partialSubPathList[i];
					if (lightState.ray->contactObject && !useUniformSur)
					{
						renderer->scene.updateOtherSampler(lightState.ray->contactObject->objectIndex ,
							lightState.ray->contactObjectTriangleID , s , lightState.throughput / (Real)lightPathNum);
					}
					else if (lightState.ray->insideObject && lightState.ray->insideObject->isVolumetric() &&
						!lightState.ray->contactObject && !useUniformVol)
					{
						vec3f thr = lightState.throughput;
						renderer->scene.updateVolumeSampler(lightState.ray->insideObject->objectIndex ,
							lightState.ray->origin , s , thr / (Real)lightPathNum);
					}
				}
				if (!useUniformSur)
					renderer->scene.endUpdateOtherSampler();
				if (!useUniformVol)
					renderer->scene.endUpdateVolumeSampler();

				/*
				Scene::SurfaceSampler *interSampler = renderer->scene.otherSurfaceSampler;
				fprintf(fp , "totWeight = %.8f\n" , interSampler->totalWeight);
				for (int i = 0; i < interSampler->targetObjects.size(); i++)
				{
					SceneObject *obj = interSampler->targetObjects[i];
					fprintf(fp , "======= objId = %d , totEnergy = %.8f , weight = %.8f =======\n" , obj->objectIndex ,
						obj->totalEnergy , obj->weight);
					for (int j = 0; j < obj->getTriangleNum(); j++)
					{
						fprintf(fp , "triId = %d , e = %.8f\n" , j , obj->energyDensity[j]);
					}
				}
				
				Scene::VolumeSampler *interVolSampler = renderer->scene.volumeSampler;
				fprintf(fp , "totWeight = %.8f\n" , interVolSampler->totalWeight);
				for (int i = 0; i < interVolSampler->targetObjects.size(); i++)
				{
					SceneObject *obj = interVolSampler->targetObjects[i];
					fprintf(fp , "======= objId = %d , totEnergy = %.8f , weight = %.8f =======\n" , obj->objectIndex ,
						obj->countHashGrid->sumWeights , obj->volumeWeight);
					for (int j = 0; j < obj->countHashGrid->effectiveWeights.size(); j++)
					{
						fprintf(fp , "cellIndex = %d , e = %.8f\n" , obj->countHashGrid->effectiveIndex[j] , 
							obj->countHashGrid->effectiveWeights[j]);
					}
				}
				*/
				if (s == 0)
					continue;
			}

			if (!useUniformSur && showLightPhotons)
			{
				std::vector<vec3f> positions;
				for (int i = 0; i < partialSubPathList.size(); i++)
				{
					if (partialSubPathList[i].ray->contactObject != NULL)
						positions.push_back(partialSubPathList[i].pos);
				}
				renderPhotonMap(positions , "light_photons.pfm");
			}

			if (!usePPM)
			{
				genIntermediatePaths(cmdLock , interPathList);
			}
			
			printf("lightPhotonNum = %d, partialPhotonNum = %d\n" , lightPhotonNum , partialPhotonNum);

			mergePartialPaths(cmdLock);

			PointKDTree<IptPathState> partialSubPaths(partialSubPathList);
				
			/*
			for (int i = 0; i < partialPhotonNum; i++)
			{
				IptPathState& subPath = partialSubPathList[i];
				if ((useUniformInterSampler && s == 0) || (!useUniformInterSampler && s == 1))
				{
					vec3f contrib;
					if (i < lightPhotonNum)
					{
						contrib = subPath.throughput;
						fprintf(fp1 , "==============\n");
						fprintf(fp1 , "dirContrib=(%.8f,%.8f,%.8f), pathNum = %.1lf\n" , contrib.x , contrib.y , contrib.z , subPath.mergedPath);
					}
					else
					{
						contrib = subPath.indirContrib;
						if (intensity(contrib) < 1e-6f)
							continue;
						fprintf(fp , "==============\n");
						fprintf(fp , "indirContrib=(%.8f,%.8f,%.8f), pathNum = %.1lf\n" , contrib.x , contrib.y , contrib.z , subPath.mergedPath);
					}			
				}
			}
			*/
			double maxVar = 0.0;

			numEyePathsPerPixel = 0;
			numFullPathsPerPixel = 0;

#pragma omp parallel for
            for (int p = 0; p < pixelNum; p++)
			{
				//fprintf(fp2 , "========== pixel id = %d ==========\n" , p);
				//fprintf(fp3 , "========== pixel id = %d ==========\n" , p);

				// disable ray marching
				/*
                for (int sid = 0; sid < samplesPerPixel; sid++)
                {
                    Path eyePath;
                    Ray cameraRay = camera.generateRay(p , sid , samplesPerPixel);
                    //Ray cameraRay = camera.generateRay(p);
                    sampleMergePath(eyePath , cameraRay , 0);
                    singleImageColors[p] += colorByRayMarching(eyePath , partialSubPaths , p);

					// Lock
					//maxVar = max(vars[p] , maxVar);
                }
                singleImageColors[p] /= (float)samplesPerPixel;
				*/
				
				Path eyePath;
				Ray cameraRay = camera.generateRay(p);
				samplePath(eyePath, cameraRay, false);
				if (eyePath.size() <= 1)
					continue;

				IptPathState cameraState;
				bool lastSpecular = 1;
				Real lastPdfW = 1.f;

				cameraState.throughput = vec3f(1.f) / (eyePath[0].originProb * eyePath[0].directionProb * eyePath[1].originProb);

				cameraState.index = eyePath.front().pixelID;

				//fprintf(fp , "===================\n");
				//for (int i = 0; i < eyePath.size(); i++)
				//{
				//	fprintf(fp , "l=%d, bsdf=(%.8f,%.8f,%.8f), originPdf=%.8f, dirPdf=%.8f\n" , i , eyePath[i].color.x ,
				//		eyePath[i].color.y , eyePath[i].color.z , eyePath[i].originProb , eyePath[i].directionProb);
				//}

				vec3f colorHitLight(0.f);

				for(unsigned i = 1; i < eyePath.size(); i++)
				{
					vec3f colorGlbIllu(0.f) , colorDirIllu(0.f);

					Real dist = std::max((eyePath[i].origin - eyePath[i - 1].origin).length() , 1e-5f);
					cameraState.throughput *= eyePath[i - 1].getRadianceDecay(dist);

					if (eyePath[i].contactObject && eyePath[i].contactObject->emissive())
					{
						vec3f contrib = ((SceneEmissiveObject*)(eyePath[i].contactObject))->getColor();
// 						float dirPdfA = eyePath[i].contactObject->getEmissionWeight() / eyePath[i].contactObject->totalArea;
// 						float mis = 1.f;
// 						if (i > 1 && !lastSpecular)
// 						{
// 							float cosine = eyePath[i].getContactNormal().dot(-eyePath[i - 1].direction);
// 							float dist = (eyePath[i].origin - eyePath[i - 1].origin).length();
// 							float dirPdfW = dirPdfA * dist * dist / abs(cosine);
// 							mis = lastPdfW / (lastPdfW + dirPdfW);
// 
// 							//fprintf(fp , "==================\n");
// 							//fprintf(fp , "thr=(%.6f,%.6f,%.6f), contrib=(%.6f,%.6f,%.6f), pdfA=%.6f, pdfW=%.6f, lastPdfW=%.6f, cosine=%.6f, mis=%.6f\n" , 
// 							//	cameraState.throughput[0] , cameraState.throughput[1] , cameraState.throughput[2] , contrib[0] ,
// 							//	contrib[1] , contrib[2] , dirPdfA , dirPdfW , lastPdfW , cosine , mis);
// 
// 						}

						colorHitLight = cameraState.throughput * contrib;

						singleImageColors[cameraState.index] += colorHitLight;

// 						omp_set_lock(&cmdLock);
// 						numEyePathsPerPixel += 1;
// 						numFullPathsPerPixel += 1;
// 						omp_unset_lock(&cmdLock);

						break;
					}

					cameraState.pos = eyePath[i].origin;
					cameraState.lastRay = &eyePath[i - 1];
					cameraState.ray = &eyePath[i];

					if (eyePath[i].directionSampleType == Ray::RANDOM)
					{
						// mis with colorHitLight
						//colorDirIllu = colorByConnectingLights(eyePath[i - 1] , eyePath[i]) * cameraState.throughput;

						colorGlbIllu = colorByMergingPaths(cameraState , partialSubPaths);
						
						singleImageColors[cameraState.index] += colorGlbIllu;

						break;
					}

					lastSpecular = (eyePath[i].directionSampleType == Ray::DEFINITE);
					lastPdfW = eyePath[i].directionProb;

					if (eyePath[i].direction.length() < 0.5f)
						break;

					if (i >= eyePath.size() - 1)
						break;

					cameraState.throughput *= (eyePath[i].color * eyePath[i].getCosineTerm()) / 
						(eyePath[i + 1].originProb * eyePath[i].directionProb);

					//fprintf(fp , "l=%d, thr=(%.8f,%.8f,%.8f), bsdf=(%.8f,%.8f,%.8f), cos=%.8f, prob=%.8f\n" , 
					//	i , cameraState.throughput[0] , cameraState.throughput[1] , cameraState.throughput[2] ,
					//	bsdfFactor[0] , bsdfFactor[1] , bsdfFactor[2] , eyePath[i].getCosineTerm() , eyePath[i].directionProb);
				}

				// try eye path length >= 1
				/*
				samplePath(eyePath, camera.generateRay(p));
				if (eyePath.size() <= 1)
					continue;

				IptPathState cameraState;
				bool lastSpecular = 1;
				Real lastPdfW = 1.f;

				cameraState.throughput = vec3f(1.f) / (eyePath[0].originProb * eyePath[0].directionProb * eyePath[1].originProb);
	
				cameraState.index = eyePath.front().pixelID;

				vector<float> weights;

				//fprintf(fp , "===================\n");
				//for (int i = 0; i < eyePath.size(); i++)
				//{
				//	fprintf(fp , "l=%d, bsdf=(%.8f,%.8f,%.8f), originPdf=%.8f, dirPdf=%.8f\n" , i , eyePath[i].color.x ,
				//		eyePath[i].color.y , eyePath[i].color.z , eyePath[i].originProb , eyePath[i].directionProb);
				//}

				int nonSpecLength = 0;
				vector<vec3f> mergeContribs;
				mergeContribs.clear();

				float weightFactor = 1.f;
				vec3f colorHitLight(0.f);

				int N = maxDepth;
				nonSpecLength = 0;

				for(unsigned i = 1; i < eyePath.size(); i++)
				//for (unsigned i = 1; i < 2; i++)
				{
					vec3f colorGlbIllu(0.f) , colorDirIllu(0.f);

					Real dist = std::max((eyePath[i].origin - eyePath[i - 1].origin).length() , 1e-5f);
					cameraState.throughput *= eyePath[i - 1].getRadianceDecay(dist);

					if (eyePath[i].contactObject && eyePath[i].contactObject->emissive())
					{
						vec3f contrib = ((SceneEmissiveObject*)(eyePath[i].contactObject))->getColor();
						float dirPdfA = eyePath[i].contactObject->getEmissionWeight() / eyePath[i].contactObject->totalArea;
						float mis = 1.f;
						if (i > 1 && !lastSpecular)
						{
							float cosine = eyePath[i].getContactNormal().dot(-eyePath[i - 1].direction);
							float dist = (eyePath[i].origin - eyePath[i - 1].origin).length();
							float dirPdfW = dirPdfA * dist * dist / abs(cosine);
							mis = lastPdfW / (lastPdfW + dirPdfW);
							
							//fprintf(fp , "==================\n");
							//fprintf(fp , "thr=(%.6f,%.6f,%.6f), contrib=(%.6f,%.6f,%.6f), pdfA=%.6f, pdfW=%.6f, lastPdfW=%.6f, cosine=%.6f, mis=%.6f\n" , 
							//	cameraState.throughput[0] , cameraState.throughput[1] , cameraState.throughput[2] , contrib[0] ,
							//	contrib[1] , contrib[2] , dirPdfA , dirPdfW , lastPdfW , cosine , mis);
							
						}
						
						colorHitLight = cameraState.throughput * contrib * mis;

						if (N > 0)
							weightFactor = 1.f - (Real)nonSpecLength / (Real)N;
						else
							weightFactor = 1.f;

						singleImageColors[cameraState.index] += colorHitLight * weightFactor;

						break;
					}

					//if (N == 0)
					//	printf("%d , error\n" , i);

					cameraState.pos = eyePath[i].origin;
					cameraState.lastRay = &eyePath[i - 1];
					cameraState.ray = &eyePath[i];

					if (eyePath[i].directionSampleType == Ray::RANDOM)
					{
						// mis with colorHitLight
						colorDirIllu = colorByConnectingLights(eyePath[i - 1] , eyePath[i]) * cameraState.throughput;
						weightFactor = 1.f - ((Real)nonSpecLength + 1.f) / (Real)N;
						colorDirIllu *= weightFactor;

						colorGlbIllu = colorByMergingPaths(cameraState , partialSubPaths);
						mergeContribs.push_back(colorDirIllu + colorGlbIllu / (Real)N);
					}

					lastSpecular = (eyePath[i].directionSampleType == Ray::DEFINITE);
					lastPdfW = eyePath[i].directionProb;

					if (eyePath[i].directionSampleType == Ray::RANDOM)
					{
						nonSpecLength++;
						
						if (nonSpecLength == N)
							break; // PPM, eye path length is 1
					}

					if (eyePath[i].direction.length() < 0.5f)
						break;

					if (i >= eyePath.size() - 1)
						break;
				
					cameraState.throughput *= (eyePath[i].color * eyePath[i].getCosineTerm()) / 
						(eyePath[i + 1].originProb * eyePath[i].directionProb);

					//fprintf(fp , "l=%d, thr=(%.8f,%.8f,%.8f), bsdf=(%.8f,%.8f,%.8f), cos=%.8f, prob=%.8f\n" , 
					//	i , cameraState.throughput[0] , cameraState.throughput[1] , cameraState.throughput[2] ,
					//	bsdfFactor[0] , bsdfFactor[1] , bsdfFactor[2] , eyePath[i].getCosineTerm() , eyePath[i].directionProb);
				}
				
				for (int i = 0; i < mergeContribs.size(); i++)
				{
					singleImageColors[cameraState.index] += mergeContribs[i];
				}
				*/
			}
			/*
			IplImage *varImg = cvCreateImage(cvSize(camera.width, camera.height), IPL_DEPTH_32F, 3);
			for (int p = 0; p < pixelNum; p++)
			{
				varColors[p] = convertLuminanceToRGB((double)vars[p] / 3000);
			}
			for(int x=0; x<renderer->camera.width; x++)
			{
				for(unsigned y=0; y<renderer->camera.height; y++)
				{
					vec3f rgb = varColors[y*renderer->camera.width + x];
					vec3f &bgr = ((vec3f*)varImg->imageData)[y*renderer->camera.width + x];
					bgr = vec3f(rgb.z, rgb.y, rgb.x);
				}
			}
			//saveImagePFM("vars.pfm" , varImg);
			cvReleaseImage(&varImg);
			*/
		}

		printf("done calculation, release memory\n");

		if(cmd == "exit")
			return pixelColors;

		for(int i=0; i<pixelColors.size(); i++)
		{
			if (!isIllegal(singleImageColors[i]))
			{
				if (useUniformInterSampler)
				{
					pixelColors[i] *= (Real)s / ((Real)s + 1.f);
					pixelColors[i] += singleImageColors[i] / ((Real)s + 1.f); 
				}
				else
				{
					pixelColors[i] *= ((Real)s - 1.f) / ((Real)s);
					pixelColors[i] += singleImageColors[i] / ((Real)s); 
				}
			}
			else
			{

				fprintf(err , "(%.8f,%.8f,%.8f) occurs in iter %d\n" , singleImageColors[i].x ,
					singleImageColors[i].y , singleImageColors[i].z , s);
				continue;
			}
		}

		if (!renderer->scene.usingGPU())
		{
			for (int i = 0; i < lightPathNum; i++)
			{
				if (lightPathList[i])
					delete lightPathList[i];
			}

			for (int i = 0; i < interPathNum; i++)
			{
				if (interPathList[i])
					delete interPathList[i];
			}
		}
		else
		{
			for (int i = 0; i < lightPathNum; i++)
				lightPathList[i] = NULL;
			for (int i = 0; i < interPathNum; i++)
				interPathList[i] = NULL;
		}

// 		printf("AvgPathSamplesPerPixel = %.6f\n" , numFullPathsPerPixel / (double)pixelNum);
// 		printf("AvgPathSamplesPerEyePath = %.6f\n" , numFullPathsPerPixel / numEyePathsPerPixel);
		printf("Iter: %d  IterTime: %.3lfs  TotalTime: %.3lfs\n", s, timer.PopCurrentTime(),
            timer.GetElapsedTime(0));

// 		if ((useUniformInterSampler && s == 0) || (!useUniformInterSampler && s == 1))
// 		{
// 			for (int y = camera.height - 1; y >= 0; y--)
// 			{
// 				for (int x = 0; x < camera.width; x++)
// 				{
// 					if (volMask[y * camera.width + x])
// 						fprintf(fm , "1 ");
// 					else
// 						fprintf(fm , "0 ");
// 				}
// 				fprintf(fm , "\n");
// 			}
// 		}

		//if (clock() / 1000 >= lastTime)
		unsigned nowTime = (unsigned)timer.GetElapsedTime(0);
		if (nowTime >= lastTime && !isDebug)
		{
			showCurrentResult(pixelColors , &nowTime , &s);
			//showCurrentResult(pixelColors , &lastTime , &s);
			lastTime += timeStep;
		}
		else
			showCurrentResult(pixelColors);

        if (timer.GetElapsedTime(0) > runtime) break;
	}	

	for(int i=0; i<pixelLocks.size(); i++)
	{
		omp_destroy_lock(&pixelLocks[i]);
	}
	omp_destroy_lock(&cmdLock);

	return pixelColors;
}

void IptTracer::genLightPaths(omp_lock_t& cmdLock , vector<Path*>& lightPathList , bool isFirstIter)
{
#pragma omp parallel for
	for(int p=0; p<lightPathNum; p++)
	{
		if (!renderer->scene.usingGPU())
		{
			Ray lightRay = genEmissiveSurfaceSample(true , false);
			lightPathList[p] = new Path;
			samplePath(*lightPathList[p] , lightRay , true);
		}
		
		Path& lightPath = *lightPathList[p];

		if (lightPath.size() <= 1)
			continue;

		IptPathState lightState;
		lightState.originRay = &lightPath[0];

		Real cosAtLight = lightPath[0].getCosineTerm();

		lightState.throughput = lightPath[0].color * cosAtLight / 
			(lightPath[0].originProb * lightPath[0].directionProb *
			lightPath[1].originProb);
		lightState.indirContrib = vec3f(0.0);
		lightState.mergedPath = 1;
		/*
		fprintf(fp , "====================\n");
		vec3f decay = lightPath[0].getRadianceDecay((lightPath[0].origin - lightPath[1].origin).length());
		fprintf(fp , "l = 0 , thr = (%.8f,%.8f,%.8f) , color = (%.8f,%.8f,%.8f)\ncosine = %.8f , dirPdf = %.8f , oPdf = %.8f\ndecay=(%.8f,%.8f,%.8f)\n" , 
			lightState.throughput.x , lightState.throughput.y , lightState.throughput.z ,
			lightPath[0].color[0] , lightPath[0].color[1] , lightPath[0].color[2] ,
			lightPath[0].getCosineTerm() , lightPath[0].directionProb , lightPath[1].originProb ,
			decay.x , decay.y , decay.z);
		*/
		int nonSpecPathLength = 0;

		for(unsigned i = 1; i < lightPath.size(); i++)
		//for (unsigned i = 1; i < 2; i++)
		{
			Real dist = std::max((lightPath[i].origin - lightPath[i - 1].origin).length() , 1e-5f);
			vec3f decayFactor = lightPath[i - 1].getRadianceDecay(dist);
			lightState.throughput *= decayFactor;

			if(lightPath[i].contactObject && lightPath[i].contactObject->emissive())
				break;

			lightState.pos = lightPath[i].origin;
			lightState.lastRay = &lightPath[i - 1];
			lightState.ray = &lightPath[i];
			lightState.pathLen = i;

			if(lightPath[i].directionSampleType == Ray::RANDOM &&
				(lightPath[i].insideObject != NULL || lightPath[i].contactObject != NULL) && 
				(lightPath[i].origin != lightPath[i - 1].origin))
			{
				//if (lightPath[i].insideObject && !lightPath[i].contactObject)
				//	fprintf(fp , "path length = %d, dirContrib = (%.8f,%.8f,%.8f)\n" , 
				//		i , lightState.dirContrib[0] , lightState.dirContrib[1] , lightState.dirContrib[2]);

				omp_set_lock(&cmdLock);
				partialSubPathList.push_back(lightState);
				omp_unset_lock(&cmdLock);
			}

			if (i == lightPath.size() - 1)
				break;
			if (lightPath[i].direction.length() < 0.5f)
				break;

			vec3f scatterFactor = (lightPath[i].color * lightPath[i].getCosineTerm() / 
				(lightPath[i + 1].originProb * lightPath[i].directionProb));

			lightState.throughput *= scatterFactor;
			/*
			vec3f decay = lightPath[i].getRadianceDecay((lightPath[i].origin - lightPath[i + 1].origin).length());
			fprintf(fp , "l = %d , thr = (%.8f,%.8f,%.8f) , color = (%.8f,%.8f,%.8f)\ncosine = %.8f , dirPdf = %.8f , oPdf = %.8f\ndecay=(%.8f,%.8f,%.8f)\n" , 
				i , lightState.throughput.x , lightState.throughput.y , lightState.throughput.z ,
				lightPath[i].color[0] , lightPath[i].color[1] , lightPath[i].color[2] ,
				lightPath[i].getCosineTerm() , lightPath[i].directionProb , lightPath[i + 1].originProb ,
				decay.x , decay.y , decay.z);
			*/
			if (lightPath[i].directionSampleType == Ray::RANDOM && useWeight)
			{
				Real pdf = lightPath[i].directionProb;
				if (pdf < 1e-7f)
					break;

				Real weightFactor;

				Real volMergeScale = 1;
				Real originProb;
				Real dirProb;
				if (lightPath[i].contactObject)
				{
					if (isFirstIter || useUniformSur)
						originProb = 1.f / totArea;
					else
						originProb = lightPath[i].contactObject->getOriginProb(lightPath[i].contactObjectTriangleID);
					if (useUniformDir)
						dirProb = INV_2_PI;
					else
						dirProb = lightPath[i].getCosineTerm() / M_PI;
				}

				//if (lightPath[i].insideObject && lightPath[i].contactObject)
				//	printf("!!!\n");
				if (lightPath[i].insideObject && !lightPath[i].contactObject && lightPath[i].insideObject->isVolumetric())
				{
					volMergeScale = 4.f / 3.f * mergeRadius;

					if (isFirstIter || useUniformVol)
						originProb = 1.f / totVol;
					else
						originProb = lightPath[i].insideObject->getOriginProb(lightPath[i].origin);
					
					dirProb = 0.25f / M_PI;
				}

				Real wc = 1 / (M_PI * mergeRadius * mergeRadius);
				Real wm = originProb;
				//weightFactor = wc / (wc + wm * lightPathNum);
				weightFactor = connectFactor(pdf) /
					(connectFactor(pdf) + mergeFactor(&volMergeScale , &originProb , &dirProb , &lightPathNum));

				if (isnan(weightFactor) || abs(pdf) < 1e-6f)
				{
					fprintf(err , "sample light path error, %.8f , %.8f\n" , connectFactor(pdf) , 
						mergeFactor(&volMergeScale , &originProb , &dirProb , &lightPathNum));
				}
				/*
				if (abs(volMergeScale - 1.f) < 1e-6)
					printf("surface %.8f\n" , weightFactor);
				else
					printf("volume %.8f %.8f\n" , weightFactor);
				*/

				//if (lightPath[i].contactObject && lightPath[i].contactObject->objectIndex == 7)
				lightState.throughput *= weightFactor;
			}
		}
	}

	lightPhotonNum = partialPhotonNum = partialSubPathList.size();
}

Ray IptTracer::genIntermediateSamplesByPhotons(vector<IptPathState>& partialSubPathList ,
    PointKDTree<IptPathState>& tree , Scene& scene , int *index)
{
    // handle surface scene only
	for (;;) {
	float randWeight = RandGenerator::genFloat();
	int pathId = (lower_bound(weights.begin() , weights.end() , randWeight) - weights.begin()) - 1;
	pathId = clamp(pathId , 0 , partialSubPathList.size() - 1);
	IptPathState& lightState = partialSubPathList[pathId];
    float chooseProb;
	while (lightState.ray == NULL ||
		// (lightState.ray->insideObject && !lightState.ray->insideObject->canMerge) ||
		(!lightState.ray->insideObject && lightState.ray->contactObject && !lightState.ray->contactObject->canMerge))
	{
		randWeight = RandGenerator::genFloat();
		pathId = (lower_bound(weights.begin() , weights.end() , randWeight) - weights.begin()) - 1;
		pathId = clamp(pathId , 0 , partialSubPathList.size() - 1);
		lightState = partialSubPathList[pathId];
	}
		if (pathId == 0)
    {
        chooseProb = weights[pathId];
    }
    else
    {
        chooseProb = weights[pathId] - weights[pathId - 1];
    }
	// if (*index && partPathMergeIndex[*index].size() > 0)
		// lightState = partialSubPathList[partPathMergeIndex[*index][0]];

	Ray ray;
	ray.originSampleType = Ray::RANDOM;
	ray.directionSampleType = Ray::RANDOM;

	// ray.insideObject = lightState.ray->insideObject;
	ray.contactObject = lightState.ray->contactObject;
	ray.contactObjectTriangleID = lightState.ray->contactObjectTriangleID;
    ray.origin = lightState.pos;

	RandGenerator rng;
	Ray inRay(*(lightState.lastRay)) , outRay(ray);

	vec3f o = lightState.pos;
	vec3f dir(0.f);
	Real originProb;

    LocalFrame lf = ray.contactObject->getAutoGenWorldLocalFrame(
        ray.contactObjectTriangleID , o, flatNormals);

    // find ray origin by disturb
    UniformSphericalSampler uniformSphericalSampler;
    float theta = RandGenerator::genFloat() * 2.f * M_PI;
	dir = lf.s * cos(theta) + lf.t * sin(theta);
    o += dir * mergeRadius;
    ray.origin = o;
    // calc inter sample origin prob
    InterSampleQuery q(o);
    tree.searchInRadius(0 , o , mergeRadius , q);
    ray.originProb = chooseProb / (M_PI * mergeRadius * mergeRadius) * q.pdf;
	//printf("%.6f , %.6f\n" , ray.originProb , 1.f / totArea);

    ray.direction = uniformSphericalSampler.genSample(lf);
    if (ray.getContactNormal().dot(ray.direction) < 0)
    {
        ray.direction = -ray.direction;
    }
    ray.directionProb = uniformSphericalSampler.getProbDensity(lf , ray.direction) * 2.f;

    ray.insideObject = scene.findInsideObject(ray , ray.contactObject);
    ray.current_tid = scene.getContactTreeTid(ray);
    ray.color = vec3f(1.f);
    /*
	if (lightState.ray->contactObject)
	{
		outRay = lightState.ray->contactObject->scatter(inRay , false);
		dir = outRay.direction;
		originProb = (weights[pathId + 1] - weights[pathId]) / 
			(M_PI * mergeRadius * mergeRadius);
		//originProb = getOriginProb(countHashGrid , o , false);
	}
	else if (lightState.ray->insideObject)
	{
		outRay = lightState.ray->insideObject->scatter(inRay , false);
		dir = outRay.direction;
		originProb = (weights[pathId + 1] - weights[pathId]) / 
			(4.f / 3.f * M_PI * mergeRadius * mergeRadius * mergeRadius);
		//originProb = getOriginProb(countHashGrid , o , true);
	}
	*/

    if (!scene.usingGPU())
    {
        Scene::ObjSourceInformation osi;
        NoSelfIntersectionCondition condition(&scene , ray);
        Real dist = scene.intersect(ray, osi, &condition);

        if (dist > 0)
        {
            ray.intersectDist = dist;
            ray.intersectObject = scene.objects[osi.objID];
            ray.intersectObjectTriangleID = osi.triangleID;
        }
    }
    /*
	if (ray.intersectObject == NULL)
		continue;

	SceneObject *insideObj = scene.findInsideObject(ray , ray.contactObject);

	if (insideObj != ray.insideObject)
		continue;
    */

    /*
	fprintf(fp , "===============\n");
	fprintf(fp , "lightState: pos=(%.3f,%.3f,%.3f), c=(%.6f,%.6f,%.6f), bsdf=(%.6f,%.6f,%.6f)\n" , lightState.pos[0] , lightState.pos[1] ,
		lightState.pos[2] , lightState.dirContrib[0] , lightState.dirContrib[1] , lightState.dirContrib[2] , bsdfFactor[0] , bsdfFactor[1] , bsdfFactor[2]);
	fprintf(fp , "--------------\n");
	fprintf(fp , "interState: pos=(%.3f,%.3f,%.3f), dir=(%.3f,%.3f,%.3f), dirProb = %.6f, %.6f\n" , ray.origin[0] , ray.origin[1] , ray.origin[2] ,
		ray.direction[0] , ray.direction[1] , ray.direction[2] , ray.directionProb , inRay.getDirectionSampleProbDensity(outRay));
	fprintf(fp , "color=(%.8f,%.8f,%.8f)\n" , ray.color[0] , ray.color[1] , ray.color[2]);
	*/
	return ray;
	}
}

Ray IptTracer::genIntermediateSamples(Scene& scene)
{
	if (totVol > 1e-7f)
	{
		float volProb = 0.5f , surProb = 1 - volProb;
		if (totArea < 1e-7f)
		{
			volProb = 1.f; surProb = 0.f;
		}
		if (RandGenerator::genFloat() < volProb)
		{
			Ray ray = genVolumeSample();
			return ray;
		}
		else
		{
			Ray ray = genOtherSurfaceSample(useUniformSur , useUniformDir);
			return ray;
		}
	}
	else
	{
		Ray ray = genOtherSurfaceSample(useUniformSur , useUniformDir);
		return ray;
	}
}

void IptTracer::genIntermediatePaths(omp_lock_t& cmdLock , vector<Path*>& interPathList)
{
	// preprocess
// 	vector<IptPathState> lightSubPathList(partialSubPathList);
//     PointKDTree<IptPathState> tree(lightSubPathList);
//	int N = lightSubPathList.size();
// 	weights.resize(N + 1 , 0);
// 	for (int i = 0; i < N; i++)
// 	{
// 		float intensity = y(lightSubPathList[i].throughput);
// 
// 		float volScale = 1.f;
// 		if ((lightSubPathList[i].ray->insideObject && !lightSubPathList[i].ray->contactObject && totVol > 0) ||
// 			totVol < 1e-7f)
// 			volScale = 1.f;
// 
// 		weights[i + 1] = weights[i] + intensity * intensity * volScale;
// 		//weights[i + 1] = weights[i] + volScale;
// 	}
// 	float sum = weights[N];
// 	for (int i = 0; i <= N; i++)
// 		weights[i] /= sum;
	std::vector<vec3f> positions;

#pragma omp parallel for
	for(int p=0; p<interPathNum; p++)
	{
		if (!renderer->scene.usingGPU())
		{
			Ray interRay = genIntermediateSamples(renderer->scene);
			//Ray interRay = genIntermediateSamplesByPhotons(lightSubPathList , tree , renderer->scene , &p);
			
			if (showInterPhotons)
			{
				omp_set_lock(&cmdLock);
				positions.push_back(interRay.origin);
				omp_unset_lock(&cmdLock);
			}
			
			interPathList[p] = new Path;
			samplePath(*interPathList[p] , interRay , true);
		}

		Path& interPath = *interPathList[p];

		//fprintf(fp , "=================\n");
		partPathMergeIndex[p].clear();

		if (interPath.size() <= 1)
			continue;

		IptPathState interState;
		interState.originRay = &interPath[0];

		interState.throughput = interPath[0].color * interPath[0].getCosineTerm() / 
			(interPath[0].originProb * interPath[0].directionProb * interPath[1].originProb);
		interState.indirContrib = vec3f(0.f);
		interState.mergedPath = 0;

		//if (intensity(interState.throughput) > 30.f)
		//	continue;

		/*
		fprintf(fp , "====================\n");
		vec3f decay = interPath[0].getRadianceDecay((interPath[0].origin - interPath[1].origin).length());
		fprintf(fp , "l = 0 , thr = (%.8f,%.8f,%.8f) , color = (%.8f,%.8f,%.8f)\ncosine = %.8f , dirPdf = %.8f , oPdf = %.8f\ndecay=(%.8f,%.8f,%.8f)\n" , 
			interState.throughput.x , interState.throughput.y , interState.throughput.z ,
			interPath[0].color[0] , interPath[0].color[1] , interPath[0].color[2] ,
			interPath[0].getCosineTerm() , interPath[0].directionProb , interPath[1].originProb ,
			decay.x , decay.y , decay.z);
		*/

		for(unsigned i = 1; i < interPath.size(); i++)
		//for (unsigned i = 1; i < 2; i++)
		{
			Real dist = std::max((interPath[i].origin - interPath[i - 1].origin).length() , 1e-5f);
			interState.throughput *= interPath[i - 1].getRadianceDecay(dist);

			if(interPath[i].contactObject && interPath[i].contactObject->emissive())
				break;

			interState.pos = interPath[i].origin;
			interState.lastRay = &interPath[i - 1];
			interState.ray = &interPath[i];
			interState.pathLen = i;
			
			if(interPath[i].directionSampleType != Ray::DEFINITE &&
				(interPath[i].insideObject != NULL || interPath[i].contactObject != NULL) &&
				(interPath[i].origin != interPath[i - 1].origin))
				//(interPath[i].insideObject && !interPath[i].contactObject)) // only volume
			{
				//fprintf(fp , "path length = %d, dirContrib = (%.8f,%.8f,%.8f)\n" , 
				//	i , interState.dirContrib[0] , interState.dirContrib[1] , interState.dirContrib[2]);

				omp_set_lock(&cmdLock);
				partialSubPathList.push_back(interState);
				partPathMergeIndex[p].push_back(partialSubPathList.size() - 1);
				omp_unset_lock(&cmdLock);
			}

			if (i == interPath.size() - 1)
				break;
			if (interPath[i].direction.length() < 0.5f)
				break;

			vec3f scatterFactor = (interPath[i].color * interPath[i].getCosineTerm() / 
				(interPath[i + 1].originProb * interPath[i].directionProb));

			interState.throughput *= scatterFactor;
			
			/*
			vec3f decay = interPath[i].getRadianceDecay((interPath[i].origin - interPath[i + 1].origin).length());
			fprintf(fp , "l = %d , thr = (%.8f,%.8f,%.8f) , color = (%.8f,%.8f,%.8f)\ncosine = %.8f , dirPdf = %.8f , oPdf = %.8f\ndecay=(%.8f,%.8f,%.8f)\n" , 
				i , interState.throughput.x , interState.throughput.y , interState.throughput.z ,
				interPath[i].color[0] , interPath[i].color[1] , interPath[i].color[2] ,
				interPath[i].getCosineTerm() , interPath[i].directionProb , interPath[i + 1].originProb ,
				decay.x , decay.y , decay.z);
			*/

			if (interPath[i].directionSampleType != Ray::DEFINITE && useWeight)
			{
				Real pdf = interPath[i].directionProb;
				if (pdf < 1e-7f)
					break;

				Real weightFactor;

				Real volMergeScale = 1.f;
				Real originProb;
				Real dirProb;
				if (interPath[i].contactObject)
				{
					if (useUniformSur)
						originProb = 1.f / totArea;
					else
						originProb = interPath[i].contactObject->getOriginProb(interPath[i].contactObjectTriangleID);
					if (useUniformDir)
						dirProb = INV_2_PI;
					else
						dirProb = interPath[i].getCosineTerm() / M_PI;
				}

				//if (interPath[i].insideObject && interPath[i].contactObject)
				//	printf("!!!\n");
				if (interPath[i].insideObject && !interPath[i].contactObject && interPath[i].insideObject->isVolumetric())
				{
					volMergeScale = 4.f / 3.f * mergeRadius;
					if (useUniformVol)
						originProb = 1.f / totVol;
					else
						originProb = interPath[i].insideObject->getOriginProb(interPath[i].origin);
					dirProb = 0.25f / M_PI;
				}
				
				Real wc = 1 / (M_PI * mergeRadius * mergeRadius);
				Real wm = originProb;
				//weightFactor = wc / (wc + wm * partialPathNum);
				weightFactor = connectFactor(pdf) /
					(connectFactor(pdf) + mergeFactor(&volMergeScale , &originProb , &dirProb , &partialPathNum));

				if (isnan(weightFactor) || abs(pdf) < 1e-6f)
				{
					fprintf(err , "sample inter path error, %.8f , %.8f\n" , connectFactor(pdf) , 
						mergeFactor(&volMergeScale , &originProb , &dirProb , &partialPathNum));
				}

				//if (interPath[i].contactObject && interPath[i].contactObject->objectIndex == 7)
				interState.throughput *= weightFactor;
			}
		}
	}

	partialPhotonNum = partialSubPathList.size();

	if (showInterPhotons)
	{
		renderPhotonMap(positions , "inter_photons.pfm");
	}
}

void IptTracer::mergePartialPaths(omp_lock_t& cmdLock)
{
	struct SearchQuery
	{
		const IptPathState* interState;
		vector<int> mergeIndex;

		SearchQuery() 
		{ 
			mergeIndex.clear();
		}

		void process(const IptPathState& lightState)
		{
			mergeIndex.push_back(lightState.index);
		}
	};

	vector<vec3f> contribs(partialPhotonNum - lightPhotonNum);
	vector<double> mergedPath(partialPhotonNum - lightPhotonNum);

	for (int i = 0; i < partialSubPathList.size(); i++)
	{
		partialSubPathList[i].index = i;
	}

	if (usePPM)
		return;

	// preprocess
	PointKDTree<IptPathState> lightTree(partialSubPathList);

	revIndex = new int[partialPhotonNum - lightPhotonNum];

	int flag = 0;

	for (int i = 0; i < interPathNum; i++)
	{
		if (partPathMergeIndex[i].size() == 0)
			continue;

		for (int j = 0; j < partPathMergeIndex[i].size(); j++)
		{
			int k = partPathMergeIndex[i][j] - lightPhotonNum;
			revIndex[k] = i;
		}
	}

#pragma omp parallel for
	for (int i = 0; i < interPathNum; i++)
	{
		if (partPathMergeIndex[i].size() == 0)
			continue;

		SearchQuery query;

		query.interState = &partialSubPathList[partPathMergeIndex[i][0]];

		lightTree.searchInRadius(0 , query.interState->originRay->origin , mergeRadius , query);

		partPathMergeIndex[i].clear();

		for (int j = 0; j < query.mergeIndex.size(); j++)
		{
			int k = query.mergeIndex[j];
			if (k < lightPhotonNum || revIndex[k - lightPhotonNum] != i)
				partPathMergeIndex[i].push_back(query.mergeIndex[j]);
		}
	}

	bool f = checkCycle;
	int checkTime = checkCycleIters;
	
	if (checkCycle)
	{
		vis.clear();
		inStack.clear();
		cannotBeCycle.clear();
		vis.resize(partialPhotonNum - lightPhotonNum);
		inStack.resize(partialPhotonNum - lightPhotonNum);
		cannotBeCycle.resize(partialPhotonNum - lightPhotonNum);

		// topological sort
		for (int i = 0; i < vis.size(); i++)
		{
			vis[i] = cannotBeCycle[i] = 0;
		}

		int totEdges = 0;
		vector<int> indeg(partialPhotonNum - lightPhotonNum , 0);
		for (int i = lightPhotonNum; i < partialPhotonNum; i++)
		{
			int k = revIndex[i - lightPhotonNum];
			for (int j = 0; j < partPathMergeIndex[k].size(); j++)
			{
				int pa = partPathMergeIndex[k][j];
				if (pa >= lightPhotonNum)
				{
					totEdges++;
					indeg[pa - lightPhotonNum]++;
				}
			}
		}
		printf("inter path graph has %d edges\n" , totEdges);

		while (!q.empty())
			q.pop();
		for (int i = lightPhotonNum; i < partialPhotonNum; i++)
			if (indeg[i - lightPhotonNum] == 0)
				q.push(i);
		while (!q.empty())
		{
			int i = q.front();
			cannotBeCycle[i - lightPhotonNum] = true;
			q.pop();
			int k = revIndex[i - lightPhotonNum];
			for (int j = 0; j < partPathMergeIndex[k].size(); j++)
			{
				int pa = partPathMergeIndex[k][j];
				if (pa >= lightPhotonNum)
				{
					indeg[pa - lightPhotonNum]--;
					if (indeg[pa - lightPhotonNum] == 0)
						q.push(pa);
				}
			}
		}
		
		int cnt = 0;
		for (int i = lightPhotonNum; i < partialPhotonNum; i++)
			if (cannotBeCycle[i - lightPhotonNum])
				cnt++;
		printf("%d/%d nodes may be in a cycle\n" , partialPhotonNum - lightPhotonNum - cnt , partialPhotonNum - lightPhotonNum);
	}

	while (f)
	{
		f = false;
		// check cycle
		for (int i = 0; i < vis.size(); i++)
		{
			vis[i] = 0;
			inStack[i] = 0;
		}

		vector<int> nodePerm(partialPhotonNum - lightPhotonNum);
		for (int i = lightPhotonNum; i < partialPhotonNum; i++)
			nodePerm[i - lightPhotonNum] = i;
		int N = nodePerm.size();
		for (int i = 0; i < nodePerm.size() / 10; i++)
		{
			int x = rand() % N;
			int y = rand() % N;
			swap(nodePerm[x] , nodePerm[y]);
		}

		for (int i = 0; i < nodePerm.size(); i++)
		{
			int st = nodePerm[i];
			if (cannotBeCycle[st - lightPhotonNum] || vis[st - lightPhotonNum])
				continue;
			while (!cycle.empty())
			{
				inStack[cycle.top() - lightPhotonNum] = 0;
				cycle.pop();
			}
			f |= dfs(1 , st);
		}

		// eliminate cycle
		for (int i = 0; i < edgeToRemove.size(); i++)
		{
			int child = edgeToRemove[i].second;
			int pa = edgeToRemove[i].first;
			int k = revIndex[child - lightPhotonNum];
			int index = -1;
			for (int j = 0; j < partPathMergeIndex[k].size(); j++)
			{
				if (partPathMergeIndex[k][j] == pa)
					index = j;
			}
			if (index != -1)
				partPathMergeIndex[k].erase(partPathMergeIndex[k].begin() + index);

		}
		edgeToRemove.clear();

		checkTime--;
		//printf("eliminating cycles: iteration %d\n" , checkCycleIters - checkTime);
		if (checkTime == 0)
			break;
	}

	printf("check & eliminate cycles, use %d iterations\n" , checkCycleIters - checkTime);
	
	f = true;
	int totMergeIter = 0;
	while (f)
	{
		++totMergeIter;
		if (totMergeIter > mergeIterations) break;
#pragma omp parallel for
		for (int i = lightPhotonNum; i < partialPhotonNum; i++)
		{
			mergePartialPaths(contribs , mergedPath , partialSubPathList[i]);
		}

		f = false;
		for (int i = lightPhotonNum; i < partialPhotonNum; i++)
		{
			if (!f && abs(intensity(partialSubPathList[i].indirContrib) - 
				intensity(contribs[i - lightPhotonNum])) > 1e-3f)
				f = true;
			partialSubPathList[i].indirContrib = contribs[i - lightPhotonNum];
			partialSubPathList[i].mergedPath = mergedPath[i - lightPhotonNum];
		}
	}

	printf("merge done... totMergeIter = %d... tracing eye paths...\n" , totMergeIter);

	vis.clear();
	for (int i = 0; i < partPathMergeIndex.size(); i++)
	{
		partPathMergeIndex[i].clear();
		partPathMergeIndex[i].shrink_to_fit();
	}
	partPathMergeIndex.clear();

	delete[] revIndex;
}

bool IptTracer::dfs(int depth , int cur)
{
	if (vis[cur - lightPhotonNum])
	{
		if (inStack[cur - lightPhotonNum])
		{
			edgeToRemove.push_back(pair<int , int>(cur , cycle.top()));
			/*
			// print cycle
			cycle.push(cur);
			fprintf(fp , "!!!!!!!!!!!!!!!! CYCLE !!!!!!!!!!!!!!!\n");
			while (!cycle.empty())
			{
				fprintf(fp , "%d " , cycle.top());
				int x = cycle.top();
				inStack[x - lightPhotonNum] = false;
				cycle.pop();
				int y;
				if (cycle.empty())
					break;
				else
					y = cycle.top();
				//fprintf(fp , "%.8f " , (partialSubPathList[x].pos - 
				//	partialSubPathList[y].originRay->origin).length());
			}
			fprintf(fp , "\n");
			// end of print
			*/
			return true;
		}
		
		return false;
	}

	if (depth > maxDepth)
		return false;

	vis[cur - lightPhotonNum] = 1;
	cycle.push(cur);
	inStack[cur - lightPhotonNum] = 1;
	int k = revIndex[cur - lightPhotonNum];
	bool isCycle = false;
	for (int j = 0; j < partPathMergeIndex[k].size(); j++)
	{
		int pa = partPathMergeIndex[k][j];
		if (pa < lightPhotonNum || cannotBeCycle[pa - lightPhotonNum])
			continue;

		isCycle |= dfs(depth + 1 , pa);
		if (isCycle)
			return true;
	}
	cycle.pop();
	inStack[cur - lightPhotonNum] = 0;
	return false;
}

vec3f IptTracer::colorByConnectingLights(Ray lastRay , Ray ray , bool dirIlluWeight)
{
	Ray lightRay = genEmissiveSurfaceSample(true , false);
	lightRay.direction = (ray.origin - lightRay.origin);
	Real dist = lightRay.direction.length();
	dist = max(dist , 1e-6f);
	Real dist2 = dist * dist;
	lightRay.direction.normalize();
	Ray outRay = ray;
	outRay.direction = -lightRay.direction;

	vec3f decayFactor = outRay.getRadianceDecay(dist);

	if(!testVisibility(outRay, lightRay))
		return vec3f(0.f);

	//outRay.direction = -cameraState.lastRay->direction;
	//vec3f bsdfFactor2 = lightRay.getBSDF(outRay);
	vec3f bsdfFactor = lastRay.getBSDF(outRay);

	if (y(bsdfFactor) < 1e-7f)
		return vec3f(0.f);

	Real cosAtLight = clampf(lightRay.getContactNormal().dot(lightRay.direction) , 0.f , 1.f);

	Real cosToLight = clampf(ray.getContactNormal().dot(-lightRay.direction) , 0.f , 1.f);

	if (cosAtLight < 1e-6f || cosToLight < 1e-6f)
		return vec3f(0.f);

	vec3f tmp = lightRay.color * cosAtLight * bsdfFactor * cosToLight
		/ (lightRay.originProb * dist2);

	//fprintf(fp , "weight = %.8f , bsdfToLightPdf = %.8f , cosAtLight = %.8f ,\ntoLightOriginPdf = %.8f , originProb = %.8f , dist = %.8f\n" , 
	//	weightFactor , bsdfToLightPdf , cosAtLight , toLightOriginPdf , lightRay.originProb , dist);

	vec3f res = tmp * decayFactor;

	if (dirIlluWeight && useRayMarching)
	{
		Real p1 = lightRay.originProb;
		Real p2 = lightRay.originProb * (cosAtLight / M_PI) * cosToLight / dist2 * 
			(partialPathNum * gatherRadius * gatherRadius * M_PI);
		Real weightFactor = p1 / (p1 + p2);
		res *= weightFactor;
	}
	
	if (dirIlluWeight && !useRayMarching)
	{
		Real p1 = lastRay.getDirectionSampleProbDensity(outRay);
		Real p2 = lightRay.originProb * dist2 / cosAtLight;
		Real weightFactor = p2 / (p1 + p2);
		res *= weightFactor;
	}
	//printf("sur weight = %.8f\n" , weightFactor);

	/*
	vec3f resx = camera.eliminateVignetting(res , cameraState.index) * pixelNum;
	if (cameraState.ray->contactObject)//(resx[0] + resx[1] + resx[2] >= 2)
	{
		fprintf(fp , "=====================\n");
		fprintf(fp , "cosAtLight = %.8f, cosToLight = %.8f, originPdf = %.8f, pdf = %.8f, weight=%.8f,\nres=(%.10f,%.10f,%.10f)\nbsdf=(%.10f,%.10f,%.10f)\n" , 
			cosAtLight , cosToLight , originProb , pdf , weightFactor , resx[0] , resx[1] , resx[2] , bsdfFactor[0] , bsdfFactor[1] , bsdfFactor[2]);
	}
	*/
	return res;
}

vec3f IptTracer::colorByMergingPaths(IptPathState& cameraState, PointKDTree<IptPathState>& partialSubPaths)
{
	GatherQuery query(this);
	query.cameraState = &cameraState;
	query.color = vec3f(0, 0, 0);

	partialSubPaths.searchInRadius(0 , query.cameraState->pos , gatherRadius , query);

// 	omp_set_lock(&cmdLock);
// 	numEyePathsPerPixel += 1;
// 	numFullPathsPerPixel += query.mergeNum;
// 	omp_unset_lock(&cmdLock);

	return query.color;
}

vec3f IptTracer::colorByRayMarching(Path& eyeMergePath , PointKDTree<IptPathState>& partialSubPaths , int pixelID)
{
	vec3f tr(1.f) , surfaceRes(0.f) , volumeRes(0.f);
	for (int i = 1; i < eyeMergePath.size(); i++)
	{
		// hit light
		if (eyeMergePath[i].contactObject && eyeMergePath[i].contactObject->emissive())
		{
			surfaceRes = eyeMergePath[i].color;
			break;
		}

		// volume
		Real dist = std::max((eyeMergePath[i - 1].origin - eyeMergePath[i].origin).length() , 1e-5f);
		if (eyeMergePath[i - 1].insideObject && eyeMergePath[i - 1].insideObject->isVolumetric())
		{
			volMask[pixelID] = true;
			if (eyeMergePath[i - 1].insideObject->isHomogeneous())
			{
				GatherQuery query(this);
				query.color = vec3f(0.f);
				query.constKernel = useConstantKernel;

				Ray volRay = eyeMergePath[i - 1];
				SceneVPMObject *vol = static_cast<SceneVPMObject*>(volRay.insideObject);
				Real stepSize = vol->stepSize;
				int N = dist / stepSize;
				if (N == 0)
					N++;

				Real step = dist / N;
				Real offset = step * RandGenerator::genFloat();
				float t = offset;
				tr *= vol->getRadianceDecay(volRay , offset);

				IptPathState cameraState;
				cameraState.throughput = vec3f(1.f);
				cameraState.lastRay = &volRay;

				Ray outRay = volRay;
				outRay.contactObject = NULL;

				for (int i = 0; i < N; i++)
				{
					query.color = vec3f(0.f);
					outRay.origin = volRay.origin + volRay.direction * t;
					cameraState.ray = &outRay;
					cameraState.pos = outRay.origin;
					query.cameraState = &cameraState;

					partialSubPaths.searchInRadius(0 , query.cameraState->pos , gatherRadius , query);

					outRay.origin -= volRay.direction * t;
					tr *= vol->getRadianceDecay(outRay , step);

					volumeRes += query.color * tr * step;
					t += step;
				}
			}
			else
			{
				GatherQuery query(this);
				query.color = vec3f(0.f);
				query.constKernel = useConstantKernel;

				Ray volRay = eyeMergePath[i - 1];
				HeterogeneousVolume *vol = static_cast<HeterogeneousVolume*>(volRay.insideObject);
				float stepSize = vol->getStepSize();
				int N = dist / stepSize;
				if (N == 0)
					N++;

				float step = dist / N;
				float offset = step * RandGenerator::genFloat();
				float t = offset;
				tr *= vol->getRadianceDecay(volRay , offset);

				IptPathState cameraState;
				cameraState.throughput = vec3f(1.f);
				cameraState.lastRay = &volRay;

				Ray outRay = volRay;
				outRay.contactObject = NULL;

				for(int i = 0; i < N; i++)
				{
					query.color = vec3f(0.f);
					outRay.origin = volRay.origin + volRay.direction * t;
					cameraState.ray = &outRay;
					cameraState.pos = outRay.origin;
					query.cameraState = &cameraState;

					partialSubPaths.searchInRadius(0 , query.cameraState->pos , gatherRadius , query);

					outRay.direction = -volRay.direction;
					tr *= vol->getRadianceDecay(outRay , step);

					volumeRes += query.color * tr * step;
					
					t += step;
				}
			}
		}
		else
		{
			if (eyeMergePath[i - 1].insideObject)
				tr *= eyeMergePath[i - 1].getRadianceDecay(dist);
		}

		// surface
		if (eyeMergePath[i].contactObject && eyeMergePath[i].directionSampleType == Ray::RANDOM)
		{
			if (eyeMergePath[i].contactObject->isVolumetric())
				continue;

			GatherQuery query(this);
			query.color = vec3f(0.f);
			query.constKernel = useConstantKernel;

			IptPathState cameraState;
			cameraState.throughput = vec3f(1.f);
			cameraState.lastRay = &eyeMergePath[i - 1];
			cameraState.ray = &eyeMergePath[i];
			cameraState.pos = cameraState.ray->origin;

			query.cameraState = &cameraState;

			partialSubPaths.searchInRadius(0 , query.cameraState->pos , gatherRadius , query);

			surfaceRes = query.color;

			if (!usePPM && useDirIllu)
			{
				vec3f dirIllu = colorByConnectingLights(eyeMergePath[i - 1] , eyeMergePath[i]);
				surfaceRes += dirIllu;
			}

			//vec3f dirVar , indirVar , totVar;
			//vec3f dirAve , indirAve , totAve;
			//dirVar = indirVar = totVar = vec3f(0.f);
			//dirAve = indirAve = totAve = vec3f(0.f);
			/*
			double dirVar , indirVar , totVar;
			double dirAve , indirAve , totAve;
			dirVar = indirVar = totVar = 0;
			dirAve = indirAve = totAve = 0;

			float dirN = (float)query.dirColors.size();
			float indirN = (float)query.indirColors.size();
			float totN = dirN + indirN;
			for (int i = 0; i < query.dirColors.size(); i++)
			{
				//dirAve += query.dirColors[i];
				//totAve += query.dirColors[i];
				dirAve += intensity(query.dirColors[i]);
				totAve += intensity(query.dirColors[i]);
			}
			for (int i = 0; i < query.indirColors.size(); i++)
			{
				//indirAve += query.indirColors[i];
				//totAve += query.indirColors[i];
				indirAve += intensity(query.indirColors[i]);
				totAve += intensity(query.indirColors[i]);
			}
			if (dirN > 0)
				dirAve /= (float)query.dirColors.size();
			if (indirN > 0)
				indirAve /= (float)query.indirColors.size();
			if (totN > 0)
				totAve /= (float)query.dirColors.size() + (float)query.indirColors.size();
			for (int i = 0; i < query.dirColors.size(); i++)
			{
				//dirVar += (query.dirColors[i] - dirAve) * (query.dirColors[i] - dirAve);
				//totVar += (query.dirColors[i] - totAve) * (query.dirColors[i] - totAve);
				dirVar += (intensity(query.dirColors[i]) - dirAve) * (intensity(query.dirColors[i]) - dirAve);
				totVar += (intensity(query.dirColors[i]) - totAve) * (intensity(query.dirColors[i]) - totAve);
			}
			for (int i = 0; i < query.indirColors.size(); i++)
			{
				//indirVar += (query.indirColors[i] - indirAve) * (query.indirColors[i] - indirAve);
				//totVar += (query.indirColors[i] - totAve) * (query.indirColors[i] - totAve);
				indirVar += (intensity(query.indirColors[i]) - indirAve) * (intensity(query.indirColors[i]) - indirAve);
				totVar += (intensity(query.indirColors[i]) - totAve) * (intensity(query.indirColors[i]) - totAve);
			}
			if (dirN > 1)
				dirVar /= (dirN - 1.f);
			if (indirN > 1)
				indirVar /= (indirN - 1.f);
			if (totN > 1)
				totVar /= (totN - 1.f);

			if (totN >= 2)
			{
				//fprintf(fp3 , "%.0f %.0f %.0f\n" , dirN , indirN , totN);

				//fprintf(fp3 , "dirVar = (%.6f,%.6f,%.6f)\n" , dirVar.x , dirVar.y , dirVar.z);
				//fprintf(fp3 , "indirVar = (%.6f,%.6f,%.6f)\n" , indirVar.x , indirVar.y , indirVar.z);
				//fprintf(fp3 , "totVar = (%.6f,%.6f,%.6f)\n" , totVar.x , totVar.y , totVar.z);
				//fprintf(fp3 , "dirVar = %.6f , indirVar = %.6f , totVar = %.6f\n" , dirVar , indirVar , totVar);
			}

			vars[pixelID] = totVar;

			break;
			*/
		}
	}

	vec3f color = tr * surfaceRes + volumeRes;
	return color;
}

void IptTracer::mergePartialPaths(vector<vec3f>& contribs , vector<double>& mergedPath , const IptPathState& interState)
{
	struct MergeQuery
	{
		vec3f color;
		double mergedPath;
		IptTracer *tracer;
		const IptPathState* interState;

		MergeQuery(IptTracer* tracer) { this->tracer = tracer; }

		void process(const IptPathState& lightState)
		{
			Real dist = (lightState.ray->origin - interState->originRay->origin).length();
			Real volMergeScale = 1.f;
			if (interState->originRay->insideObject && !interState->originRay->contactObject && 
				interState->originRay->insideObject->isVolumetric())
				volMergeScale = 4.f / 3.f * tracer->mergeRadius;
			
			if (dist >= tracer->mergeRadius)
				printf("dist error\n");

			if (interState->originRay->insideObject && !interState->originRay->contactObject &&
				interState->originRay->insideObject->isVolumetric())
			{
				if (lightState.ray->insideObject == NULL || 
					lightState.ray->contactObject != NULL ||
					interState->originRay->insideObject != lightState.ray->insideObject ||
					!interState->originRay->insideObject->isVolumetric() ||
					!lightState.ray->insideObject->isVolumetric())
				{
					return;
				}
				
				volMergeScale = 4.0 / 3.0 * tracer->mergeRadius;
			}
			else if (interState->originRay->contactObject)
			{
				//if (interState->originRay->insideObject)
				//	printf("should not have insideObject\n");

				if (lightState.ray->contactObject != interState->originRay->contactObject ||
					!lightState.ray->contactObject->canMerge ||
					!interState->originRay->contactObject->canMerge)
				{
					return;
				}
			}
			else
			{
				return;
			}
			
			vec3f totContrib;
			if (lightState.index < tracer->lightPhotonNum)
				totContrib = lightState.throughput;
			else
				totContrib = lightState.indirContrib;

			if (y(totContrib) < 1e-7f)
				return;

			Ray outRay , inRay;
			vec3f bsdfFactor;
		
			outRay = *lightState.ray;
			outRay.direction = interState->originRay->direction;

			inRay = *lightState.lastRay;
			inRay.direction = interState->originRay->origin - lightState.lastRay->origin;
			inRay.direction.normalize();
			//vec3f bsdfFactor2 = lightState.lastRay->getBSDF(outRay);
			bsdfFactor = inRay.getBSDF(*interState->originRay);

			//if ((bsdfFactor - bsdfFactor2).length() > 1e-6f)
			//{
			//	printf("bsdf error, (%.8f,%.8f,%.8f),(%.8f,%.8f,%.8f)\n" , 
			//		bsdfFactor.x , bsdfFactor.y , bsdfFactor.z ,
			//		bsdfFactor2.x , bsdfFactor2.y , bsdfFactor2.z);
			//}

			if (y(bsdfFactor) < 1e-7f)
				return;

			vec3f tmp = totContrib * bsdfFactor * interState->throughput;

			Real lastPdf , weightFactor;
			//Real lastPdf2 = lightState.lastRay->getDirectionSampleProbDensity(outRay);
			lastPdf = inRay.getDirectionSampleProbDensity(*interState->originRay);

			//if (abs(lastPdf - lastPdf2) > 1e-6f)
			//{
			//	printf("pdf error, %.8f, %.8f\n" , lastPdf , lastPdf2);
			//}

			//if (lastPdf < 1e-7f)
			//	return;

			/*
			fprintf(fp , "================\n");
			fprintf(fp , "f1=(%.8f,%.8f,%.8f), f2=(%.8f,%.8f,%.8f), p1=%.8f, p2=%.8f\n" ,
				bsdfFactor[0] , bsdfFactor[1] , bsdfFactor[2] , bsdfFactor2[0] , bsdfFactor2[1] , bsdfFactor2[2] , 
				lastPdf , lastPdf2);
			*/

			//if (_isnan(weightFactor) || abs(lastPdf) < 1e-6f || 
			//	abs(tracer->mergeFactor(&volMergeScale , &interState->originRay->originProb , &interState->originRay->directionProb)) < 1e-6f)
			//{
			//	fprintf(err , "merge partial path error, %.8f , %.8f\n" , tracer->connectFactor(lastPdf) , 
			//		tracer->mergeFactor(&volMergeScale , &interState->originRay->originProb , &interState->originRay->directionProb));
			//}

			vec3f res;

			Real wc = 1.f / (M_PI * tracer->mergeRadius * tracer->mergeRadius);
			Real wm = lightState.lastRay->directionProb;
			if (lightState.index < tracer->lightPhotonNum)
			{
				//weightFactor = (wm * tracer->lightPathNum) / (wc + wm * tracer->lightPathNum);
				//weightFactor = tracer->mergeFactor(&volMergeScale , &interState->originRay->originProb , &interState->originRay->directionProb , &tracer->lightPathNum) /
				//	(tracer->connectFactor(lastPdf) + tracer->mergeFactor(&volMergeScale , &interState->originRay->originProb , &interState->originRay->directionProb , &tracer->lightPathNum));
				Real originProb = 1.f / tracer->totArea;
				Real dirProb = 0.5f / M_PI;
				weightFactor = tracer->mergeFactor(&volMergeScale , &originProb , &dirProb , &tracer->lightPathNum) /
					(tracer->connectFactor(lastPdf) + tracer->mergeFactor(&volMergeScale , &originProb , &dirProb , &tracer->lightPathNum));

				res = tmp * (tracer->lightMergeKernel / volMergeScale);
			}
			else
			{
				//weightFactor = (wm * tracer->partialPathNum) / (wc + wm * tracer->partialPathNum);
				//weightFactor = tracer->mergeFactor(&volMergeScale , &interState->originRay->originProb , &interState->originRay->directionProb , &tracer->partialPathNum) /
				//	(tracer->connectFactor(lastPdf) + tracer->mergeFactor(&volMergeScale , &interState->originRay->originProb , &interState->originRay->directionProb , &tracer->partialPathNum));
				Real originProb = 1.f / tracer->totArea;
				Real dirProb = 0.5f / M_PI;
				weightFactor = tracer->mergeFactor(&volMergeScale , &originProb , &dirProb , &tracer->partialPathNum) /
					(tracer->connectFactor(lastPdf) + tracer->mergeFactor(&volMergeScale , &originProb , &dirProb , &tracer->partialPathNum));
				res = tmp * (tracer->interMergeKernel / volMergeScale);
			}
			/*
			vec3f factor;
			Real cosTerm;
			if (abs(volMergeScale - 1.f) < 1e-6f)
				cosTerm = interState->originRay->getContactNormal().dot(interState->originRay->direction);
			else
				cosTerm = 1.f;
			factor = (bsdfFactor * cosTerm * weightFactor) / interState->originRay->originProb / interState->originRay->directionProb;
			if (lightState.index < tracer->lightPhotonNum)
				factor *= (tracer->lightMergeKernel / volMergeScale);
			else
				factor *= (tracer->interMergeKernel / volMergeScale);
			if (factor.x >= 1.f || factor.y >= 1.f || factor.z >= 1.f)
				printf("(%.8f,%.8f,%.8f)\n" , factor.x , factor.y , factor.z);
			*/

			if (tracer->useWeight)
				res *= weightFactor;

			color += res;
			mergedPath += lightState.mergedPath;

			//if (res.x > totContrib.x || res.y > totContrib.y || res.z > totContrib.z)
			//{
			//	printf("(%.8f,%.8f,%.8f)>(%.8f,%.8f,%.8f)\n" , res.x , res.y , res.z ,
			//		totContrib.x , totContrib.y , totContrib.z);
			//}

			/*
			vec3f resx = res;	
			if (resx.x > totContrib.x || resx.y > totContrib.y || resx.z > totContrib.z)
			{
				fprintf(fp , "----------------\n");
				fprintf(fp , "res = (%.8f,%.8f,%.8f), totContrib = (%.8f,%.8f,%.8f), \nbsdf = (%.8f,%.8f,%.8f), \ninterThr = (%.8f,%.8f,%.8f), weightFactor = %.8f\nPc = %.8f, Pm = %.8f, originProb = %.8f, dirProb = %.8f\n" ,
					resx[0] , resx[1] , resx[2] , totContrib[0] , totContrib[1] , totContrib[2] , bsdfFactor[0] , bsdfFactor[1] , bsdfFactor[2] , 
					interState->throughput[0] , interState->throughput[1] , interState->throughput[2] , weightFactor ,
					tracer->connectFactor(lastPdf) , tracer->mergeFactor(&volMergeScale , &interState->originRay->originProb ,
					&interState->originRay->directionProb) , interState->originRay->originProb , interState->originRay->directionProb);
			}
			*/
		}
	};

	MergeQuery query(this);
	query.interState = &interState;
	query.color = vec3f(0, 0, 0);
	query.mergedPath = 0;

	int pa = revIndex[interState.index - lightPhotonNum];
	/*
	if (partPathMergeIndex[pa].size() > 0)
		fprintf(fp , "======%d=======\n" , partPathMergeIndex[pa].size());
	*/

	for (int j = 0; j < partPathMergeIndex[pa].size(); j++)
	{
		int k = partPathMergeIndex[pa][j];
		query.process(partialSubPathList[k]);
	}

	contribs[interState.index - lightPhotonNum] = query.color;
	mergedPath[interState.index - lightPhotonNum] = query.mergedPath;
}

void IptTracer::movePaths(omp_lock_t& cmdLock , vector<Path>& pathListGPU , vector<Path*>& pathList)
{
	for (int i = 0; i < pathListGPU.size(); i++)
	{
		pathList[i] = &pathListGPU[i];
	}
}

void IptTracer::sampleMergePath(Path &path, Ray &prevRay, uint depth) 
{
	path.push_back(prevRay);

	Ray terminateRay;
	terminateRay.origin = prevRay.origin;
	terminateRay.color = vec3f(0,0,0);
	terminateRay.direction = vec3f(0,0,0);
	terminateRay.directionSampleType = Ray::DEFINITE;
	terminateRay.insideObject =	terminateRay.contactObject = terminateRay.intersectObject = NULL;

	Ray nextRay;
	//if (prevRay.insideObject && !prevRay.insideObject->isVolumetric()) 
	if (prevRay.insideObject && prevRay.insideObject->isVolumetric()) // modified 2015.05.21
	{
		nextRay = prevRay.insideObject->scatter(prevRay , false , true);
	}
	else 
	{
		if (prevRay.intersectObject)
		{
			if (prevRay.intersectObject->isVolumetric() && 
				prevRay.contactObject && prevRay.contactObject->isVolumetric())
			{
				prevRay.origin += prevRay.direction * prevRay.intersectDist;
				prevRay.intersectDist = 0;
			}
			nextRay = prevRay.intersectObject->scatter(prevRay , false , true);
		}
		else
		{
			path.push_back(terminateRay);
			return;
		}
	}

	if (nextRay.direction.length() > 0.5 && nextRay.insideObject == NULL && nextRay.contactObject != NULL)
	{
		vec3f geoN = nextRay.getContactNormal(false);
		vec3f shdN = nextRay.getContactNormal(true);
		vec3f wi = -prevRay.direction;
		vec3f wo = nextRay.direction;
		float wiDotGeoN = geoN.dot(wi);
		float woDotGeoN = geoN.dot(wo);
		float wiDotShdN = shdN.dot(wi);
		float woDotShdN = shdN.dot(wo);

		// prevent light leak due to shading normals
		if (wiDotGeoN * wiDotShdN <= 0 || woDotGeoN * woDotShdN <= 0)
		{
			//printf("wi: %.6f %.6f, wo: %.6f %.6f\n" , wiDotGeoN , wiDotShdN , woDotGeoN , woDotShdN);
			nextRay.direction = vec3f(0.f);
			nextRay.color = vec3f(0.f);
		}
	}

	if (nextRay.direction.length() < 0.5) 
	{
		path.push_back(nextRay);		
		return;
	}

	if (depth + 1 > maxDepth)
	{
		path.push_back(terminateRay);
		return;
	}
	
	terminateRay.origin = nextRay.origin;
	if (nextRay.contactObject && (nextRay.contactObject->emissive() || nextRay.directionSampleType == Ray::RANDOM))
	{
		path.push_back(nextRay);
		path.push_back(terminateRay);
		return;
	}
	
	NoSelfIntersectionCondition condition(&renderer->scene, nextRay);
	Scene::ObjSourceInformation info;
	float dist = renderer->scene.intersect(nextRay, info, &condition);
	if (dist < 0)
	{
		path.push_back(nextRay);
		path.push_back(terminateRay);
		return;
	}
	else
	{
		nextRay.intersectObject = renderer->scene.objects[info.objID];
		nextRay.intersectObjectTriangleID = info.triangleID;
		nextRay.intersectDist = dist;
	}
	sampleMergePath(path, nextRay, depth + 1);
}

void IptTracer::renderPhotonMap(std::vector<vec3f>& positions , const char* fileName)
{
	Camera& camera = renderer->camera;
	IplImage* image = cvCreateImage(cvSize(camera.width, camera.height), IPL_DEPTH_32F, 3);
	vector<vec3f> eyeRays = camera.generateRays();
	std::vector<Colormap::color> colors;
	int numBins = 8;
	Colormap::colormapHeatColor(numBins , colors);

	std::vector<float> photonDens(camera.width * camera.height);
	for (int i = 0; i < positions.size(); i++)
	{
		class SmoothNormal : public KDTree::Condition
		{
		public:
			Scene* scene;
			virtual bool legal(const KDTree::Ray& ray, const KDTree::Triangle& tri, const float dist) const
			{
				SceneObject *intersectObject = scene->objects[((Scene::ObjSourceInformation*)tri.sourceInformation)->objID];
				unsigned fi = ((Scene::ObjSourceInformation*)tri.sourceInformation)->triangleID;
				bool in = ray.direction.dot(intersectObject->getWorldNormal(fi, ray.origin + ray.direction*dist, flatNormals))<0;
				return in;
			}
		} condition;

		vec2<float> pCoord = camera.transToPixel(positions[i]);
		int x = pCoord.x;
		int y = pCoord.y;

		if(!(x >= 0 && x < camera.width && y >= 0 && y < camera.height)) continue;
		
		condition.scene = &renderer->scene;
		Ray ray;
		ray.origin = camera.position;
		ray.direction = positions[i] - ray.origin;
		float length = ray.direction.length();
		ray.direction /= length;
		Scene::ObjSourceInformation osi;
		float dist = renderer->scene.intersect(ray, osi, &condition);
		if (std::abs(dist - length) < 1e-6f)
		{
			photonDens[y * camera.width + x] += 1.f;
		}
	}

	float maxDensity = *std::max_element(photonDens.begin() , photonDens.end());

// 	fprintf(fp , "max = %.1f\n" , maxDensity);
// 	for (int y = 0; y < camera.height; y++)
// 	{
// 		for (int x = 0; x < camera.width; x++)
// 		{
// 			fprintf(fp , "%.1f " , photonDens[y * camera.width + x]);
// 		}
// 		fprintf(fp , "\n");
// 	}

#pragma omp parallel for
	for(int x=0; x<camera.width; x++)
	{
		class SmoothNormal : public KDTree::Condition
		{
		public:
			Scene* scene;
			virtual bool legal(const KDTree::Ray& ray, const KDTree::Triangle& tri, const float dist) const
			{
				SceneObject *intersectObject = scene->objects[((Scene::ObjSourceInformation*)tri.sourceInformation)->objID];
				unsigned fi = ((Scene::ObjSourceInformation*)tri.sourceInformation)->triangleID;
				bool in = ray.direction.dot(intersectObject->getWorldNormal(fi, ray.origin + ray.direction*dist, flatNormals))<0;
				return in;
			}
		} condition;
		for(unsigned y=0; y<camera.height; y++)
		{
			condition.scene = &renderer->scene;
			Ray ray;
			ray.direction = eyeRays[y*camera.width + x];
			ray.origin = camera.position;
			Scene::ObjSourceInformation osi;
			float dist = renderer->scene.intersect(ray, osi, &condition);
			vec3f normal = dist >= 0 ? renderer->scene.objects[osi.objID]->getWorldNormal(osi.triangleID, ray.origin + ray.direction*dist, flatNormals) : vec3f(0, 0, 0);
			SceneObject *obj = renderer->scene.objects[osi.objID];
			//if (dist >= 0 && obj->canMerge && !obj->isVolumetric() && !obj->emissive() &&
			//	obj->energyDensity[osi.triangleID] > 0)
// 			if (photonDens[y * camera.width + x] > 0)
 			{
				int level = (int)(powf((maxDensity - photonDens[y * camera.width + x]) / (maxDensity) , 2) * numBins);
				level = clamp(level , 0 , numBins - 1);
				((vec3f*)image->imageData)[y*camera.width + x] = vec3f(colors[level].b, colors[level].g, colors[level].r);
			}
// 			else
// 			{
// 				((vec3f*)image->imageData)[y*camera.width + x] = vec3f(1, 1, 1) * abs(ray.direction.dot(normal));
// 			}
		}
	}
	saveImagePFM(fileName , image);
	cvReleaseImage(&image);
}