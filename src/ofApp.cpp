#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{
#ifdef DEBUG
    std::cerr << "START SETUP" << std::endl;
#endif
    sonoSetup();
#ifdef DEBUG
    std::cerr << "END SETUP" << std::endl;
#endif
}

//--------------------------------------------------------------
void ofApp::sonoSetup()
{
    
#ifdef _USE_LIVE_VIDEO
    camWidth = CAMWIDTH;
    camHeight = CAMHEIGHT;
    vidGrabber.setVerbose(true);
    deviceName = DEVICE_NAME;
    vector<ofVideoDevice> devices = vidGrabber.listDevices();
    
    for(vector<ofVideoDevice>::iterator it = devices.begin(); it != devices.end(); ++it)
    {
#ifdef DEBUG
        cerr << "DEVICE: " << it->deviceName << " ID: " << it->id << endl;
#endif
        if(it->deviceName == deviceName)
        {
            vidGrabber.setDeviceID(it->id);
            break;
        }
    }
    
    vidGrabber.initGrabber(camWidth, camHeight, false);
#else
    // FILENAME
    filename = "videos/Movie_okjpg.mov";
    
    //check if file exists
    bool bFileThere = false;
    fstream fin;
    string fileNameInOF = ofToDataPath(filename, false); // since OF files are in the data directory, we need to do this
    fin.open(fileNameInOF.c_str(),ios::in);
    
    if ( fin.is_open() )
    {
#ifdef DEBUG
        cerr << "Founded file " << fileNameInOF << endl;
#endif
        bFileThere =true;
    }
    fin.close();
    
    if (bFileThere)
    {
        vidPlayer.loadMovie(filename);
        vidPlayer.play();
        camWidth = vidPlayer.getWidth();
        camHeight = vidPlayer.getHeight();
    }
    else
    {
#ifdef DEBUG
        cerr << "File " << fileNameInOF << " is not here!" << endl;
#endif
        ofApp::exit();
    }
#endif
    
#ifdef DEBUG
    cerr << "Input size: width =" << camWidth << " height = " << camHeight << endl;
#endif
    
    // ALLOCATE IMAGES SIZES
    colorImg.allocate(camWidth, camHeight);
    grayImage.allocate(camWidth, camHeight);
    grayBg.allocate(camWidth,camHeight);
    grayDiff.allocate(camWidth,camHeight);
    
    // SETUP PARAMS
    bLearnBackground = true;
    Threshold = THRESHOLD;
    ofSetCircleResolution(CIRCLE_RESOLUTION); 	   //set resolution of circle
    ofEnableSmoothing();
    ofSetFrameRate(FRAMERATE);
    blobMax     = BLOBMAX;
    contour_min = COUNTOUR_MIN;
    margin      = MARGIN;
    scalef = camWidth / OUTPUT_WIDTH;
    spacerx = camWidth + margin;
    spacery = camHeight + margin;
    blur = BLUR;
    brightness = BRIGHTNESS;
    contrast = CONTRAST;
    bOscCountour = false;
    showDebugInterface = true;
    // this runtime option is independent from the DEFINE _USE_SYPHON_VIDEO
    showSyphonOutput = true;
    
    // BLOB TRACKING
    blobTracker.setListener( this );
    
    // SETUP VBUFFER
    // Crea un buffer (array) size 60 PER OGNI ATTORE (0,1,2)
    // FIXME hai diviso per 4?
    
    for (int i =0; i<BLOBMAX; i++)
    {
        vbuffer.insert(pair<int, vector<float> >(i, vector<float>(FRAMERATE/4,0.0)));
        //vx
        vbufferx.insert(pair<int, vector<float> >(i, vector<float>(FRAMERATE/4,0.0)));
        //vy
        vbuffery.insert(pair<int, vector<float> >(i, vector<float>(FRAMERATE/4,0.0)));
    }
   
    
#ifdef DEBUG
    for ( map<int, vector<float> >::const_iterator it = vbuffer.begin(); it != vbuffer.end(); ++it)
    {
        cerr << "Actor: " << it->first << " vbuffer size: " << it->second.size() << endl;
    }
    
    for ( map<int, vector<float> >::const_iterator it = vbufferx.begin(); it != vbufferx.end(); ++it)
    {
        cerr << "Actor: " << it->first << " vbufferx size: " << it->second.size() << endl;
    }
    
    for ( map<int, vector<float> >::const_iterator it = vbuffery.begin(); it != vbuffery.end(); ++it)
    {
        cerr << "Actor: " << it->first << " vbuffery size: " << it->second.size() << endl;
    }
    
#endif
    
    // SETUP OSC
    sender.setup(HOST, PORT);
    
#ifdef DEBUG
    cerr << "sending osc messages to" << string(HOST) << ":" << ofToString(PORT) << endl;
#endif
    
    // SETUP SYPHON
#ifdef _USE_SYPHON_VIDEO
    syphonScreenServer.setName("SonosAlphaDebug");
    syphonBlobTextureServer.setName("SonosBlobs");
    syphonDiffTextureServer.setName("SonosDiff");
    blobsTexture.allocate(camWidth,camHeight,GL_RGBA);
    diffTexture.allocate(camWidth, camHeight, GL_LUMINANCE);
#endif
}

