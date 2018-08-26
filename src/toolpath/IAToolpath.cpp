//
//  IAToolpath.cpp
//
//  Copyright (c) 2013-2018 Matthias Melcher. All rights reserved.
//


#include "IAToolpath.h"

#include "Iota.h"

#include <FL/gl.h>

#include <math.h>


/* Bresenham line drawing
 */

bool isBlack(uint8_t *rgb, IAVector3d v)
{
    IAVector3d s = v * (1024.0 / 214.0);
    int xo = (int)s.x(), yo = (int)s.y();
    uint8_t *c = rgb + (xo+1024*yo)*3;
    if (c[0]<128 && c[1]<128 && c[2]<128) {
        return true;
    } else {
        return false;
    }
}

void IAToolpath::colorize(uint8_t *rgb, IAToolpath *black, IAToolpath *white)
{
    for (auto e: pList) {
        IAToolpathMotion *m = dynamic_cast<IAToolpathMotion*>(e);
        if (m) {
            if (!m->pIsRapid) {
#if 0 // simple way to find color by looking at the start point
                if (isBlack(rgb, m->pStart)) {
                    black->pList.push_back(m->clone());
                } else {
                    white->pList.push_back(m->clone());
                }
#else // look at the vector and check the point color at every milimeter, splicing motion if needed
                IAVector3d startVec = m->pStart;
                IAVector3d currStartVec = startVec;
                IAVector3d currVec = startVec;
                IAVector3d endVec = m->pEnd;
                IAVector3d deltaVec = endVec - startVec;
                double len = (endVec-startVec).length();
                double incr = 0.1;
                bool color = isBlack(rgb, startVec);
                for (double i=incr; i<len; i+=incr) {
                    currVec = startVec + (deltaVec*(i/len));
                    bool colorNow = isBlack(rgb, currVec);
                    if (colorNow!=color) {
                        IAToolpathMotion *mtn = new IAToolpathMotion(currStartVec, currVec);
                        if (color) black->pList.push_back(mtn);
                        else white->pList.push_back(mtn);
                        currStartVec = currVec;
                        color = colorNow;
                    }
                }
                if (currStartVec!=endVec) {
                    IAToolpathMotion *mtn = new IAToolpathMotion(currStartVec, endVec);
                    if (color) black->pList.push_back(mtn);
                    else white->pList.push_back(mtn);
                }
#endif
            } else {

            }
        }
    }
}


uint32_t getRGB(uint8_t *rgb, IAVector3d v)
{
    IAVector3d s = v * (1024.0 / 214.0);
    int xo = (int)s.x(), yo = (int)s.y();
    uint8_t *c = rgb + (xo+1024*yo)*3;
    return ((c[0]<<16)|(c[1]<<8)|(c[2]));
}

bool differ(uint32_t c1, uint32_t c2)
{
    if (c1==c2) return false;
    int r1 = (c1>>16)&255, r2 = (c2>>16)&255, rd = r1-r2;
    if (rd>10 || rd<-10) return true;
    int g1 = (c1>>8)&255, g2 = (c2>>8)&255, gd = g1-g2;
    if (gd>10 || gd<-10) return true;
    int b1 = (c1>>0)&255, b2 = (c2>>0)&255, bd = b1-b2;
    if (bd>10 || bd<-10) return true;
    return false;
}


void IAToolpath::colorizeSoft(uint8_t *rgb, IAToolpath *dst)
{
    for (auto e: pList) {
        IAToolpathMotion *m = dynamic_cast<IAToolpathMotion*>(e);
        if (m) {
            if (!m->pIsRapid) {
                IAVector3d startVec = m->pStart;
                IAVector3d currStartVec = startVec;
                IAVector3d currVec = startVec;
                IAVector3d endVec = m->pEnd;
                IAVector3d deltaVec = endVec - startVec;
                double len = (endVec-startVec).length();
                double incr = 0.1;
                uint32_t color = getRGB(rgb, startVec);
                for (double i=incr; i<len; i+=incr) {
                    currVec = startVec + (deltaVec*(i/len));
                    uint32_t colorNow = getRGB(rgb, startVec);
                    if (differ(colorNow, color)) {
                        IAToolpathMotion *mtn = new IAToolpathMotion(currStartVec, currVec);
                        mtn->setColor(color);
                        dst->pList.push_back(mtn);
                        currStartVec = currVec;
                        color = colorNow;
                    }
                }
                if (currStartVec!=endVec) {
                    IAToolpathMotion *mtn = new IAToolpathMotion(currStartVec, endVec);
                    mtn->setColor(color);
                    dst->pList.push_back(mtn);
                }
            } else {

            }
        }
    }
}





/**
 * Create a list of toolpaths for the entire printout.
 */
IAMachineToolpath::IAMachineToolpath()
{
}


/**
 * Free all allocations.
 */
IAMachineToolpath::~IAMachineToolpath()
{
    clear();
}


/**
 * Free all allocations.
 */
void IAMachineToolpath::clear()
{
    delete pStartupPath;
    pStartupPath = nullptr;
    for (auto p: pLayerMap) {
        delete p.second;
    }
    pLayerMap.clear();
    delete pShutdownPath;
    pShutdownPath = nullptr;
}


/**
 * Draw the toolpath into the scene at world coordinates.
 */
void IAMachineToolpath::draw()
{
    if (pStartupPath) pStartupPath->draw();
    for (auto p: pLayerMap) {
        p.second->draw();
    }
    if (pShutdownPath) pShutdownPath->draw();
}


/**
 * DRaw the toolpath of only one layer.
 */
void IAMachineToolpath::drawLayer(double z)
{
    auto p = findLayer(z);
    if (p)
        p->draw();
}


/**
 * Return a layer at the give z height, or nullptr if none found.
 */
IAToolpath *IAMachineToolpath::findLayer(double z)
{
    int layer = roundLayerNumber(z);
    auto p = pLayerMap.find(layer);
    if (p==pLayerMap.end())
        return nullptr;
    else
        return p->second;
}


/**
 * Create a new toolpath for a layer at the give z height.
 */
IAToolpath *IAMachineToolpath::createLayer(double z)
{
    int layer = roundLayerNumber(z);
    auto p = pLayerMap.find(layer);
    if (p==pLayerMap.end()) {
        IAToolpath *tp = new IAToolpath(z);
        pLayerMap.insert(std::make_pair(layer, tp));
        return tp;
    } else {
        return p->second;
    }
}


/**
 * Delete a toolpath at the give heigt.
 */
void IAMachineToolpath::deleteLayer(double z)
{
    int layer = roundLayerNumber(z);
    pLayerMap.erase(layer);
}


/**
 * Round the z height into a layer number to avoid imprecissions of floating
 * point math.
 */
int IAMachineToolpath::roundLayerNumber(double z)
{
    return (int)lround(z*1000.0);
}


/**
 * Save the toolpath as a GCode file.
 */
bool IAMachineToolpath::saveGCode(const char *filename /*, printer */)
{
    bool ret = false;
    IAGcodeWriter w;
    if (w.open(filename)) {
        w.macroInit();
        if (pStartupPath)
            pStartupPath->saveGCode(w);
        for (auto p: pLayerMap) {
            w.cmdComment("");
            w.cmdComment("==== layer at z=%g", p.first / 1000.0);
            w.cmdComment("");
            w.cmdResetExtruder();
            // send all motion commands
            p.second->saveGCode(w);
        }
        if (pShutdownPath)
            pShutdownPath->saveGCode(w);
        w.macroShutdown();
        w.close();
        ret = true;
    }
    return ret;
}





/**
 * Manage a single toolpath.
 */
IAToolpath::IAToolpath(double z)
:   tFirst( 0.0, 0.0, z ),
    tPrev( 0.0, 0.0, z )
{
}


/**
 * Delete a toolpath.
 */
IAToolpath::~IAToolpath()
{
    clear(0.0);
}


/**
 * Clear a toolpath for its next use.
 */
void IAToolpath::clear(double z)
{
    pZ = z;
    for (auto e: pList) {
        delete e;
    }
    pList.clear();
    tFirst = { 0.0, 0.0, z };
    tPrev = { 0.0, 0.0, z };
}


void IAToolpath::add(IAToolpath &tp)
{
    for (auto e: tp.pList) {
        pList.push_back(e->clone());
    }
}


/**
 * Draw the current toolpath into the scene viewer at world coordinates.
 */
