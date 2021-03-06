#pragma once
#include "SceneEmissiveObject.h"

class SceneParallelLight : public SceneEmissiveObject
{
public:
	SceneParallelLight(Scene* scene) : SceneEmissiveObject(scene){
		canMerge = false; // FIX ME
	}
	Ray scatter(const Ray& inRay , const bool fixIsLight , const bool russian) const;
	Ray emit();
	float getDirectionSampleProbDensity(const Ray& inRay, const Ray& outRay);

	virtual std::string getType() { return "L"; }
};

