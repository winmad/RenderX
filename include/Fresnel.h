#pragma once
#include "nvVector.h"
#include <cmath>

// fraction of energy reflects
inline float fresnelDielectric(float cosi , float cost , float etai , float etat)
{
	float Rperp = (etai * cosi - etat * cost) / (etai * cosi + etat * cost);
	float Rparl = (etat * cosi - etai * cost) / (etat * cosi + etai * cost);
	return (Rperp * Rperp + Rparl * Rparl) * 0.5f;
}
