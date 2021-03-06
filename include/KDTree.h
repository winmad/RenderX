#pragma once
#include <vector>
#include <unordered_set>
#include <algorithm>
#include "nvVector.h"

//using namespace std;
using namespace nv;

class KDTree
{
public:
	struct Ray
	{
		vec3f origin;
		vec3f direction;
	};

	struct Triangle
	{
		vec3ui vertexIndices;
		void* sourceInformation;
	};
	enum Strategy{ LOOP, BEST } strategy;

	class Condition
	{
	public:
		virtual bool legal(const Ray& ray, const Triangle& tri, const float dist) const
		{
			return true;
		}
	};
public:
	struct Node;
	Node* root;
private:
	unsigned maxLeafTriangleNum;
	unsigned maxDepth;
	void splitLoop(Node* node, unsigned dim, unsigned depth = 0);
	void splitBest(Node* node, unsigned depth = 0);
	void destroy(Node* node);
	float intersect(const Ray& ray, const Node* node, unsigned& triangleID, const Condition* condition) const;
	float intersect(const Ray& ray, const Triangle& tri, const Condition* condition) const;
	void serializeForGPU(Node* node, int parent, std::vector<vec4f>& nodes, std::vector<vec4f>& nodes_minCoords, std::vector<vec4f>& nodes_maxCoords, std::vector<vec4f>& leaf_v1, std::vector<vec4f>& leaf_v2, std::vector<vec4f>& leaf_v3) const;
public:
    std::vector<vec3f> vertexPositionList;
    std::vector<Triangle> triangleList;

	float intersect(const Ray& ray, unsigned& triangleID, const Condition* condition = NULL) const;

	KDTree();
	vec3f getDiagonal();
	void build(Strategy strategy = BEST);
	void destroy();
	void serializeForGPU(std::vector<vec4f>& nodes, std::vector<vec4f>& nodes_minCoords, std::vector<vec4f>& nodes_maxCoords, std::vector<vec4f>& leaf_v1, std::vector<vec4f>& leaf_v2, std::vector<vec4f>& leaf_v3) const;
	~KDTree();

};

struct KDTree::Node
{
	Node* left;
	Node* right;
    std::vector<unsigned> triangleIndices;
	struct BoundingBox
	{
		vec3f minCoord;
		vec3f maxCoord;
		float intersect(const KDTree::Ray& ray) const;
	} boundingBox;
};