//--------------------------------------------------------------
void ofApp::update()
{
    // IS A NEW FRAME?
    bNewFrame = false;
#ifdef _USE_LIVE_VIDEO
    vidGrabber.update();
    bNewFrame = vidGrabber.isFrameNew();
#else
    vidPlayer.update();
    bNewFrame = vidPlayer.isFrameNew();
#endif
    
    if (bNewFrame)
    {
        blobUpdate();
        oscUpdate();
    }
}

//--------------------------------------------------------------
void ofApp::blobUpdate()
{
    // 1. SET IMAGE from current pixels, move to greyImg, if learn background do it
#ifdef _USE_LIVE_VIDEO
    colorImg.setFromPixels(vidGrabber.getPixels(), camWidth, camHeight);
#else
    colorImg.setFromPixels(vidPlayer.getPixels(), camWidth,camHeight);
#endif
    grayImage = colorImg;
    
    // BLUR
    grayImage.blur(blur);
    grayImage.brightnessContrast(brightness, contrast);
    
    if (bLearnBackground == true)
    {
        grayBg = grayImage;		// the = sign copys the pixels from grayImage into grayBg (operator overloading)
        bLearnBackground = false;
    }
    
    // take the abs value of the difference between background and incoming and then threshold:
    grayDiff.absDiff(grayBg, grayImage);
    grayDiff.threshold(Threshold);
    
    //flag updated the cv image
    grayImage.flagImageChanged();
    grayDiff.flagImageChanged();
    
    /* COUNTOUR FINDER FUNCTION
     
     ofxCvContourFinder::findContours(
     ofxCvGrayscaleImage&  input,
     int minArea,
     int maxArea,
     int nConsidered,
     bool bFindHoles,
     bool bUseApproximation
     )
     
     find contours which are between the size of 20 pixels and 1/3 the w*h pixels.
     also, find holes is set to true so we will get interior contours as well...  */
    contourFinder.findContours(grayDiff,
                               contour_min,
                               (camWidth*camHeight)/3,
                               blobMax,
                               false,
                               true);
    // Blob tracking
    blobTracker.trackBlobs( contourFinder.blobs );
    
}

