#pragma once
#include "Camera.h"
#include "Scene.h"
#include "ConfigManager.h"
#include <opencv/cv.h>
#include <opencv/highgui.h>

class MCRenderer;

class Renderer
{
	friend class ConfigManager;
	friend class MCRenderer;
public:
	MCRenderer* mcRenderer;
	ConfigManager* configManager;
	Scene scene;
	Camera camera;
	
public:
	Camera getCamera() const { return camera; }
	void loadConfig(const string& configFilePath);
	void showWindow() { cvNamedWindow("Renderer"); }
	void preview();
	void waitForCommand(const string& filename);
	void render();
	Renderer();
	~Renderer();
};

