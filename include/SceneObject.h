#pragma once
#include <string>
#include "Ray.h"
#include "simpleshape.h"
#include "Scene.h"
#include "RandGenerator.h"
#include "CosineSphericalSampler.h"
#include "NoSelfIntersectionCondition.h"
#include "Material.h"
#include "Texture.h"

class Ray;

class Scene;

class Material;

class CountHashGrid;

class SceneObject : public SimpleShape
{
protected:
	Scene *scene;
	vector<float> areaValues;
	vector<Material*> materialList;
public:
	CountHashGrid *countHashGrid;
	float totalEnergy;
	vector<float> energyDensity;
	float weight;
	float volumeWeight;
	float totalArea;
	float totalVolume;
	int objectIndex;
	Texture bumpTex;
	SceneObject* findInsideObject(const Ray& ray) const;
	bool canMerge;
	virtual Ray scatter(const Ray& inRay, const bool fixIsLight, const bool russian) const;
	SceneObject(){ this->scene = NULL; countHashGrid = NULL; }
	SceneObject(Scene* scene)
	{
		bumpTex.setColor(vec3f(1, 1, 1));
		this->scene = scene;
		countHashGrid = NULL;
		canMerge = false;
		unitize();
	}
	SceneObject(Scene* scene, const string &fileName, bool normalize = true) : SimpleShape(fileName, normalize)
	{
		this->scene = scene;
	}
	virtual vec3f getWorldNormal(unsigned fi, const vec3f& position, bool flat) const;
	virtual LocalFrame getAutoGenWorldLocalFrame(unsigned fi, const vec3f& position, bool flat) const;
    virtual bool hasCosineTerm(){ return true; }
	virtual bool emissive() const{ return false; }
	virtual bool isVolumetric() { return false; }
	virtual bool isHomogeneous() { return true; }
	virtual float getG() const { return 0.f; }
	virtual float getRefrCoeff() const{ return 1; }

	virtual void preprocessEmissionSampler();
	virtual void preprocessOtherSampler();
	virtual void preprocessVolumeSampler(bool isUniformOrigin , float mergeRadius);
	void normalizeEmissionWeight(float totalWeight){ weight /= totalWeight; }
	void normalizeVolumeWeight(float totalWeight);
	virtual Ray emit(bool isUniformOrigin , bool isUniformDir) const;
	virtual Ray emitVolume(bool isUniformDir) const;
	virtual float getEmissionWeight() const { return weight; }
	virtual void scaleEnergyDensity(const float scale);
	virtual void addEnergyDensity(const int triId , const vec3f& thr);
	virtual void addEnergyDensity(const vec3f& pos , const vec3f& thr);
	virtual void singleEnergyToSumEnergy();
	virtual void sumEnergyToSingleEnergy();
	virtual float getOriginProb(const int triId);
	virtual float getOriginProb(const vec3f& pos);
	virtual float getTotalEnergy() const;

	virtual float getDirectionSampleProbDensity(const Ray& inRay, const Ray& outRay) const;
	virtual float getOriginSampleProbDensity(const Ray& inRay, const Ray& outRay) const;
	virtual float getContinueProbability(const Ray &inRay, const Ray &outRay) const;
	virtual float evaluatePdfW(const Ray &inRay, const Ray &outRay) const { return 1; }
	virtual vec3f getBSDF(const Ray& inRay, const Ray& outRay) const;
	virtual vec3f getRadianceDecay(const Ray& inRay, const float& dist) const;

	virtual std::string getType() { return "X"; }

	virtual ~SceneObject();
};