//--------------------------------------------------------------
void ofApp::oscUpdate()
{
    if (blobTracker.blobs.size() > 0)
    {
        for (int i=0; i < blobTracker.blobs.size(); i++)
        {
            ofxCvTrackedBlob b = blobTracker.blobs[i];
            blobTracker.findVelocity(b.id, &b.velocity);
            int order = blobTracker.findOrder(b.id);
            
            stringstream addr;
            addr.str(std::string());
            addr << "/blob/" << order << "/centroid";
            m.setAddress(addr.str());
            m.addFloatArg(b.centroid.x);
            m.addFloatArg(b.centroid.y);
            sender.sendMessage(m);
            m.clear();
            
            addr.str(std::string());
            addr << "/blob/" << order << "/area";
            m.setAddress(addr.str());
            m.addFloatArg(b.area);
            sender.sendMessage(m);
            m.clear();
            
            addr.str(std::string());
            addr << "/blob/" << order << "/boundingrect";
            m.setAddress(addr.str());
            m.addFloatArg(b.boundingRect.width);
            m.addFloatArg(b.boundingRect.height);
            sender.sendMessage(m);
            m.clear();
            
            addr.str(std::string());
            addr << "/blob/" << order << "/delta/pos";
            m.setAddress(addr.str());
            m.addFloatArg(b.deltaLoc.x);
            m.addFloatArg(b.deltaLoc.y);
            sender.sendMessage(m);
            m.clear();

            addr.str(std::string());
            addr << "/blob/" << order << "/delta/area";
            m.setAddress(addr.str());
            m.addFloatArg(b.deltaArea);
            sender.sendMessage(m);
            m.clear();

            addr.str(std::string());
            addr << "/blob/" << order << "/velocity";
            m.setAddress(addr.str());
            m.addFloatArg(b.velocity.x);
            m.addFloatArg(b.velocity.y);
            sender.sendMessage(m);
            m.clear();
            
            addr.str(std::string());
            addr << "/blob/" << order << "/instantvelocity";
            m.setAddress(addr.str());
            m.addFloatArg(b.velocity.length());
            sender.sendMessage(m);
            m.clear();
            
            if (order < blobMax)
            {
                // record velocity for vbuffer meanvelocity
                vector<float> v = vbuffer[order];
                // erase first element
                v.erase(v.begin());
                // add current v as last element
                v.push_back(b.velocity.length());
                vbuffer[order] = v;
                float sum = accumulate(v.begin(), v.end(), 0.0);
                float mean = sum/v.size();
                addr.str(std::string());
                addr << "/blob/" << order << "/meanvelocity";
                m.setAddress(addr.str());
                m.addFloatArg(mean);
                sender.sendMessage(m);
                m.clear();
                
                // record velocity for vbuffer Velocity x
                vector<float> vx = vbufferx[order];
                // erase first element
                vx.erase(vx.begin());
                // add current v as last element
                vx.push_back(b.velocity.x);
                vbufferx[order] = vx;
                float sumx = accumulate(vx.begin(), vx.end(), 0.0);
                float meanx = sumx/vx.size();
                // record velocity for vbuffer Velocity Y
                vector<float> vy = vbuffery[order];
                // erase first element
                vy.erase(vy.begin());
                // add current v as last element
                vy.push_back(b.velocity.y);
                vbuffery[order] = vy;
                float sumy = accumulate(vy.begin(), vy.end(), 0.0);
                float meany = sumy/vy.size();
                addr.str(std::string());
                addr << "/blob/" << order << "/meanvelocityxy";
                m.setAddress(addr.str());
                m.addFloatArg(meanx);
                m.addFloatArg(meany);
                sender.sendMessage(m);
                m.clear();
                
            }
            
            
            if (bOscCountour)
            {
                for( int j=0; j<blobTracker.blobs[i].pts.size(); j++ )
                {
                    addr.str(std::string());
                    addr << "/contour/" << blobTracker.findOrder(b.id);
                    m.setAddress(addr.str());
                    m.addIntArg(j);
                    m.addFloatArg(blobTracker.blobs[i].pts[j].x);
                    m.addFloatArg(blobTracker.blobs[i].pts[j].y);
                    sender.sendMessage(m);
                    m.clear();
                }
            }
        }
    }
}

//--------------------------------------------------------------
void ofApp::draw()
{
    if (showDebugInterface)
    {
        debugDraw();
    }
    
#ifdef _USE_SYPHON_VIDEO
    if (showSyphonOutput)
    {
        syphonDraw();
    }
#endif
}

