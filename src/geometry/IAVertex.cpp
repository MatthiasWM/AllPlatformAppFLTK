//
//  geometry.cpp
//
//  Copyright (c) 2013-2018 Matthias Melcher. All rights reserved.
//


#include "IAVertex.h"

#include "Iota.h"

#include <math.h>


/**
 Create a vertex at 0, 0, 0.
 */
IAVertex::IAVertex()
{
}


/**
 Dublicate another vertex.
 */
IAVertex::IAVertex(const IAVertex *v)
{
    pLocalPosition = v->pLocalPosition;
    pNormal = v->pNormal;
    pTex = v->pTex;
    pNNormal = v->pNNormal;
}


/**
 Add a vector to the current normal and increase the normal count.
 This method is used to calculate the average of the normals of all
 connected triangles.
 \see IAVertex::averageNormal()
 */
void IAVertex::addNormal(const IAVector3d &v)
{
    IAVector3d vn(v);
    vn.normalize();
    pNormal += vn;
    pNNormal++;
}


/**
 Divide the normal vector by the number of normals we accumulated.
 \see IAVertex::addNormal(const IAVector3d &v)
 */
void IAVertex::averageNormal()
{
    if (pNNormal>0) {
        double len = 1.0/pNNormal;
        pNormal *= len;
    }
}


/**
 Print the position of a vertex.
 */
void IAVertex::print()
{
    printf("v=[%g, %g, %g]\n", pLocalPosition.x(), pLocalPosition.y(), pLocalPosition.z());
}


/**
 Project a texture onto this vertex in a mesh.
 */
void IAVertex::projectTexture(double x, double y, double w, double h, int type)
{
    double a;
    switch (type) {
        case IA_PROJECTION_FRONT:
            pTex.set((pLocalPosition.x()+x)*w, -(pLocalPosition.z()+y)*h, 0.0);
            break;
        case IA_PROJECTION_CYLINDRICAL:
            a = atan2(pLocalPosition.x(), -pLocalPosition.y());
            pTex.set((a/2.0/M_PI)*w, -(pLocalPosition.z()+y)*h, 0.0);
            break;
        case IA_PROJECTION_SPHERICAL:
            break;
    }
}

