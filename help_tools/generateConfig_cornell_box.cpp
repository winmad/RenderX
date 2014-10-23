#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <iostream>
using namespace std;

void writeConfig(string fileName , int *hours , int *mins , string scenePath ,
                 string sceneName , string algo , int *spp , int *maxDepth , double *radius ,
                 double *gatherRadius , int *pathNum , double *pathRatio , double *mergeRatio ,
                 int *useUniformSur , int *useDirIllu , int *isDebug , string savePath)
{
    string str;
    char buf[128];
    str += "<Config>\n";
    if (hours != NULL || mins != NULL)
    {
        str += " <time>\n";
        if (hours != NULL)
        {
            sprintf(buf , "%d" , *hours);
            str += "  <hours>" + string(buf) + "</hours>\n";
        }
        if (mins != NULL)
        {
            sprintf(buf , "%d" , *mins);
            str += "  <minutes>" + string(buf) + "</minutes>\n";
        }
        str += " </time>\n";
    }
    
    str += " <Scene>\n";
    str += "  <path>" + scenePath + "</path>\n";
    str += "  <name>" + sceneName + "</name>\n";
    str += "  <camera>c1</camera>\n";
    str += " </Scene>\n";
    
    str += " <Renderer>\n";
    str += "  <type>" + algo + "</type>\n";
    if (spp != NULL)
    {
        sprintf(buf , "%d" , *spp);
        str += "  <spp>" + string(buf) + "</spp>\n";
    }
    if (maxDepth != NULL)
    {
        sprintf(buf , "%d" , *maxDepth);
        str += "  <maxDepth>" + string(buf) + "</maxDepth>\n";
    }
    if (radius != NULL)
    {
        sprintf(buf , "%.6f" , *radius);
        str += "  <radius>" + string(buf) + "</radius>\n";
    }
    if (gatherRadius != NULL)
    {
        sprintf(buf , "%.6f" , *gatherRadius);
        str += "  <gatherRadius>" + string(buf) + "</gatherRadius>\n";
    }
    if (pathNum != NULL)
    {
        sprintf(buf , "%d" , *pathNum);
        str += "  <pathNum>" + string(buf) + "</pathNum>\n";
    }
    if (pathRatio != NULL)
    {
        sprintf(buf , "%.6f" , *pathRatio);
        str += "  <pathRatio>" + string(buf) + "</pathRatio>\n";
    }
    if (mergeRatio != NULL)
    {
        sprintf(buf , "%.6f" , *mergeRatio);
        str += "  <mergeRatio>" + string(buf) + "</mergeRatio>\n";
    }
    
    str += "  <usePPM>0</usePPM>\n";
    
    if (useUniformSur != NULL)
    {
        sprintf(buf , "%d" , *useUniformSur);
        str += "  <useUniformSur>" + string(buf) + "</useUniformSur>\n";
    }
    if (useDirIllu != NULL)
    {
        sprintf(buf , "%d" , *useDirIllu);
        str += "  <useDirIllu>" + string(buf) + "</useDirIllu>\n";
    }
    if (isDebug != NULL)
    {
        sprintf(buf , "%d" , *isDebug);
        str += "  <isDebug>" + string(buf) + "</isDebug>\n";
    }
    str += " </Renderer>\n";
    
    str += " <savePath>" + savePath + "</savePath>\n";
    
    str += "</Config>\n";
    
    FILE *fp = fopen(fileName.c_str() , "w");
    fprintf(fp , "%s" , str.c_str());
    fclose(fp);
}

int main()
{
    int mins = 10;
    int N = 0;
    
    int spp[4];
    spp[0] = 1; spp[1] = 64;
    
    int maxDepth = 40;
    
    double radius[4];
    radius[0] = 0.001; radius[1] = 0.003; radius[2] = 0.005; radius[3] = 0.01;
    
    int pathNum[4];
    pathNum[0] = 100000; pathNum[0] = 500000; pathNum[1] = 1500000;
    
    double mergeRatio = 1;
    int useDirIllu = 0;
    int isDebug = 1;
    
    char buf[128];
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 4; b++)
            for (int c = 0; c < 4; c++)
                for (int d = 0; d < 3; d++)
                    for (double pathRatio = 0.2; pathRatio < 1.001; pathRatio += 0.3)
                        for (int useUniformSur = 0; useUniformSur < 2; useUniformSur++)
                        {
                            N++;
                            sprintf(buf , "%d" , N);
                            string fileName = "cb_config_" + string(buf) + ".xml";
                            string savePath = "results/cornell_box/cb_" + string(buf) + ".pfm";
                            writeConfig(fileName , NULL , &mins , "scenes/2balls_cornell_box.xml" ,
                                        "Balls" , "IPT" , &spp[a] , &maxDepth , &radius[b] ,
                                        &radius[c] , &pathNum[d] , &pathRatio , &mergeRatio ,
                                        &useUniformSur , &useDirIllu , &isDebug , savePath);
                        }
    return 0;
}