//--------------------------------------------------------------
void ofApp::debugDraw()
{
    ofPushMatrix();
    ofScale(scalef, scalef, 1.0);
    ofBackground(0, 0, 0);
    colorImg.draw(margin,margin);
    grayImage.draw(spacerx,margin);
    grayBg.draw(margin,spacery);
    grayDiff.draw(spacerx, spacery);
    
    // then draw the contours:
    ofFill();
    ofSetHexColor(0x333333);
    ofRect(spacerx*2, margin,camWidth,camHeight);
    ofSetHexColor(0xffffff);
    
    // draw the whole contour finder
    // contourFinder.draw(spacerx, spacery);
    for (int i = 0; i < contourFinder.nBlobs; i++)
    {
        contourFinder.blobs[i].draw(spacerx*2, margin);
        // draw over the centroid
        ofCircle(contourFinder.blobs[i].boundingRect.getCenter().x + spacerx*2,
                 contourFinder.blobs[i].boundingRect.getCenter().y + margin,
                 1.0f);
    }
    
    // finally, a report:
    string syphon_status = showSyphonOutput ? "active" : "inactive";
    
    ofSetHexColor(0xffffff);
    stringstream reportStr;
    reportStr << "[h] hide inteface" << endl
    << "[s] hide/show syphon output, current status: " << syphon_status << endl
    << "bg subtraction and blob detection" << endl
    << "press ' ' to capture bg" << endl
    << "threshold " << Threshold << " (press: +/-)" << endl
    << "brightness: " << brightness << " (press: 1/2)" << endl
    << "contrast: " << contrast << " (press: 3/4)" << endl
    << "blur: " << blur << " (press: q/w)" << endl
    << "num blobs found " << contourFinder.nBlobs << ", fps: "
    << ofGetFrameRate() << endl
    << "tracked blobs: " << blobTracker.blobs.size() << endl
    << "send contour via osc: " << bOscCountour << " (press o)" << endl;
    
    if (blobTracker.blobs.size() > 0)
    {
        for (int i=0; i < blobTracker.blobs.size(); i++)
        {
            ofxCvTrackedBlob b = blobTracker.blobs[i];
            blobTracker.findVelocity(b.id, &b.velocity);
            int order = blobTracker.findOrder(b.id);
            
            
            reportStr << "Blob ActorID (order): " << order << endl
            << " temporary id: " << b.id << endl
            << " /blob/ActorID/centroid (float x, float y): " << b.centroid.x << ", " << b.centroid.y << endl
            << " /blob/ActorID/area (float area): " << b.area << endl
            << " /blob/ActorID/boundingrect (float width, heihgt): " << b.boundingRect.width << ", " << b.boundingRect.height << endl
            << " /blob/ActorID/velocity (float x, float y): " << b.velocity.x << ", " << b.velocity.y << endl
            << " /blob/ActorID/delta/pos (float x, float y): " << b.deltaLoc.x << ", " << b.deltaLoc.y << endl
            << " /blob/ActorID/delta/area (float area): " << b.deltaArea << endl
            << " /blob/ActorID/instantvelocity (float v): " << b.velocity.length() << endl;
            
            if (order < blobMax)
            {
                vector<float> v = vbuffer[order];
                float sum = accumulate(v.begin(), v.end(), 0.0);
                float mean = sum/v.size();
                reportStr << " /blob/ActorID/meanvelocity (float v): " << mean <<  endl;
                
                vector<float> vx = vbufferx[order];
                float sumx = accumulate(vx.begin(), vx.end(), 0.0);
                float meanx = sumx/vx.size();
                vector<float> vy = vbuffery[order];
                float sumy = accumulate(vy.begin(), vy.end(), 0.0);
                float meany = sumy/vy.size();

                reportStr << " /blob/ActorID/meanvelocity x-y (float vx, vy): " << meanx << ", " << meany <<   endl;
                
            }
            reportStr << endl;
        }
    }
    
    blobTracker.draw( spacerx*2, spacery );
    ofDrawBitmapString(reportStr.str(), spacerx * 3, margin*2);

#ifdef _USE_SYPHON_VIDEO
    blobsTexture.draw(margin, spacery*2);
    diffTexture.draw(spacerx, spacery*2);
#endif
    ofPopMatrix();
}

