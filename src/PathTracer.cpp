#include "StdAfx.h"
#include "PathTracer.h"

static FILE* fp = fopen("debug_pt.txt" , "w");

vector<vec3f> PathTracer::renderPixels(const Camera& camera)
{
	//int t_start = clock();
	timer.StartStopWatch();
	vector<vec3f> pixelColors(camera.width*camera.height, vec3f(0, 0, 0));

	if(useConnection)
		renderer->scene.preprocessEmissionSampler();

	if(!renderer->scene.usingGPU())
	{
		for(unsigned s=0; s<spp; s++)
		{
			//int t = clock();
			timer.PushCurrentTime();
#pragma omp parallel for
			for(int p=0; p<pixelColors.size(); p++)
			{
				Path eyePath;
                Ray cameraRay = camera.generateRay(p);
				//printf("=============\n");
				//fprintf(fp , "============\n");
				samplePath(eyePath, cameraRay, false);

				for (int i = 0; i < eyePath.size(); i++)
				{
					if (eyePath[i].contactObject == NULL) break;
// 					fprintf(fp , "%s, bsdf = (%.6f,%.6f,%.6f)\n" , 
// 						eyePath[i].contactObject->getType().c_str() , 
// 						eyePath[i].color.x , eyePath[i].color.y , eyePath[i].color.z);
				}

				pixelColors[p] *= s/float(s+1);

				if (!(eyePath.back().contactObject && eyePath.back().contactObject->emissive()))
					continue;

				vec3f color = vec3f(1, 1, 1);
				for(unsigned i=0; i<eyePath.size(); i++)
				{
					color *= eyePath[i].color / eyePath[i].directionProb / eyePath[i].originProb;
				
					if(i!=eyePath.size()-1)
					{
						color *= eyePath[i].getCosineTerm();
						float dist = (eyePath[i+1].origin - eyePath[i].origin).length();
						// NOTE: Must multiply the decay !!!!!!!!!
						color *= eyePath[i].getRadianceDecay(dist);
					}
				}

				pixelColors[p] += renderer->camera.eliminateVignetting(color, p)/(s+1);//*camera.width*camera.height;
				//pixelColors[p] += color * eyePath[0].directionProb / (s+1);
			}

			printf("Iter: %d  IterTime: %.3lfs  TotalTime: %.3lfs\n", s+1, timer.PopCurrentTime(), 
				timer.GetElapsedTime(0));

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
	}
	else
	{
		
		for(unsigned s=0; s<spp; s++)
		{
			int t = clock();
			vector<Ray> eyeRays(pixelColors.size());

#pragma omp parallel for
			for(int p=0; p<pixelColors.size(); p++)
			{
				eyeRays[p] = camera.generateRay(p);
			}

			int clk = clock();
			vector<Path> pathList = samplePathList(eyeRays, true);

			vector<Path> lightPathList;
			vector<vector<unsigned> > visList;

			if(useConnection)
			{
				lightPathList.resize(pathList.size());
				for(unsigned i=0; i<pathList.size(); i++)
					lightPathList[i].push_back(genEmissiveSurfaceSample(true , false));
				visList = testPathListVisibility(pathList, lightPathList);
			}

#pragma omp parallel for
			for(int p=0; p<pathList.size(); p++)
			{
				pixelColors[p] *= s/float(s+1);

				vec3f color = vec3f(0, 0, 0);

				//pathList[p][0].directionProb = 1.f;

				//if(!useConnection || mustUsePT(pathList[p]) || 
				//	pathList[p].size()==2 && pathList[p].back().contactObject && pathList[p].back().contactObject->emissive())
				if (pathList[p].back().contactObject && pathList[p].back().contactObject->emissive())
				{
					vec3f c(1, 1, 1);
					for(unsigned i=0; i<pathList[p].size(); i++)
					{
						c *= pathList[p][i].color / pathList[p][i].directionProb / pathList[p][i].originProb;

						if(i!=pathList[p].size()-1)
						{
							c *= pathList[p][i].getCosineTerm();
							float dist = (pathList[p][i+1].origin - pathList[p][i].origin).length();
							// NOTE: Must multiply the decay !!!!!!!!!
							c *= pathList[p][i].getRadianceDecay(dist);
						}
					}
					color += c;
				}
				/*
				else
				{
					Ray &lightRay = lightPathList[p][0];
					for(unsigned i=1; i<pathList[p].size(); i++)
					{
						if(!((visList[p][i/32]>>(i%32)) & 1))
							continue;
						Path connectedPath;
						connectedPath.push_back(lightRay);
						Path &eyePath = pathList[p];
						if(eyePath[i].contactObject && eyePath[i].contactObject->emissive())
							break;
						if(eyePath[i].directionSampleType != Ray::RANDOM)
							continue;
						for(unsigned k=0; k<=i; k++)
							connectedPath.push_back(eyePath[i-k]);
						connectRays(connectedPath, 0);
						vec4f color_prob = connectColorProb(connectedPath, 0);
						if(vec3f(color_prob).length()>0 && color_prob.w > 0)
							color += vec3f(color_prob) / color_prob.w;// / camera.width / camera.height;
					}
				}
				*/
				pixelColors[p] += renderer->camera.eliminateVignetting(color, p)/(s+1);//*camera.width*camera.height;
				//pixelColors[p] += color / (s+1);
			}

			printf("Iter: %d  IterTime: %.3lfs  TotalTime: %.3lfs\n", s+1, timer.PopCurrentTime(), 
				timer.GetElapsedTime(0));

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
	}
	return pixelColors;
}


vec4f PathTracer::connectColorProb(const Path& connectedPath, int connectIndex)
{
	vec3f color(1, 1, 1);
	float prob = 1;

	float connectDist;

	for(int i=0; i<connectedPath.size(); i++)
	{
		color *= connectedPath[i].color;

		double dist;

		if(i <= connectIndex)
		{
			dist = max2((connectedPath[i+1].origin - connectedPath[i].origin).length(), EPSILON);
			color *= connectedPath[i].getRadianceDecay(dist);
		}
		else if(i>connectIndex+1)
		{
			dist = max2((connectedPath[i-1].origin - connectedPath[i].origin).length(), EPSILON);
			color *= connectedPath[i].getRadianceDecay(dist);
		}


		if(i==connectIndex && i<connectedPath.size()-1)
		{
			color *= connectedPath[i].getCosineTerm() * connectedPath[i+1].getCosineTerm() / (dist*dist);
			connectDist = dist;
		}

		if(i!=connectIndex && i!=connectIndex+1)
			color *= connectedPath[i].getCosineTerm();

		prob *= connectedPath[i].directionProb * connectedPath[i].originProb;
	}

	return vec4<float>(color, prob);
}
