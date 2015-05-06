#pragma once

#include "ofMain.h"
#include "ofxOpenCv.h"
#include "ofxOsc.h"
#include "ofxCvBlobTracker.h"
#include "ofxCvTrackedBlob.h"
#include "ofxCvTrackingConstants.h"

#define OUTPUT_HEIGHT      1024.0
#define OUTPUT_WIDTH       1400.0
#define HOST               "localhost"
#define PORT               8000
#define THRESHOLD          18
#define CIRCLE_RESOLUTION  40
#define BLOBMAX            3
#define COUNTOUR_MIN       350
#define MARGIN             20
#define BLUR               1
#define CAMWIDTH           640
#define CAMHEIGHT          480
#define DEVICE_NAME        "DV-VCR"
#define BRIGHTNESS         0
#define CONTRAST           0

//#define DEBUG

// Uncomment to activate VideoLive
#define _USE_LIVE_VIDEO
#define _USE_SYPHON_VIDEO

#ifdef _USE_SYPHON_VIDEO
#include "ofxSyphon.h"
#endif

class ofApp : public ofBaseApp, public ofxCvBlobListener
{

	public:
    
    void setup();
    void update();
    void draw();
    void exit();
    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y );
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);
    
    private:
    
    void sonoSetup();
    void blobUpdate();
    void debugDraw();
    void syphonDraw();
    void oscUpdate();
    void blobOn( int x, int y, int id, int order );
    void blobMoved( int x, int y, int id, int order );
    void blobOff( int x, int y, int id, int order );

    std::string filename;
    std::string deviceName;
    int 		camWidth;
    int 		camHeight;
    bool        bLearnBackground;
    int         Threshold;
    bool        bNewFrame;
    int         contour_min;
    int	        blobMax;
    int         margin;
    float       scalef;
    int         spacerx;
    int         spacery;
    int         blur;
    float       brightness;
    float       contrast;
    bool        bOscCountour;
    
    // Buffer for velocity
    map<int, vector <float> > vbuffer;
    
    // Buffer for velocity x
    map<int, vector <float> > vbufferx;
    
    // Buffer for velocity y
    map<int, vector <float> > vbuffery;
    
#ifdef _USE_LIVE_VIDEO
    ofVideoGrabber 		vidGrabber;
#else
    ofVideoPlayer 		vidPlayer;
#endif

    ofxCvColorImage			colorImg;
    ofxCvGrayscaleImage 	grayImage;
    ofxCvGrayscaleImage 	grayBg;
    ofxCvGrayscaleImage 	grayDiff;
    ofxCvContourFinder		contourFinder;
    ofxOscSender            sender;
    ofxOscMessage           m;
    ofxCvBlobTracker        blobTracker;
    ofBuffer                blobBuffer;
    
#ifdef _USE_SYPHON_VIDEO
    ofxSyphonServer         syphonBlobTextureServer;
    ofxSyphonServer         syphonDiffTextureServer;
    ofxSyphonServer         syphonScreenServer;
    ofFbo                   blobsTexture;
    ofTexture               diffTexture;
#endif
    
};