#ifdef _USE_SYPHON_VIDEO
//--------------------------------------------------------------
void ofApp::syphonDraw()
{
    syphonScreenServer.publishScreen();
    // draw static into our textures.
    ofPushStyle();
    ofPushMatrix();
    blobsTexture.begin();
    ofClear(0,0,0,0);
    ofSetHexColor(0xffffff);
    for( int i=0; i<blobTracker.blobs.size(); i++ )
    {
        glLineWidth(3.0);
        glBegin(GL_LINE_LOOP);
        for( int j=0; j<blobTracker.blobs[i].pts.size(); j++ )
        {
            glVertex2f( blobTracker.blobs[i].pts[j].x, blobTracker.blobs[i].pts[j].y );
        }
        glEnd();
    }
    blobsTexture.end();
    ofPopMatrix();
    ofPopStyle();
    syphonBlobTextureServer.publishTexture(&blobsTexture.getTextureReference());
    
    diffTexture.loadData(grayDiff.getPixels(), camWidth,camHeight, GL_LUMINANCE);
    syphonDiffTextureServer.publishTexture(&diffTexture);
    
}
#endif

//--------------------------------------------------------------
void ofApp::exit()
{
    //magari c' da chiudere la cam o i video da verificare;
    OF_EXIT_APP(0);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
    
    switch (key)
    {
        case 'o':
            bOscCountour = !bOscCountour;
            break;
        case 'q':
            blur--;
            if (blur < 1) blur = 1;
            break;
        case 'w':
            blur++;
            if (blur > 100) blur = 100;
            break;
        case '1':
            brightness -= 0.01;
            if (brightness < -1) brightness = -1;
            break;
        case '2':
            brightness += 0.01;
            if (brightness > 1) brightness = 1;
            break;
        case '3':
            contrast -= 0.01;
            if (contrast < -1) contrast = -1;
            break;
        case '4':
            contrast += 0.01;
            if (contrast > 1) contrast = 1;
            break;
        case ' ':
            bLearnBackground = true;
            break;
        case '+':
            Threshold ++;
            if (Threshold > 255) Threshold = 255;
            break;
        case '-':
            Threshold --;
            if (Threshold < 0) Threshold = 0;
            break;
        case 'h':
            showDebugInterface = !showDebugInterface;
            break;
        case 's':
            showSyphonOutput = !showSyphonOutput;
            break;
#ifdef _USE_LIVE_VIDEO
            
#else
        case 'z':
            vidPlayer.setFrame(336);
            break;
        case 'x':
            vidPlayer.setFrame(4151);
            break;
        case 'c':
            vidPlayer.setFrame(11424);
            break;
#endif
        default:
            break;
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}

//--------------------------------------------------------------
void ofApp::blobOn( int x, int y, int id, int order )
{
    m.setAddress("/blob/on");
    m.addIntArg(order);
    sender.sendMessage(m);
    m.clear();
#ifdef DEBUG
    cerr << "BLOB ON ID: " << id << " ORDER: " << order << endl;
#endif
}

//--------------------------------------------------------------
void ofApp::blobMoved( int x, int y, int id, int order)
{
    
}

//--------------------------------------------------------------
void ofApp::blobOff( int x, int y, int id, int order )
{
    m.setAddress("/blob/off");
    m.addIntArg(order);
    sender.sendMessage(m);
    m.clear();
#ifdef DEBUG
    cerr << "BLOB OFF ID: " << id << " ORDER: " << order << endl;
#endif
}


