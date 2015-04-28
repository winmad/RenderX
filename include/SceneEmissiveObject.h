#pragma once
#include "SceneObject.h"

class SceneEmissiveObject : public SceneObject
{
protected:
	vec3f color;
public:
	vec3f getColor() const { return color; }
	void setColor(const vec3f& color){ this->color = color; }
	SceneEmissiveObject(Scene* scene) : SceneObject(scene){
		canMerge = false; // FIX ME
	}
	bool emissive() const { return true; }

	Ray scatter(const Ray& inRay , const bool fixIsLight , const bool russian) const;
	vec3f getBSDF(const Ray& inRay, const Ray& outRay) const;

	virtual std::string getType() { return "L"; }
};

