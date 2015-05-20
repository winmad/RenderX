#include "StdAfx.h"
#include "RefractiveMaterial.h"
#include "Fresnel.h"
/*
Ray RefractiveMaterial::scatter(const SceneObject* object, const Ray& inRay, 
	const bool fixIsLight, const bool russian) const
{
	Ray outRay;
	vec3f position = inRay.origin + inRay.direction*inRay.intersectDist;

	bool go_in = (inRay.intersectObject == object) && (inRay.insideObject != object);
	bool go_out = (inRay.insideObject == object);

	//==== FIX ME =====
	if (inRay.intersectObject == NULL)
	{
		outRay.direction = vec3f(0.f);
		outRay.contactObject = NULL;
		outRay.contactObjectTriangleID = -1;
		outRay.directionSampleType = Ray::DEFINITE;
		return outRay;
	}
	//=================

	LocalFrame lf = inRay.intersectObject->getAutoGenWorldLocalFrame(inRay.intersectObjectTriangleID, position, flatNormals);
	vec3f normal = lf.n;
	//vec3f normalGeo = inRay.intersectObject->getWorldNormal(inRay.intersectObjectTriangleID, position, true);

	// scatter--start
	outRay.color = surfColor;
	outRay.origin = position;
	outRay.direction = inRay.direction;
	vec3f reflDir = -normal.dot(inRay.direction)*normal*2 + inRay.direction;
	reflDir.normalize();
	outRay.contactObject = (SceneObject*)object;
	outRay.contactObjectTriangleID = inRay.intersectObjectTriangleID;

	outRay.directionSampleType = Ray::DEFINITE;

	if(refrCoeff < 0)
	{
		outRay.direction = reflDir;
		outRay.insideObject = inRay.insideObject;
		outRay.directionProb = 1;
		outRay.color /= outRay.getCosineTerm();
		
// 		if (!fixIsLight) printf("S, pos = (%.6f,%.6f,%.6f)\n" , outRay.origin.x ,
// 			outRay.origin.y , outRay.origin.z);
		
		return outRay;
	}

	if(inRay.intersectObject != object && inRay.insideObject == object)
	{
		outRay = inRay.intersectObject->scatter(inRay , fixIsLight , russian);
		return outRay;
	}

	SceneObject* currentInsideObject = inRay.insideObject;
	SceneObject* outSideObject = (SceneObject*) object;
	if(inRay.insideObject == object)
		outSideObject = object->findInsideObject(outRay);
	float current_n = currentInsideObject ? currentInsideObject->getRefrCoeff() : 1; // ei
	float next_n = outSideObject ? outSideObject->getRefrCoeff() : 1; // et
	outRay.intersectObject = NULL;
	outRay.directionProb = 1;

	float cos_theta = std::abs(normal.dot(-inRay.direction));
// 	bool entering = (normal.dot(-inRay.direction) > 0);
// 	if (go_in != entering)
// 	{
// 		printf("error: %d %d, cos_i = %.6f\n" , go_in?1:0 , entering?1:0 , cos_theta);
// 	}
	float sin_theta = sqrt(1.f - cos_theta * cos_theta);
	float eta = current_n / next_n;
	float sin_phi = eta * sin_theta;
	float cos_phi = sqrt(std::max(0.f , 1.f - sin_phi * sin_phi));
	float er = fresnelDielectric(cos_theta , cos_phi , current_n , next_n);

	if(RandGenerator::genFloat() < er)
	{
		outRay.direction = reflDir;
		outRay.color *= er / outRay.getCosineTerm();
		outRay.photonProb = outRay.directionProb = er;
		outRay.insideObject = inRay.insideObject;

// 			if (!fixIsLight) printf("S, pos = (%.6f,%.6f,%.6f)\n" , outRay.origin.x ,
// 				outRay.origin.y , outRay.origin.z);
	}
	else
	{
		if(sin_phi >= 1) // no total internal reflection
		{
			outRay.direction = vec3f(0.f);
			outRay.contactObject = NULL;
			outRay.contactObjectTriangleID = -1;
			outRay.directionSampleType = Ray::DEFINITE;
			return outRay;
		}

		vec3f localWi = lf.toLocal(-inRay.direction);
		//if (std::abs(localWi.length() - 1.f) > 1e-6) printf("localWi not normalized! , %.6f\n" , localWi.length());
		if (localWi.y > 0) cos_phi = -cos_phi;
		vec3f localWo = vec3f(-eta * localWi.x , cos_phi , -eta * localWi.z);
		//if (std::abs(localWo.length() - 1.f) > 1e-6) printf("localWo not normalized! , %.6f\n" , localWo.length());
		localWo.normalize();
		outRay.direction = lf.toWorld(localWo);
		//if (std::abs(outRay.direction.length() - 1.f) > 1e-6) printf("worldWo not normalized\n");

		if (!fixIsLight) 
		{
			outRay.color *= (current_n * current_n) / (next_n * next_n); 				
		}
		outRay.color *= (1-er) / outRay.getCosineTerm();
		outRay.photonProb = outRay.directionProb = 1-er;
		outRay.insideObject = outSideObject;

// 			if (!fixIsLight)
// 			{
// 				printf("T, pos = (%.6f,%.6f,%.6f), in = %d, out = %d\nfactor = %.6f/%.6f, cos_i = %.6f, cos_o = %.6f\nbsdf = (%.6f,%.6f,%6f)\n" , 
// 					outRay.origin.x , outRay.origin.y , outRay.origin.z , 
// 					go_in?1:0 , go_out?1:0 , current_n , next_n , 
// 					normal.dot(-inRay.direction) , normal.dot(outRay.direction) ,
// 					outRay.color.x , outRay.color.y , outRay.color.z);
// 			}
	}
	return outRay;
}
*/