void IAToolpath::draw()
{
    glLineWidth(5.0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glColor3f(0, 1, 0);
    for (auto e: pList) {
        e->draw();
    }
    glLineWidth(1.0);
}


void IAToolpath::drawFlat(double w)
{
    glLineWidth(w);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    for (auto e: pList) {
        e->drawFlat();
    }
    glLineWidth(1.0);
}

/**
 * Start a new path.
 */
void IAToolpath::startPath(double x, double y, double z)
{
    IAVector3d next(x, y, z);
    tFirst = next;
    pList.push_back(new IAToolpathMotion(tPrev, next, true));
    tPrev = next;
}


/**
 * Add a motion segment to the path.
 */
void IAToolpath::continuePath(double x, double y, double z)
{
    IAVector3d next(x, y, z);
    if (!(tPrev==next))
        pList.push_back(new IAToolpathMotion(tPrev, next));
    tPrev = next;
}


/**
 * Create a loop by moving back to the very first vector.
 */
void IAToolpath::closePath()
{
    if (!(tPrev==tFirst))
        pList.push_back(new IAToolpathMotion(tPrev, tFirst));
}


/**
 * Save the toolpath as a GCode file.
 */
void IAToolpath::saveGCode(IAGcodeWriter &w)
{
    w.cmdComment("Send generated toolpath...");
    for (auto p: pList) {
        p->saveGCode(w);
    }
}





/**
 * Create any sort of toolpath element.
 */
IAToolpathElement::IAToolpathElement()
{
}


/**
 * Destroy an element.
 */
IAToolpathElement::~IAToolpathElement()
{
}


IAToolpathElement *IAToolpathElement::clone()
{
    return new IAToolpathElement();
}



/**
 * Draw any element.
 */
void IAToolpathElement::draw()
{
    // nothing to here
}



/**
 * Create any sort of toolpath element.
 */
IAToolpathExtruder::IAToolpathExtruder(int tool)
:   pTool( tool )
{
}


/**
 * Destroy an element.
 */
IAToolpathExtruder::~IAToolpathExtruder()
{
}


IAToolpathElement *IAToolpathExtruder::clone()
{
    IAToolpathExtruder *tpe = new IAToolpathExtruder(pTool);
    return tpe;
}


/**
 * Save the toolpath element as a GCode file.
 */
void IAToolpathExtruder::saveGCode(IAGcodeWriter &w)
{
    w.cmdComment("");
    w.cmdComment("---- Change to extruder %d", pTool);
    // deactivate the other extruder
    int otherTool = 1-pTool;
    w.cmdSelectExtruder(otherTool);
    w.cmdResetExtruder();
    w.cmdExtrude(-4.0);

    // activate the new extruder
    w.cmdSelectExtruder(pTool);
    w.cmdResetExtruder();
    w.cmdExtrude(4.0);
    int x = pTool ? 100 : 48;
    int pw = 20;
    w.cmdRapidMove(x, 10.0);
    int i;
    for (i=0; i<4; i++) {
        w.cmdMove(x+pw, 10.0+i);
        w.cmdMove(x+pw, 10.0+i+0.5);
        w.cmdMove(x, 10.0+i+0.5);
        w.cmdMove(x, 10.0+i+1.0);
    }
    w.cmdSelectExtruder(pTool); // redundant
    w.cmdResetExtruder();
    w.cmdComment("Extruder %d ready", pTool);
    w.cmdComment("");
}



/**
 * Create a toolpath for a head motion to a new position.
 */
IAToolpathMotion::IAToolpathMotion(IAVector3d &a, IAVector3d &b, bool rapid)
:   IAToolpathElement(),
    pStart( a ),
    pEnd( b ),
    pIsRapid( rapid )
{
}


IAToolpathElement *IAToolpathMotion::clone()
{
    IAToolpathMotion *mtn = new IAToolpathMotion(pStart, pEnd, pIsRapid);
    mtn->pColor = pColor;
    return mtn;
}


/**
 * Draw the toolpath motion into the scene viewer.
 */
void IAToolpathMotion::draw()
{
    if (pIsRapid) {
        glLineWidth(1.0);
        glColor3f(1.0, 1.0, 0.0);
    } else {
        glLineWidth(2.0);
        glColor3f(1.0, 0.0, 1.0);
    }
    glBegin(GL_LINES);
    glVertex3dv(pStart.dataPointer());
    glVertex3dv(pEnd.dataPointer());
    glEnd();
    glLineWidth(1.0);
}


/**
 * Draw the toolpath motion into the scene viewer.
 */
void IAToolpathMotion::drawFlat()
{
    if (!pIsRapid) {
        glBegin(GL_LINES);
        glVertex3dv(pStart.dataPointer());
        glVertex3dv(pEnd.dataPointer());
        glEnd();
    }
}


/**
 * Save the toolpath element as a GCode file.
 */
void IAToolpathMotion::saveGCode(IAGcodeWriter &w)
{
#ifdef IA_QUAD
    if (pIsRapid) {
        w.cmdExtrudeRel(-1.0);
        w.cmdRapidMove(pEnd);
        w.cmdExtrudeRel(+1.0);
    } else {
        if (w.position()!=pStart) {
            w.cmdExtrudeRel(-1.0);
            w.cmdRapidMove(pStart);
            w.cmdExtrudeRel(+1.0);
        }
        w.cmdMove(pEnd, pColor);
    }
#else
    if (pIsRapid) {
        w.cmdExtrude(-1.0);
        w.cmdRapidMove(pEnd);
        w.cmdExtrude(+1.0);
    } else {
        if (w.position()!=pStart) {
            w.cmdExtrude(-1.0);
            w.cmdRapidMove(pStart);
            w.cmdExtrude(+1.0);
        }
        w.cmdMove(pEnd);
    }
#endif
}

/*
 < 16:22:20: ok 6950
 < 16:22:20: SelectExtruder:1
 < 16:22:20: FlowMultiply:100
 < 16:22:20: Echo:N6950 T1
 < 16:22:20: ok 6951
 < 16:22:20: T:102.65 / 0 B:51.39 / 0 B@:0 @:0 T0:42.88 / 0 @0:0 T1:102.65 / 0 @1:0
 < 16:22:20: Echo:N6951 M105
 < 16:22:21: wait
 < 16:22:21: ok 6952
 < 16:22:21: T:102.10 / 0 B:51.39 / 0 B@:0 @:0 T0:42.88 / 0 @0:0 T1:102.10 / 0 @1:0
 < 16:22:21: Echo:N6952 M105
 < 16:22:22: wait
 < 16:22:23: ok 6953
 < 16:22:23: T:101.54 / 0 B:51.33 / 0 B@:0 @:0 T0:42.88 / 0 @0:0 T1:101.54 / 0 @1:0
 < 16:22:23: Echo:N6953 M105
 < 16:22:24: wait
 < 16:22:24: ok 6954
 < 16:22:24: T:101.12 / 0 B:51.33 / 0 B@:0 @:0 T0:42.66 / 0 @0:0 T1:101.12 / 0 @1:0
 < 16:22:24: Echo:N6954 M105
 < 16:22:25: ok 6955
 < 16:22:25: T:100.64 / 0 B:51.28 / 0 B@:0 @:0 T0:42.88 / 0 @0:0 T1:100.64 / 0 @1:0
 < 16:22:25: Echo:N6955 M105
 < 16:22:26: wait
 < 16:22:26: ok 6956
 < 16:22:26: T:100.22 / 0 B:51.28 / 0 B@:0 @:0 T0:42.88 / 0 @0:0 T1:100.22 / 0 @1:0
 < 16:22:26: Echo:N6956 M105
 < 16:22:27: ok 6957
 < 16:22:27: T:99.74 / 0 B:51.22 / 0 B@:0 @:0 T0:42.66 / 0 @0:0 T1:99.74 / 0 @1:0
 < 16:22:27: Echo:N6957 M105
 > 16:22:27: N6958 G0 E5.0000 F1000.000 *60
 < 16:22:27: ok 6958
 < 16:22:27: Echo:N6958 G0  E5.0000 F1000.00
 < 16:22:28: ok 6959
 < 16:22:28: T:99.32 / 0 B:51.17 / 0 B@:0 @:0 T0:42.66 / 0 @0:0 T1:99.32 / 0 @1:0
 < 16:22:28: Echo:N6959 M105
 < 16:22:29: wait
 < 16:22:29: ok 6960
 < 16:22:29: T:98.83 / 0 B:51.11 / 0 B@:0 @:0 T0:42.66 / 0 @0:0 T1:98.83 / 0 @1:0
 < 16:22:29: Echo:N6960 M105
 < 16:22:30: wait
 < 16:22:30: ok 6961
 < 16:22:30: T:98.42 / 0 B:51.11 / 0 B@:0 @:0 T0:42.66 / 0 @0:0 T1:98.42 / 0 @1:0
 < 16:22:30: Echo:N6961 M105
 < 16:22:31: wait
 < 16:22:31: ok 6962
 < 16:22:31: T:98.00 / 0 B:51.06 / 0 B@:0 @:0 T0:42.66 / 0 @0:0 T1:98.00 / 0 @1:0
 < 16:22:31: Echo:N6962 M105
 > 16:22:32: N6963 G28 *9
 < 16:22:32: ok 6963
 < 16:22:32: SelectExtruder:0
 < 16:22:32: FlowMultiply:100
 < 16:22:34: busy:processing
 < 16:22:36: busy:processing
 < 16:22:38: busy:processing
 < 16:22:39: X:0.00 Y:0.00 Z:0.000 E:5.0000
 < 16:22:39: Echo:N6963 G28
 < 16:22:39: ok 6964
 < 16:22:39: T:42.44 / 0 B:50.78 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:94.18 / 0 @1:0
 < 16:22:39: Echo:N6964 M105
 < 16:22:39: ok 6965
 < 16:22:39: T:42.44 / 0 B:50.78 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:94.18 / 0 @1:0
 < 16:22:39: Echo:N6965 M105
 < 16:22:39: ok 6966
 < 16:22:39: T:42.44 / 0 B:50.78 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:94.18 / 0 @1:0
 < 16:22:39: Echo:N6966 M105
 < 16:22:39: ok 6967
 < 16:22:39: T:42.44 / 0 B:50.78 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:94.18 / 0 @1:0
 < 16:22:39: Echo:N6967 M105
 < 16:22:39: ok 6968
 < 16:22:39: T:42.44 / 0 B:50.78 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:94.18 / 0 @1:0
 < 16:22:39: Echo:N6968 M105
 < 16:22:39: ok 6969
 < 16:22:39: T:42.44 / 0 B:50.72 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:93.92 / 0 @1:0
 < 16:22:39: Echo:N6969 M105
 < 16:22:40: wait
 < 16:22:41: ok 6970
 < 16:22:41: T:42.44 / 0 B:50.72 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:93.50 / 0 @1:0
 < 16:22:41: Echo:N6970 M105
 > 16:22:41: N6971 G91 *8
 > 16:22:41: N6972 G1 Y10.00 F12000.000 *47
 < 16:22:41: ok 6971
 > 16:22:41: N6973 G90 *11
 < 16:22:41: Echo:N6971 G91
 < 16:22:41: ok 6972
 < 16:22:41: Echo:N6972 G1  Y10.00 F12000.00
 < 16:22:41: ok 6973
 < 16:22:41: Echo:N6973 G90
 > 16:22:41: N6974 G91 *13
 > 16:22:41: N6975 G1 Y10.00 F12000.000 *40
 < 16:22:41: ok 6974
 > 16:22:41: N6976 G90 *14
 < 16:22:41: Echo:N6974 G91
 < 16:22:41: ok 6975
 < 16:22:41: Echo:N6975 G1  Y10.00 F12000.00
 < 16:22:41: ok 6976
 < 16:22:41: Echo:N6976 G90
 > 16:22:42: N6977 G91 *14
 > 16:22:42: N6978 G1 Y10.00 F12000.000 *37
 < 16:22:42: ok 6977
 > 16:22:42: N6979 G90 *1
 < 16:22:42: Echo:N6977 G91
 < 16:22:42: ok 6978
 < 16:22:42: Echo:N6978 G1  Y10.00 F12000.00
 < 16:22:42: ok 6979
 < 16:22:42: Echo:N6979 G90
 < 16:22:42: ok 6980
 < 16:22:42: T:42.44 / 0 B:50.67 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:92.99 / 0 @1:0
 < 16:22:42: Echo:N6980 M105
 > 16:22:42: N6981 G91 *7
 > 16:22:42: N6982 G1 Y10.00 F12000.000 *32
 < 16:22:42: ok 6981
 > 16:22:42: N6983 G90 *4
 < 16:22:42: Echo:N6981 G91
 < 16:22:42: ok 6982
 < 16:22:42: Echo:N6982 G1  Y10.00 F12000.00
 < 16:22:42: ok 6983
 < 16:22:42: Echo:N6983 G90
 > 16:22:42: N6984 G91 *2
 > 16:22:42: N6985 G1 Y10.00 F12000.000 *39
 < 16:22:42: ok 6984
 > 16:22:42: N6986 G90 *1
 < 16:22:42: Echo:N6984 G91
 < 16:22:42: ok 6985
 < 16:22:42: Echo:N6985 G1  Y10.00 F12000.00
 < 16:22:42: ok 6986
 < 16:22:42: Echo:N6986 G90
 > 16:22:42: N6987 G91 *1
 > 16:22:42: N6988 G1 Y10.00 F12000.000 *42
 < 16:22:42: ok 6987
 > 16:22:42: N6989 G90 *14
 < 16:22:42: Echo:N6987 G91
 < 16:22:42: ok 6988
 < 16:22:42: Echo:N6988 G1  Y10.00 F12000.00
 < 16:22:42: ok 6989
 < 16:22:42: Echo:N6989 G90
 > 16:22:43: N6990 G91 *7
 > 16:22:43: N6991 G1 Y10.00 F12000.000 *34
 < 16:22:43: ok 6990
 > 16:22:43: N6992 G90 *4
 < 16:22:43: Echo:N6990 G91
 < 16:22:43: ok 6991
 < 16:22:43: Echo:N6991 G1  Y10.00 F12000.00
 < 16:22:43: ok 6992
 < 16:22:43: Echo:N6992 G90
 < 16:22:43: ok 6993
 < 16:22:43: T:42.44 / 0 B:50.67 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:92.65 / 0 @1:0
 < 16:22:43: Echo:N6993 M105
 > 16:22:43: N6994 G91 *3
 > 16:22:43: N6995 G1 Y10.00 F12000.000 *38
 < 16:22:43: ok 6994
 > 16:22:43: N6996 G90 *0
 < 16:22:43: Echo:N6994 G91
 < 16:22:43: ok 6995
 < 16:22:43: Echo:N6995 G1  Y10.00 F12000.00
 < 16:22:43: ok 6996
 < 16:22:43: Echo:N6996 G90
 < 16:22:44: ok 6997
 < 16:22:44: T:42.22 / 0 B:50.61 / 0 B@:0 @:0 T0:42.22 / 0 @0:0 T1:92.14 / 0 @1:0
 < 16:22:44: Echo:N6997 M105
 < 16:22:45: wait
 < 16:22:45: ok 6998
 < 16:22:45: T:42.22 / 0 B:50.61 / 0 B@:0 @:0 T0:42.22 / 0 @0:0 T1:91.71 / 0 @1:0
 < 16:22:45: Echo:N6998 M105
 < 16:22:46: wait
 < 16:22:46: ok 6999
 < 16:22:46: T:42.44 / 0 B:50.61 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:91.20 / 0 @1:0
 < 16:22:46: Echo:N6999 M105
 > 16:22:46: N7000 T1 *44
 < 16:22:46: ok 7000
 < 16:22:46: SelectExtruder:1
 < 16:22:46: FlowMultiply:100
 < 16:22:47: Echo:N7000 T1
 < 16:22:47: ok 7001
 < 16:22:47: T:90.86 / 0 B:50.56 / 0 B@:0 @:0 T0:42.22 / 0 @0:0 T1:90.86 / 0 @1:0
 < 16:22:47: Echo:N7001 M105
 < 16:22:48: ok 7002
 < 16:22:48: T:90.44 / 0 B:50.56 / 0 B@:0 @:0 T0:42.22 / 0 @0:0 T1:90.44 / 0 @1:0
 < 16:22:48: Echo:N7002 M105
 < 16:22:49: wait
 < 16:22:49: ok 7003
 < 16:22:49: T:90.01 / 0 B:50.50 / 0 B@:0 @:0 T0:42.22 / 0 @0:0 T1:90.01 / 0 @1:0
 < 16:22:49: Echo:N7003 M105
 > 16:22:50: N7004 T0 *41
 < 16:22:50: ok 7004
 < 16:22:50: SelectExtruder:0
 < 16:22:50: FlowMultiply:100
 < 16:22:50: Echo:N7004 T0
 < 16:22:50: ok 7005
 < 16:22:50: T:42.22 / 0 B:50.44 / 0 B@:0 @:0 T0:42.22 / 0 @0:0 T1:89.42 / 0 @1:0
 < 16:22:50: Echo:N7005 M105
 < 16:22:51: ok 7006
 < 16:22:51: T:42.22 / 0 B:50.44 / 0 B@:0 @:0 T0:42.22 / 0 @0:0 T1:89.08 / 0 @1:0
 < 16:22:51: Echo:N7006 M105
 < 16:22:52: wait
 < 16:22:52: ok 7007
 < 16:22:52: T:42.22 / 0 B:50.44 / 0 B@:0 @:0 T0:42.22 / 0 @0:0 T1:88.74 / 0 @1:0
 < 16:22:52: Echo:N7007 M105
 < 16:22:53: wait
 < 16:22:53: ok 7008
 < 16:22:53: T:42.22 / 0 B:50.39 / 0 B@:0 @:0 T0:42.22 / 0 @0:0 T1:88.31 / 0 @1:0
 < 16:22:53: Echo:N7008 M105
 < 16:22:54: wait
 < 16:22:54: ok 7009
 < 16:22:54: T:42.00 / 0 B:50.39 / 0 B@:0 @:0 T0:42.00 / 0 @0:0 T1:87.97 / 0 @1:0
 < 16:22:54: Echo:N7009 M105
 < 16:22:55: ok 7010
 < 16:22:55: T:42.00 / 0 B:50.33 / 0 B@:0 @:0 T0:42.00 / 0 @0:0 T1:87.55 / 0 @1:0
 < 16:22:55: Echo:N7010 M105
 < 16:22:56: wait
 < 16:22:56: ok 7011
 < 16:22:56: T:42.00 / 0 B:50.33 / 0 B@:0 @:0 T0:42.00 / 0 @0:0 T1:87.21 / 0 @1:0
 < 16:22:56: Echo:N7011 M105
 < 16:22:57: wait
 < 16:22:58: ok 7012
 < 16:22:58: T:42.00 / 0 B:50.28 / 0 B@:0 @:0 T0:42.00 / 0 @0:0 T1:86.87 / 0 @1:0
 < 16:22:58: Echo:N7012 M105
 < 16:22:59: wait
 < 16:22:59: ok 7013
 < 16:22:59: T:42.00 / 0 B:50.28 / 0 B@:0 @:0 T0:42.00 / 0 @0:0 T1:86.44 / 0 @1:0
 < 16:22:59: Echo:N7013 M105
 < 16:23:00: ok 7014
 < 16:23:00: T:42.00 / 0 B:50.28 / 0 B@:0 @:0 T0:42.00 / 0 @0:0 T1:86.10 / 0 @1:0
 < 16:23:00: Echo:N7014 M105
 < 16:23:01: wait
 < 16:23:01: ok 7015
 < 16:23:01: T:41.58 / 0 B:50.22 / 0 B@:0 @:0 T0:41.58 / 0 @0:0 T1:85.76 / 0 @1:0
 < 16:23:01: Echo:N7015 M105
 < 16:23:02: wait
 < 16:23:02: ok 7016
 < 16:23:02: T:42.00 / 0 B:50.22 / 0 B@:0 @:0 T0:42.00 / 0 @0:0 T1:85.51 / 0 @1:0
 < 16:23:02: Echo:N7016 M105
 > 16:23:03: N7017 G1T1 E5.0000 F1000.000 *91
 < 16:23:03: ok 7017
 < 16:23:03: Echo:N7017 G1 T1  E5.0000 F1000.00
 < 16:23:03: ok 7018
 < 16:23:03: T:42.00 / 0 B:50.22 / 0 B@:0 @:0 T0:42.00 / 0 @0:0 T1:85.08 / 0 @1:0
 < 16:23:03: Echo:N7018 M105
 < 16:23:04: ok 7019
 < 16:23:04: T:41.58 / 0 B:50.17 / 0 B@:0 @:0 T0:41.58 / 0 @0:0 T1:84.78 / 0 @1:0
 < 16:23:04: Echo:N7019 M105
 < 16:23:05: wait
 < 16:23:05: ok 7020
 < 16:23:05: T:41.58 / 0 B:50.17 / 0 B@:0 @:0 T0:41.58 / 0 @0:0 T1:84.34 / 0 @1:0
 < 16:23:05: Echo:N7020 M105
 < 16:23:06: wait
 < 16:23:06: ok 7021
 < 16:23:06: T:41.58 / 0 B:50.11 / 0 B@:0 @:0 T0:41.58 / 0 @0:0 T1:83.90 / 0 @1:0
 < 16:23:06: Echo:N7021 M105
 < 16:23:07: ok 7022
 < 16:23:07: T:41.58 / 0 B:50.11 / 0 B@:0 @:0 T0:41.58 / 0 @0:0 T1:83.45 / 0 @1:0
 < 16:23:07: Echo:N7022 M105
 < 16:23:08: wait
 < 16:23:08: ok 7023
 < 16:23:08: T:41.58 / 0 B:50.11 / 0 B@:0 @:0 T0:41.58 / 0 @0:0 T1:83.01 / 0 @1:0
 < 16:23:08: Echo:N7023 M105
 < 16:23:09: wait
 < 16:23:09: ok 7024
 < 16:23:09: T:41.58 / 0 B:50.11 / 0 B@:0 @:0 T0:41.58 / 0 @0:0 T1:82.68 / 0 @1:0
 < 16:23:09: Echo:N7024 M105
 < 16:23:10: wait
 < 16:23:10: ok 7025
 < 16:23:10: T:41.58 / 0 B:50.06 / 0 B@:0 @:0 T0:41.58 / 0 @0:0 T1:82.24 / 0 @1:0
 < 16:23:10: Echo:N7025 M105
 < 16:23:11: wait
 < 16:23:11: ok 7026
 < 16:23:11: T:41.58 / 0 B:50.06 / 0 B@:0 @:0 T0:41.58 / 0 @0:0 T1:81.80 / 0 @1:0
 < 16:23:11: Echo:N7026 M105
 < 16:23:12: wait
 < 16:23:12: ok 7027
 < 16:23:12: T:41.16 / 0 B:50.00 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:81.36 / 0 @1:0
 < 16:23:12: Echo:N7027 M105
 < 16:23:13: wait
 < 16:23:14: ok 7028
 < 16:23:14: T:41.16 / 0 B:50.00 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:80.91 / 0 @1:0
 < 16:23:14: Echo:N7028 M105
 < 16:23:15: wait
 < 16:23:15: ok 7029
 < 16:23:15: T:41.16 / 0 B:50.00 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:80.58 / 0 @1:0
 < 16:23:15: Echo:N7029 M105
 < 16:23:16: wait
 < 16:23:16: ok 7030
 < 16:23:16: T:41.16 / 0 B:49.94 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:80.25 / 0 @1:0
 < 16:23:16: Echo:N7030 M105
 < 16:23:17: wait
 < 16:23:17: ok 7031
 < 16:23:17: T:41.16 / 0 B:49.94 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:79.81 / 0 @1:0
 < 16:23:17: Echo:N7031 M105
 < 16:23:18: wait
 < 16:23:18: ok 7032
 < 16:23:18: T:41.16 / 0 B:49.94 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:79.48 / 0 @1:0
 < 16:23:18: Echo:N7032 M105
 < 16:23:19: ok 7033
 < 16:23:19: T:41.16 / 0 B:49.94 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:79.15 / 0 @1:0
 < 16:23:19: Echo:N7033 M105
 < 16:23:20: wait
 < 16:23:20: ok 7034
 < 16:23:20: T:40.74 / 0 B:49.87 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:78.71 / 0 @1:0
 < 16:23:20: Echo:N7034 M105
 > 16:23:20: N7035 T1 *42
 < 16:23:20: ok 7035
 < 16:23:20: SelectExtruder:1
 < 16:23:20: FlowMultiply:100
 < 16:23:21: Echo:N7035 T1
 < 16:23:21: ok 7036
 < 16:23:21: T:78.37 / 0 B:49.87 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:78.37 / 0 @1:0
 < 16:23:21: Echo:N7036 M105
 < 16:23:22: ok 7037
 < 16:23:22: T:78.04 / 0 B:49.81 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:78.04 / 0 @1:0
 < 16:23:22: Echo:N7037 M105
 > 16:23:22: N7038 T1 *39
 < 16:23:22: ok 7038
 < 16:23:22: SelectExtruder:1
 < 16:23:22: FlowMultiply:100
 < 16:23:22: Echo:N7038 T1
 < 16:23:23: ok 7039
 < 16:23:23: T:77.71 / 0 B:49.81 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:77.71 / 0 @1:0
 < 16:23:23: Echo:N7039 M105
 < 16:23:24: wait
 < 16:23:24: ok 7040
 < 16:23:24: T:77.38 / 0 B:49.81 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:77.38 / 0 @1:0
 < 16:23:24: Echo:N7040 M105
 > 16:23:25: N7041 T0 *40
 < 16:23:25: ok 7041
 < 16:23:25: SelectExtruder:0
 < 16:23:25: FlowMultiply:100
 < 16:23:25: Echo:N7041 T0
 < 16:23:25: ok 7042
 < 16:23:25: T:41.16 / 0 B:49.81 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:77.05 / 0 @1:0
 < 16:23:25: Echo:N7042 M105
 < 16:23:26: wait
 < 16:23:26: ok 7043
 < 16:23:26: T:40.74 / 0 B:49.74 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:76.61 / 0 @1:0
 < 16:23:26: Echo:N7043 M105
 < 16:23:28: ok 7044
 < 16:23:28: T:40.74 / 0 B:49.74 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:76.39 / 0 @1:0
 < 16:23:28: Echo:N7044 M105
 < 16:23:28: ok 7045
 < 16:23:28: T:40.74 / 0 B:49.74 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:76.06 / 0 @1:0
 < 16:23:28: Echo:N7045 M105
 < 16:23:29: ok 7046
 < 16:23:29: T:40.74 / 0 B:49.74 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:75.83 / 0 @1:0
 < 16:23:29: Echo:N7046 M105
 < 16:23:30: wait
 < 16:23:31: ok 7047
 < 16:23:31: T:40.74 / 0 B:49.68 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:75.50 / 0 @1:0
 < 16:23:31: Echo:N7047 M105
 < 16:23:32: wait
 < 16:23:32: ok 7048
 < 16:23:32: T:40.74 / 0 B:49.68 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:75.28 / 0 @1:0
 < 16:23:32: Echo:N7048 M105
 > 16:23:32: N7049 G1 Y90.00 *108
 < 16:23:32: ok 7049
 < 16:23:32: Echo:N7049 G1  Y90.00
 < 16:23:33: ok 7050
 < 16:23:33: T:40.74 / 0 B:49.61 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:74.95 / 0 @1:0
 < 16:23:33: Echo:N7050 M105
 < 16:23:34: ok 7051
 < 16:23:34: T:40.74 / 0 B:49.61 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:74.62 / 0 @1:0
 < 16:23:34: Echo:N7051 M105
 < 16:23:35: wait
 < 16:23:35: ok 7052
 < 16:23:35: T:40.74 / 0 B:49.61 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:74.29 / 0 @1:0
 < 16:23:35: Echo:N7052 M105
 < 16:23:36: wait
 < 16:23:36: ok 7053
 < 16:23:36: T:40.74 / 0 B:49.55 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:74.07 / 0 @1:0
 < 16:23:36: Echo:N7053 M105
 > 16:23:37: N7054 G1 Y100.00 *88
 < 16:23:37: ok 7054
 < 16:23:37: Echo:N7054 G1  Y100.00
 < 16:23:37: ok 7055
 < 16:23:37: T:40.33 / 0 B:49.55 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:73.74 / 0 @1:0
 < 16:23:37: Echo:N7055 M105
 < 16:23:38: ok 7056
 < 16:23:38: T:40.33 / 0 B:49.55 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:73.52 / 0 @1:0
 < 16:23:38: Echo:N7056 M105
 < 16:23:39: wait
 < 16:23:39: ok 7057
 < 16:23:39: T:40.33 / 0 B:49.48 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:73.18 / 0 @1:0
 < 16:23:39: Echo:N7057 M105
 < 16:23:40: wait
 < 16:23:40: ok 7058
 < 16:23:40: T:40.33 / 0 B:49.48 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:72.96 / 0 @1:0
 < 16:23:40: Echo:N7058 M105
 < 16:23:41: ok 7059
 < 16:23:41: T:40.33 / 0 B:49.48 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:72.63 / 0 @1:0
 < 16:23:41: Echo:N7059 M105
 > 16:23:42: N7060 G1 Y90.00 *103
 < 16:23:42: ok 7060
 < 16:23:42: Echo:N7060 G1  Y90.00
 < 16:23:42: ok 7061
 < 16:23:42: T:40.33 / 0 B:49.42 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:72.41 / 0 @1:0
 < 16:23:42: Echo:N7061 M105
 < 16:23:43: wait
 < 16:23:43: ok 7062
 < 16:23:43: T:40.33 / 0 B:49.42 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:72.08 / 0 @1:0
 < 16:23:43: Echo:N7062 M105
 < 16:23:44: ok 7063
 < 16:23:44: T:40.33 / 0 B:49.42 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:71.86 / 0 @1:0
 < 16:23:44: Echo:N7063 M105
 < 16:23:45: ok 7064
 < 16:23:45: T:40.33 / 0 B:49.35 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:71.64 / 0 @1:0
 < 16:23:45: Echo:N7064 M105
 < 16:23:46: ok 7065
 < 16:23:46: T:39.91 / 0 B:49.35 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:71.42 / 0 @1:0
 < 16:23:46: Echo:N7065 M105
 < 16:23:47: wait
 < 16:23:48: ok 7066
 < 16:23:48: T:39.91 / 0 B:49.35 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:71.20 / 0 @1:0
 < 16:23:48: Echo:N7066 M105
 < 16:23:49: ok 7067
 < 16:23:49: T:39.91 / 0 B:49.29 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:70.98 / 0 @1:0
 < 16:23:49: Echo:N7067 M105
 < 16:23:50: wait
 < 16:23:50: ok 7068
 < 16:23:50: T:39.91 / 0 B:49.29 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:70.75 / 0 @1:0
 < 16:23:50: Echo:N7068 M105
 > 16:23:50: N7069 G1T1 Y100.00 *51
 < 16:23:50: ok 7069
 < 16:23:50: Echo:N7069 G1 T1  Y100.00
 < 16:23:51: ok 7070
 < 16:23:51: T:39.91 / 0 B:49.29 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:70.42 / 0 @1:0
 < 16:23:51: Echo:N7070 M105
 < 16:23:52: wait
 < 16:23:52: ok 7071
 < 16:23:52: T:39.91 / 0 B:49.29 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:70.31 / 0 @1:0
 < 16:23:52: Echo:N7071 M105
 < 16:23:53: wait
 < 16:23:53: ok 7072
 < 16:23:53: T:39.91 / 0 B:49.29 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:70.09 / 0 @1:0
 < 16:23:53: Echo:N7072 M105
 < 16:23:54: ok 7073
 < 16:23:54: T:39.91 / 0 B:49.22 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:69.87 / 0 @1:0
 < 16:23:54: Echo:N7073 M105
 < 16:23:55: wait
 < 16:23:55: ok 7074
 < 16:23:55: T:39.91 / 0 B:49.22 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:69.65 / 0 @1:0
 < 16:23:55: Echo:N7074 M105
 < 16:23:56: wait
 < 16:23:56: ok 7075
 < 16:23:56: T:39.49 / 0 B:49.16 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:69.32 / 0 @1:0
 < 16:23:56: Echo:N7075 M105
 < 16:23:57: ok 7076
 < 16:23:57: T:39.49 / 0 B:49.16 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:69.10 / 0 @1:0
 < 16:23:57: Echo:N7076 M105
 < 16:23:58: wait
 < 16:23:58: ok 7077
 < 16:23:58: T:39.49 / 0 B:49.09 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:68.88 / 0 @1:0
 < 16:23:58: Echo:N7077 M105
 < 16:23:59: ok 7078
 < 16:23:59: T:39.49 / 0 B:49.09 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:68.66 / 0 @1:0
 < 16:23:59: Echo:N7078 M105
 < 16:24:00: wait
 < 16:24:00: ok 7079
 < 16:24:00: T:39.49 / 0 B:49.09 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:68.44 / 0 @1:0
 < 16:24:00: Echo:N7079 M105
 < 16:24:01: wait
 < 16:24:01: ok 7080
 < 16:24:01: T:39.49 / 0 B:49.09 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:68.33 / 0 @1:0
 < 16:24:01: Echo:N7080 M105
 < 16:24:02: wait
 < 16:24:03: ok 7081
 < 16:24:03: T:39.49 / 0 B:49.03 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:68.10 / 0 @1:0
 < 16:24:03: Echo:N7081 M105
 < 16:24:04: ok 7082
 < 16:24:04: T:39.49 / 0 B:49.03 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:67.88 / 0 @1:0
 < 16:24:04: Echo:N7082 M105
 < 16:24:05: wait
 < 16:24:05: ok 7083
 < 16:24:05: T:39.49 / 0 B:49.03 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:67.66 / 0 @1:0
 < 16:24:05: Echo:N7083 M105
 < 16:24:06: ok 7084
 < 16:24:06: T:39.49 / 0 B:48.96 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:67.55 / 0 @1:0
 < 16:24:06: Echo:N7084 M105
 < 16:24:07: wait
 < 16:24:07: ok 7085
 < 16:24:07: T:39.49 / 0 B:48.96 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:67.33 / 0 @1:0
 < 16:24:07: Echo:N7085 M105
 < 16:24:08: wait
 < 16:24:08: ok 7086
 < 16:24:08: T:39.49 / 0 B:48.96 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:67.11 / 0 @1:0
 < 16:24:08: Echo:N7086 M105
 > 16:24:08: N7087 G1 E5.0000 F800.000 *14
 < 16:24:08: ok 7087
 < 16:24:08: Echo:N7087 G1  E5.0000 F800.00
 < 16:24:09: ok 7088
 < 16:24:09: T:39.49 / 0 B:48.90 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:66.82 / 0 @1:0
 < 16:24:09: Echo:N7088 M105
 < 16:24:10: wait
 < 16:24:10: ok 7089
 < 16:24:10: T:39.07 / 0 B:48.90 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:66.64 / 0 @1:0
 < 16:24:10: Echo:N7089 M105
 < 16:24:11: wait
 < 16:24:11: ok 7090
 < 16:24:11: T:39.07 / 0 B:48.90 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:66.28 / 0 @1:0
 < 16:24:11: Echo:N7090 M105
 < 16:24:12: wait
 < 16:24:12: ok 7091
 < 16:24:12: T:39.07 / 0 B:48.83 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:66.10 / 0 @1:0
 < 16:24:12: Echo:N7091 M105
 < 16:24:13: ok 7092
 < 16:24:13: T:39.07 / 0 B:48.83 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:65.74 / 0 @1:0
 < 16:24:13: Echo:N7092 M105
 < 16:24:14: wait
 < 16:24:14: ok 7093
 < 16:24:14: T:39.07 / 0 B:48.77 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:65.56 / 0 @1:0
 < 16:24:14: Echo:N7093 M105
 < 16:24:15: wait
 < 16:24:15: ok 7094
 < 16:24:15: T:39.07 / 0 B:48.77 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:65.20 / 0 @1:0
 < 16:24:15: Echo:N7094 M105
 < 16:24:16: ok 7095
 < 16:24:16: T:39.07 / 0 B:48.77 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:64.84 / 0 @1:0
 < 16:24:16: Echo:N7095 M105
 < 16:24:17: ok 7096
 < 16:24:17: T:39.07 / 0 B:48.77 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:64.66 / 0 @1:0
 < 16:24:17: Echo:N7096 M105
 < 16:24:18: wait
 < 16:24:18: ok 7097
 < 16:24:18: T:39.07 / 0 B:48.77 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:64.30 / 0 @1:0
 < 16:24:18: Echo:N7097 M105
 < 16:24:19: wait
 < 16:24:19: ok 7098
 < 16:24:19: T:39.07 / 0 B:48.70 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:64.11 / 0 @1:0
 < 16:24:19: Echo:N7098 M105
 < 16:24:20: ok 7099
 < 16:24:20: T:38.65 / 0 B:48.70 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:63.93 / 0 @1:0
 < 16:24:20: Echo:N7099 M105
 < 16:24:21: wait
 < 16:24:21: ok 7100
 < 16:24:21: T:39.07 / 0 B:48.70 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:63.75 / 0 @1:0
 < 16:24:21: Echo:N7100 M105
 < 16:24:22: wait
 < 16:24:23: ok 7101
 < 16:24:23: T:38.65 / 0 B:48.64 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:63.21 / 0 @1:0
 < 16:24:23: Echo:N7101 M105
 < 16:24:24: ok 7102
 < 16:24:24: T:39.07 / 0 B:48.70 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:63.21 / 0 @1:0
 < 16:24:24: Echo:N7102 M105
 < 16:24:25: wait
 < 16:24:25: ok 7103
 < 16:24:25: T:38.65 / 0 B:48.64 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:62.85 / 0 @1:0
 < 16:24:25: Echo:N7103 M105
 < 16:24:26: wait
 < 16:24:26: ok 7104
 < 16:24:26: T:38.65 / 0 B:48.57 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:62.67 / 0 @1:0
 < 16:24:26: Echo:N7104 M105
 < 16:24:27: wait
 < 16:24:27: ok 7105
 < 16:24:27: T:38.65 / 0 B:48.57 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:62.31 / 0 @1:0
 < 16:24:27: Echo:N7105 M105
 < 16:24:28: ok 7106
 < 16:24:28: T:38.65 / 0 B:48.57 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:62.13 / 0 @1:0
 < 16:24:28: Echo:N7106 M105
 < 16:24:29: wait
 < 16:24:29: ok 7107
 < 16:24:29: T:38.65 / 0 B:48.51 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:61.95 / 0 @1:0
 < 16:24:29: Echo:N7107 M105
 < 16:24:30: ok 7108
 < 16:24:30: T:38.65 / 0 B:48.51 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:61.59 / 0 @1:0
 < 16:24:30: Echo:N7108 M105
 < 16:24:31: ok 7109
 < 16:24:31: T:38.65 / 0 B:48.51 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:61.41 / 0 @1:0
 < 16:24:31: Echo:N7109 M105
 < 16:24:32: ok 7110
 < 16:24:32: T:38.65 / 0 B:48.51 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:61.23 / 0 @1:0
 < 16:24:32: Echo:N7110 M105
 < 16:24:33: wait
 < 16:24:33: ok 7111
 < 16:24:33: T:38.65 / 0 B:48.44 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:61.05 / 0 @1:0
 < 16:24:33: Echo:N7111 M105
 < 16:24:34: wait
 < 16:24:34: ok 7112
 < 16:24:34: T:38.23 / 0 B:48.44 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:60.69 / 0 @1:0
 < 16:24:34: Echo:N7112 M105
 < 16:24:35: ok 7113
 < 16:24:35: T:38.23 / 0 B:48.44 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:60.51 / 0 @1:0
 < 16:24:35: Echo:N7113 M105
 < 16:24:36: wait
 < 16:24:36: ok 7114
 < 16:24:36: T:38.23 / 0 B:48.38 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:60.33 / 0 @1:0
 < 16:24:36: Echo:N7114 M105
 < 16:24:37: wait
 < 16:24:37: ok 7115
 < 16:24:37: T:38.23 / 0 B:48.38 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:60.15 / 0 @1:0
 < 16:24:37: Echo:N7115 M105
 < 16:24:38: ok 7116
 < 16:24:38: T:38.23 / 0 B:48.38 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:59.97 / 0 @1:0
 < 16:24:38: Echo:N7116 M105
 < 16:24:39: wait
 < 16:24:40: ok 7117
 < 16:24:40: T:38.23 / 0 B:48.31 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:59.79 / 0 @1:0
 < 16:24:40: Echo:N7117 M105
 < 16:24:41: ok 7118
 < 16:24:41: T:38.23 / 0 B:48.31 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:59.43 / 0 @1:0
 < 16:24:41: Echo:N7118 M105
 < 16:24:42: wait
 < 16:24:42: ok 7119
 < 16:24:42: T:38.23 / 0 B:48.31 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:59.25 / 0 @1:0
 < 16:24:42: Echo:N7119 M105
 < 16:24:43: ok 7120
 < 16:24:43: T:38.23 / 0 B:48.25 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:59.07 / 0 @1:0
 < 16:24:43: Echo:N7120 M105
 < 16:24:44: wait
 < 16:24:44: ok 7121
 < 16:24:44: T:38.23 / 0 B:48.25 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:58.89 / 0 @1:0
 < 16:24:44: Echo:N7121 M105
 < 16:24:45: ok 7122
 < 16:24:45: T:38.23 / 0 B:48.25 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:58.70 / 0 @1:0
 < 16:24:45: Echo:N7122 M105
 < 16:24:46: wait
 < 16:24:46: ok 7123
 < 16:24:46: T:38.23 / 0 B:48.25 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:58.52 / 0 @1:0
 < 16:24:46: Echo:N7123 M105
 < 16:24:47: wait
 < 16:24:47: ok 7124
 < 16:24:47: T:37.81 / 0 B:48.25 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:58.34 / 0 @1:0
 < 16:24:47: Echo:N7124 M105
 < 16:24:48: wait
 < 16:24:48: ok 7125
 < 16:24:48: T:38.23 / 0 B:48.18 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:58.16 / 0 @1:0
 < 16:24:48: Echo:N7125 M105
 < 16:24:49: ok 7126
 < 16:24:49: T:38.23 / 0 B:48.18 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:57.98 / 0 @1:0
 < 16:24:49: Echo:N7126 M105
 < 16:24:50: wait
 < 16:24:50: ok 7127
 < 16:24:50: T:37.81 / 0 B:48.12 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:57.80 / 0 @1:0
 < 16:24:50: Echo:N7127 M105
 < 16:24:51: wait
 < 16:24:51: ok 7128
 < 16:24:51: T:37.81 / 0 B:48.12 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:57.62 / 0 @1:0
 < 16:24:51: Echo:N7128 M105
 < 16:24:52: wait
 < 16:24:52: ok 7129
 < 16:24:52: T:37.81 / 0 B:48.12 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:57.44 / 0 @1:0
 < 16:24:52: Echo:N7129 M105
 < 16:24:53: wait
 < 16:24:53: ok 7130
 < 16:24:53: T:37.81 / 0 B:48.12 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:57.26 / 0 @1:0
 < 16:24:53: Echo:N7130 M105
 < 16:24:54: wait
 < 16:24:54: ok 7131
 < 16:24:54: T:37.81 / 0 B:48.05 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:57.08 / 0 @1:0
 < 16:24:54: Echo:N7131 M105
 < 16:24:55: wait
 < 16:24:56: ok 7132
 < 16:24:56: T:37.81 / 0 B:48.05 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:56.90 / 0 @1:0
 < 16:24:56: Echo:N7132 M105
 < 16:24:57: wait
 < 16:24:57: ok 7133
 < 16:24:57: T:37.81 / 0 B:48.05 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:56.72 / 0 @1:0
 < 16:24:57: Echo:N7133 M105
 < 16:24:58: wait
 < 16:24:58: ok 7134
 < 16:24:58: T:37.81 / 0 B:47.99 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:56.54 / 0 @1:0
 < 16:24:58: Echo:N7134 M105
 > 16:24:58: N7135 M104 T1 S230 *49
 < 16:24:58: ok 7135
 < 16:24:58: TargetExtr 1:230
 < 16:24:58: Echo:N7135 M104 T1  S230
 < 16:24:59: ok 7136
 < 16:24:59: T:37.40 / 0 B:47.47 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:56.18 /230 @1:255
 < 16:24:59: Echo:N7136 M105
 < 16:25:00: ok 7137
 < 16:25:00: T:37.81 / 0 B:47.60 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:56.18 /230 @1:255
 < 16:25:00: Echo:N7137 M105
 < 16:25:01: wait
 < 16:25:01: ok 7138
 < 16:25:01: T:37.81 / 0 B:47.66 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:56.18 /230 @1:255
 < 16:25:01: Echo:N7138 M105
 < 16:25:02: wait
 < 16:25:02: ok 7139
 < 16:25:02: T:37.81 / 0 B:47.73 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:56.36 /230 @1:255
 < 16:25:02: Echo:N7139 M105
 < 16:25:03: wait
 < 16:25:03: ok 7140
 < 16:25:03: T:37.81 / 0 B:47.73 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:56.72 /230 @1:255
 < 16:25:03: Echo:N7140 M105
 < 16:25:04: wait
 < 16:25:04: ok 7141
 < 16:25:04: T:37.40 / 0 B:47.79 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:57.62 /230 @1:255
 < 16:25:04: Echo:N7141 M105
 < 16:25:05: wait
 < 16:25:05: ok 7142
 < 16:25:05: T:37.40 / 0 B:47.79 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:58.89 /230 @1:255
 < 16:25:05: Echo:N7142 M105
 < 16:25:06: ok 7143
 < 16:25:06: T:37.81 / 0 B:47.86 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:60.69 /230 @1:255
 < 16:25:06: Echo:N7143 M105
 < 16:25:07: wait
 < 16:25:07: ok 7144
 < 16:25:07: T:37.81 / 0 B:47.86 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:62.85 /230 @1:255
 < 16:25:07: Echo:N7144 M105
 < 16:25:08: ok 7145
 < 16:25:08: T:37.40 / 0 B:47.86 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:65.38 /230 @1:255
 < 16:25:08: Echo:N7145 M105
 < 16:25:09: wait
 < 16:25:09: ok 7146
 < 16:25:09: T:37.81 / 0 B:47.86 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:67.77 /230 @1:255
 < 16:25:09: Echo:N7146 M105
 < 16:25:10: ok 7147
 < 16:25:10: T:37.40 / 0 B:47.79 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:69.87 /230 @1:255
 < 16:25:10: Echo:N7147 M105
 < 16:25:11: wait
 < 16:25:11: ok 7148
 < 16:25:11: T:37.40 / 0 B:47.79 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:72.30 /230 @1:255
 < 16:25:11: Echo:N7148 M105
 < 16:25:12: ok 7149
 < 16:25:12: T:37.40 / 0 B:47.79 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:75.06 /230 @1:255
 < 16:25:12: Echo:N7149 M105
 < 16:25:13: wait
 < 16:25:14: ok 7150
 < 16:25:14: T:37.40 / 0 B:47.73 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:78.48 /230 @1:255
 < 16:25:14: Echo:N7150 M105
 < 16:25:15: wait
 < 16:25:15: ok 7151
 < 16:25:15: T:36.98 / 0 B:47.73 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:81.91 /230 @1:255
 < 16:25:15: Echo:N7151 M105
 < 16:25:16: wait
 < 16:25:16: ok 7152
 < 16:25:16: T:37.40 / 0 B:47.73 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:85.76 /230 @1:255
 < 16:25:16: Echo:N7152 M105
 < 16:25:17: wait
 < 16:25:17: ok 7153
 < 16:25:17: T:36.98 / 0 B:47.66 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:88.91 /230 @1:255
 < 16:25:17: Echo:N7153 M105
 < 16:25:18: wait
 < 16:25:18: ok 7154
 < 16:25:18: T:36.98 / 0 B:47.66 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:92.73 /230 @1:255
 < 16:25:18: Echo:N7154 M105
 < 16:25:19: ok 7155
 < 16:25:19: T:36.98 / 0 B:47.66 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:95.96 /230 @1:255
 < 16:25:19: Echo:N7155 M105
 < 16:25:20: wait
 < 16:25:20: ok 7156
 < 16:25:20: T:37.40 / 0 B:47.66 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:99.88 /230 @1:255
 < 16:25:20: Echo:N7156 M105
 < 16:25:21: ok 7157
 < 16:25:21: T:36.98 / 0 B:47.66 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:103.00 /230 @1:255
 < 16:25:21: Echo:N7157 M105
 < 16:25:22: ok 7158
 < 16:25:22: T:36.98 / 0 B:47.66 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:106.61 /230 @1:255
 < 16:25:22: Echo:N7158 M105
 < 16:25:23: wait
 < 16:25:23: ok 7159
 < 16:25:23: T:36.98 / 0 B:47.60 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:110.14 /230 @1:255
 < 16:25:23: Echo:N7159 M105
 < 16:25:24: wait
 < 16:25:24: ok 7160
 < 16:25:24: T:36.98 / 0 B:47.60 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:113.19 /230 @1:255
 < 16:25:24: Echo:N7160 M105
 < 16:25:25: ok 7161
 < 16:25:25: T:36.98 / 0 B:47.60 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:116.40 /230 @1:255
 < 16:25:25: Echo:N7161 M105
 < 16:25:26: wait
 < 16:25:26: ok 7162
 < 16:25:26: T:36.98 / 0 B:47.53 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:119.66 /230 @1:255
 < 16:25:26: Echo:N7162 M105
 < 16:25:27: ok 7163
 < 16:25:27: T:36.98 / 0 B:47.53 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:122.99 /230 @1:255
 < 16:25:27: Echo:N7163 M105
 < 16:25:28: wait
 < 16:25:28: ok 7164
 < 16:25:28: T:36.98 / 0 B:47.53 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:126.31 /230 @1:255
 < 16:25:28: Echo:N7164 M105
 < 16:25:29: wait
 < 16:25:29: ok 7165
 < 16:25:29: T:36.98 / 0 B:47.53 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:129.67 /230 @1:255
 < 16:25:29: Echo:N7165 M105
 < 16:25:31: ok 7166
 < 16:25:31: T:36.56 / 0 B:47.47 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:132.66 /230 @1:255
 < 16:25:31: Echo:N7166 M105
 < 16:25:32: wait
 < 16:25:32: ok 7167
 < 16:25:32: T:36.98 / 0 B:47.47 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:135.40 /230 @1:255
 < 16:25:32: Echo:N7167 M105
 < 16:25:33: ok 7168
 < 16:25:33: T:36.98 / 0 B:47.47 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:138.21 /230 @1:255
 < 16:25:33: Echo:N7168 M105
 < 16:25:34: wait
 < 16:25:34: ok 7169
 < 16:25:34: T:36.56 / 0 B:47.47 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:140.99 /230 @1:255
 < 16:25:34: Echo:N7169 M105
 < 16:25:35: wait
 < 16:25:35: ok 7170
 < 16:25:35: T:36.56 / 0 B:47.40 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:144.04 /230 @1:255
 < 16:25:35: Echo:N7170 M105
 < 16:25:36: wait
 < 16:25:36: ok 7171
 < 16:25:36: T:36.56 / 0 B:47.40 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:146.97 /230 @1:255
 < 16:25:36: Echo:N7171 M105
 < 16:25:37: ok 7172
 < 16:25:37: T:36.56 / 0 B:47.40 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:149.86 /230 @1:255
 < 16:25:37: Echo:N7172 M105
 < 16:25:38: wait
 < 16:25:38: ok 7173
 < 16:25:38: T:36.56 / 0 B:47.40 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:152.75 /230 @1:255
 < 16:25:38: Echo:N7173 M105
 < 16:25:39: ok 7174
 < 16:25:39: T:36.56 / 0 B:47.40 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:155.60 /230 @1:255
 < 16:25:39: Echo:N7174 M105
 < 16:25:40: wait
 < 16:25:40: ok 7175
 < 16:25:40: T:36.56 / 0 B:47.34 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:158.73 /230 @1:255
 < 16:25:40: Echo:N7175 M105
 < 16:25:41: wait
 < 16:25:41: ok 7176
 < 16:25:41: T:36.56 / 0 B:47.34 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:161.53 /230 @1:255
 < 16:25:41: Echo:N7176 M105
 < 16:25:42: wait
 < 16:25:42: ok 7177
 < 16:25:42: T:36.56 / 0 B:47.34 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:164.59 /230 @1:255
 < 16:25:42: Echo:N7177 M105
 < 16:25:43: ok 7178
 < 16:25:43: T:36.56 / 0 B:47.27 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:167.28 /230 @1:255
 < 16:25:43: Echo:N7178 M105
 < 16:25:44: wait
 < 16:25:44: ok 7179
 < 16:25:44: T:36.56 / 0 B:47.27 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:170.52 /230 @1:255
 < 16:25:44: Echo:N7179 M105
 < 16:25:45: wait
 < 16:25:45: ok 7180
 < 16:25:45: T:36.56 / 0 B:47.27 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:173.40 /230 @1:255
 < 16:25:45: Echo:N7180 M105
 < 16:25:46: ok 7181
 < 16:25:46: T:36.56 / 0 B:47.27 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:176.18 /230 @1:255
 < 16:25:46: Echo:N7181 M105
 < 16:25:47: ok 7182
 < 16:25:47: T:36.56 / 0 B:47.27 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:178.88 /230 @1:255
 < 16:25:47: Echo:N7182 M105
 < 16:25:48: wait
 < 16:25:49: ok 7183
 < 16:25:49: T:36.56 / 0 B:47.21 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:181.45 /230 @1:255
 < 16:25:49: Echo:N7183 M105
 < 16:25:50: ok 7184
 < 16:25:50: T:36.56 / 0 B:47.21 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:183.90 /230 @1:255
 < 16:25:50: Echo:N7184 M105
 < 16:25:51: ok 7185
 < 16:25:51: T:36.56 / 0 B:47.21 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:186.08 /230 @1:255
 < 16:25:51: Echo:N7185 M105
 < 16:25:52: wait
 < 16:25:52: ok 7186
 < 16:25:52: T:36.56 / 0 B:47.21 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:188.83 /230 @1:255
 < 16:25:52: Echo:N7186 M105
 < 16:25:53: ok 7187
 < 16:25:53: T:36.56 / 0 B:47.14 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:191.10 /230 @1:255
 < 16:25:53: Echo:N7187 M105
 < 16:25:54: wait
 < 16:25:54: ok 7188
 < 16:25:54: T:36.56 / 0 B:47.14 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:193.75 /230 @1:255
 < 16:25:54: Echo:N7188 M105
 < 16:25:55: ok 7189
 < 16:25:55: T:36.14 / 0 B:47.14 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:195.88 /230 @1:255
 < 16:25:55: Echo:N7189 M105
 < 16:25:56: ok 7190
 < 16:25:56: T:36.56 / 0 B:47.14 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:198.19 /230 @1:255
 < 16:25:56: Echo:N7190 M105
 < 16:25:57: ok 7191
 < 16:25:57: T:36.14 / 0 B:47.14 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:200.97 /230 @1:255
 < 16:25:57: Echo:N7191 M105
 < 16:25:58: wait
 < 16:25:58: ok 7192
 < 16:25:58: T:36.56 / 0 B:47.08 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:203.76 /230 @1:255
 < 16:25:58: Echo:N7192 M105
 < 16:25:59: ok 7193
 < 16:25:59: T:36.14 / 0 B:47.08 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:206.36 /230 @1:255
 < 16:25:59: Echo:N7193 M105
 < 16:26:00: wait
 < 16:26:00: ok 7194
 < 16:26:00: T:36.14 / 0 B:47.08 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:208.84 /230 @1:255
 < 16:26:00: Echo:N7194 M105
 < 16:26:01: ok 7195
 < 16:26:01: T:36.14 / 0 B:47.08 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:211.26 /230 @1:230
 < 16:26:01: Echo:N7195 M105
 < 16:26:02: ok 7196
 < 16:26:02: T:36.14 / 0 B:47.01 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:213.34 /230 @1:230
 < 16:26:02: Echo:N7196 M105
 < 16:26:03: ok 7197
 < 16:26:03: T:36.14 / 0 B:47.01 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:215.59 /230 @1:230
 < 16:26:03: Echo:N7197 M105
 < 16:26:04: wait
 < 16:26:04: ok 7198
 < 16:26:04: T:36.14 / 0 B:47.01 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:217.90 /230 @1:230
 < 16:26:04: Echo:N7198 M105
 < 16:26:05: ok 7199
 < 16:26:05: T:36.14 / 0 B:47.01 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:219.72 /230 @1:230
 < 16:26:05: Echo:N7199 M105
 < 16:26:06: wait
 < 16:26:06: ok 7200
 < 16:26:06: T:36.14 / 0 B:46.95 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:221.96 /230 @1:230
 < 16:26:06: Echo:N7200 M105
 < 16:26:07: wait
 < 16:26:07: ok 7201
 < 16:26:07: T:37.81 / 0 B:47.27 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:224.08 /230 @1:0
 < 16:26:07: Echo:N7201 M105
 < 16:26:08: wait
 < 16:26:08: ok 7202
 < 16:26:08: T:36.14 / 0 B:47.40 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:226.34 /230 @1:0
 < 16:26:08: Echo:N7202 M105
 < 16:26:09: wait
 < 16:26:09: ok 7203
 < 16:26:09: T:36.14 / 0 B:47.27 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:228.05 /230 @1:0
 < 16:26:09: Echo:N7203 M105
 < 16:26:10: ok 7204
 < 16:26:10: T:36.56 / 0 B:47.21 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:229.77 /230 @1:0
 < 16:26:10: Echo:N7204 M105
 < 16:26:11: ok 7205
 < 16:26:11: T:36.14 / 0 B:47.08 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:231.26 /230 @1:0
 < 16:26:11: Echo:N7205 M105
 < 16:26:12: wait
 < 16:26:12: ok 7206
 < 16:26:12: T:36.14 / 0 B:47.01 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:232.75 /230 @1:0
 < 16:26:12: Echo:N7206 M105
 < 16:26:13: wait
 < 16:26:13: ok 7207
 < 16:26:13: T:36.14 / 0 B:47.01 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:233.89 /230 @1:0
 < 16:26:13: Echo:N7207 M105
 < 16:26:14: wait
 < 16:26:15: ok 7208
 < 16:26:15: T:36.14 / 0 B:46.95 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:234.77 /230 @1:0
 < 16:26:15: Echo:N7208 M105
 < 16:26:16: wait
 < 16:26:16: ok 7209
 < 16:26:16: T:36.14 / 0 B:46.88 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:235.30 /230 @1:0
 < 16:26:16: Echo:N7209 M105
 < 16:26:17: ok 7210
 < 16:26:17: T:36.14 / 0 B:46.88 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:235.56 /230 @1:0
 < 16:26:17: Echo:N7210 M105
 < 16:26:18: ok 7211
 < 16:26:18: T:36.14 / 0 B:46.82 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:235.65 /230 @1:0
 < 16:26:18: Echo:N7211 M105
 < 16:26:19: ok 7212
 < 16:26:19: T:36.14 / 0 B:46.82 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:235.47 /230 @1:0
 < 16:26:19: Echo:N7212 M105
 < 16:26:20: wait
 < 16:26:20: ok 7213
 < 16:26:20: T:36.14 / 0 B:46.82 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:235.12 /230 @1:0
 < 16:26:20: Echo:N7213 M105
 < 16:26:21: wait
 < 16:26:21: ok 7214
 < 16:26:21: T:36.14 / 0 B:46.75 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:234.60 /230 @1:0
 < 16:26:21: Echo:N7214 M105
 > 16:26:22: N7215 G1T1 E20.0000 F800.000 *85
 < 16:26:22: ok 7215
 < 16:26:22: Echo:N7215 G1 T1  E20.0000 F800.00
 < 16:26:22: ok 7216
 < 16:26:22: T:36.14 / 0 B:46.75 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:233.98 /230 @1:0
 < 16:26:22: Echo:N7216 M105
 < 16:26:23: ok 7217
 < 16:26:23: T:36.14 / 0 B:46.69 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:233.19 /230 @1:0
 < 16:26:23: Echo:N7217 M105
 < 16:26:24: wait
 < 16:26:24: ok 7218
 < 16:26:24: T:34.47 / 0 B:46.43 / 0 B@:0 @:0 T0:34.47 / 0 @0:0 T1:232.32 /230 @1:230
 < 16:26:24: Echo:N7218 M105
 < 16:26:25: ok 7219
 < 16:26:25: T:36.14 / 0 B:46.23 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:231.44 /230 @1:230
 < 16:26:25: Echo:N7219 M105
 < 16:26:26: wait
 < 16:26:26: ok 7220
 < 16:26:26: T:36.14 / 0 B:46.36 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:230.59 /230 @1:230
 < 16:26:26: Echo:N7220 M105
 < 16:26:27: ok 7221
 < 16:26:27: T:35.72 / 0 B:46.36 / 0 B@:0 @:0 T0:35.72 / 0 @0:0 T1:229.90 /230 @1:230
 < 16:26:27: Echo:N7221 M105
 < 16:26:28: wait
 < 16:26:28: ok 7222
 < 16:26:28: T:36.14 / 0 B:46.43 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:229.29 /230 @1:230
 < 16:26:28: Echo:N7222 M105
 < 16:26:29: wait
 < 16:26:29: ok 7223
 < 16:26:29: T:35.72 / 0 B:46.43 / 0 B@:0 @:0 T0:35.72 / 0 @0:0 T1:228.95 /230 @1:230
 < 16:26:29: Echo:N7223 M105
 < 16:26:30: wait
 < 16:26:30: ok 7224
 < 16:26:30: T:35.72 / 0 B:46.49 / 0 B@:0 @:0 T0:35.72 / 0 @0:0 T1:228.81 /230 @1:230
 < 16:26:30: Echo:N7224 M105
 < 16:26:31: ok 7225
 < 16:26:31: T:35.72 / 0 B:46.49 / 0 B@:0 @:0 T0:35.72 / 0 @0:0 T1:228.95 /230 @1:230
 < 16:26:31: Echo:N7225 M105
 < 16:26:32: wait
 < 16:26:33: ok 7226
 < 16:26:33: T:36.14 / 0 B:47.01 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:229.36 /230 @1:0
 < 16:26:33: Echo:N7226 M105
 < 16:26:34: wait
 < 16:26:34: ok 7227
 < 16:26:34: T:35.72 / 0 B:46.88 / 0 B@:0 @:0 T0:35.72 / 0 @0:0 T1:229.84 /230 @1:0
 < 16:26:34: Echo:N7227 M105
 < 16:26:35: wait
 < 16:26:35: ok 7228
 < 16:26:35: T:35.72 / 0 B:46.75 / 0 B@:0 @:0 T0:35.72 / 0 @0:0 T1:230.59 /230 @1:0
 < 16:26:35: Echo:N7228 M105
 < 16:26:36: ok 7229
 < 16:26:36: T:35.72 / 0 B:46.69 / 0 B@:0 @:0 T0:35.72 / 0 @0:0 T1:231.18 /230 @1:0
 < 16:26:36: Echo:N7229 M105
 > 16:26:37: N7230 T1 *45
 < 16:26:37: ok 7230
 < 16:26:37: SelectExtruder:1
 < 16:26:37: FlowMultiply:100
 < 16:26:37: Echo:N7230 T1
 < 16:26:37: ok 7231
 < 16:26:37: T:232.14 /230 B:46.62 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:232.14 /230 @1:0
 < 16:26:37: Echo:N7231 M105
 < 16:26:38: ok 7232
 < 16:26:38: T:232.49 /230 B:46.56 / 0 B@:0 @:0 T0:35.72 / 0 @0:0 T1:232.49 /230 @1:0
 < 16:26:38: Echo:N7232 M105
 < 16:26:39: wait
 < 16:26:39: ok 7233
 < 16:26:39: T:232.75 /230 B:46.56 / 0 B@:0 @:0 T0:35.72 / 0 @0:0 T1:232.75 /230 @1:0
 < 16:26:39: Echo:N7233 M105
 < 16:26:40: ok 7234
 < 16:26:40: T:232.84 /230 B:46.49 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:232.84 /230 @1:0
 < 16:26:40: Echo:N7234 M105
 < 16:26:41: wait
 < 16:26:41: ok 7235
 < 16:26:41: T:232.75 /230 B:46.49 / 0 B@:0 @:0 T0:35.72 / 0 @0:0 T1:232.75 /230 @1:0
 < 16:26:41: Echo:N7235 M105
 < 16:26:42: wait
 < 16:26:42: ok 7236
 < 16:26:42: T:232.40 /230 B:46.43 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:232.40 /230 @1:0
 < 16:26:42: Echo:N7236 M105
 < 16:26:43: wait
 < 16:26:43: ok 7237
 < 16:26:43: T:231.96 /230 B:46.43 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:231.96 /230 @1:0
 < 16:26:43: Echo:N7237 M105
 < 16:26:44: ok 7238
 < 16:26:44: T:231.35 /230 B:45.84 / 0 B@:0 @:230 T0:35.72 / 0 @0:0 T1:231.35 /230 @1:230
 < 16:26:44: Echo:N7238 M105
 < 16:26:45: wait
 < 16:26:45: ok 7239
 < 16:26:45: T:230.73 /230 B:45.97 / 0 B@:0 @:230 T0:36.14 / 0 @0:0 T1:230.73 /230 @1:230
 < 16:26:45: Echo:N7239 M105
 < 16:26:46: wait
 < 16:26:46: ok 7240
 < 16:26:46: T:230.04 /230 B:46.04 / 0 B@:0 @:230 T0:36.14 / 0 @0:0 T1:230.04 /230 @1:230
 < 16:26:46: Echo:N7240 M105
 < 16:26:47: ok 7241
 < 16:26:47: T:229.49 /230 B:46.10 / 0 B@:0 @:230 T0:36.14 / 0 @0:0 T1:229.49 /230 @1:230
 < 16:26:47: Echo:N7241 M105
 < 16:26:48: ok 7242
 < 16:26:48: T:229.15 /230 B:46.10 / 0 B@:0 @:230 T0:36.14 / 0 @0:0 T1:229.15 /230 @1:230
 < 16:26:48: Echo:N7242 M105
 < 16:26:49: ok 7243
 < 16:26:49: T:228.95 /230 B:46.17 / 0 B@:0 @:230 T0:36.14 / 0 @0:0 T1:228.95 /230 @1:230
 < 16:26:49: Echo:N7243 M105
 > 16:26:50: N7244 G0 E10.0000 F1000.000 *15
 < 16:26:50: ok 7244
 < 16:26:50: Echo:N7244 G0  E10.0000 F1000.00
 < 16:26:51: ok 7245
 < 16:26:51: T:229.01 /230 B:46.10 / 0 B@:0 @:230 T0:36.14 / 0 @0:0 T1:229.01 /230 @1:230
 < 16:26:51: Echo:N7245 M105
 < 16:26:52: ok 7246
 < 16:26:52: T:229.29 /230 B:46.43 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:229.29 /230 @1:0
 < 16:26:52: Echo:N7246 M105
 < 16:26:53: wait
 < 16:26:53: ok 7247
 < 16:26:53: T:229.70 /230 B:46.56 / 0 B@:0 @:0 T0:36.14 / 0 @0:0 T1:229.70 /230 @1:0
 < 16:26:53: Echo:N7247 M105
 < 16:26:54: ok 7248
 < 16:26:54: T:230.32 /230 B:46.49 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:230.32 /230 @1:0
 < 16:26:54: Echo:N7248 M105
 < 16:26:55: wait
 < 16:26:55: ok 7249
 < 16:26:55: T:231.00 /230 B:46.36 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:231.00 /230 @1:0
 < 16:26:55: Echo:N7249 M105
 < 16:26:56: wait
 < 16:26:56: ok 7250
 < 16:26:56: T:231.61 /230 B:46.30 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:231.61 /230 @1:0
 < 16:26:56: Echo:N7250 M105
 < 16:26:57: wait
 < 16:26:57: ok 7251
 < 16:26:57: T:232.23 /230 B:46.23 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:232.23 /230 @1:0
 < 16:26:57: Echo:N7251 M105
 < 16:26:58: wait
 < 16:26:58: ok 7252
 < 16:26:58: T:232.58 /230 B:46.23 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:232.58 /230 @1:0
 < 16:26:58: Echo:N7252 M105
 < 16:26:59: wait
 < 16:26:59: ok 7253
 < 16:26:59: T:232.75 /230 B:46.17 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:232.75 /230 @1:0
 < 16:26:59: Echo:N7253 M105
 < 16:27:00: wait
 < 16:27:00: ok 7254
 < 16:27:00: T:232.67 /230 B:46.23 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:232.67 /230 @1:0
 < 16:27:00: Echo:N7254 M105
 < 16:27:01: ok 7255
 < 16:27:01: T:232.49 /230 B:46.17 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:232.49 /230 @1:0
 < 16:27:01: Echo:N7255 M105
 < 16:27:02: ok 7256
 < 16:27:02: T:232.14 /230 B:46.10 / 0 B@:0 @:0 T0:36.56 / 0 @0:0 T1:232.14 /230 @1:0
 < 16:27:02: Echo:N7256 M105
 < 16:27:03: wait
 < 16:27:03: ok 7257
 < 16:27:03: T:231.61 /230 B:46.10 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:231.61 /230 @1:0
 < 16:27:03: Echo:N7257 M105
 > 16:27:04: N7258 G1 E10.0000 F800.000 *58
 < 16:27:04: ok 7258
 < 16:27:04: Echo:N7258 G1  E10.0000 F800.00
 < 16:27:04: ok 7259
 < 16:27:04: T:230.93 /230 B:45.58 / 0 B@:0 @:230 T0:36.56 / 0 @0:0 T1:230.93 /230 @1:230
 < 16:27:04: Echo:N7259 M105
 < 16:27:05: wait
 < 16:27:05: ok 7260
 < 16:27:05: T:230.38 /230 B:45.71 / 0 B@:0 @:230 T0:36.56 / 0 @0:0 T1:230.38 /230 @1:230
 < 16:27:05: Echo:N7260 M105
 < 16:27:06: wait
 < 16:27:06: ok 7261
 < 16:27:06: T:229.77 /230 B:45.78 / 0 B@:0 @:230 T0:36.56 / 0 @0:0 T1:229.77 /230 @1:230
 < 16:27:06: Echo:N7261 M105
 < 16:27:07: ok 7262
 < 16:27:07: T:229.36 /230 B:45.78 / 0 B@:0 @:230 T0:36.56 / 0 @0:0 T1:229.36 /230 @1:230
 < 16:27:07: Echo:N7262 M105
 < 16:27:08: ok 7263
 < 16:27:08: T:229.08 /230 B:45.84 / 0 B@:0 @:230 T0:36.56 / 0 @0:0 T1:229.08 /230 @1:230
 < 16:27:08: Echo:N7263 M105
 < 16:27:09: wait
 < 16:27:10: ok 7264
 < 16:27:10: T:228.95 /230 B:45.84 / 0 B@:0 @:230 T0:36.56 / 0 @0:0 T1:228.95 /230 @1:230
 < 16:27:10: Echo:N7264 M105
 < 16:27:11: wait
 < 16:27:11: ok 7265
 < 16:27:11: T:229.08 /230 B:45.84 / 0 B@:0 @:230 T0:36.56 / 0 @0:0 T1:229.08 /230 @1:230
 < 16:27:11: Echo:N7265 M105
 < 16:27:12: wait
 < 16:27:12: ok 7266
 < 16:27:12: T:229.49 /230 B:46.36 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:229.49 /230 @1:0
 < 16:27:12: Echo:N7266 M105
 < 16:27:13: ok 7267
 < 16:27:13: T:229.97 /230 B:46.23 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:229.97 /230 @1:0
 < 16:27:13: Echo:N7267 M105
 < 16:27:14: wait
 < 16:27:14: ok 7268
 < 16:27:14: T:230.66 /230 B:46.17 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:230.66 /230 @1:0
 < 16:27:14: Echo:N7268 M105
 < 16:27:15: ok 7269
 < 16:27:15: T:231.26 /230 B:46.10 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:231.26 /230 @1:0
 < 16:27:15: Echo:N7269 M105
 < 16:27:16: wait
 < 16:27:16: ok 7270
 < 16:27:16: T:231.88 /230 B:46.04 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:231.88 /230 @1:0
 < 16:27:16: Echo:N7270 M105
 < 16:27:17: wait
 < 16:27:17: ok 7271
 < 16:27:17: T:232.32 /230 B:45.97 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:232.32 /230 @1:0
 < 16:27:17: Echo:N7271 M105
 < 16:27:18: wait
 < 16:27:18: ok 7272
 < 16:27:18: T:232.58 /230 B:45.91 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:232.58 /230 @1:0
 < 16:27:18: Echo:N7272 M105
 < 16:27:19: wait
 < 16:27:19: ok 7273
 < 16:27:19: T:232.58 /230 B:45.91 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:232.58 /230 @1:0
 < 16:27:19: Echo:N7273 M105
 < 16:27:20: ok 7274
 < 16:27:20: T:232.40 /230 B:45.84 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:232.40 /230 @1:0
 < 16:27:20: Echo:N7274 M105
 < 16:27:21: wait
 < 16:27:21: ok 7275
 < 16:27:21: T:232.14 /230 B:45.84 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:232.14 /230 @1:0
 < 16:27:21: Echo:N7275 M105
 < 16:27:22: wait
 < 16:27:22: ok 7276
 < 16:27:22: T:231.70 /230 B:45.78 / 0 B@:0 @:0 T0:36.98 / 0 @0:0 T1:231.70 /230 @1:0
 < 16:27:22: Echo:N7276 M105
 < 16:27:23: wait
 < 16:27:23: ok 7277
 < 16:27:23: T:231.00 /230 B:45.32 / 0 B@:0 @:230 T0:36.98 / 0 @0:0 T1:231.00 /230 @1:230
 < 16:27:23: Echo:N7277 M105
 < 16:27:24: wait
 < 16:27:24: ok 7278
 < 16:27:24: T:230.45 /230 B:45.39 / 0 B@:0 @:230 T0:36.98 / 0 @0:0 T1:230.45 /230 @1:230
 < 16:27:24: Echo:N7278 M105
 < 16:27:25: wait
 < 16:27:25: ok 7279
 < 16:27:25: T:229.97 /230 B:45.45 / 0 B@:0 @:230 T0:37.40 / 0 @0:0 T1:229.97 /230 @1:230
 < 16:27:25: Echo:N7279 M105
 < 16:27:26: wait
 < 16:27:27: ok 7280
 < 16:27:27: T:229.49 /230 B:45.52 / 0 B@:0 @:230 T0:37.40 / 0 @0:0 T1:229.49 /230 @1:230
 < 16:27:27: Echo:N7280 M105
 < 16:27:28: wait
 < 16:27:28: ok 7281
 < 16:27:28: T:229.22 /230 B:45.58 / 0 B@:0 @:230 T0:37.40 / 0 @0:0 T1:229.22 /230 @1:230
 < 16:27:28: Echo:N7281 M105
 < 16:27:29: wait
 < 16:27:29: ok 7282
 < 16:27:29: T:229.15 /230 B:45.58 / 0 B@:0 @:230 T0:37.40 / 0 @0:0 T1:229.15 /230 @1:230
 < 16:27:29: Echo:N7282 M105
 < 16:27:30: wait
 < 16:27:30: ok 7283
 < 16:27:30: T:229.36 /230 B:45.58 / 0 B@:0 @:230 T0:37.40 / 0 @0:0 T1:229.36 /230 @1:230
 < 16:27:30: Echo:N7283 M105
 < 16:27:31: wait
 < 16:27:31: ok 7284
 < 16:27:31: T:229.84 /230 B:46.04 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:229.84 /230 @1:0
 < 16:27:31: Echo:N7284 M105
 < 16:27:32: ok 7285
 < 16:27:32: T:230.32 /230 B:45.91 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:230.32 /230 @1:0
 < 16:27:32: Echo:N7285 M105
 > 16:27:32: N7286 M83 *3
 < 16:27:32: ok 7286
 < 16:27:32: Echo:N7286 M83
 < 16:27:33: ok 7287
 < 16:27:33: T:230.93 /230 B:45.84 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:230.93 /230 @1:0
 < 16:27:33: Echo:N7287 M105
 < 16:27:34: wait
 < 16:27:34: ok 7288
 < 16:27:34: T:231.61 /230 B:45.78 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:231.61 /230 @1:0
 < 16:27:34: Echo:N7288 M105
 < 16:27:35: wait
 < 16:27:35: ok 7289
 < 16:27:35: T:232.23 /230 B:45.71 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:232.23 /230 @1:0
 < 16:27:35: Echo:N7289 M105
 < 16:27:36: ok 7290
 < 16:27:36: T:232.58 /230 B:45.65 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:232.58 /230 @1:0
 < 16:27:36: Echo:N7290 M105
 < 16:27:37: wait
 < 16:27:37: ok 7291
 < 16:27:37: T:232.84 /230 B:45.65 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:232.84 /230 @1:0
 < 16:27:37: Echo:N7291 M105
 < 16:27:38: wait
 < 16:27:38: ok 7292
 < 16:27:38: T:232.84 /230 B:45.58 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:232.84 /230 @1:0
 < 16:27:38: Echo:N7292 M105
 < 16:27:39: wait
 < 16:27:39: ok 7293
 < 16:27:39: T:232.67 /230 B:45.58 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:232.67 /230 @1:0
 < 16:27:39: Echo:N7293 M105
 > 16:27:40: N7294 G1 E10.0000 F800.000 *58
 < 16:27:40: ok 7294
 < 16:27:40: Echo:N7294 G1  E10.0000 F800.00
 < 16:27:41: ok 7295
 < 16:27:41: T:232.32 /230 B:45.52 / 0 B@:0 @:0 T0:37.40 / 0 @0:0 T1:232.32 /230 @1:0
 < 16:27:41: Echo:N7295 M105
 < 16:27:42: ok 7296
 < 16:27:42: T:231.88 /230 B:45.52 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:231.88 /230 @1:0
 < 16:27:42: Echo:N7296 M105
 < 16:27:43: ok 7297
 < 16:27:43: T:231.26 /230 B:45.00 / 0 B@:0 @:230 T0:37.40 / 0 @0:0 T1:231.26 /230 @1:230
 < 16:27:43: Echo:N7297 M105
 < 16:27:44: wait
 < 16:27:44: ok 7298
 < 16:27:44: T:230.59 /230 B:45.13 / 0 B@:0 @:230 T0:37.81 / 0 @0:0 T1:230.59 /230 @1:230
 < 16:27:44: Echo:N7298 M105
 < 16:27:45: wait
 < 16:27:45: ok 7299
 < 16:27:45: T:229.97 /230 B:45.19 / 0 B@:0 @:230 T0:37.81 / 0 @0:0 T1:229.97 /230 @1:230
 < 16:27:45: Echo:N7299 M105
 < 16:27:46: wait
 < 16:27:46: ok 7300
 < 16:27:46: T:229.49 /230 B:45.26 / 0 B@:0 @:230 T0:37.81 / 0 @0:0 T1:229.49 /230 @1:230
 < 16:27:46: Echo:N7300 M105
 < 16:27:47: wait
 < 16:27:47: ok 7301
 < 16:27:47: T:229.15 /230 B:45.26 / 0 B@:0 @:230 T0:37.81 / 0 @0:0 T1:229.15 /230 @1:230
 < 16:27:47: Echo:N7301 M105
 < 16:27:48: ok 7302
 < 16:27:48: T:229.01 /230 B:45.26 / 0 B@:0 @:230 T0:37.81 / 0 @0:0 T1:229.01 /230 @1:230
 < 16:27:48: Echo:N7302 M105
 < 16:27:49: wait
 < 16:27:49: ok 7303
 < 16:27:49: T:229.08 /230 B:45.32 / 0 B@:0 @:230 T0:37.81 / 0 @0:0 T1:229.08 /230 @1:230
 < 16:27:49: Echo:N7303 M105
 < 16:27:50: wait
 < 16:27:50: ok 7304
 < 16:27:50: T:229.42 /230 B:45.84 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:229.42 /230 @1:0
 < 16:27:50: Echo:N7304 M105
 < 16:27:51: wait
 < 16:27:51: ok 7305
 < 16:27:51: T:229.97 /230 B:45.71 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:229.97 /230 @1:0
 < 16:27:51: Echo:N7305 M105
 < 16:27:52: ok 7306
 < 16:27:52: T:230.59 /230 B:45.65 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:230.59 /230 @1:0
 < 16:27:52: Echo:N7306 M105
 < 16:27:53: wait
 < 16:27:53: ok 7307
 < 16:27:53: T:231.26 /230 B:45.58 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:231.26 /230 @1:0
 < 16:27:53: Echo:N7307 M105
 < 16:27:54: wait
 < 16:27:54: ok 7308
 < 16:27:54: T:232.05 /230 B:45.52 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:232.05 /230 @1:0
 < 16:27:54: Echo:N7308 M105
 > 16:27:55: N7309 G1 E20.0000 F800.000 *60
 < 16:27:55: ok 7309
 < 16:27:55: Echo:N7309 G1  E20.0000 F800.00
 < 16:27:55: ok 7310
 < 16:27:55: T:232.58 /230 B:45.45 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:232.58 /230 @1:0
 < 16:27:55: Echo:N7310 M105
 < 16:27:56: wait
 < 16:27:56: ok 7311
 < 16:27:56: T:232.93 /230 B:45.39 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:232.93 /230 @1:0
 < 16:27:56: Echo:N7311 M105
 < 16:27:57: wait
 < 16:27:57: ok 7312
 < 16:27:57: T:233.11 /230 B:45.32 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:233.11 /230 @1:0
 < 16:27:57: Echo:N7312 M105
 < 16:27:58: ok 7313
 < 16:27:58: T:232.84 /230 B:45.32 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:232.84 /230 @1:0
 < 16:27:58: Echo:N7313 M105
 < 16:28:00: wait
 < 16:28:00: ok 7314
 < 16:28:00: T:232.23 /230 B:45.32 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:232.23 /230 @1:0
 < 16:28:00: Echo:N7314 M105
 < 16:28:01: wait
 < 16:28:01: ok 7315
 < 16:28:01: T:231.53 /230 B:44.74 / 0 B@:0 @:230 T0:37.81 / 0 @0:0 T1:231.53 /230 @1:230
 < 16:28:01: Echo:N7315 M105
 < 16:28:02: ok 7316
 < 16:28:02: T:230.79 /230 B:44.81 / 0 B@:0 @:230 T0:38.23 / 0 @0:0 T1:230.79 /230 @1:230
 < 16:28:02: Echo:N7316 M105
 < 16:28:03: wait
 < 16:28:03: ok 7317
 < 16:28:03: T:230.18 /230 B:44.94 / 0 B@:0 @:230 T0:38.23 / 0 @0:0 T1:230.18 /230 @1:230
 < 16:28:03: Echo:N7317 M105
 < 16:28:04: wait
 < 16:28:04: ok 7318
 < 16:28:04: T:229.63 /230 B:45.00 / 0 B@:0 @:230 T0:38.23 / 0 @0:0 T1:229.63 /230 @1:230
 < 16:28:04: Echo:N7318 M105
 < 16:28:05: ok 7319
 < 16:28:05: T:229.29 /230 B:45.06 / 0 B@:0 @:230 T0:38.23 / 0 @0:0 T1:229.29 /230 @1:230
 < 16:28:05: Echo:N7319 M105
 < 16:28:06: ok 7320
 < 16:28:06: T:229.15 /230 B:45.06 / 0 B@:0 @:230 T0:38.23 / 0 @0:0 T1:229.15 /230 @1:230
 < 16:28:06: Echo:N7320 M105
 < 16:28:07: wait
 < 16:28:07: ok 7321
 < 16:28:07: T:229.22 /230 B:45.13 / 0 B@:0 @:230 T0:38.65 / 0 @0:0 T1:229.22 /230 @1:230
 < 16:28:07: Echo:N7321 M105
 < 16:28:08: wait
 < 16:28:08: ok 7322
 < 16:28:08: T:229.56 /230 B:45.58 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:229.56 /230 @1:0
 < 16:28:08: Echo:N7322 M105
 < 16:28:09: ok 7323
 < 16:28:09: T:230.04 /230 B:45.45 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:230.04 /230 @1:0
 < 16:28:09: Echo:N7323 M105
 > 16:28:10: N7324 T0 *40
 < 16:28:10: ok 7324
 < 16:28:10: SelectExtruder:0
 < 16:28:10: FlowMultiply:100
 < 16:28:10: Echo:N7324 T0
 < 16:28:10: ok 7325
 < 16:28:10: T:38.65 / 0 B:45.39 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:230.59 /230 @1:0
 < 16:28:10: Echo:N7325 M105
 < 16:28:11: wait
 < 16:28:12: ok 7326
 < 16:28:12: T:38.23 / 0 B:45.26 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:231.35 /230 @1:0
 < 16:28:12: Echo:N7326 M105
 < 16:28:13: wait
 < 16:28:13: ok 7327
 < 16:28:13: T:38.23 / 0 B:45.26 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:231.96 /230 @1:0
 < 16:28:13: Echo:N7327 M105
 < 16:28:14: wait
 < 16:28:14: ok 7328
 < 16:28:14: T:38.23 / 0 B:45.19 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:232.49 /230 @1:0
 < 16:28:14: Echo:N7328 M105
 < 16:28:15: wait
 < 16:28:15: ok 7329
 < 16:28:15: T:38.23 / 0 B:45.13 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:232.75 /230 @1:0
 < 16:28:15: Echo:N7329 M105
 < 16:28:16: wait
 < 16:28:16: ok 7330
 < 16:28:16: T:38.23 / 0 B:45.13 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:232.75 /230 @1:0
 < 16:28:16: Echo:N7330 M105
 < 16:28:17: wait
 < 16:28:17: ok 7331
 < 16:28:17: T:38.23 / 0 B:45.06 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:232.67 /230 @1:0
 < 16:28:17: Echo:N7331 M105
 < 16:28:18: ok 7332
 < 16:28:18: T:38.23 / 0 B:45.06 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:232.40 /230 @1:0
 < 16:28:18: Echo:N7332 M105
 < 16:28:19: wait
 < 16:28:19: ok 7333
 < 16:28:19: T:38.23 / 0 B:45.00 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:231.88 /230 @1:0
 < 16:28:19: Echo:N7333 M105
 < 16:28:20: ok 7334
 < 16:28:20: T:37.81 / 0 B:44.48 / 0 B@:0 @:0 T0:37.81 / 0 @0:0 T1:231.35 /230 @1:230
 < 16:28:20: Echo:N7334 M105
 < 16:28:21: ok 7335
 < 16:28:21: T:38.23 / 0 B:44.61 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:230.73 /230 @1:230
 < 16:28:21: Echo:N7335 M105
 < 16:28:22: wait
 < 16:28:22: ok 7336
 < 16:28:22: T:38.23 / 0 B:44.68 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:230.11 /230 @1:230
 < 16:28:22: Echo:N7336 M105
 < 16:28:23: wait
 < 16:28:23: ok 7337
 < 16:28:23: T:38.23 / 0 B:44.74 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:229.63 /230 @1:230
 < 16:28:23: Echo:N7337 M105
 < 16:28:24: ok 7338
 < 16:28:24: T:38.23 / 0 B:44.74 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:229.22 /230 @1:230
 < 16:28:24: Echo:N7338 M105
 < 16:28:25: wait
 < 16:28:25: ok 7339
 < 16:28:25: T:38.23 / 0 B:44.81 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:229.08 /230 @1:230
 < 16:28:25: Echo:N7339 M105
 < 16:28:26: wait
 < 16:28:27: ok 7340
 < 16:28:27: T:38.65 / 0 B:44.87 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:229.15 /230 @1:230
 < 16:28:27: Echo:N7340 M105
 < 16:28:28: wait
 < 16:28:28: ok 7341
 < 16:28:28: T:39.91 / 0 B:45.39 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:229.42 /230 @1:0
 < 16:28:28: Echo:N7341 M105
 < 16:28:29: ok 7342
 < 16:28:29: T:38.65 / 0 B:45.26 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:229.90 /230 @1:0
 < 16:28:29: Echo:N7342 M105
 < 16:28:30: wait
 < 16:28:30: ok 7343
 < 16:28:30: T:38.23 / 0 B:45.13 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:230.52 /230 @1:0
 < 16:28:30: Echo:N7343 M105
 > 16:28:30: N7344 G1T1 E50.0000 F800.000 *87
 < 16:28:30: ok 7344
 < 16:28:30: Echo:N7344 G1 T1  E50.0000 F800.00
 < 16:28:31: ok 7345
 < 16:28:31: T:38.23 / 0 B:45.13 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:231.18 /230 @1:0
 < 16:28:31: Echo:N7345 M105
 < 16:28:32: ok 7346
 < 16:28:32: T:38.23 / 0 B:45.06 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:231.88 /230 @1:0
 < 16:28:32: Echo:N7346 M105
 < 16:28:33: wait
 < 16:28:33: ok 7347
 < 16:28:33: T:38.23 / 0 B:45.00 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:232.49 /230 @1:0
 < 16:28:33: Echo:N7347 M105
 < 16:28:34: wait
 < 16:28:34: ok 7348
 < 16:28:34: T:38.23 / 0 B:44.87 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:232.84 /230 @1:0
 < 16:28:34: Echo:N7348 M105
 < 16:28:35: ok 7349
 < 16:28:35: T:38.23 / 0 B:44.87 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:233.02 /230 @1:0
 < 16:28:35: Echo:N7349 M105
 < 16:28:36: ok 7350
 < 16:28:36: T:38.23 / 0 B:44.87 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:233.02 /230 @1:0
 < 16:28:36: Echo:N7350 M105
 > 16:28:37: N7351 T1 *43
 < 16:28:37: ok 7351
 < 16:28:37: SelectExtruder:1
 < 16:28:37: FlowMultiply:100
 < 16:28:37: Echo:N7351 T1
 < 16:28:37: ok 7352
 < 16:28:37: T:232.75 /230 B:44.81 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:232.75 /230 @1:0
 < 16:28:37: Echo:N7352 M105
 < 16:28:38: ok 7353
 < 16:28:38: T:232.49 /230 B:44.81 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:232.49 /230 @1:0
 < 16:28:38: Echo:N7353 M105
 < 16:28:39: wait
 < 16:28:39: ok 7354
 < 16:28:39: T:231.88 /230 B:44.81 / 0 B@:0 @:0 T0:38.23 / 0 @0:0 T1:231.88 /230 @1:0
 < 16:28:39: Echo:N7354 M105
 < 16:28:40: wait
 < 16:28:40: ok 7355
 < 16:28:40: T:231.26 /230 B:44.22 / 0 B@:0 @:230 T0:38.23 / 0 @0:0 T1:231.26 /230 @1:230
 < 16:28:40: Echo:N7355 M105
 < 16:28:41: ok 7356
 < 16:28:41: T:230.59 /230 B:44.35 / 0 B@:0 @:230 T0:38.23 / 0 @0:0 T1:230.59 /230 @1:230
 < 16:28:41: Echo:N7356 M105
 < 16:28:42: wait
 < 16:28:42: ok 7357
 < 16:28:42: T:229.97 /230 B:44.42 / 0 B@:0 @:230 T0:38.23 / 0 @0:0 T1:229.97 /230 @1:230
 < 16:28:42: Echo:N7357 M105
 < 16:28:43: wait
 > 16:28:43: N7358 G1 E20.0000 F800.000 *56
 < 16:28:43: ok 7358
 < 16:28:43: Echo:N7358 G1  E20.0000 F800.00
 < 16:28:43: ok 7359
 < 16:28:43: T:229.36 /230 B:44.48 / 0 B@:0 @:230 T0:38.23 / 0 @0:0 T1:229.36 /230 @1:230
 < 16:28:43: Echo:N7359 M105
 < 16:28:44: wait
 < 16:28:44: ok 7360
 < 16:28:44: T:228.95 /230 B:44.48 / 0 B@:0 @:230 T0:38.23 / 0 @0:0 T1:228.95 /230 @1:230
 < 16:28:44: Echo:N7360 M105
 < 16:28:45: wait
 < 16:28:45: ok 7361
 < 16:28:45: T:228.74 /230 B:44.55 / 0 B@:0 @:230 T0:38.65 / 0 @0:0 T1:228.74 /230 @1:230
 < 16:28:45: Echo:N7361 M105
 < 16:28:46: ok 7362
 < 16:28:46: T:228.60 /230 B:44.55 / 0 B@:0 @:230 T0:38.65 / 0 @0:0 T1:228.60 /230 @1:230
 < 16:28:46: Echo:N7362 M105
 < 16:28:47: wait
 < 16:28:48: ok 7363
 < 16:28:48: T:228.67 /230 B:44.55 / 0 B@:0 @:230 T0:38.65 / 0 @0:0 T1:228.67 /230 @1:230
 < 16:28:48: Echo:N7363 M105
 < 16:28:49: wait
 < 16:28:49: ok 7364
 < 16:28:49: T:228.95 /230 B:44.55 / 0 B@:0 @:230 T0:38.65 / 0 @0:0 T1:228.95 /230 @1:230
 < 16:28:49: Echo:N7364 M105
 < 16:28:50: ok 7365
 < 16:28:50: T:229.36 /230 B:45.06 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:229.36 /230 @1:0
 < 16:28:50: Echo:N7365 M105
 < 16:28:51: wait
 < 16:28:51: ok 7366
 < 16:28:51: T:230.11 /230 B:44.87 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:230.11 /230 @1:0
 < 16:28:51: Echo:N7366 M105
 < 16:28:52: wait
 < 16:28:52: ok 7367
 < 16:28:52: T:230.79 /230 B:44.81 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:230.79 /230 @1:0
 < 16:28:52: Echo:N7367 M105
 < 16:28:53: wait
 < 16:28:53: ok 7368
 < 16:28:53: T:231.61 /230 B:44.74 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:231.61 /230 @1:0
 < 16:28:53: Echo:N7368 M105
 < 16:28:54: wait
 < 16:28:54: ok 7369
 < 16:28:54: T:232.40 /230 B:44.68 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:232.40 /230 @1:0
 < 16:28:54: Echo:N7369 M105
 < 16:28:55: wait
 < 16:28:55: ok 7370
 < 16:28:55: T:233.02 /230 B:44.61 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:233.02 /230 @1:0
 < 16:28:55: Echo:N7370 M105
 < 16:28:56: ok 7371
 < 16:28:56: T:233.28 /230 B:44.61 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:233.28 /230 @1:0
 < 16:28:56: Echo:N7371 M105
 < 16:28:57: wait
 < 16:28:57: ok 7372
 < 16:28:57: T:233.37 /230 B:44.55 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:233.37 /230 @1:0
 < 16:28:57: Echo:N7372 M105
 > 16:28:58: N7373 G1 E400.0000 F20.000 *61
 < 16:28:58: ok 7373
 < 16:28:58: Echo:N7373 G1  E400.0000 F20.00
 < 16:28:58: ok 7374
 < 16:28:58: T:233.37 /230 B:44.55 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:233.37 /230 @1:0
 < 16:28:58: Echo:N7374 M105
 < 16:28:59: wait
 < 16:28:59: ok 7375
 < 16:28:59: T:233.11 /230 B:44.55 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:233.11 /230 @1:0
 < 16:28:59: Echo:N7375 M105
 < 16:29:00: wait
 < 16:29:00: ok 7376
 < 16:29:00: T:232.67 /230 B:44.48 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:232.67 /230 @1:0
 < 16:29:00: Echo:N7376 M105
 < 16:29:01: wait
 < 16:29:01: ok 7377
 < 16:29:01: T:232.23 /230 B:44.48 / 0 B@:0 @:0 T0:38.65 / 0 @0:0 T1:232.23 /230 @1:0
 < 16:29:01: Echo:N7377 M105
 < 16:29:02: wait
 < 16:29:02: ok 7378
 < 16:29:02: T:231.44 /230 B:43.96 / 0 B@:0 @:230 T0:39.07 / 0 @0:0 T1:231.44 /230 @1:230
 < 16:29:02: Echo:N7378 M105
 < 16:29:03: ok 7379
 < 16:29:03: T:230.79 /230 B:44.03 / 0 B@:0 @:230 T0:39.07 / 0 @0:0 T1:230.79 /230 @1:230
 < 16:29:03: Echo:N7379 M105
 < 16:29:04: wait
 < 16:29:05: ok 7380
 < 16:29:05: T:230.18 /230 B:44.09 / 0 B@:0 @:230 T0:39.07 / 0 @0:0 T1:230.18 /230 @1:230
 < 16:29:05: Echo:N7380 M105
 > 16:29:06: N7381 G1 E20.0000 F800.000 *60
 < 16:29:06: ok 7381
 < 16:29:06: Echo:N7381 G1  E20.0000 F800.00
 < 16:29:06: ok 7382
 < 16:29:06: T:229.63 /230 B:44.16 / 0 B@:0 @:230 T0:39.07 / 0 @0:0 T1:229.63 /230 @1:230
 < 16:29:06: Echo:N7382 M105
 < 16:29:07: wait
 < 16:29:07: ok 7383
 < 16:29:07: T:229.22 /230 B:44.22 / 0 B@:0 @:230 T0:39.07 / 0 @0:0 T1:229.22 /230 @1:230
 < 16:29:07: Echo:N7383 M105
 < 16:29:08: wait
 < 16:29:08: ok 7384
 < 16:29:08: T:229.01 /230 B:44.22 / 0 B@:0 @:230 T0:39.07 / 0 @0:0 T1:229.01 /230 @1:230
 < 16:29:08: Echo:N7384 M105
 < 16:29:09: ok 7385
 < 16:29:09: T:228.95 /230 B:44.22 / 0 B@:0 @:230 T0:39.07 / 0 @0:0 T1:228.95 /230 @1:230
 < 16:29:09: Echo:N7385 M105
 < 16:29:10: ok 7386
 < 16:29:10: T:229.08 /230 B:44.29 / 0 B@:0 @:230 T0:39.07 / 0 @0:0 T1:229.08 /230 @1:230
 < 16:29:10: Echo:N7386 M105
 < 16:29:11: wait
 < 16:29:11: ok 7387
 < 16:29:11: T:229.42 /230 B:44.81 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:229.42 /230 @1:0
 < 16:29:11: Echo:N7387 M105
 < 16:29:12: wait
 < 16:29:12: ok 7388
 < 16:29:12: T:229.90 /230 B:44.68 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:229.90 /230 @1:0
 < 16:29:12: Echo:N7388 M105
 < 16:29:13: ok 7389
 < 16:29:13: T:230.52 /230 B:44.61 / 0 B@:0 @:0 T0:39.07 / 0 @0:0 T1:230.52 /230 @1:0
 < 16:29:13: Echo:N7389 M105
 < 16:29:14: wait
 < 16:29:14: ok 7390
 < 16:29:14: T:231.26 /230 B:44.55 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:231.26 /230 @1:0
 < 16:29:14: Echo:N7390 M105
 > 16:29:15: N7391 G1 E20.0000 F400.000 *49
 < 16:29:15: ok 7391
 < 16:29:15: Echo:N7391 G1  E20.0000 F400.00
 < 16:29:15: ok 7392
 < 16:29:15: T:232.05 /230 B:44.42 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:232.05 /230 @1:0
 < 16:29:15: Echo:N7392 M105
 < 16:29:16: wait
 < 16:29:16: ok 7393
 < 16:29:16: T:232.67 /230 B:44.42 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:232.67 /230 @1:0
 < 16:29:16: Echo:N7393 M105
 < 16:29:17: wait
 < 16:29:17: ok 7394
 < 16:29:17: T:233.02 /230 B:44.35 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:233.02 /230 @1:0
 < 16:29:17: Echo:N7394 M105
 < 16:29:18: ok 7395
 < 16:29:18: T:233.11 /230 B:44.35 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:233.11 /230 @1:0
 < 16:29:18: Echo:N7395 M105
 < 16:29:19: wait
 < 16:29:19: ok 7396
 < 16:29:19: T:232.93 /230 B:44.35 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:232.93 /230 @1:0
 < 16:29:19: Echo:N7396 M105
 < 16:29:20: wait
 < 16:29:21: ok 7397
 < 16:29:21: T:232.40 /230 B:44.29 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:232.40 /230 @1:0
 < 16:29:21: Echo:N7397 M105
 < 16:29:22: wait
 < 16:29:22: ok 7398
 < 16:29:22: T:231.88 /230 B:44.29 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:231.88 /230 @1:0
 < 16:29:22: Echo:N7398 M105
 < 16:29:23: wait
 < 16:29:23: ok 7399
 < 16:29:23: T:231.09 /230 B:43.77 / 0 B@:0 @:230 T0:39.49 / 0 @0:0 T1:231.09 /230 @1:230
 < 16:29:23: Echo:N7399 M105
 < 16:29:24: wait
 < 16:29:24: ok 7400
 < 16:29:24: T:230.45 /230 B:43.90 / 0 B@:0 @:230 T0:39.49 / 0 @0:0 T1:230.45 /230 @1:230
 < 16:29:24: Echo:N7400 M105
 < 16:29:25: wait
 < 16:29:25: ok 7401
 < 16:29:25: T:229.77 /230 B:43.90 / 0 B@:0 @:230 T0:39.49 / 0 @0:0 T1:229.77 /230 @1:230
 < 16:29:25: Echo:N7401 M105
 < 16:29:26: wait
 < 16:29:26: ok 7402
 < 16:29:26: T:229.36 /230 B:43.96 / 0 B@:0 @:230 T0:39.49 / 0 @0:0 T1:229.36 /230 @1:230
 < 16:29:26: Echo:N7402 M105
 < 16:29:27: wait
 < 16:29:27: ok 7403
 < 16:29:27: T:229.15 /230 B:44.03 / 0 B@:0 @:230 T0:39.91 / 0 @0:0 T1:229.15 /230 @1:230
 < 16:29:27: Echo:N7403 M105
 < 16:29:28: ok 7404
 < 16:29:28: T:229.15 /230 B:44.03 / 0 B@:0 @:230 T0:39.49 / 0 @0:0 T1:229.15 /230 @1:230
 < 16:29:28: Echo:N7404 M105
 < 16:29:29: wait
 < 16:29:29: ok 7405
 < 16:29:29: T:229.36 /230 B:44.35 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:229.36 /230 @1:0
 < 16:29:29: Echo:N7405 M105
 > 16:29:30: N7406 G1 E20.0000 F400.000 *56
 < 16:29:30: ok 7406
 < 16:29:30: Echo:N7406 G1  E20.0000 F400.00
 < 16:29:30: ok 7407
 < 16:29:30: T:229.84 /230 B:44.48 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:229.84 /230 @1:0
 < 16:29:30: Echo:N7407 M105
 < 16:29:31: wait
 < 16:29:31: ok 7408
 < 16:29:31: T:230.45 /230 B:44.35 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:230.45 /230 @1:0
 < 16:29:31: Echo:N7408 M105
 < 16:29:32: ok 7409
 < 16:29:32: T:231.09 /230 B:44.29 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:231.09 /230 @1:0
 < 16:29:32: Echo:N7409 M105
 < 16:29:33: ok 7410
 < 16:29:33: T:231.61 /230 B:44.22 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:231.61 /230 @1:0
 < 16:29:33: Echo:N7410 M105
 < 16:29:34: ok 7411
 < 16:29:34: T:232.05 /230 B:44.22 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:232.05 /230 @1:0
 < 16:29:34: Echo:N7411 M105
 < 16:29:35: ok 7412
 < 16:29:35: T:232.23 /230 B:44.16 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:232.23 /230 @1:0
 < 16:29:35: Echo:N7412 M105
 < 16:29:36: wait
 < 16:29:36: ok 7413
 < 16:29:36: T:232.14 /230 B:44.09 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:232.14 /230 @1:0
 < 16:29:36: Echo:N7413 M105
 < 16:29:37: wait
 < 16:29:37: ok 7414
 < 16:29:37: T:231.88 /230 B:44.09 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:231.88 /230 @1:0
 < 16:29:37: Echo:N7414 M105
 < 16:29:38: wait
 < 16:29:39: ok 7415
 < 16:29:39: T:231.53 /230 B:44.03 / 0 B@:0 @:0 T0:39.91 / 0 @0:0 T1:231.53 /230 @1:0
 < 16:29:39: Echo:N7415 M105
 < 16:29:40: wait
 < 16:29:40: ok 7416
 < 16:29:40: T:231.00 /230 B:43.51 / 0 B@:0 @:230 T0:39.91 / 0 @0:0 T1:231.00 /230 @1:230
 < 16:29:40: Echo:N7416 M105
 > 16:29:40: N7417 G1T1 E20.0000 F400.000 *93
 < 16:29:40: ok 7417
 < 16:29:40: Echo:N7417 G1 T1  E20.0000 F400.00
 < 16:29:41: ok 7418
 < 16:29:41: T:230.59 /230 B:43.64 / 0 B@:0 @:230 T0:39.91 / 0 @0:0 T1:230.59 /230 @1:230
 < 16:29:41: Echo:N7418 M105
 < 16:29:42: wait
 < 16:29:42: ok 7419
 < 16:29:42: T:230.04 /230 B:43.70 / 0 B@:0 @:230 T0:39.91 / 0 @0:0 T1:230.04 /230 @1:230
 < 16:29:42: Echo:N7419 M105
 < 16:29:43: ok 7420
 < 16:29:43: T:229.63 /230 B:43.77 / 0 B@:0 @:230 T0:39.91 / 0 @0:0 T1:229.63 /230 @1:230
 < 16:29:43: Echo:N7420 M105
 < 16:29:44: ok 7421
 < 16:29:44: T:229.29 /230 B:43.77 / 0 B@:0 @:230 T0:39.91 / 0 @0:0 T1:229.29 /230 @1:230
 < 16:29:44: Echo:N7421 M105
 < 16:29:45: wait
 < 16:29:45: ok 7422
 < 16:29:45: T:229.08 /230 B:43.83 / 0 B@:0 @:230 T0:39.91 / 0 @0:0 T1:229.08 /230 @1:230
 < 16:29:45: Echo:N7422 M105
 < 16:29:46: ok 7423
 < 16:29:46: T:229.01 /230 B:43.83 / 0 B@:0 @:230 T0:39.91 / 0 @0:0 T1:229.01 /230 @1:230
 < 16:29:46: Echo:N7423 M105
 < 16:29:47: ok 7424
 < 16:29:47: T:229.22 /230 B:43.83 / 0 B@:0 @:230 T0:40.33 / 0 @0:0 T1:229.22 /230 @1:230
 < 16:29:47: Echo:N7424 M105
 < 16:29:48: wait
 < 16:29:48: ok 7425
 < 16:29:48: T:229.63 /230 B:44.35 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:229.63 /230 @1:0
 < 16:29:48: Echo:N7425 M105
 > 16:29:48: N7426 G1T0 E20.0000 F400.000 *94
 < 16:29:48: ok 7426
 < 16:29:48: Echo:N7426 G1 T0  E20.0000 F400.00
 < 16:29:49: ok 7427
 < 16:29:49: T:230.25 /230 B:44.29 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:230.25 /230 @1:0
 < 16:29:49: Echo:N7427 M105
 < 16:29:50: wait
 < 16:29:50: ok 7428
 < 16:29:50: T:231.00 /230 B:44.09 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:231.00 /230 @1:0
 < 16:29:50: Echo:N7428 M105
 < 16:29:51: ok 7429
 < 16:29:51: T:231.70 /230 B:44.09 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:231.70 /230 @1:0
 < 16:29:51: Echo:N7429 M105
 < 16:29:52: ok 7430
 < 16:29:52: T:232.32 /230 B:44.03 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:232.32 /230 @1:0
 < 16:29:52: Echo:N7430 M105
 < 16:29:53: wait
 < 16:29:53: ok 7431
 < 16:29:53: T:232.58 /230 B:44.03 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:232.58 /230 @1:0
 < 16:29:53: Echo:N7431 M105
 < 16:29:54: wait
 < 16:29:54: ok 7432
 < 16:29:54: T:232.67 /230 B:43.90 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:232.67 /230 @1:0
 < 16:29:54: Echo:N7432 M105
 < 16:29:55: wait
 < 16:29:55: ok 7433
 < 16:29:55: T:232.49 /230 B:43.90 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:232.49 /230 @1:0
 < 16:29:55: Echo:N7433 M105
 < 16:29:56: ok 7434
 < 16:29:56: T:232.23 /230 B:43.83 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:232.23 /230 @1:0
 < 16:29:56: Echo:N7434 M105
 < 16:29:57: wait
 < 16:29:57: ok 7435
 < 16:29:57: T:231.88 /230 B:43.83 / 0 B@:0 @:0 T0:40.33 / 0 @0:0 T1:231.88 /230 @1:0
 < 16:29:57: Echo:N7435 M105
 < 16:29:58: ok 7436
 < 16:29:58: T:231.35 /230 B:43.25 / 0 B@:0 @:230 T0:38.65 / 0 @0:0 T1:231.35 /230 @1:230
 < 16:29:58: Echo:N7436 M105
 < 16:29:59: wait
 < 16:30:00: ok 7437
 < 16:30:00: T:230.73 /230 B:43.38 / 0 B@:0 @:230 T0:40.33 / 0 @0:0 T1:230.73 /230 @1:230
 < 16:30:00: Echo:N7437 M105
 > 16:30:00: N7438 G1T0 E20.0000 F400.000 *81
 < 16:30:00: ok 7438
 < 16:30:00: Echo:N7438 G1 T0  E20.0000 F400.00
 < 16:30:01: ok 7439
 < 16:30:01: T:230.25 /230 B:43.44 / 0 B@:0 @:230 T0:40.33 / 0 @0:0 T1:230.25 /230 @1:230
 < 16:30:01: Echo:N7439 M105
 < 16:30:02: wait
 < 16:30:02: ok 7440
 < 16:30:02: T:229.77 /230 B:43.51 / 0 B@:0 @:230 T0:40.33 / 0 @0:0 T1:229.77 /230 @1:230
 < 16:30:02: Echo:N7440 M105
 < 16:30:03: ok 7441
 < 16:30:03: T:229.42 /230 B:43.57 / 0 B@:0 @:230 T0:40.74 / 0 @0:0 T1:229.42 /230 @1:230
 < 16:30:03: Echo:N7441 M105
 < 16:30:04: ok 7442
 < 16:30:04: T:229.22 /230 B:43.57 / 0 B@:0 @:230 T0:40.74 / 0 @0:0 T1:229.22 /230 @1:230
 < 16:30:04: Echo:N7442 M105
 < 16:30:05: wait
 < 16:30:05: ok 7443
 < 16:30:05: T:229.08 /230 B:43.57 / 0 B@:0 @:230 T0:40.33 / 0 @0:0 T1:229.08 /230 @1:230
 < 16:30:05: Echo:N7443 M105
 < 16:30:06: ok 7444
 < 16:30:06: T:229.15 /230 B:43.64 / 0 B@:0 @:230 T0:40.33 / 0 @0:0 T1:229.15 /230 @1:230
 < 16:30:06: Echo:N7444 M105
 < 16:30:07: ok 7445
 < 16:30:07: T:229.49 /230 B:44.16 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:229.49 /230 @1:0
 < 16:30:07: Echo:N7445 M105
 < 16:30:08: ok 7446
 < 16:30:08: T:229.97 /230 B:44.03 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:229.97 /230 @1:0
 < 16:30:08: Echo:N7446 M105
 < 16:30:09: wait
 < 16:30:09: ok 7447
 < 16:30:09: T:230.59 /230 B:43.96 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:230.59 /230 @1:0
 < 16:30:09: Echo:N7447 M105
 < 16:30:10: wait
 < 16:30:10: ok 7448
 < 16:30:10: T:231.26 /230 B:43.90 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:231.26 /230 @1:0
 < 16:30:10: Echo:N7448 M105
 < 16:30:11: wait
 < 16:30:11: ok 7449
 < 16:30:11: T:232.05 /230 B:43.83 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:232.05 /230 @1:0
 < 16:30:11: Echo:N7449 M105
 > 16:30:12: N7450 G1 E10.0000 F200.000 *62
 < 16:30:12: ok 7450
 < 16:30:12: Echo:N7450 G1  E10.0000 F200.00
 < 16:30:12: ok 7451
 < 16:30:12: T:232.67 /230 B:43.77 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:232.67 /230 @1:0
 < 16:30:12: Echo:N7451 M105
 < 16:30:13: wait
 < 16:30:13: ok 7452
 < 16:30:13: T:233.02 /230 B:43.70 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:233.02 /230 @1:0
 < 16:30:13: Echo:N7452 M105
 < 16:30:14: ok 7453
 < 16:30:14: T:233.19 /230 B:43.70 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:233.19 /230 @1:0
 < 16:30:14: Echo:N7453 M105
 < 16:30:15: wait
 < 16:30:15: ok 7454
 < 16:30:15: T:233.11 /230 B:43.70 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:233.11 /230 @1:0
 < 16:30:15: Echo:N7454 M105
 < 16:30:16: wait
 < 16:30:16: ok 7455
 < 16:30:16: T:232.84 /230 B:43.64 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:232.84 /230 @1:0
 < 16:30:16: Echo:N7455 M105
 < 16:30:17: wait
 < 16:30:17: ok 7456
 < 16:30:17: T:232.40 /230 B:43.64 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:232.40 /230 @1:0
 < 16:30:17: Echo:N7456 M105
 < 16:30:18: ok 7457
 < 16:30:18: T:231.70 /230 B:43.05 / 0 B@:0 @:230 T0:39.07 / 0 @0:0 T1:231.70 /230 @1:230
 < 16:30:18: Echo:N7457 M105
 < 16:30:19: wait
 < 16:30:20: ok 7458
 < 16:30:20: T:230.93 /230 B:43.12 / 0 B@:0 @:230 T0:40.74 / 0 @0:0 T1:230.93 /230 @1:230
 < 16:30:20: Echo:N7458 M105
 < 16:30:21: ok 7459
 < 16:30:21: T:230.38 /230 B:43.25 / 0 B@:0 @:230 T0:40.74 / 0 @0:0 T1:230.38 /230 @1:230
 < 16:30:21: Echo:N7459 M105
 < 16:30:22: wait
 < 16:30:22: ok 7460
 < 16:30:22: T:229.77 /230 B:43.31 / 0 B@:0 @:230 T0:40.74 / 0 @0:0 T1:229.77 /230 @1:230
 < 16:30:22: Echo:N7460 M105
 < 16:30:23: wait
 < 16:30:23: ok 7461
 < 16:30:23: T:229.36 /230 B:43.31 / 0 B@:0 @:230 T0:41.16 / 0 @0:0 T1:229.36 /230 @1:230
 < 16:30:23: Echo:N7461 M105
 < 16:30:24: wait
 < 16:30:24: ok 7462
 < 16:30:24: T:229.15 /230 B:43.38 / 0 B@:0 @:230 T0:41.16 / 0 @0:0 T1:229.15 /230 @1:230
 < 16:30:24: Echo:N7462 M105
 < 16:30:25: ok 7463
 < 16:30:25: T:229.22 /230 B:43.38 / 0 B@:0 @:230 T0:41.16 / 0 @0:0 T1:229.22 /230 @1:230
 < 16:30:25: Echo:N7463 M105
 < 16:30:26: wait
 < 16:30:26: ok 7464
 < 16:30:26: T:229.49 /230 B:43.70 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:229.49 /230 @1:0
 < 16:30:26: Echo:N7464 M105
 < 16:30:27: ok 7465
 < 16:30:27: T:229.90 /230 B:43.83 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:229.90 /230 @1:0
 < 16:30:27: Echo:N7465 M105
 < 16:30:28: ok 7466
 < 16:30:28: T:230.45 /230 B:43.77 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:230.45 /230 @1:0
 < 16:30:28: Echo:N7466 M105
 < 16:30:29: wait
 < 16:30:29: ok 7467
 < 16:30:29: T:231.18 /230 B:43.64 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:231.18 /230 @1:0
 < 16:30:29: Echo:N7467 M105
 > 16:30:30: N7468 G1T1 E10.0000 F200.000 *80
 < 16:30:30: ok 7468
 < 16:30:30: Echo:N7468 G1 T1  E10.0000 F200.00
 < 16:30:30: ok 7469
 < 16:30:30: T:231.88 /230 B:43.64 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:231.88 /230 @1:0
 < 16:30:30: Echo:N7469 M105
 < 16:30:31: wait
 < 16:30:31: ok 7470
 < 16:30:31: T:232.49 /230 B:43.57 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:232.49 /230 @1:0
 < 16:30:31: Echo:N7470 M105
 < 16:30:32: wait
 < 16:30:32: ok 7471
 < 16:30:32: T:232.93 /230 B:43.51 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:232.93 /230 @1:0
 < 16:30:32: Echo:N7471 M105
 < 16:30:33: ok 7472
 < 16:30:33: T:233.02 /230 B:43.51 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:233.02 /230 @1:0
 < 16:30:33: Echo:N7472 M105
 < 16:30:34: wait
 < 16:30:34: ok 7473
 < 16:30:34: T:233.02 /230 B:43.44 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:233.02 /230 @1:0
 < 16:30:34: Echo:N7473 M105
 < 16:30:35: ok 7474
 < 16:30:35: T:232.67 /230 B:43.44 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:232.67 /230 @1:0
 < 16:30:35: Echo:N7474 M105
 > 16:30:36: N7475 T0 *43
 < 16:30:36: ok 7475
 < 16:30:36: SelectExtruder:0
 < 16:30:36: FlowMultiply:100
 < 16:30:37: Echo:N7475 T0
 < 16:30:37: ok 7476
 < 16:30:37: T:41.16 / 0 B:43.44 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:232.05 /230 @1:0
 < 16:30:37: Echo:N7476 M105
 < 16:30:37: ok 7477
 < 16:30:37: T:39.49 / 0 B:42.86 / 0 B@:0 @:0 T0:39.49 / 0 @0:0 T1:231.53 /230 @1:230
 < 16:30:37: Echo:N7477 M105
 < 16:30:38: ok 7478
 < 16:30:38: T:41.16 / 0 B:42.92 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:230.86 /230 @1:230
 < 16:30:38: Echo:N7478 M105
 < 16:30:39: wait
 < 16:30:40: ok 7479
 < 16:30:40: T:41.16 / 0 B:43.05 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:230.25 /230 @1:230
 < 16:30:40: Echo:N7479 M105
 < 16:30:41: ok 7480
 < 16:30:41: T:41.16 / 0 B:43.12 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:229.77 /230 @1:230
 < 16:30:41: Echo:N7480 M105
 < 16:30:42: ok 7481
 < 16:30:42: T:41.16 / 0 B:43.12 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:229.36 /230 @1:230
 < 16:30:42: Echo:N7481 M105
 < 16:30:43: wait
 > 16:30:43: N7482 G1T1 E10.0000 F200.000 *84
 < 16:30:43: ok 7482
 < 16:30:43: Echo:N7482 G1 T1  E10.0000 F200.00
 < 16:30:43: ok 7483
 < 16:30:43: T:41.16 / 0 B:43.18 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:229.22 /230 @1:230
 < 16:30:43: Echo:N7483 M105
 < 16:30:44: wait
 < 16:30:44: ok 7484
 < 16:30:44: T:41.16 / 0 B:43.18 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:229.22 /230 @1:230
 < 16:30:44: Echo:N7484 M105
 < 16:30:45: ok 7485
 < 16:30:45: T:42.44 / 0 B:43.77 / 0 B@:0 @:0 T0:42.44 / 0 @0:0 T1:229.49 /230 @1:0
 < 16:30:45: Echo:N7485 M105
 < 16:30:46: ok 7486
 < 16:30:46: T:41.16 / 0 B:43.64 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:229.90 /230 @1:0
 < 16:30:46: Echo:N7486 M105
 < 16:30:47: wait
 < 16:30:47: ok 7487
 < 16:30:47: T:41.16 / 0 B:43.57 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:230.52 /230 @1:0
 < 16:30:47: Echo:N7487 M105
 < 16:30:48: wait
 < 16:30:48: ok 7488
 < 16:30:48: T:41.16 / 0 B:43.51 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:231.26 /230 @1:0
 < 16:30:48: Echo:N7488 M105
 < 16:30:49: ok 7489
 < 16:30:49: T:41.16 / 0 B:43.44 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:231.88 /230 @1:0
 < 16:30:49: Echo:N7489 M105
 < 16:30:50: wait
 < 16:30:50: ok 7490
 < 16:30:50: T:41.16 / 0 B:43.38 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:232.40 /230 @1:0
 < 16:30:50: Echo:N7490 M105
 < 16:30:51: wait
 < 16:30:51: ok 7491
 < 16:30:51: T:41.16 / 0 B:43.38 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:232.75 /230 @1:0
 < 16:30:51: Echo:N7491 M105
 < 16:30:52: ok 7492
 < 16:30:52: T:41.16 / 0 B:43.38 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:232.84 /230 @1:0
 < 16:30:52: Echo:N7492 M105
 > 16:30:52: N7493 G1 E10.0000 F200.000 *49
 < 16:30:52: ok 7493
 < 16:30:52: Echo:N7493 G1  E10.0000 F200.00
 < 16:30:53: ok 7494
 < 16:30:53: T:40.74 / 0 B:43.31 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:232.84 /230 @1:0
 < 16:30:53: Echo:N7494 M105
 < 16:30:54: ok 7495
 < 16:30:54: T:41.16 / 0 B:43.31 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:232.67 /230 @1:0
 < 16:30:54: Echo:N7495 M105
 > 16:30:55: N7496 G1 E10.0000 F200.000 *52
 < 16:30:55: ok 7496
 < 16:30:55: Echo:N7496 G1  E10.0000 F200.00
 < 16:30:55: ok 7497
 < 16:30:55: T:41.16 / 0 B:43.25 / 0 B@:0 @:0 T0:41.16 / 0 @0:0 T1:232.23 /230 @1:0
 < 16:30:55: Echo:N7497 M105
 < 16:30:56: wait
 < 16:30:56: ok 7498
 < 16:30:56: T:40.74 / 0 B:43.18 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:231.70 /230 @1:0
 < 16:30:56: Echo:N7498 M105
 < 16:30:57: ok 7499
 < 16:30:57: T:40.74 / 0 B:42.73 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:231.00 /230 @1:230
 < 16:30:57: Echo:N7499 M105
 > 16:30:58: N7500 G1T1 E10.0000 F200.000 *95
 < 16:30:58: ok 7500
 < 16:30:58: Echo:N7500 G1 T1  E10.0000 F200.00
 < 16:30:58: ok 7501
 < 16:30:58: T:40.74 / 0 B:42.86 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:230.38 /230 @1:230
 < 16:30:58: Echo:N7501 M105
 < 16:30:59: wait
 < 16:30:59: ok 7502
 < 16:30:59: T:40.74 / 0 B:42.86 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:229.84 /230 @1:230
 < 16:30:59: Echo:N7502 M105
 < 16:31:00: wait
 < 16:31:01: ok 7503
 < 16:31:01: T:40.74 / 0 B:42.92 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:229.36 /230 @1:230
 < 16:31:01: Echo:N7503 M105
 < 16:31:02: ok 7504
 < 16:31:02: T:40.74 / 0 B:42.99 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:229.15 /230 @1:230
 < 16:31:02: Echo:N7504 M105
 < 16:31:03: ok 7505
 < 16:31:03: T:40.74 / 0 B:42.99 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:229.08 /230 @1:230
 < 16:31:03: Echo:N7505 M105
 < 16:31:04: ok 7506
 < 16:31:04: T:40.74 / 0 B:42.99 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:229.22 /230 @1:230
 < 16:31:04: Echo:N7506 M105
 < 16:31:05: wait
 < 16:31:05: ok 7507
 < 16:31:05: T:40.74 / 0 B:43.51 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:229.84 /230 @1:0
 < 16:31:05: Echo:N7507 M105
 < 16:31:06: wait
 < 16:31:06: ok 7508
 < 16:31:06: T:40.74 / 0 B:43.38 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:230.45 /230 @1:0
 < 16:31:06: Echo:N7508 M105
 < 16:31:07: wait
 < 16:31:07: ok 7509
 < 16:31:07: T:40.74 / 0 B:43.31 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:231.09 /230 @1:0
 < 16:31:07: Echo:N7509 M105
 < 16:31:08: ok 7510
 < 16:31:08: T:40.74 / 0 B:43.25 / 0 B@:0 @:0 T0:40.74 / 0 @0:0 T1:231.79 /230 @1:0
 < 16:31:08: Echo:N7510 M105
 16:33:13: Communication timeout - reset send buffer block
 16:33:30: Connection closed
*/

