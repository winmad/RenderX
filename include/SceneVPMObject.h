#pragma once
#include "SceneObject.h"
#include "HomoMediaDistSampler.h"
#include "HGPhaseSampler.h"
#include "IsotropicPhaseSampler.h"
#include "HGPhaseFunc.h"
#include "IsotropicPhaseFunc.h"
class SceneVPMObject : public SceneObject
{
public:
	BSDF* bsdf;
	vec3f ds, da, dt;
	float g;
	float stepSize;
	float IOR;
	Ray scatter(const Ray& inRay, const bool fixIsLight, const bool russian) const;
	bool isVolumetric() { return true; }
	bool hasCosineTerm() { return false; }
	bool isHomogeneous() { return true; }
	virtual std::string getType() { return "V"; }
 protected:
	float y(vec3f c) const {
        const float YWeight[3] = { 0.212671f, 0.715160f, 0.072169f };
        return YWeight[0] * c[0] + YWeight[1] * c[1] + YWeight[2] * c[2];
    }
 
	vec3f transmittance(float dist) const{
		vec3f tr;
		for(int i = 0; i < 3; i++){
			tr[i] = exp(-dt[i] * dist);
		}
		return tr;
	}
	float p_medium(float s) const{
		vec3f pm;
		for(int i = 0; i < 3; i++){
			pm[i] = dt[i] * exp(-dt[i] * s);
		}
		return y(pm);
	}
	float P_surface(float d) const{
		vec3f Pd;
		for(int i = 0; i < 3; i++){
			Pd[i] = exp(-dt[i] * d);
		}
		return y(Pd);
	}
	 
public:
	virtual vec3f getRadianceDecay(const Ray &inRay, const float &dist) const ;
	vec3f getBSDF(const Ray& inRay, const Ray& outRay) const;
	float getDirectionSampleProbDensity(const Ray& inRay, const Ray& outRay) const;
	float getOriginSampleProbDensity(const Ray& inRay, const Ray& outRay) const;
	float getContinueProbability(const Ray &inRay, const Ray &outRay) const;
	float getRefrCoeff() const {
		return IOR;
	}
	float getG() const {
		return g;
	}
	SceneVPMObject(Scene* scene) : SceneObject(scene)
	{
		canMerge = true;
		stepSize = 0.0005;
	}
	~SceneVPMObject()
	{
		delete bsdf;
	}
	float getAlbedo() const{
		return y(ds) / y(dt);
	}
    /*
	void setSSS(std::string name){
		std::cout << "in VPM setSSS" << " " << name << std::endl;
		bool success = search_sss_color(name, da, ds);
		std::cout << "ds: " << ds.x << ' ' << ds.y << ' ' << ds.z << std::endl;
		std::cout << "da: " << da.x << ' ' << da.y << ' ' << da.z << std::endl;
		if(!success){
			std::cerr << "sss not found " << std::endl;
		}
	}
    */
	void setSigma(vec3f sigma_s, vec3f sigma_a){
		ds = sigma_s;
		da = sigma_a;
		dt = ds + da;
	}
	void setInRate(float ior){
		IOR = ior;
	}
	void setG(float g){
		this->g = g;
		bsdf = new HGPhaseFunc(g);
		//std::cout << "new hg phase function g= " << g<<std::endl;
	}
	void setStepSize(float step){
		stepSize = step;
	}
	void setScale(float scale){
		ds *= scale;
		da *= scale;
		dt *= scale;
	}
};