Ray RefractiveMaterial::scatter(const SceneObject* object, const Ray& inRay, 
	const bool fixIsLight, const bool russian) const
{
	Ray outRay;
	vec3f position = inRay.origin + inRay.direction*inRay.intersectDist;

	bool go_in = (inRay.intersectObject == object) && (inRay.insideObject != object);
	bool go_out = (inRay.insideObject == object);

	//==== FIX ME =====
	if (inRay.intersectObject == NULL)
	{
		outRay.direction = vec3f(0.f);
		outRay.contactObject = NULL;
		outRay.contactObjectTriangleID = -1;
		outRay.directionSampleType = Ray::DEFINITE;
		return outRay;
	}
	//=================

	LocalFrame lf = inRay.intersectObject->getAutoGenWorldLocalFrame(inRay.intersectObjectTriangleID, position, flatNormals);
	vec3f normal = lf.n;
// 	bool entering = (normal.dot(-inRay.direction) > -1e-6);
// 	if (go_in != entering)
// 	{
// 		printf("error: %d %d\n" , go_in?1:0 , entering?1:0);
// 	}

	// scatter--start
	outRay.color = surfColor;
	outRay.origin = position;
	outRay.direction = inRay.direction;
	vec3f reflDir = -normal.dot(inRay.direction)*normal*2 + inRay.direction;
	outRay.contactObject = (SceneObject*)object;
	outRay.contactObjectTriangleID = inRay.intersectObjectTriangleID;

	outRay.directionSampleType = Ray::DEFINITE;

	if(refrCoeff < 0)
	{
		outRay.direction = reflDir;
		outRay.insideObject = inRay.insideObject;
		outRay.directionProb = 1;
		outRay.color /= outRay.getCosineTerm();
		
// 		if (!fixIsLight) printf("S, pos = (%.6f,%.6f,%.6f)\n" , outRay.origin.x ,
// 			outRay.origin.y , outRay.origin.z);
		
		return outRay;
	}

	if(inRay.intersectObject != object && inRay.insideObject == object)
	{
		outRay = inRay.intersectObject->scatter(inRay , fixIsLight , russian);
		return outRay;
	}

	float theta = acos(clampf(inRay.direction.dot(normal), -1, 1));

	SceneObject* currentInsideObject = inRay.insideObject;
	SceneObject* outSideObject = (SceneObject*) object;
	if(inRay.insideObject == object)
		outSideObject = object->findInsideObject(outRay);
	float current_n = currentInsideObject ? currentInsideObject->getRefrCoeff() : 1;
	float next_n = outSideObject ? outSideObject->getRefrCoeff() : 1;
	outRay.intersectObject = NULL;
	outRay.directionProb = 1;

	float sin_phi = current_n / next_n * sin(theta);

	if(sin_phi >= 1) // no total internal reflection
	{
		outRay.direction = vec3f(0.f);
		outRay.contactObject = NULL;
		outRay.contactObjectTriangleID = -1;
		outRay.directionSampleType = Ray::DEFINITE;
		return outRay;
		
// 		outRay.direction = reflDir;
// 		outRay.insideObject = inRay.insideObject;
// 		outRay.directionProb = 1;
// 		outRay.color /= outRay.getCosineTerm();
// 
// 		if (!fixIsLight) printf("S, pos = (%.6f,%.6f,%.6f)\n" , outRay.origin.x ,
// 			outRay.origin.y , outRay.origin.z);
	}
	else
	{
		float phi = asin(sin_phi);
		if(theta > M_PI/2)
			phi = M_PI - phi;
		vec3f axis = normal.cross(inRay.direction);
		axis.normalize();
		outRay.direction = vec3f(rotMat(axis, phi) * vec4<float>(normal, 0));
		outRay.direction.normalize();

		float cos_theta = abs(cos(theta));
		float cos_phi = abs(cos(phi));
		float esr = powf(abs(current_n*cos_theta-next_n*cos_phi)/(current_n*cos_theta+next_n*cos_phi),2);
		float epr = powf(abs(next_n*cos_theta-current_n*cos_phi)/(next_n*cos_theta+current_n*cos_phi),2);
		float er = (esr+epr)/2;

		float p = clampf(er , 0.f , 1.f);
		
		if(RandGenerator::genFloat() < p)
		{
			outRay.direction = reflDir;
			outRay.color *= er / outRay.getCosineTerm();
			outRay.photonProb = outRay.directionProb = p;
			outRay.insideObject = inRay.insideObject;

// 			if (!fixIsLight) printf("S, pos = (%.6f,%.6f,%.6f)\n" , outRay.origin.x ,
// 				outRay.origin.y , outRay.origin.z);
		}
		else
		{
			if (!fixIsLight) 
			{
				outRay.color *= (current_n * current_n) / (next_n * next_n); 				
			}
			outRay.color *= (1-er) / outRay.getCosineTerm();
			outRay.photonProb = outRay.directionProb = 1-p;
			outRay.insideObject = outSideObject;

// 			if (!fixIsLight)
// 			{
// 				printf("T, pos = (%.6f,%.6f,%.6f), in = %d, out = %d\nfactor = %.6f/%.6f, cos_i = %.6f, cos_o = %.6f\nbsdf = (%.6f,%.6f,%6f)\n" , 
// 					outRay.origin.x , outRay.origin.y , outRay.origin.z , 
// 					go_in?1:0 , go_out?1:0 , current_n , next_n , 
// 					normal.dot(-inRay.direction) , normal.dot(outRay.direction) ,
// 					outRay.color.x , outRay.color.y , outRay.color.z);
// 			}
		}
	}
	outRay.direction.normalize();

	return outRay;
}
