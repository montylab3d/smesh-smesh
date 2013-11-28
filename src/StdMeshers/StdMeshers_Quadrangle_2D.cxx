// Copyright (C) 2007-2013  CEA/DEN, EDF R&D, OPEN CASCADE
//
// Copyright (C) 2003-2007  OPEN CASCADE, EADS/CCR, LIP6, CEA/DEN,
// CEDRAT, EDF R&D, LEG, PRINCIPIA R&D, BUREAU VERITAS
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
//
// See http://www.salome-platform.org/ or email : webmaster.salome@opencascade.com
//

//  File   : StdMeshers_Quadrangle_2D.cxx
//  Author : Paul RASCLE, EDF
//  Module : SMESH

#include "StdMeshers_Quadrangle_2D.hxx"

#include "SMDS_EdgePosition.hxx"
#include "SMDS_FacePosition.hxx"
#include "SMDS_MeshElement.hxx"
#include "SMDS_MeshNode.hxx"
#include "SMESH_Block.hxx"
#include "SMESH_Comment.hxx"
#include "SMESH_Gen.hxx"
#include "SMESH_Mesh.hxx"
#include "SMESH_MesherHelper.hxx"
#include "SMESH_subMesh.hxx"
#include "StdMeshers_FaceSide.hxx"
#include "StdMeshers_QuadrangleParams.hxx"
#include "StdMeshers_ViscousLayers2D.hxx"

#include <BRep_Tool.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_Surface.hxx>
#include <NCollection_DefineArray2.hxx>
#include <Precision.hxx>
#include <Quantity_Parameter.hxx>
#include <TColStd_SequenceOfInteger.hxx>
#include <TColStd_SequenceOfReal.hxx>
#include <TColgp_SequenceOfXY.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_DataMapOfShapeReal.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_MapOfShape.hxx>
#include <TopoDS.hxx>

#include "utilities.h"
#include "Utils_ExceptHandlers.hxx"

#ifndef StdMeshers_Array2OfNode_HeaderFile
#define StdMeshers_Array2OfNode_HeaderFile
typedef const SMDS_MeshNode* SMDS_MeshNodePtr;
DEFINE_BASECOLLECTION (StdMeshers_BaseCollectionNodePtr, SMDS_MeshNodePtr)
DEFINE_ARRAY2(StdMeshers_Array2OfNode,
              StdMeshers_BaseCollectionNodePtr, SMDS_MeshNodePtr)
#endif

using namespace std;

typedef gp_XY gp_UV;
typedef SMESH_Comment TComm;

//=============================================================================
/*!
 *
 */
//=============================================================================

StdMeshers_Quadrangle_2D::StdMeshers_Quadrangle_2D (int hypId, int studyId,
                                                    SMESH_Gen* gen)
  : SMESH_2D_Algo(hypId, studyId, gen),
    myQuadranglePreference(false),
    myTrianglePreference(false),
    myTriaVertexID(-1),
    myNeedSmooth(false),
    myQuadType(QUAD_STANDARD),
    myHelper( 0 )
{
  MESSAGE("StdMeshers_Quadrangle_2D::StdMeshers_Quadrangle_2D");
  _name = "Quadrangle_2D";
  _shapeType = (1 << TopAbs_FACE);
  _compatibleHypothesis.push_back("QuadrangleParams");
  _compatibleHypothesis.push_back("QuadranglePreference");
  _compatibleHypothesis.push_back("TrianglePreference");
  _compatibleHypothesis.push_back("ViscousLayers2D");
}

//=============================================================================
/*!
 *
 */
//=============================================================================

StdMeshers_Quadrangle_2D::~StdMeshers_Quadrangle_2D()
{
  MESSAGE("StdMeshers_Quadrangle_2D::~StdMeshers_Quadrangle_2D");
}

//=============================================================================
/*!
 *  
 */
//=============================================================================

bool StdMeshers_Quadrangle_2D::CheckHypothesis
                         (SMESH_Mesh&                          aMesh,
                          const TopoDS_Shape&                  aShape,
                          SMESH_Hypothesis::Hypothesis_Status& aStatus)
{
  myTriaVertexID = -1;
  myQuadType = QUAD_STANDARD;
  myQuadranglePreference = false;
  myTrianglePreference = false;
  myQuadStruct.reset();
  myHelper = NULL;

  bool isOk = true;
  aStatus = SMESH_Hypothesis::HYP_OK;

  const list <const SMESHDS_Hypothesis * >& hyps =
    GetUsedHypothesis(aMesh, aShape, false);
  const SMESHDS_Hypothesis * aHyp = 0;

  bool isFirstParams = true;

  // First assigned hypothesis (if any) is processed now
  if (hyps.size() > 0) {
    aHyp = hyps.front();
    if (strcmp("QuadrangleParams", aHyp->GetName()) == 0) {
      const StdMeshers_QuadrangleParams* aHyp1 = 
        (const StdMeshers_QuadrangleParams*)aHyp;
      myTriaVertexID = aHyp1->GetTriaVertex();
      myQuadType = aHyp1->GetQuadType();
      if (myQuadType == QUAD_QUADRANGLE_PREF ||
          myQuadType == QUAD_QUADRANGLE_PREF_REVERSED)
        myQuadranglePreference = true;
      else if (myQuadType == QUAD_TRIANGLE_PREF)
        myTrianglePreference = true;
    }
    else if (strcmp("QuadranglePreference", aHyp->GetName()) == 0) {
      isFirstParams = false;
      myQuadranglePreference = true;
    }
    else if (strcmp("TrianglePreference", aHyp->GetName()) == 0){
      isFirstParams = false;
      myTrianglePreference = true; 
    }
    else {
      isFirstParams = false;
    }
  }

  // Second(last) assigned hypothesis (if any) is processed now
  if (hyps.size() > 1) {
    aHyp = hyps.back();
    if (isFirstParams) {
      if (strcmp("QuadranglePreference", aHyp->GetName()) == 0) {
        myQuadranglePreference = true;
        myTrianglePreference = false; 
        myQuadType = QUAD_STANDARD;
      }
      else if (strcmp("TrianglePreference", aHyp->GetName()) == 0){
        myQuadranglePreference = false;
        myTrianglePreference = true; 
        myQuadType = QUAD_STANDARD;
      }
    }
    else {
      const StdMeshers_QuadrangleParams* aHyp2 = 
        (const StdMeshers_QuadrangleParams*)aHyp;
      myTriaVertexID = aHyp2->GetTriaVertex();

      if (!myQuadranglePreference && !myTrianglePreference) { // priority of hypos
        myQuadType = aHyp2->GetQuadType();
        if (myQuadType == QUAD_QUADRANGLE_PREF ||
            myQuadType == QUAD_QUADRANGLE_PREF_REVERSED)
          myQuadranglePreference = true;
        else if (myQuadType == QUAD_TRIANGLE_PREF)
          myTrianglePreference = true;
      }
    }
  }

  return isOk;
}

//=============================================================================
/*!
 *  
 */
//=============================================================================

bool StdMeshers_Quadrangle_2D::Compute (SMESH_Mesh&         aMesh,
                                        const TopoDS_Shape& aShape)
{
  const TopoDS_Face& F = TopoDS::Face(aShape);
  aMesh.GetSubMesh( F );

  SMESH_MesherHelper helper (aMesh);
  myHelper = &helper;

  myProxyMesh = StdMeshers_ViscousLayers2D::Compute( aMesh, F );
  if ( !myProxyMesh )
    return false;

  _quadraticMesh = myHelper->IsQuadraticSubMesh(aShape);
  myNeedSmooth = false;

  FaceQuadStruct::Ptr quad = CheckNbEdges( aMesh, F, /*considerMesh=*/true );
  if (!quad)
    return false;
  myQuadStruct = quad;

  enum { NOT_COMPUTED = -1, COMPUTE_FAILED = 0, COMPUTE_OK = 1 };
  int res = NOT_COMPUTED;
  if (myQuadranglePreference)
  {
    int n1    = quad->side[0]->NbPoints();
    int n2    = quad->side[1]->NbPoints();
    int n3    = quad->side[2]->NbPoints();
    int n4    = quad->side[3]->NbPoints();
    int nfull = n1+n2+n3+n4;
    int ntmp  = nfull/2;
    ntmp = ntmp*2;
    if (nfull == ntmp && ((n1 != n3) || (n2 != n4)))
    {
      // special path genarating only quandrangle faces
      res = computeQuadPref( aMesh, F, quad );
    }
  }
  else if (myQuadType == QUAD_REDUCED)
  {
    int n1     = quad->side[0]->NbPoints();
    int n2     = quad->side[1]->NbPoints();
    int n3     = quad->side[2]->NbPoints();
    int n4     = quad->side[3]->NbPoints();
    int n13    = n1 - n3;
    int n24    = n2 - n4;
    int n13tmp = n13/2; n13tmp = n13tmp*2;
    int n24tmp = n24/2; n24tmp = n24tmp*2;
    if ((n1 == n3 && n2 != n4 && n24tmp == n24) ||
        (n2 == n4 && n1 != n3 && n13tmp == n13))
    {
      res = computeReduced( aMesh, F, quad );
    }
    else
    {
      if ( n1 != n3 && n2 != n4 )
        error( COMPERR_WARNING,
               "To use 'Reduced' transition, "
               "two opposite sides should have same number of segments, "
               "but actual number of segments is different on all sides. "
               "'Standard' transion has been used.");
      else
        error( COMPERR_WARNING,
               "To use 'Reduced' transition, "
               "two opposite sides should have an even difference in number of segments. "
               "'Standard' transion has been used.");
    }
  }

  if ( res == NOT_COMPUTED )
  {
    res = computeQuadDominant( aMesh, F, quad );
  }

  if ( res == COMPUTE_OK && myNeedSmooth )
    smooth( quad );

  return ( res == COMPUTE_OK );
}

//================================================================================
/*!
 * \brief Compute quadrangles and possibly triangles
 */
//================================================================================

bool StdMeshers_Quadrangle_2D::computeQuadDominant(SMESH_Mesh&         aMesh,
                                                   const TopoDS_Face&  aFace,
                                                   FaceQuadStruct::Ptr quad)
{
  // set normalized grid on unit square in parametric domain

  if (!setNormalizedGrid(aMesh, aFace, quad))
    return false;

  // --- compute 3D values on points, store points & quadrangles

  int nbdown  = quad->side[0]->NbPoints();
  int nbup    = quad->side[2]->NbPoints();

  int nbright = quad->side[1]->NbPoints();
  int nbleft  = quad->side[3]->NbPoints();

  int nbhoriz  = Min(nbdown, nbup);
  int nbvertic = Min(nbright, nbleft);

  // internal mesh nodes
  SMESHDS_Mesh *  meshDS = aMesh.GetMeshDS();
  Handle(Geom_Surface) S = BRep_Tool::Surface(aFace);
  int i, j, geomFaceID = meshDS->ShapeToIndex(aFace);
  for (i = 1; i < nbhoriz - 1; i++) {
    for (j = 1; j < nbvertic - 1; j++) {
      int ij = j * nbhoriz + i;
      double u = quad->uv_grid[ij].u;
      double v = quad->uv_grid[ij].v;
      gp_Pnt P = S->Value(u, v);
      SMDS_MeshNode * node = meshDS->AddNode(P.X(), P.Y(), P.Z());
      meshDS->SetNodeOnFace(node, geomFaceID, u, v);
      quad->uv_grid[ij].node = node;
    }
  }
  
  // mesh faces

  //             [2]
  //      --.--.--.--.--.--  nbvertic
  //     |                 | ^
  //     |                 | ^
  // [3] |                 | ^ j  [1]
  //     |                 | ^
  //     |                 | ^
  //      ---.----.----.---  0
  //     0 > > > > > > > > nbhoriz
  //              i
  //             [0]
  
  i = 0;
  int ilow = 0;
  int iup = nbhoriz - 1;
  if (quad->isEdgeOut[3]) { ilow++; } else { if (quad->isEdgeOut[1]) iup--; }
  
  int jlow = 0;
  int jup = nbvertic - 1;
  if (quad->isEdgeOut[0]) { jlow++; } else { if (quad->isEdgeOut[2]) jup--; }
  
  // regular quadrangles
  for (i = ilow; i < iup; i++) {
    for (j = jlow; j < jup; j++) {
      const SMDS_MeshNode *a, *b, *c, *d;
      a = quad->uv_grid[j       * nbhoriz + i    ].node;
      b = quad->uv_grid[j       * nbhoriz + i + 1].node;
      c = quad->uv_grid[(j + 1) * nbhoriz + i + 1].node;
      d = quad->uv_grid[(j + 1) * nbhoriz + i    ].node;
      SMDS_MeshFace* face = myHelper->AddFace(a, b, c, d);
      if (face) {
        meshDS->SetMeshElementOnShape(face, geomFaceID);
      }
    }
  }

  const vector<UVPtStruct>& uv_e0 = quad->side[0]->GetUVPtStruct(true,0);
  const vector<UVPtStruct>& uv_e1 = quad->side[1]->GetUVPtStruct(false,1);
  const vector<UVPtStruct>& uv_e2 = quad->side[2]->GetUVPtStruct(true,1);
  const vector<UVPtStruct>& uv_e3 = quad->side[3]->GetUVPtStruct(false,0);

  if (uv_e0.empty() || uv_e1.empty() || uv_e2.empty() || uv_e3.empty())
    return error(COMPERR_BAD_INPUT_MESH);

  double eps = Precision::Confusion();

  // Boundary quadrangles
  
  if (quad->isEdgeOut[0]) {
    // Down edge is out
    // 
    // |___|___|___|___|___|___|
    // |   |   |   |   |   |   |
    // |___|___|___|___|___|___|
    // |   |   |   |   |   |   |
    // |___|___|___|___|___|___| __ first row of the regular grid
    // .  .  .  .  .  .  .  .  . __ down edge nodes
    // 
    // >->->->->->->->->->->->-> -- direction of processing
      
    int g = 0; // number of last processed node in the regular grid
    
    // number of last node of the down edge to be processed
    int stop = nbdown - 1;
    // if right edge is out, we will stop at a node, previous to the last one
    if (quad->isEdgeOut[1]) stop--;
    
    // for each node of the down edge find nearest node
    // in the first row of the regular grid and link them
    for (i = 0; i < stop; i++) {
      const SMDS_MeshNode *a, *b, *c, *d;
      a = uv_e0[i].node;
      b = uv_e0[i + 1].node;
      gp_Pnt pb (b->X(), b->Y(), b->Z());
      
      // find node c in the regular grid, which will be linked with node b
      int near = g;
      if (i == stop - 1) {
        // right bound reached, link with the rightmost node
        near = iup;
        c = quad->uv_grid[nbhoriz + iup].node;
      }
      else {
        // find in the grid node c, nearest to the b
        double mind = RealLast();
        for (int k = g; k <= iup; k++) {
          
          const SMDS_MeshNode *nk;
          if (k < ilow) // this can be, if left edge is out
            nk = uv_e3[1].node; // get node from the left edge
          else
            nk = quad->uv_grid[nbhoriz + k].node; // get one of middle nodes

          gp_Pnt pnk (nk->X(), nk->Y(), nk->Z());
          double dist = pb.Distance(pnk);
          if (dist < mind - eps) {
            c = nk;
            near = k;
            mind = dist;
          } else {
            break;
          }
        }
      }

      if (near == g) { // make triangle
        SMDS_MeshFace* face = myHelper->AddFace(a, b, c);
        if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
      }
      else { // make quadrangle
        if (near - 1 < ilow)
          d = uv_e3[1].node;
        else
          d = quad->uv_grid[nbhoriz + near - 1].node;
        //SMDS_MeshFace* face = meshDS->AddFace(a, b, c, d);
        
        if (!myTrianglePreference){
          SMDS_MeshFace* face = myHelper->AddFace(a, b, c, d);
          if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
        }
        else {
          splitQuad(meshDS, geomFaceID, a, b, c, d);
        }

        // if node d is not at position g - make additional triangles
        if (near - 1 > g) {
          for (int k = near - 1; k > g; k--) {
            c = quad->uv_grid[nbhoriz + k].node;
            if (k - 1 < ilow)
              d = uv_e3[1].node;
            else
              d = quad->uv_grid[nbhoriz + k - 1].node;
            SMDS_MeshFace* face = myHelper->AddFace(a, c, d);
            if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
          }
        }
        g = near;
      }
    }
  } else {
    if (quad->isEdgeOut[2]) {
      // Up edge is out
      // 
      // <-<-<-<-<-<-<-<-<-<-<-<-< -- direction of processing
      // 
      // .  .  .  .  .  .  .  .  . __ up edge nodes
      //  ___ ___ ___ ___ ___ ___  __ first row of the regular grid
      // |   |   |   |   |   |   |
      // |___|___|___|___|___|___|
      // |   |   |   |   |   |   |
      // |___|___|___|___|___|___|
      // |   |   |   |   |   |   |

      int g = nbhoriz - 1; // last processed node in the regular grid

      int stop = 0;
      // if left edge is out, we will stop at a second node
      if (quad->isEdgeOut[3]) stop++;

      // for each node of the up edge find nearest node
      // in the first row of the regular grid and link them
      for (i = nbup - 1; i > stop; i--) {
        const SMDS_MeshNode *a, *b, *c, *d;
        a = uv_e2[i].node;
        b = uv_e2[i - 1].node;
        gp_Pnt pb (b->X(), b->Y(), b->Z());

        // find node c in the grid, which will be linked with node b
        int near = g;
        if (i == stop + 1) { // left bound reached, link with the leftmost node
          c = quad->uv_grid[nbhoriz*(nbvertic - 2) + ilow].node;
          near = ilow;
        } else {
          // find node c in the grid, nearest to the b
          double mind = RealLast();
          for (int k = g; k >= ilow; k--) {
            const SMDS_MeshNode *nk;
            if (k > iup)
              nk = uv_e1[nbright - 2].node;
            else
              nk = quad->uv_grid[nbhoriz*(nbvertic - 2) + k].node;
            gp_Pnt pnk (nk->X(), nk->Y(), nk->Z());
            double dist = pb.Distance(pnk);
            if (dist < mind - eps) {
              c = nk;
              near = k;
              mind = dist;
            } else {
              break;
            }
          }
        }

        if (near == g) { // make triangle
          SMDS_MeshFace* face = myHelper->AddFace(a, b, c);
          if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
        }
        else { // make quadrangle
          if (near + 1 > iup)
            d = uv_e1[nbright - 2].node;
          else
            d = quad->uv_grid[nbhoriz*(nbvertic - 2) + near + 1].node;
          //SMDS_MeshFace* face = meshDS->AddFace(a, b, c, d);
          if (!myTrianglePreference){
            SMDS_MeshFace* face = myHelper->AddFace(a, b, c, d);
            if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
          }
          else {
            splitQuad(meshDS, geomFaceID, a, b, c, d);
          }

          if (near + 1 < g) { // if d not is at g - make additional triangles
            for (int k = near + 1; k < g; k++) {
              c = quad->uv_grid[nbhoriz*(nbvertic - 2) + k].node;
              if (k + 1 > iup)
                d = uv_e1[nbright - 2].node;
              else
                d = quad->uv_grid[nbhoriz*(nbvertic - 2) + k + 1].node;
              SMDS_MeshFace* face = myHelper->AddFace(a, c, d);
              if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
            }
          }
          g = near;
        }
      }
    }
  }

  // right or left boundary quadrangles
  if (quad->isEdgeOut[1]) {
//    MESSAGE("right edge is out");
    int g = 0; // last processed node in the grid
    int stop = nbright - 1;
    if (quad->isEdgeOut[2]) stop--;
    for (i = 0; i < stop; i++) {
      const SMDS_MeshNode *a, *b, *c, *d;
      a = uv_e1[i].node;
      b = uv_e1[i + 1].node;
      gp_Pnt pb (b->X(), b->Y(), b->Z());

      // find node c in the grid, nearest to the b
      int near = g;
      if (i == stop - 1) { // up bondary reached
        c = quad->uv_grid[nbhoriz*(jup + 1) - 2].node;
        near = jup;
      } else {
        double mind = RealLast();
        for (int k = g; k <= jup; k++) {
          const SMDS_MeshNode *nk;
          if (k < jlow)
            nk = uv_e0[nbdown - 2].node;
          else
            nk = quad->uv_grid[nbhoriz*(k + 1) - 2].node;
          gp_Pnt pnk (nk->X(), nk->Y(), nk->Z());
          double dist = pb.Distance(pnk);
          if (dist < mind - eps) {
            c = nk;
            near = k;
            mind = dist;
          } else {
            break;
          }
        }
      }

      if (near == g) { // make triangle
        SMDS_MeshFace* face = myHelper->AddFace(a, b, c);
        if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
      }
      else { // make quadrangle
        if (near - 1 < jlow)
          d = uv_e0[nbdown - 2].node;
        else
          d = quad->uv_grid[nbhoriz*near - 2].node;
        //SMDS_MeshFace* face = meshDS->AddFace(a, b, c, d);

        if (!myTrianglePreference){
          SMDS_MeshFace* face = myHelper->AddFace(a, b, c, d);
          if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
        }
        else {
          splitQuad(meshDS, geomFaceID, a, b, c, d);
        }

        if (near - 1 > g) { // if d not is at g - make additional triangles
          for (int k = near - 1; k > g; k--) {
            c = quad->uv_grid[nbhoriz*(k + 1) - 2].node;
            if (k - 1 < jlow)
              d = uv_e0[nbdown - 2].node;
            else
              d = quad->uv_grid[nbhoriz*k - 2].node;
            SMDS_MeshFace* face = myHelper->AddFace(a, c, d);
            if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
          }
        }
        g = near;
      }
    }
  } else {
    if (quad->isEdgeOut[3]) {
//      MESSAGE("left edge is out");
      int g = nbvertic - 1; // last processed node in the grid
      int stop = 0;
      if (quad->isEdgeOut[0]) stop++;
      for (i = nbleft - 1; i > stop; i--) {
        const SMDS_MeshNode *a, *b, *c, *d;
        a = uv_e3[i].node;
        b = uv_e3[i - 1].node;
        gp_Pnt pb (b->X(), b->Y(), b->Z());

        // find node c in the grid, nearest to the b
        int near = g;
        if (i == stop + 1) { // down bondary reached
          c = quad->uv_grid[nbhoriz*jlow + 1].node;
          near = jlow;
        } else {
          double mind = RealLast();
          for (int k = g; k >= jlow; k--) {
            const SMDS_MeshNode *nk;
            if (k > jup)
              nk = uv_e2[1].node;
            else
              nk = quad->uv_grid[nbhoriz*k + 1].node;
            gp_Pnt pnk (nk->X(), nk->Y(), nk->Z());
            double dist = pb.Distance(pnk);
            if (dist < mind - eps) {
              c = nk;
              near = k;
              mind = dist;
            } else {
              break;
            }
          }
        }

        if (near == g) { // make triangle
          SMDS_MeshFace* face = myHelper->AddFace(a, b, c);
          if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
        }
        else { // make quadrangle
          if (near + 1 > jup)
            d = uv_e2[1].node;
          else
            d = quad->uv_grid[nbhoriz*(near + 1) + 1].node;
          //SMDS_MeshFace* face = meshDS->AddFace(a, b, c, d);
          if (!myTrianglePreference){
            SMDS_MeshFace* face = myHelper->AddFace(a, b, c, d);
            if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
          }
          else {
            splitQuad(meshDS, geomFaceID, a, b, c, d);
          }

          if (near + 1 < g) { // if d not is at g - make additional triangles
            for (int k = near + 1; k < g; k++) {
              c = quad->uv_grid[nbhoriz*k + 1].node;
              if (k + 1 > jup)
                d = uv_e2[1].node;
              else
                d = quad->uv_grid[nbhoriz*(k + 1) + 1].node;
              SMDS_MeshFace* face = myHelper->AddFace(a, c, d);
              if (face) meshDS->SetMeshElementOnShape(face, geomFaceID);
            }
          }
          g = near;
        }
      }
    }
  }

  bool isOk = true;
  return isOk;
}


//=============================================================================
/*!
 *  Evaluate
 */
//=============================================================================

bool StdMeshers_Quadrangle_2D::Evaluate(SMESH_Mesh&         aMesh,
                                        const TopoDS_Shape& aFace,
                                        MapShapeNbElems&    aResMap)

{
  aMesh.GetSubMesh(aFace);

  std::vector<int> aNbNodes(4);
  bool IsQuadratic = false;
  if (!checkNbEdgesForEvaluate(aMesh, aFace, aResMap, aNbNodes, IsQuadratic)) {
    std::vector<int> aResVec(SMDSEntity_Last);
    for (int i=SMDSEntity_Node; i<SMDSEntity_Last; i++) aResVec[i] = 0;
    SMESH_subMesh * sm = aMesh.GetSubMesh(aFace);
    aResMap.insert(std::make_pair(sm,aResVec));
    SMESH_ComputeErrorPtr& smError = sm->GetComputeError();
    smError.reset(new SMESH_ComputeError(COMPERR_ALGO_FAILED,"Submesh can not be evaluated",this));
    return false;
  }

  if (myQuadranglePreference) {
    int n1 = aNbNodes[0];
    int n2 = aNbNodes[1];
    int n3 = aNbNodes[2];
    int n4 = aNbNodes[3];
    int nfull = n1+n2+n3+n4;
    int ntmp = nfull/2;
    ntmp = ntmp*2;
    if (nfull==ntmp && ((n1!=n3) || (n2!=n4))) {
      // special path for using only quandrangle faces
      return evaluateQuadPref(aMesh, aFace, aNbNodes, aResMap, IsQuadratic);
      //return true;
    }
  }

  int nbdown  = aNbNodes[0];
  int nbup    = aNbNodes[2];

  int nbright = aNbNodes[1];
  int nbleft  = aNbNodes[3];

  int nbhoriz  = Min(nbdown, nbup);
  int nbvertic = Min(nbright, nbleft);

  int dh = Max(nbdown, nbup) - nbhoriz;
  int dv = Max(nbright, nbleft) - nbvertic;

  //int kdh = 0;
  //if (dh>0) kdh = 1;
  //int kdv = 0;
  //if (dv>0) kdv = 1;

  int nbNodes = (nbhoriz-2)*(nbvertic-2);
  //int nbFaces3 = dh + dv + kdh*(nbvertic-1)*2 + kdv*(nbhoriz-1)*2;
  int nbFaces3 = dh + dv;
  //if (kdh==1 && kdv==1) nbFaces3 -= 2;
  //if (dh>0 && dv>0) nbFaces3 -= 2;
  //int nbFaces4 = (nbhoriz-1-kdh)*(nbvertic-1-kdv);
  int nbFaces4 = (nbhoriz-1)*(nbvertic-1);

  std::vector<int> aVec(SMDSEntity_Last);
  for (int i=SMDSEntity_Node; i<SMDSEntity_Last; i++) aVec[i] = 0;
  if (IsQuadratic) {
    aVec[SMDSEntity_Quad_Triangle] = nbFaces3;
    aVec[SMDSEntity_Quad_Quadrangle] = nbFaces4;
    int nbbndedges = nbdown + nbup + nbright + nbleft -4;
    int nbintedges = (nbFaces4*4 + nbFaces3*3 - nbbndedges) / 2;
    aVec[SMDSEntity_Node] = nbNodes + nbintedges;
    if (aNbNodes.size()==5) {
      aVec[SMDSEntity_Quad_Triangle] = nbFaces3 + aNbNodes[3] -1;
      aVec[SMDSEntity_Quad_Quadrangle] = nbFaces4 - aNbNodes[3] +1;
    }
  }
  else {
    aVec[SMDSEntity_Node] = nbNodes;
    aVec[SMDSEntity_Triangle] = nbFaces3;
    aVec[SMDSEntity_Quadrangle] = nbFaces4;
    if (aNbNodes.size()==5) {
      aVec[SMDSEntity_Triangle] = nbFaces3 + aNbNodes[3] - 1;
      aVec[SMDSEntity_Quadrangle] = nbFaces4 - aNbNodes[3] + 1;
    }
  }
  SMESH_subMesh * sm = aMesh.GetSubMesh(aFace);
  aResMap.insert(std::make_pair(sm,aVec));

  return true;
}


//================================================================================
/*!
 * \brief Return true if only two given edges meat at their common vertex
 */
//================================================================================

static bool twoEdgesMeatAtVertex(const TopoDS_Edge& e1,
                                 const TopoDS_Edge& e2,
                                 SMESH_Mesh &       mesh)
{
  TopoDS_Vertex v;
  if (!TopExp::CommonVertex(e1, e2, v))
    return false;
  TopTools_ListIteratorOfListOfShape ancestIt(mesh.GetAncestors(v));
  for (; ancestIt.More() ; ancestIt.Next())
    if (ancestIt.Value().ShapeType() == TopAbs_EDGE)
      if (!e1.IsSame(ancestIt.Value()) && !e2.IsSame(ancestIt.Value()))
        return false;
  return true;
}

//=============================================================================
/*!
 *  
 */
//=============================================================================

FaceQuadStruct::Ptr StdMeshers_Quadrangle_2D::CheckNbEdges(SMESH_Mesh &         aMesh,
                                                           const TopoDS_Shape & aShape,
                                                           const bool           considerMesh)
{
  if ( myQuadStruct && myQuadStruct->face.IsSame( aShape ))
    return myQuadStruct;

  TopoDS_Face F = TopoDS::Face(aShape);
  if ( F.Orientation() >= TopAbs_INTERNAL ) F.Orientation( TopAbs_FORWARD );
  const bool ignoreMediumNodes = _quadraticMesh;

  // verify 1 wire only, with 4 edges
  list< TopoDS_Edge > edges;
  list< int > nbEdgesInWire;
  int nbWire = SMESH_Block::GetOrderedEdges (F, edges, nbEdgesInWire);
  if (nbWire != 1) {
    error(COMPERR_BAD_SHAPE, TComm("Wrong number of wires: ") << nbWire);
    return FaceQuadStruct::Ptr();
  }

  // find corner vertices of the quad
  vector<TopoDS_Vertex> corners;
  int nbDegenEdges, nbSides = getCorners( F, aMesh, edges, corners, nbDegenEdges, considerMesh );
  if ( nbSides == 0 )
  {
    return FaceQuadStruct::Ptr();
  }
  FaceQuadStruct::Ptr quad( new FaceQuadStruct );
  quad->uv_grid = 0;
  quad->side.reserve(nbEdgesInWire.front());
  quad->face = F;

  list< TopoDS_Edge >::iterator edgeIt = edges.begin();
  if ( nbSides == 3 ) // 3 sides and corners[0] is a vertex with myTriaVertexID
  {
    for ( int iSide = 0; iSide < 3; ++iSide )
    {
      list< TopoDS_Edge > sideEdges;
      TopoDS_Vertex nextSideV = corners[( iSide + 1 ) % 3 ];
      while ( edgeIt != edges.end() &&
              !nextSideV.IsSame( SMESH_MesherHelper::IthVertex( 0, *edgeIt )))
        if ( SMESH_Algo::isDegenerated( *edgeIt ))
          ++edgeIt;
        else
          sideEdges.push_back( *edgeIt++ );
      if ( !sideEdges.empty() )
        quad->side.push_back(new StdMeshers_FaceSide(F, sideEdges, &aMesh, iSide < QUAD_TOP_SIDE,
                                                     ignoreMediumNodes, myProxyMesh));
      else
        --iSide;
    }
    const vector<UVPtStruct>& UVPSleft  = quad->side[0]->GetUVPtStruct(true,0);
    /*  vector<UVPtStruct>& UVPStop   = */quad->side[1]->GetUVPtStruct(false,1);
    /*  vector<UVPtStruct>& UVPSright = */quad->side[2]->GetUVPtStruct(true,1);
    const SMDS_MeshNode* aNode = UVPSleft[0].node;
    gp_Pnt2d aPnt2d(UVPSleft[0].u, UVPSleft[0].v);
    quad->side.push_back(new StdMeshers_FaceSide(quad->side[1], aNode, &aPnt2d));
    myNeedSmooth = ( nbDegenEdges > 0 );
    return quad;
  }
  else // 4 sides
  {
    myNeedSmooth = ( corners.size() == 4 && nbDegenEdges > 0 );
    int iSide = 0, nbUsedDegen = 0, nbLoops = 0;
    for ( ; edgeIt != edges.end(); ++nbLoops )
    {
      list< TopoDS_Edge > sideEdges;
      TopoDS_Vertex nextSideV = corners[( iSide + 1 - nbUsedDegen ) % corners.size() ];
      while ( edgeIt != edges.end() &&
              !nextSideV.IsSame( myHelper->IthVertex( 0, *edgeIt )))
      {
        if ( SMESH_Algo::isDegenerated( *edgeIt ) )
        {
          if ( myNeedSmooth )
          {
            ++edgeIt; // no side on the degenerated EDGE
          }
          else
          {
            if ( sideEdges.empty() )
            {
              ++nbUsedDegen;
              sideEdges.push_back( *edgeIt++ ); // a degenerated side
              break;
            }
            else
            {
              break; // do not append a degenerated EDGE to a regular side
            }
          }
        }
        else
        {
          sideEdges.push_back( *edgeIt++ );
        }
      }
      if ( !sideEdges.empty() )
      {
        quad->side.push_back(new StdMeshers_FaceSide(F, sideEdges, &aMesh, iSide < QUAD_TOP_SIDE,
                                                     ignoreMediumNodes, myProxyMesh));
        ++iSide;
      }
      else if ( !SMESH_Algo::isDegenerated( *edgeIt ) && // closed EDGE
                myHelper->IthVertex( 0, *edgeIt ).IsSame( myHelper->IthVertex( 1, *edgeIt )))
      {
        quad->side.push_back(new StdMeshers_FaceSide(F, *edgeIt++, &aMesh, iSide < QUAD_TOP_SIDE,
                                                     ignoreMediumNodes, myProxyMesh));
        ++iSide;
      }
      if ( nbLoops > 8 )
      {
        error(TComm("Bug: infinite loop in StdMeshers_Quadrangle_2D::CheckNbEdges()"));
        quad.reset();
        break;
      }
    }
    if ( quad && quad->side.size() != 4 )
    {
      error(TComm("Bug: ") << quad->side.size()  << " sides found instead of 4");
      quad.reset();
    }
  }

  return quad;
}


//=============================================================================
/*!
 *  
 */
//=============================================================================

bool StdMeshers_Quadrangle_2D::checkNbEdgesForEvaluate(SMESH_Mesh&          aMesh,
                                                       const TopoDS_Shape & aShape,
                                                       MapShapeNbElems&     aResMap,
                                                       std::vector<int>&    aNbNodes,
                                                       bool&                IsQuadratic)

{
  const TopoDS_Face & F = TopoDS::Face(aShape);

  // verify 1 wire only, with 4 edges
  list< TopoDS_Edge > edges;
  list< int > nbEdgesInWire;
  int nbWire = SMESH_Block::GetOrderedEdges (F, edges, nbEdgesInWire);
  if (nbWire != 1) {
    return false;
  }

  aNbNodes.resize(4);

  int nbSides = 0;
  list< TopoDS_Edge >::iterator edgeIt = edges.begin();
  SMESH_subMesh * sm = aMesh.GetSubMesh(*edgeIt);
  MapShapeNbElemsItr anIt = aResMap.find(sm);
  if (anIt==aResMap.end()) {
    return false;
  }
  std::vector<int> aVec = (*anIt).second;
  IsQuadratic = (aVec[SMDSEntity_Quad_Edge] > aVec[SMDSEntity_Edge]);
  if (nbEdgesInWire.front() == 3) { // exactly 3 edges
    if (myTriaVertexID>0) {
      SMESHDS_Mesh* meshDS = aMesh.GetMeshDS();
      TopoDS_Vertex V = TopoDS::Vertex(meshDS->IndexToShape(myTriaVertexID));
      if (!V.IsNull()) {
        TopoDS_Edge E1,E2,E3;
        for (; edgeIt != edges.end(); ++edgeIt) {
          TopoDS_Edge E =  TopoDS::Edge(*edgeIt);
          TopoDS_Vertex VF, VL;
          TopExp::Vertices(E, VF, VL, true);
          if (VF.IsSame(V))
            E1 = E;
          else if (VL.IsSame(V))
            E3 = E;
          else
            E2 = E;
        }
        SMESH_subMesh * sm = aMesh.GetSubMesh(E1);
        MapShapeNbElemsItr anIt = aResMap.find(sm);
        if (anIt==aResMap.end()) return false;
        std::vector<int> aVec = (*anIt).second;
        if (IsQuadratic)
          aNbNodes[0] = (aVec[SMDSEntity_Node]-1)/2 + 2;
        else
          aNbNodes[0] = aVec[SMDSEntity_Node] + 2;
        sm = aMesh.GetSubMesh(E2);
        anIt = aResMap.find(sm);
        if (anIt==aResMap.end()) return false;
        aVec = (*anIt).second;
        if (IsQuadratic)
          aNbNodes[1] = (aVec[SMDSEntity_Node]-1)/2 + 2;
        else
          aNbNodes[1] = aVec[SMDSEntity_Node] + 2;
        sm = aMesh.GetSubMesh(E3);
        anIt = aResMap.find(sm);
        if (anIt==aResMap.end()) return false;
        aVec = (*anIt).second;
        if (IsQuadratic)
          aNbNodes[2] = (aVec[SMDSEntity_Node]-1)/2 + 2;
        else
          aNbNodes[2] = aVec[SMDSEntity_Node] + 2;
        aNbNodes[3] = aNbNodes[1];
        aNbNodes.resize(5);
        nbSides = 4;
      }
    }
  }
  if (nbEdgesInWire.front() == 4) { // exactly 4 edges
    for (; edgeIt != edges.end(); edgeIt++) {
      SMESH_subMesh * sm = aMesh.GetSubMesh(*edgeIt);
      MapShapeNbElemsItr anIt = aResMap.find(sm);
      if (anIt==aResMap.end()) {
        return false;
      }
      std::vector<int> aVec = (*anIt).second;
      if (IsQuadratic)
        aNbNodes[nbSides] = (aVec[SMDSEntity_Node]-1)/2 + 2;
      else
        aNbNodes[nbSides] = aVec[SMDSEntity_Node] + 2;
      nbSides++;
    }
  }
  else if (nbEdgesInWire.front() > 4) { // more than 4 edges - try to unite some
    list< TopoDS_Edge > sideEdges;
    while (!edges.empty()) {
      sideEdges.clear();
      sideEdges.splice(sideEdges.end(), edges, edges.begin()); // edges.front() -> sideEdges.end()
      bool sameSide = true;
      while (!edges.empty() && sameSide) {
        sameSide = SMESH_Algo::IsContinuous(sideEdges.back(), edges.front());
        if (sameSide)
          sideEdges.splice(sideEdges.end(), edges, edges.begin());
      }
      if (nbSides == 0) { // go backward from the first edge
        sameSide = true;
        while (!edges.empty() && sameSide) {
          sameSide = SMESH_Algo::IsContinuous(sideEdges.front(), edges.back());
          if (sameSide)
            sideEdges.splice(sideEdges.begin(), edges, --edges.end());
        }
      }
      list<TopoDS_Edge>::iterator ite = sideEdges.begin();
      aNbNodes[nbSides] = 1;
      for (; ite!=sideEdges.end(); ite++) {
        SMESH_subMesh * sm = aMesh.GetSubMesh(*ite);
        MapShapeNbElemsItr anIt = aResMap.find(sm);
        if (anIt==aResMap.end()) {
          return false;
        }
        std::vector<int> aVec = (*anIt).second;
        if (IsQuadratic)
          aNbNodes[nbSides] += (aVec[SMDSEntity_Node]-1)/2 + 1;
        else
          aNbNodes[nbSides] += aVec[SMDSEntity_Node] + 1;
      }
      ++nbSides;
    }
    // issue 20222. Try to unite only edges shared by two same faces
    if (nbSides < 4) {
      nbSides = 0;
      SMESH_Block::GetOrderedEdges (F, edges, nbEdgesInWire);
      while (!edges.empty()) {
        sideEdges.clear();
        sideEdges.splice(sideEdges.end(), edges, edges.begin());
        bool sameSide = true;
        while (!edges.empty() && sameSide) {
          sameSide =
            SMESH_Algo::IsContinuous(sideEdges.back(), edges.front()) &&
            twoEdgesMeatAtVertex(sideEdges.back(), edges.front(), aMesh);
          if (sameSide)
            sideEdges.splice(sideEdges.end(), edges, edges.begin());
        }
        if (nbSides == 0) { // go backward from the first edge
          sameSide = true;
          while (!edges.empty() && sameSide) {
            sameSide =
              SMESH_Algo::IsContinuous(sideEdges.front(), edges.back()) &&
              twoEdgesMeatAtVertex(sideEdges.front(), edges.back(), aMesh);
            if (sameSide)
              sideEdges.splice(sideEdges.begin(), edges, --edges.end());
          }
        }
        list<TopoDS_Edge>::iterator ite = sideEdges.begin();
        aNbNodes[nbSides] = 1;
        for (; ite!=sideEdges.end(); ite++) {
          SMESH_subMesh * sm = aMesh.GetSubMesh(*ite);
          MapShapeNbElemsItr anIt = aResMap.find(sm);
          if (anIt==aResMap.end()) {
            return false;
          }
          std::vector<int> aVec = (*anIt).second;
          if (IsQuadratic)
            aNbNodes[nbSides] += (aVec[SMDSEntity_Node]-1)/2 + 1;
          else
            aNbNodes[nbSides] += aVec[SMDSEntity_Node] + 1;
        }
        ++nbSides;
      }
    }
  }
  if (nbSides != 4) {
    if (!nbSides)
      nbSides = nbEdgesInWire.front();
    error(COMPERR_BAD_SHAPE, TComm("Face must have 4 sides but not ") << nbSides);
    return false;
  }

  return true;
}


//=============================================================================
/*!
 *  CheckAnd2Dcompute
 */
//=============================================================================

FaceQuadStruct::Ptr
StdMeshers_Quadrangle_2D::CheckAnd2Dcompute (SMESH_Mesh &         aMesh,
                                             const TopoDS_Shape & aShape,
                                             const bool           CreateQuadratic)
{
  _quadraticMesh = CreateQuadratic;

  FaceQuadStruct::Ptr quad = CheckNbEdges(aMesh, aShape);
  if ( quad )
  {
    // set normalized grid on unit square in parametric domain
    if ( ! setNormalizedGrid( aMesh, TopoDS::Face( aShape ), quad))
      quad.reset();
  }
  return quad;
}

//=============================================================================
/*!
 *
 */
//=============================================================================

faceQuadStruct::~faceQuadStruct()
{
  for (size_t i = 0; i < side.size(); i++) {
    if (side[i]) {
      delete side[i];
      for (size_t j = i+1; j < side.size(); j++)
        if ( side[i] == side[j] )
          side[j] = 0;
    }
  }
  side.clear();

  if (uv_grid) {
    delete [] uv_grid;
    uv_grid = 0;
  }
}

namespace
{
  inline const vector<UVPtStruct>& getUVPtStructIn(FaceQuadStruct::Ptr& quad, int i, int nbSeg)
  {
    bool   isXConst   = (i == QUAD_BOTTOM_SIDE || i == QUAD_TOP_SIDE);
    double constValue = (i == QUAD_BOTTOM_SIDE || i == QUAD_LEFT_SIDE) ? 0 : 1;
    return
      quad->isEdgeOut[i] ?
      quad->side[i]->SimulateUVPtStruct(nbSeg,isXConst,constValue) :
      quad->side[i]->GetUVPtStruct(isXConst,constValue);
  }
  inline gp_UV calcUV(double x, double y,
                      const gp_UV& a0,const gp_UV& a1,const gp_UV& a2,const gp_UV& a3,
                      const gp_UV& p0,const gp_UV& p1,const gp_UV& p2,const gp_UV& p3)
  {
    return
      ((1 - y) * p0 + x * p1 + y * p2 + (1 - x) * p3 ) -
      ((1 - x) * (1 - y) * a0 + x * (1 - y) * a1 + x * y * a2 + (1 - x) * y * a3);
  }
}

//=============================================================================
/*!
 *  
 */
//=============================================================================

bool StdMeshers_Quadrangle_2D::setNormalizedGrid (SMESH_Mesh &          aMesh,
                                                  const TopoDS_Face&    aFace,
                                                  FaceQuadStruct::Ptr & quad)
{
  // Algorithme décrit dans "Génération automatique de maillages"
  // P.L. GEORGE, MASSON, § 6.4.1 p. 84-85
  // traitement dans le domaine paramétrique 2d u,v
  // transport - projection sur le carré unité

  //      max             min                    0     x1     1
  //     |<----north-2-------^                a3 -------------> a2
  //     |                   |                   ^1          1^
  //    west-3            east-1 =right          |            |
  //     |                   |         ==>       |            |
  //  y0 |                   | y1                |            |
  //     |                   |                   |0          0|
  //     v----south-0-------->                a0 -------------> a1
  //      min             max                    0     x0     1
  //             =down
  //

  updateDegenUV( quad );

  int nbhoriz  = Min(quad->side[0]->NbPoints(), quad->side[2]->NbPoints());
  int nbvertic = Min(quad->side[1]->NbPoints(), quad->side[3]->NbPoints());

  quad->isEdgeOut[0] = (quad->side[0]->NbPoints() > quad->side[2]->NbPoints());
  quad->isEdgeOut[1] = (quad->side[1]->NbPoints() > quad->side[3]->NbPoints());
  quad->isEdgeOut[2] = (quad->side[2]->NbPoints() > quad->side[0]->NbPoints());
  quad->isEdgeOut[3] = (quad->side[3]->NbPoints() > quad->side[1]->NbPoints());

  UVPtStruct *uv_grid = quad->uv_grid = new UVPtStruct[nbvertic * nbhoriz];

  const vector<UVPtStruct>& uv_e0 = getUVPtStructIn(quad, 0, nbhoriz - 1);
  const vector<UVPtStruct>& uv_e1 = getUVPtStructIn(quad, 1, nbvertic - 1);
  const vector<UVPtStruct>& uv_e2 = getUVPtStructIn(quad, 2, nbhoriz - 1);
  const vector<UVPtStruct>& uv_e3 = getUVPtStructIn(quad, 3, nbvertic - 1);

  if (uv_e0.empty() || uv_e1.empty() || uv_e2.empty() || uv_e3.empty())
    //return error("Can't find nodes on sides");
    return error(COMPERR_BAD_INPUT_MESH);

  // copy data of face boundary
  {
    const int j = 0;
    for (int i = 0; i < nbhoriz; i++)       // down
      uv_grid[ j * nbhoriz + i ] = uv_e0[i];
  }
  {
    const int i = nbhoriz - 1;
    for (int j = 0; j < nbvertic; j++)      // right
      uv_grid[ j * nbhoriz + i ] = uv_e1[j];
  }
  {
    const int j = nbvertic - 1;
    for (int i = 0; i < nbhoriz; i++)       // up
      uv_grid[ j * nbhoriz + i ] = uv_e2[i];
  }
  {
    const int i = 0;
    for (int j = 0; j < nbvertic; j++)      // left
      uv_grid[ j * nbhoriz + i ] = uv_e3[j];
  }

  // normalized 2d parameters on grid

  for (int i = 0; i < nbhoriz; i++) {
    for (int j = 0; j < nbvertic; j++) {
      int ij = j * nbhoriz + i;
      // --- droite i cste : x = x0 + y(x1-x0)
      double x0 = uv_e0[i].normParam;   // bas  - sud
      double x1 = uv_e2[i].normParam;   // haut - nord
      // --- droite j cste : y = y0 + x(y1-y0)
      double y0 = uv_e3[j].normParam;   // gauche - ouest
      double y1 = uv_e1[j].normParam;   // droite - est
      // --- intersection : x=x0+(y0+x(y1-y0))(x1-x0)
      double x = (x0 + y0 * (x1 - x0)) / (1 - (y1 - y0) * (x1 - x0));
      double y = y0 + x * (y1 - y0);
      uv_grid[ij].x = x;
      uv_grid[ij].y = y;
    }
  }

  // projection on 2d domain (u,v)

  gp_UV a0 (uv_e0.front().u, uv_e0.front().v);
  gp_UV a1 (uv_e0.back().u,  uv_e0.back().v );
  gp_UV a2 (uv_e2.back().u,  uv_e2.back().v );
  gp_UV a3 (uv_e2.front().u, uv_e2.front().v);

  for (int i = 0; i < nbhoriz; i++)
  {
    gp_UV p0( uv_e0[i].u, uv_e0[i].v );
    gp_UV p2( uv_e2[i].u, uv_e2[i].v );
    for (int j = 0; j < nbvertic; j++)
    {
      gp_UV p1( uv_e1[j].u, uv_e1[j].v );
      gp_UV p3( uv_e3[j].u, uv_e3[j].v );

      int ij = j * nbhoriz + i;
      double x = uv_grid[ij].x;
      double y = uv_grid[ij].y;

      gp_UV uv = calcUV(x,y, a0,a1,a2,a3, p0,p1,p2,p3);

      uv_grid[ij].u = uv.X();
      uv_grid[ij].v = uv.Y();
    }
  }
  return true;
}

//=======================================================================
//function : ShiftQuad
//purpose  : auxilary function for computeQuadPref
//=======================================================================

static void shiftQuad(FaceQuadStruct::Ptr& quad, const int num)
{
  quad->shift( num, /*ori=*/true );
}

//================================================================================
/*!
 * \brief Rotate sides of a quad by nb
 *  \param nb  - number of rotation quartes
 *  \param ori - to keep orientation of sides as in an unit quad or not
 */
//================================================================================

void FaceQuadStruct::shift( size_t nb, bool ori )
{
  if ( nb == 0 ) return;
  StdMeshers_FaceSide* sideArr[4] = { side[0], side[1], side[2], side[3] };
  for (int i = QUAD_BOTTOM_SIDE; i < NB_QUAD_SIDES; ++i) {
    int id = (i + nb) % NB_QUAD_SIDES;
    bool wasForward = (i  < QUAD_TOP_SIDE);
    bool newForward = (id < QUAD_TOP_SIDE);
    if (ori && wasForward != newForward)
      sideArr[ i ]->Reverse();
    side[ id ] = sideArr[ i ];
  }
}

//=======================================================================
//function : calcUV
//purpose  : auxilary function for computeQuadPref
//=======================================================================

static gp_UV calcUV(double x0, double x1, double y0, double y1,
                    FaceQuadStruct::Ptr& quad,
                    const gp_UV& a0, const gp_UV& a1,
                    const gp_UV& a2, const gp_UV& a3)
{
  double x = (x0 + y0 * (x1 - x0)) / (1 - (y1 - y0) * (x1 - x0));
  double y = y0 + x * (y1 - y0);

  gp_UV p0 = quad->side[QUAD_BOTTOM_SIDE]->Value2d(x).XY();
  gp_UV p1 = quad->side[QUAD_RIGHT_SIDE ]->Value2d(y).XY();
  gp_UV p2 = quad->side[QUAD_TOP_SIDE   ]->Value2d(x).XY();
  gp_UV p3 = quad->side[QUAD_LEFT_SIDE  ]->Value2d(y).XY();

  gp_UV uv = calcUV(x,y, a0,a1,a2,a3, p0,p1,p2,p3);

  return uv;
}

//=======================================================================
//function : calcUV2
//purpose  : auxilary function for computeQuadPref
//=======================================================================

static gp_UV calcUV2(double x, double y,
                     FaceQuadStruct::Ptr& quad,
                     const gp_UV& a0, const gp_UV& a1,
                     const gp_UV& a2, const gp_UV& a3)
{
  gp_UV p0 = quad->side[QUAD_BOTTOM_SIDE]->Value2d(x).XY();
  gp_UV p1 = quad->side[QUAD_RIGHT_SIDE ]->Value2d(y).XY();
  gp_UV p2 = quad->side[QUAD_TOP_SIDE   ]->Value2d(x).XY();
  gp_UV p3 = quad->side[QUAD_LEFT_SIDE  ]->Value2d(y).XY();

  gp_UV uv = calcUV(x,y, a0,a1,a2,a3, p0,p1,p2,p3);

  return uv;
}


//=======================================================================
/*!
 * Create only quandrangle faces
 */
//=======================================================================

bool StdMeshers_Quadrangle_2D::computeQuadPref (SMESH_Mesh &        aMesh,
                                                const TopoDS_Face&  aFace,
                                                FaceQuadStruct::Ptr quad)
{
  // Auxilary key in order to keep old variant
  // of meshing after implementation new variant
  // for bug 0016220 from Mantis.
  bool OldVersion = (myQuadType == QUAD_QUADRANGLE_PREF_REVERSED);

  SMESHDS_Mesh *  meshDS = aMesh.GetMeshDS();
  Handle(Geom_Surface) S = BRep_Tool::Surface(aFace);
  bool WisF = true;
  int i,j,geomFaceID = meshDS->ShapeToIndex(aFace);

  updateDegenUV( quad );

  int nb = quad->side[0]->NbPoints();
  int nr = quad->side[1]->NbPoints();
  int nt = quad->side[2]->NbPoints();
  int nl = quad->side[3]->NbPoints();
  int dh = abs(nb-nt);
  int dv = abs(nr-nl);

  // rotate sides to be as in the picture below and to have
  // dh >= dv and nt > nb
  if ( dh >= dv )
    shiftQuad( quad, ( nt > nb ) ? 0 : 2 );
  else
    shiftQuad( quad, ( nr > nl ) ? 1 : 3 );

  nb = quad->side[0]->NbPoints();
  nr = quad->side[1]->NbPoints();
  nt = quad->side[2]->NbPoints();
  nl = quad->side[3]->NbPoints();
  dh = abs(nb-nt);
  dv = abs(nr-nl);
  int nbh  = Max(nb,nt);
  int nbv  = Max(nr,nl);
  int addh = 0;
  int addv = 0;

  // Orientation of face and 3 main domain for future faces
  // ----------- Old version ---------------
  //       0   top    1
  //      1------------1
  //       |   |  |   |
  //       |   |C |   |
  //       | L |  | R |
  //  left |   |__|   | rigth
  //       |  /    \  |
  //       | /  C   \ |
  //       |/        \|
  //      0------------0
  //       0  bottom  1

  // ----------- New version ---------------
  //       0   top    1
  //      1------------1
  //       |   |__|   |
  //       |  /    \  |
  //       | /  C   \ |
  //  left |/________\| rigth
  //       |          |
  //       |    C     |
  //       |          |
  //      0------------0
  //       0  bottom  1

  if ( dh > dv ) {
    addv = (dh-dv)/2;
    nbv  = nbv + addv;
  }
  else { // dv >= dh
    addh = (dv-dh)/2;
    nbh  = nbh + addh;
  }

  const vector<UVPtStruct>& uv_eb = quad->side[0]->GetUVPtStruct(true,0);
  const vector<UVPtStruct>& uv_er = quad->side[1]->GetUVPtStruct(false,1);
  const vector<UVPtStruct>& uv_et = quad->side[2]->GetUVPtStruct(true,1);
  const vector<UVPtStruct>& uv_el = quad->side[3]->GetUVPtStruct(false,0);

  if (uv_eb.size() != nb || uv_er.size() != nr || uv_et.size() != nt || uv_el.size() != nl)
    return error(COMPERR_BAD_INPUT_MESH);

  if ( !OldVersion )
  {
    // dh/2, Min(nb,nt), dh - dh/2, dv
  }

  // arrays for normalized params
  TColStd_SequenceOfReal npb, npr, npt, npl;
  for (i=0; i<nb; i++) {
    npb.Append(uv_eb[i].normParam);
  }
  for (i=0; i<nr; i++) {
    npr.Append(uv_er[i].normParam);
  }
  for (i=0; i<nt; i++) {
    npt.Append(uv_et[i].normParam);
  }
  for (i=0; i<nl; i++) {
    npl.Append(uv_el[i].normParam);
  }

  int dl,dr;
  if (OldVersion) {
    // add some params to right and left after the first param
    // insert to right
    dr = nbv - nr;
    double dpr = (npr.Value(2) - npr.Value(1))/(dr+1);
    for (i=1; i<=dr; i++) {
      npr.InsertAfter(1,npr.Value(2)-dpr);
    }
    // insert to left
    dl = nbv - nl;
    dpr = (npl.Value(2) - npl.Value(1))/(dl+1);
    for (i=1; i<=dl; i++) {
      npl.InsertAfter(1,npl.Value(2)-dpr);
    }
  }
  
  gp_XY a0(uv_eb.front().u, uv_eb.front().v);
  gp_XY a1(uv_eb.back().u,  uv_eb.back().v);
  gp_XY a2(uv_et.back().u,  uv_et.back().v);
  gp_XY a3(uv_et.front().u, uv_et.front().v);

  int nnn = Min(nr,nl);
  // auxilary sequence of XY for creation nodes
  // in the bottom part of central domain
  // Length of UVL and UVR must be == nbv-nnn
  TColgp_SequenceOfXY UVL, UVR, UVT;

  if (OldVersion) {
    // step1: create faces for left domain
    StdMeshers_Array2OfNode NodesL(1,dl+1,1,nl);
    // add left nodes
    for (j=1; j<=nl; j++)
      NodesL.SetValue(1,j,uv_el[j-1].node);
    if (dl>0) {
      // add top nodes
      for (i=1; i<=dl; i++) 
        NodesL.SetValue(i+1,nl,uv_et[i].node);
      // create and add needed nodes
      TColgp_SequenceOfXY UVtmp;
      for (i=1; i<=dl; i++) {
        double x0 = npt.Value(i+1);
        double x1 = x0;
        // diagonal node
        double y0 = npl.Value(i+1);
        double y1 = npr.Value(i+1);
        gp_UV UV = calcUV(x0, x1, y0, y1, quad, a0, a1, a2, a3);
        gp_Pnt P = S->Value(UV.X(),UV.Y());
        SMDS_MeshNode * N = meshDS->AddNode(P.X(), P.Y(), P.Z());
        meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
        NodesL.SetValue(i+1,1,N);
        if (UVL.Length()<nbv-nnn) UVL.Append(UV);
        // internal nodes
        for (j=2; j<nl; j++) {
          double y0 = npl.Value(dl+j);
          double y1 = npr.Value(dl+j);
          gp_UV UV = calcUV(x0, x1, y0, y1, quad, a0, a1, a2, a3);
          gp_Pnt P = S->Value(UV.X(),UV.Y());
          SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
          meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
          NodesL.SetValue(i+1,j,N);
          if (i==dl) UVtmp.Append(UV);
        }
      }
      for (i=1; i<=UVtmp.Length() && UVL.Length()<nbv-nnn; i++) {
        UVL.Append(UVtmp.Value(i));
      }
      // create faces
      for (i=1; i<=dl; i++) {
        for (j=1; j<nl; j++) {
          if (WisF) {
            SMDS_MeshFace* F =
              myHelper->AddFace(NodesL.Value(i,j), NodesL.Value(i+1,j),
                              NodesL.Value(i+1,j+1), NodesL.Value(i,j+1));
            if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
          }
          else {
            SMDS_MeshFace* F =
              myHelper->AddFace(NodesL.Value(i,j), NodesL.Value(i,j+1),
                              NodesL.Value(i+1,j+1), NodesL.Value(i+1,j));
            if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
          }
        }
      }
    }
    else {
      // fill UVL using c2d
      for (i=1; i<npl.Length() && UVL.Length()<nbv-nnn; i++) {
        UVL.Append(gp_UV (uv_el[i].u, uv_el[i].v));
      }
    }
    
    // step2: create faces for right domain
    StdMeshers_Array2OfNode NodesR(1,dr+1,1,nr);
    // add right nodes
    for (j=1; j<=nr; j++) 
      NodesR.SetValue(1,j,uv_er[nr-j].node);
    if (dr>0) {
      // add top nodes
      for (i=1; i<=dr; i++) 
        NodesR.SetValue(i+1,1,uv_et[nt-1-i].node);
      // create and add needed nodes
      TColgp_SequenceOfXY UVtmp;
      for (i=1; i<=dr; i++) {
        double x0 = npt.Value(nt-i);
        double x1 = x0;
        // diagonal node
        double y0 = npl.Value(i+1);
        double y1 = npr.Value(i+1);
        gp_UV UV = calcUV(x0, x1, y0, y1, quad, a0, a1, a2, a3);
        gp_Pnt P = S->Value(UV.X(),UV.Y());
        SMDS_MeshNode * N = meshDS->AddNode(P.X(), P.Y(), P.Z());
        meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
        NodesR.SetValue(i+1,nr,N);
        if (UVR.Length()<nbv-nnn) UVR.Append(UV);
        // internal nodes
        for (j=2; j<nr; j++) {
          double y0 = npl.Value(nbv-j+1);
          double y1 = npr.Value(nbv-j+1);
          gp_UV UV = calcUV(x0, x1, y0, y1, quad, a0, a1, a2, a3);
          gp_Pnt P = S->Value(UV.X(),UV.Y());
          SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
          meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
          NodesR.SetValue(i+1,j,N);
          if (i==dr) UVtmp.Prepend(UV);
        }
      }
      for (i=1; i<=UVtmp.Length() && UVR.Length()<nbv-nnn; i++) {
        UVR.Append(UVtmp.Value(i));
      }
      // create faces
      for (i=1; i<=dr; i++) {
        for (j=1; j<nr; j++) {
          if (WisF) {
            SMDS_MeshFace* F =
              myHelper->AddFace(NodesR.Value(i,j), NodesR.Value(i+1,j),
                              NodesR.Value(i+1,j+1), NodesR.Value(i,j+1));
            if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
          }
          else {
            SMDS_MeshFace* F =
              myHelper->AddFace(NodesR.Value(i,j), NodesR.Value(i,j+1),
                              NodesR.Value(i+1,j+1), NodesR.Value(i+1,j));
            if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
          }
        }
      }
    }
    else {
      // fill UVR using c2d
      for (i=1; i<npr.Length() && UVR.Length()<nbv-nnn; i++) {
        UVR.Append(gp_UV(uv_er[i].u, uv_er[i].v));
      }
    }
    
    // step3: create faces for central domain
    StdMeshers_Array2OfNode NodesC(1,nb,1,nbv);
    // add first line using NodesL
    for (i=1; i<=dl+1; i++)
      NodesC.SetValue(1,i,NodesL(i,1));
    for (i=2; i<=nl; i++)
      NodesC.SetValue(1,dl+i,NodesL(dl+1,i));
    // add last line using NodesR
    for (i=1; i<=dr+1; i++)
      NodesC.SetValue(nb,i,NodesR(i,nr));
    for (i=1; i<nr; i++)
      NodesC.SetValue(nb,dr+i+1,NodesR(dr+1,nr-i));
    // add top nodes (last columns)
    for (i=dl+2; i<nbh-dr; i++) 
      NodesC.SetValue(i-dl,nbv,uv_et[i-1].node);
    // add bottom nodes (first columns)
    for (i=2; i<nb; i++)
      NodesC.SetValue(i,1,uv_eb[i-1].node);
    
    // create and add needed nodes
    // add linear layers
    for (i=2; i<nb; i++) {
      double x0 = npt.Value(dl+i);
      double x1 = x0;
      for (j=1; j<nnn; j++) {
        double y0 = npl.Value(nbv-nnn+j);
        double y1 = npr.Value(nbv-nnn+j);
        gp_UV UV = calcUV(x0, x1, y0, y1, quad, a0, a1, a2, a3);
        gp_Pnt P = S->Value(UV.X(),UV.Y());
        SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
        meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
        NodesC.SetValue(i,nbv-nnn+j,N);
        if ( j==1 )
          UVT.Append( UV );
      }
    }
    // add diagonal layers
    gp_UV A2 = UVR.Value(nbv-nnn);
    gp_UV A3 = UVL.Value(nbv-nnn);
    for (i=1; i<nbv-nnn; i++) {
      gp_UV p1 = UVR.Value(i);
      gp_UV p3 = UVL.Value(i);
      double y = i / double(nbv-nnn);
      for (j=2; j<nb; j++) {
        double x = npb.Value(j);
        gp_UV p0( uv_eb[j-1].u, uv_eb[j-1].v );
        gp_UV p2 = UVT.Value( j-1 );
        gp_UV UV = calcUV(x, y, a0, a1, A2, A3, p0,p1,p2,p3 );
        gp_Pnt P = S->Value(UV.X(),UV.Y());
        SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
        meshDS->SetNodeOnFace(N, geomFaceID, UV.X(),UV.Y());
        NodesC.SetValue(j,i+1,N);
      }
    }
    // create faces
    for (i=1; i<nb; i++) {
      for (j=1; j<nbv; j++) {
        if (WisF) {
          SMDS_MeshFace* F =
            myHelper->AddFace(NodesC.Value(i,j), NodesC.Value(i+1,j),
                            NodesC.Value(i+1,j+1), NodesC.Value(i,j+1));
          if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
        }
        else {
          SMDS_MeshFace* F =
            myHelper->AddFace(NodesC.Value(i,j), NodesC.Value(i,j+1),
                            NodesC.Value(i+1,j+1), NodesC.Value(i+1,j));
          if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
        }
      }
    }
  }

  else { // New version (!OldVersion)
    // step1: create faces for bottom rectangle domain
    StdMeshers_Array2OfNode NodesBRD(1,nb,1,nnn-1);
    // fill UVL and UVR using c2d
    for (j=0; j<nb; j++) {
      NodesBRD.SetValue(j+1,1,uv_eb[j].node);
    }
    for (i=1; i<nnn-1; i++) {
      NodesBRD.SetValue(1,i+1,uv_el[i].node);
      NodesBRD.SetValue(nb,i+1,uv_er[i].node);
      for (j=2; j<nb; j++) {
        double x = npb.Value(j);
        double y = (1-x) * npl.Value(i+1) + x * npr.Value(i+1);
        gp_UV UV = calcUV2(x, y, quad, a0, a1, a2, a3);
        gp_Pnt P = S->Value(UV.X(),UV.Y());
        SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
        meshDS->SetNodeOnFace(N, geomFaceID, UV.X(),UV.Y());
        NodesBRD.SetValue(j,i+1,N);
      }
    }
    for (j=1; j<nnn-1; j++) {
      for (i=1; i<nb; i++) {
        if (WisF) {
          SMDS_MeshFace* F =
            myHelper->AddFace(NodesBRD.Value(i,j), NodesBRD.Value(i+1,j),
                            NodesBRD.Value(i+1,j+1), NodesBRD.Value(i,j+1));
          if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
        }
        else {
          SMDS_MeshFace* F =
            myHelper->AddFace(NodesBRD.Value(i,j), NodesBRD.Value(i,j+1),
                            NodesBRD.Value(i+1,j+1), NodesBRD.Value(i+1,j));
          if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
        }
      }
    }
    int drl = abs(nr-nl);
    // create faces for region C
    StdMeshers_Array2OfNode NodesC(1,nb,1,drl+1+addv);
    // add nodes from previous region
    for (j=1; j<=nb; j++) {
      NodesC.SetValue(j,1,NodesBRD.Value(j,nnn-1));
    }
    if ((drl+addv) > 0) {
      int n1,n2;
      if (nr>nl) {
        n1 = 1;
        n2 = drl + 1;
        TColgp_SequenceOfXY UVtmp;
        double drparam = npr.Value(nr) - npr.Value(nnn-1);
        double dlparam = npl.Value(nnn) - npl.Value(nnn-1);
        double y0,y1;
        for (i=1; i<=drl; i++) {
          // add existed nodes from right edge
          NodesC.SetValue(nb,i+1,uv_er[nnn+i-2].node);
          //double dtparam = npt.Value(i+1);
          y1 = npr.Value(nnn+i-1); // param on right edge
          double dpar = (y1 - npr.Value(nnn-1))/drparam;
          y0 = npl.Value(nnn-1) + dpar*dlparam; // param on left edge
          double dy = y1 - y0;
          for (j=1; j<nb; j++) {
            double x = npt.Value(i+1) + npb.Value(j)*(1-npt.Value(i+1));
            double y = y0 + dy*x;
            gp_UV UV = calcUV2(x, y, quad, a0, a1, a2, a3);
            gp_Pnt P = S->Value(UV.X(),UV.Y());
            SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
            meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
            NodesC.SetValue(j,i+1,N);
          }
        }
        double dy0 = (1-y0)/(addv+1);
        double dy1 = (1-y1)/(addv+1);
        for (i=1; i<=addv; i++) {
          double yy0 = y0 + dy0*i;
          double yy1 = y1 + dy1*i;
          double dyy = yy1 - yy0;
          for (j=1; j<=nb; j++) {
            double x = npt.Value(i+1+drl) + 
              npb.Value(j) * (npt.Value(nt-i) - npt.Value(i+1+drl));
            double y = yy0 + dyy*x;
            gp_UV UV = calcUV2(x, y, quad, a0, a1, a2, a3);
            gp_Pnt P = S->Value(UV.X(),UV.Y());
            SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
            meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
            NodesC.SetValue(j,i+drl+1,N);
          }
        }
      }
      else { // nr<nl
        n2 = 1;
        n1 = drl + 1;
        TColgp_SequenceOfXY UVtmp;
        double dlparam = npl.Value(nl) - npl.Value(nnn-1);
        double drparam = npr.Value(nnn) - npr.Value(nnn-1);
        double y0 = npl.Value(nnn-1);
        double y1 = npr.Value(nnn-1);
        for (i=1; i<=drl; i++) {
          // add existed nodes from right edge
          NodesC.SetValue(1,i+1,uv_el[nnn+i-2].node);
          y0 = npl.Value(nnn+i-1); // param on left edge
          double dpar = (y0 - npl.Value(nnn-1))/dlparam;
          y1 = npr.Value(nnn-1) + dpar*drparam; // param on right edge
          double dy = y1 - y0;
          for (j=2; j<=nb; j++) {
            double x = npb.Value(j)*npt.Value(nt-i);
            double y = y0 + dy*x;
            gp_UV UV = calcUV2(x, y, quad, a0, a1, a2, a3);
            gp_Pnt P = S->Value(UV.X(),UV.Y());
            SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
            meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
            NodesC.SetValue(j,i+1,N);
          }
        }
        double dy0 = (1-y0)/(addv+1);
        double dy1 = (1-y1)/(addv+1);
        for (i=1; i<=addv; i++) {
          double yy0 = y0 + dy0*i;
          double yy1 = y1 + dy1*i;
          double dyy = yy1 - yy0;
          for (j=1; j<=nb; j++) {
            double x = npt.Value(i+1) + 
              npb.Value(j) * (npt.Value(nt-i-drl) - npt.Value(i+1));
            double y = yy0 + dyy*x;
            gp_UV UV = calcUV2(x, y, quad, a0, a1, a2, a3);
            gp_Pnt P = S->Value(UV.X(),UV.Y());
            SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
            meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
            NodesC.SetValue(j,i+drl+1,N);
          }
        }
      }
      // create faces
      for (j=1; j<=drl+addv; j++) {
        for (i=1; i<nb; i++) {
          if (WisF) {
            SMDS_MeshFace* F =
              myHelper->AddFace(NodesC.Value(i,j), NodesC.Value(i+1,j),
                              NodesC.Value(i+1,j+1), NodesC.Value(i,j+1));
            if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
          }
          else {
            SMDS_MeshFace* F =
              myHelper->AddFace(NodesC.Value(i,j), NodesC.Value(i,j+1),
                              NodesC.Value(i+1,j+1), NodesC.Value(i+1,j));
            if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
          }
        }
      } // end nr<nl

      StdMeshers_Array2OfNode NodesLast(1,nt,1,2);
      for (i=1; i<=nt; i++) {
        NodesLast.SetValue(i,2,uv_et[i-1].node);
      }
      int nnn=0;
      for (i=n1; i<drl+addv+1; i++) {
        nnn++;
        NodesLast.SetValue(nnn,1,NodesC.Value(1,i));
      }
      for (i=1; i<=nb; i++) {
        nnn++;
        NodesLast.SetValue(nnn,1,NodesC.Value(i,drl+addv+1));
      }
      for (i=drl+addv; i>=n2; i--) {
        nnn++;
        NodesLast.SetValue(nnn,1,NodesC.Value(nb,i));
      }
      for (i=1; i<nt; i++) {
        if (WisF) {
          SMDS_MeshFace* F =
            myHelper->AddFace(NodesLast.Value(i,1), NodesLast.Value(i+1,1),
                            NodesLast.Value(i+1,2), NodesLast.Value(i,2));
          if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
        }
        else {
          SMDS_MeshFace* F =
            myHelper->AddFace(NodesLast.Value(i,1), NodesLast.Value(i,2),
                            NodesLast.Value(i+1,2), NodesLast.Value(i+1,2));
          if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
        }
      }
    } // if ((drl+addv) > 0)

  } // end new version implementation

  bool isOk = true;
  return isOk;
}


//=======================================================================
/*!
 * Evaluate only quandrangle faces
 */
//=======================================================================

bool StdMeshers_Quadrangle_2D::evaluateQuadPref(SMESH_Mesh &        aMesh,
                                                const TopoDS_Shape& aShape,
                                                std::vector<int>&   aNbNodes,
                                                MapShapeNbElems&    aResMap,
                                                bool                IsQuadratic)
{
  // Auxilary key in order to keep old variant
  // of meshing after implementation new variant
  // for bug 0016220 from Mantis.
  bool OldVersion = false;
  if (myQuadType == QUAD_QUADRANGLE_PREF_REVERSED)
    OldVersion = true;

  const TopoDS_Face& F = TopoDS::Face(aShape);
  Handle(Geom_Surface) S = BRep_Tool::Surface(F);

  int nb = aNbNodes[0];
  int nr = aNbNodes[1];
  int nt = aNbNodes[2];
  int nl = aNbNodes[3];
  int dh = abs(nb-nt);
  int dv = abs(nr-nl);

  if (dh>=dv) {
    if (nt>nb) {
      // it is a base case => not shift 
    }
    else {
      // we have to shift on 2
      nb = aNbNodes[2];
      nr = aNbNodes[3];
      nt = aNbNodes[0];
      nl = aNbNodes[1];
    }
  }
  else {
    if (nr>nl) {
      // we have to shift quad on 1
      nb = aNbNodes[3];
      nr = aNbNodes[0];
      nt = aNbNodes[1];
      nl = aNbNodes[2];
    }
    else {
      // we have to shift quad on 3
      nb = aNbNodes[1];
      nr = aNbNodes[2];
      nt = aNbNodes[3];
      nl = aNbNodes[0];
    }
  }

  dh = abs(nb-nt);
  dv = abs(nr-nl);
  int nbh  = Max(nb,nt);
  int nbv = Max(nr,nl);
  int addh = 0;
  int addv = 0;

  if (dh>dv) {
    addv = (dh-dv)/2;
    nbv = nbv + addv;
  }
  else { // dv>=dh
    addh = (dv-dh)/2;
    nbh = nbh + addh;
  }

  int dl,dr;
  if (OldVersion) {
    // add some params to right and left after the first param
    // insert to right
    dr = nbv - nr;
    // insert to left
    dl = nbv - nl;
  }
  
  int nnn = Min(nr,nl);

  int nbNodes = 0;
  int nbFaces = 0;
  if (OldVersion) {
    // step1: create faces for left domain
    if (dl>0) {
      nbNodes += dl*(nl-1);
      nbFaces += dl*(nl-1);
    }
    // step2: create faces for right domain
    if (dr>0) {
      nbNodes += dr*(nr-1);
      nbFaces += dr*(nr-1);
    }
    // step3: create faces for central domain
    nbNodes += (nb-2)*(nnn-1) + (nbv-nnn-1)*(nb-2);
    nbFaces += (nb-1)*(nbv-1);
  }
  else { // New version (!OldVersion)
    nbNodes += (nnn-2)*(nb-2);
    nbFaces += (nnn-2)*(nb-1);
    int drl = abs(nr-nl);
    nbNodes += drl*(nb-1) + addv*nb;
    nbFaces += (drl+addv)*(nb-1) + (nt-1);
  } // end new version implementation

  std::vector<int> aVec(SMDSEntity_Last);
  for (int i=SMDSEntity_Node; i<SMDSEntity_Last; i++) aVec[i] = 0;
  if (IsQuadratic) {
    aVec[SMDSEntity_Quad_Quadrangle] = nbFaces;
    aVec[SMDSEntity_Node] = nbNodes + nbFaces*4;
    if (aNbNodes.size()==5) {
      aVec[SMDSEntity_Quad_Triangle] = aNbNodes[3] - 1;
      aVec[SMDSEntity_Quad_Quadrangle] = nbFaces - aNbNodes[3] + 1;
    }
  }
  else {
    aVec[SMDSEntity_Node] = nbNodes;
    aVec[SMDSEntity_Quadrangle] = nbFaces;
    if (aNbNodes.size()==5) {
      aVec[SMDSEntity_Triangle] = aNbNodes[3] - 1;
      aVec[SMDSEntity_Quadrangle] = nbFaces - aNbNodes[3] + 1;
    }
  }
  SMESH_subMesh * sm = aMesh.GetSubMesh(aShape);
  aResMap.insert(std::make_pair(sm,aVec));

  return true;
}

//=============================================================================
/*! Split quadrangle in to 2 triangles by smallest diagonal
 *   
 */
//=============================================================================

void StdMeshers_Quadrangle_2D::splitQuad(SMESHDS_Mesh *       theMeshDS,
                                         int                  theFaceID,
                                         const SMDS_MeshNode* theNode1,
                                         const SMDS_MeshNode* theNode2,
                                         const SMDS_MeshNode* theNode3,
                                         const SMDS_MeshNode* theNode4)
{
  SMDS_MeshFace* face;
  if ( SMESH_TNodeXYZ( theNode1 ).SquareDistance( theNode3 ) >
       SMESH_TNodeXYZ( theNode2 ).SquareDistance( theNode4 ) )
  {
    face = myHelper->AddFace(theNode2, theNode4 , theNode1);
    if (face) theMeshDS->SetMeshElementOnShape(face, theFaceID);
    face = myHelper->AddFace(theNode2, theNode3, theNode4);
    if (face) theMeshDS->SetMeshElementOnShape(face, theFaceID);
  }
  else
  {
    face = myHelper->AddFace(theNode1, theNode2 ,theNode3);
    if (face) theMeshDS->SetMeshElementOnShape(face, theFaceID);
    face = myHelper->AddFace(theNode1, theNode3, theNode4);
    if (face) theMeshDS->SetMeshElementOnShape(face, theFaceID);
  }
}

namespace
{
  enum uvPos { UV_A0, UV_A1, UV_A2, UV_A3, UV_B, UV_R, UV_T, UV_L, UV_SIZE };

  inline  SMDS_MeshNode* makeNode( UVPtStruct &         uvPt,
                                   const double         y,
                                   FaceQuadStruct::Ptr& quad,
                                   const gp_UV*         UVs,
                                   SMESH_MesherHelper*  helper,
                                   Handle(Geom_Surface) S)
  {
    const vector<UVPtStruct>& uv_eb = quad->side[QUAD_BOTTOM_SIDE]->GetUVPtStruct();
    const vector<UVPtStruct>& uv_et = quad->side[QUAD_TOP_SIDE   ]->GetUVPtStruct();
    double rBot = ( uv_eb.size() - 1 ) * uvPt.normParam;
    double rTop = ( uv_et.size() - 1 ) * uvPt.normParam;
    int iBot = int( rBot );
    int iTop = int( rTop );
    double xBot = uv_eb[ iBot ].normParam + ( rBot - iBot ) * ( uv_eb[ iBot+1 ].normParam - uv_eb[ iBot ].normParam );
    double xTop = uv_et[ iTop ].normParam + ( rTop - iTop ) * ( uv_et[ iTop+1 ].normParam - uv_et[ iTop ].normParam );
    double x = xBot + y * ( xTop - xBot );
    
    gp_UV uv = calcUV(/*x,y=*/x, y,
                      /*a0,...=*/UVs[UV_A0], UVs[UV_A1], UVs[UV_A2], UVs[UV_A3],
                      /*p0=*/quad->side[QUAD_BOTTOM_SIDE]->Value2d( x ).XY(),
                      /*p1=*/UVs[ UV_R ],
                      /*p2=*/quad->side[QUAD_TOP_SIDE   ]->Value2d( x ).XY(),
                      /*p3=*/UVs[ UV_L ]);
    gp_Pnt P = S->Value( uv.X(), uv.Y() );
    uvPt.u = uv.X();
    uvPt.v = uv.Y();
    return helper->AddNode(P.X(), P.Y(), P.Z(), 0, uv.X(), uv.Y() );
  }

  void reduce42( const vector<UVPtStruct>& curr_base,
                 vector<UVPtStruct>&       next_base,
                 const int                 j,
                 int &                     next_base_len,
                 FaceQuadStruct::Ptr&      quad,
                 gp_UV*                    UVs,
                 const double              y,
                 SMESH_MesherHelper*       helper,
                 Handle(Geom_Surface)&     S)
  {
    // add one "HH": nodes a,b,c,d,e and faces 1,2,3,4,5,6
    //
    //  .-----a-----b i + 1
    //  |\ 5  | 6  /|
    //  | \   |   / |
    //  |  c--d--e  |
    //  |1 |2 |3 |4 |
    //  |  |  |  |  |
    //  .--.--.--.--. i
    //
    //  j     j+2   j+4

    // a (i + 1, j + 2)
    const SMDS_MeshNode*& Na = next_base[ ++next_base_len ].node;
    if ( !Na )
      Na = makeNode( next_base[ next_base_len ], y, quad, UVs, helper, S );

    // b (i + 1, j + 4)
    const SMDS_MeshNode*& Nb = next_base[ ++next_base_len ].node;
    if ( !Nb )
      Nb = makeNode( next_base[ next_base_len ], y, quad, UVs, helper, S );

    // c
    double u = (curr_base[j + 2].u + next_base[next_base_len - 2].u) / 2.0;
    double v = (curr_base[j + 2].v + next_base[next_base_len - 2].v) / 2.0;
    gp_Pnt P = S->Value(u,v);
    SMDS_MeshNode* Nc = helper->AddNode(P.X(), P.Y(), P.Z(), 0, u, v);

    // d
    u = (curr_base[j + 2].u + next_base[next_base_len - 1].u) / 2.0;
    v = (curr_base[j + 2].v + next_base[next_base_len - 1].v) / 2.0;
    P = S->Value(u,v);
    SMDS_MeshNode* Nd = helper->AddNode(P.X(), P.Y(), P.Z(), 0, u, v);

    // e
    u = (curr_base[j + 2].u + next_base[next_base_len].u) / 2.0;
    v = (curr_base[j + 2].v + next_base[next_base_len].v) / 2.0;
    P = S->Value(u,v);
    SMDS_MeshNode* Ne = helper->AddNode(P.X(), P.Y(), P.Z(), 0, u, v);

    // Faces
    helper->AddFace(curr_base[j + 0].node,
                    curr_base[j + 1].node, Nc,
                    next_base[next_base_len - 2].node);

    helper->AddFace(curr_base[j + 1].node,
                    curr_base[j + 2].node, Nd, Nc);

    helper->AddFace(curr_base[j + 2].node,
                    curr_base[j + 3].node, Ne, Nd);

    helper->AddFace(curr_base[j + 3].node,
                    curr_base[j + 4].node, Nb, Ne);

    helper->AddFace(Nc, Nd, Na, next_base[next_base_len - 2].node);

    helper->AddFace(Nd, Ne, Nb, Na);
  }

  void reduce31( const vector<UVPtStruct>& curr_base,
                 vector<UVPtStruct>&       next_base,
                 const int                 j,
                 int &                     next_base_len,
                 FaceQuadStruct::Ptr&      quad,
                 gp_UV*                    UVs,
                 const double              y,
                 SMESH_MesherHelper*       helper,
                 Handle(Geom_Surface)&     S)
  {
    // add one "H": nodes b,c,e and faces 1,2,4,5
    //
    //  .---------b i + 1
    //  |\   5   /|
    //  | \     / |
    //  |  c---e  |
    //  |1 |2  |4 |
    //  |  |   |  |
    //  .--.---.--. i
    //
    //  j j+1 j+2 j+3

    // b (i + 1, j + 3)
    const SMDS_MeshNode*& Nb = next_base[ ++next_base_len ].node;
    if ( !Nb )
      Nb = makeNode( next_base[ next_base_len ], y, quad, UVs, helper, S );

    // c and e
    double u1 = (curr_base[ j   ].u + next_base[ next_base_len-1 ].u ) / 2.0;
    double u2 = (curr_base[ j+3 ].u + next_base[ next_base_len   ].u ) / 2.0;
    double u3 = (u2 - u1) / 3.0;
    //
    double v1 = (curr_base[ j   ].v + next_base[ next_base_len-1 ].v ) / 2.0;
    double v2 = (curr_base[ j+3 ].v + next_base[ next_base_len   ].v ) / 2.0;
    double v3 = (v2 - v1) / 3.0;
    // c
    double u = u1 + u3;
    double v = v1 + v3;
    gp_Pnt P = S->Value(u,v);
    SMDS_MeshNode* Nc = helper->AddNode( P.X(), P.Y(), P.Z(), 0, u, v );
    // e
    u = u1 + u3 + u3;
    v = v1 + v3 + v3;
    P = S->Value(u,v);
    SMDS_MeshNode* Ne = helper->AddNode( P.X(), P.Y(), P.Z(), 0, u, v );

    // Faces
    // 1
    helper->AddFace( curr_base[ j + 0 ].node,
                     curr_base[ j + 1 ].node,
                     Nc,
                     next_base[ next_base_len - 1 ].node);
    // 2
    helper->AddFace( curr_base[ j + 1 ].node,
                     curr_base[ j + 2 ].node, Ne, Nc);
    // 4
    helper->AddFace( curr_base[ j + 2 ].node,
                     curr_base[ j + 3 ].node, Nb, Ne);
    // 5
    helper->AddFace(Nc, Ne, Nb,
                    next_base[ next_base_len - 1 ].node);
  }

  typedef void (* PReduceFunction) ( const vector<UVPtStruct>& curr_base,
                                     vector<UVPtStruct>&       next_base,
                                     const int                 j,
                                     int &                     next_base_len,
                                     FaceQuadStruct::Ptr &     quad,
                                     gp_UV*                    UVs,
                                     const double              y,
                                     SMESH_MesherHelper*       helper,
                                     Handle(Geom_Surface)&     S);

} // namespace

//=======================================================================
/*!
 *  Implementation of Reduced algorithm (meshing with quadrangles only)
 */
//=======================================================================

bool StdMeshers_Quadrangle_2D::computeReduced (SMESH_Mesh &        aMesh,
                                               const TopoDS_Face&  aFace,
                                               FaceQuadStruct::Ptr quad)
{
  SMESHDS_Mesh * meshDS  = aMesh.GetMeshDS();
  Handle(Geom_Surface) S = BRep_Tool::Surface(aFace);
  int i,j,geomFaceID     = meshDS->ShapeToIndex(aFace);

  int nb = quad->side[0]->NbPoints(); // bottom
  int nr = quad->side[1]->NbPoints(); // right
  int nt = quad->side[2]->NbPoints(); // top
  int nl = quad->side[3]->NbPoints(); // left

  //  Simple Reduce 10->8->6->4 (3 steps)     Multiple Reduce 10->4 (1 step)
  //
  //  .-----.-----.-----.-----.               .-----.-----.-----.-----.
  //  |    / \    |    / \    |               |    / \    |    / \    |
  //  |   /    .--.--.    \   |               |    / \    |    / \    |
  //  |   /   /   |   \   \   |               |   /  .----.----.  \   |
  //  .---.---.---.---.---.---.               |   / / \   |   / \ \   |
  //  |   /  / \  |  / \  \   |               |   / / \   |   / \ \   |
  //  |  /  /   .-.-.   \  \  |               |  / /  .---.---.  \ \  |
  //  |  /  /  /  |  \  \  \  |               |  / / / \  |  / \ \ \  |
  //  .--.--.--.--.--.--.--.--.               |  / / /  \ | /  \ \ \  |
  //  |  / /  / \ | / \  \ \  |               | / / /   .-.-.   \ \ \ |
  //  | / /  /  .-.-.  \  \ \ |               | / / /  /  |  \  \ \ \ |
  //  | / / /  /  |  \  \ \ \ |               | / / /  /  |  \  \ \ \ |
  //  .-.-.-.--.--.--.--.-.-.-.               .-.-.-.--.--.--.--.-.-.-.

  bool MultipleReduce = false;
  {
    int nb1 = nb;
    int nr1 = nr;
    int nt1 = nt;

    if (nr == nl) {
      if (nb < nt) {
        nt1 = nb;
        nb1 = nt;
      }
    }
    else if (nb == nt) {
      nr1 = nb; // and == nt
      if (nl < nr) {
        nt1 = nl;
        nb1 = nr;
      }
      else {
        nt1 = nr;
        nb1 = nl;
      }
    }
    else {
      return false;
    }

    // number of rows and columns
    int nrows    = nr1 - 1;
    int ncol_top = nt1 - 1;
    int ncol_bot = nb1 - 1;
    // number of rows needed to reduce ncol_bot to ncol_top using simple 3->1 "tree" (see below)
    int nrows_tree31 =
      int( ceil( log( double(ncol_bot) / ncol_top) / log( 3.))); // = log x base 3
    if ( nrows < nrows_tree31 )
    {
      MultipleReduce = true;
      error( COMPERR_WARNING,
             SMESH_Comment("To use 'Reduced' transition, "
                           "number of face rows should be at least ")
             << nrows_tree31 << ". Actual number of face rows is " << nrows << ". "
             "'Quadrangle preference (reversed)' transion has been used.");
    }
  }

  if (MultipleReduce) { // == computeQuadPref QUAD_QUADRANGLE_PREF_REVERSED
    //==================================================
    int dh = abs(nb-nt);
    int dv = abs(nr-nl);

    if (dh >= dv) {
      if (nt > nb) {
        // it is a base case => not shift quad but may be replacement is need
        shiftQuad(quad,0);
      }
      else {
        // we have to shift quad on 2
        shiftQuad(quad,2);
      }
    }
    else {
      if (nr > nl) {
        // we have to shift quad on 1
        shiftQuad(quad,1);
      }
      else {
        // we have to shift quad on 3
        shiftQuad(quad,3);
      }
    }

    nb = quad->side[0]->NbPoints();
    nr = quad->side[1]->NbPoints();
    nt = quad->side[2]->NbPoints();
    nl = quad->side[3]->NbPoints();
    dh = abs(nb-nt);
    dv = abs(nr-nl);
    int nbh = Max(nb,nt);
    int nbv = Max(nr,nl);
    int addh = 0;
    int addv = 0;

    if (dh>dv) {
      addv = (dh-dv)/2;
      nbv = nbv + addv;
    }
    else { // dv>=dh
      addh = (dv-dh)/2;
      nbh = nbh + addh;
    }

    const vector<UVPtStruct>& uv_eb = quad->side[0]->GetUVPtStruct(true,0);
    const vector<UVPtStruct>& uv_er = quad->side[1]->GetUVPtStruct(false,1);
    const vector<UVPtStruct>& uv_et = quad->side[2]->GetUVPtStruct(true,1);
    const vector<UVPtStruct>& uv_el = quad->side[3]->GetUVPtStruct(false,0);

    if (uv_eb.size() != nb || uv_er.size() != nr || uv_et.size() != nt || uv_el.size() != nl)
      return error(COMPERR_BAD_INPUT_MESH);

    updateDegenUV( quad );

    // arrays for normalized params
    TColStd_SequenceOfReal npb, npr, npt, npl;
    for (j = 0; j < nb; j++) {
      npb.Append(uv_eb[j].normParam);
    }
    for (i = 0; i < nr; i++) {
      npr.Append(uv_er[i].normParam);
    }
    for (j = 0; j < nt; j++) {
      npt.Append(uv_et[j].normParam);
    }
    for (i = 0; i < nl; i++) {
      npl.Append(uv_el[i].normParam);
    }

    int dl,dr;
    // orientation of face and 3 main domain for future faces
    //       0   top    1
    //      1------------1
    //       |   |  |   |
    //       |   |  |   |
    //       | L |  | R |
    //  left |   |  |   | rigth
    //       |  /    \  |
    //       | /  C   \ |
    //       |/        \|
    //      0------------0
    //       0  bottom  1

    // add some params to right and left after the first param
    // insert to right
    dr = nbv - nr;
    double dpr = (npr.Value(2) - npr.Value(1))/(dr+1);
    for (i=1; i<=dr; i++) {
      npr.InsertAfter(1,npr.Value(2)-dpr);
    }
    // insert to left
    dl = nbv - nl;
    dpr = (npl.Value(2) - npl.Value(1))/(dl+1);
    for (i=1; i<=dl; i++) {
      npl.InsertAfter(1,npl.Value(2)-dpr);
    }
  
    gp_XY a0 (uv_eb.front().u, uv_eb.front().v);
    gp_XY a1 (uv_eb.back().u,  uv_eb.back().v);
    gp_XY a2 (uv_et.back().u,  uv_et.back().v);
    gp_XY a3 (uv_et.front().u, uv_et.front().v);

    int nnn = Min(nr,nl);
    // auxilary sequence of XY for creation of nodes
    // in the bottom part of central domain
    // it's length must be == nbv-nnn-1
    TColgp_SequenceOfXY UVL;
    TColgp_SequenceOfXY UVR;
    //==================================================

    // step1: create faces for left domain
    StdMeshers_Array2OfNode NodesL(1,dl+1,1,nl);
    // add left nodes
    for (j=1; j<=nl; j++)
      NodesL.SetValue(1,j,uv_el[j-1].node);
    if (dl>0) {
      // add top nodes
      for (i=1; i<=dl; i++) 
        NodesL.SetValue(i+1,nl,uv_et[i].node);
      // create and add needed nodes
      TColgp_SequenceOfXY UVtmp;
      for (i=1; i<=dl; i++) {
        double x0 = npt.Value(i+1);
        double x1 = x0;
        // diagonal node
        double y0 = npl.Value(i+1);
        double y1 = npr.Value(i+1);
        gp_UV UV = calcUV(x0, x1, y0, y1, quad, a0, a1, a2, a3);
        gp_Pnt P = S->Value(UV.X(),UV.Y());
        SMDS_MeshNode * N = meshDS->AddNode(P.X(), P.Y(), P.Z());
        meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
        NodesL.SetValue(i+1,1,N);
        if (UVL.Length()<nbv-nnn-1) UVL.Append(UV);
        // internal nodes
        for (j=2; j<nl; j++) {
          double y0 = npl.Value(dl+j);
          double y1 = npr.Value(dl+j);
          gp_UV UV = calcUV(x0, x1, y0, y1, quad, a0, a1, a2, a3);
          gp_Pnt P = S->Value(UV.X(),UV.Y());
          SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
          meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
          NodesL.SetValue(i+1,j,N);
          if (i==dl) UVtmp.Append(UV);
        }
      }
      for (i=1; i<=UVtmp.Length() && UVL.Length()<nbv-nnn-1; i++) {
        UVL.Append(UVtmp.Value(i));
      }
      // create faces
      for (i=1; i<=dl; i++) {
        for (j=1; j<nl; j++) {
            SMDS_MeshFace* F =
              myHelper->AddFace(NodesL.Value(i,j), NodesL.Value(i+1,j),
                                NodesL.Value(i+1,j+1), NodesL.Value(i,j+1));
            if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
        }
      }
    }
    else {
      // fill UVL using c2d
      for (i=1; i<npl.Length() && UVL.Length()<nbv-nnn-1; i++) {
        UVL.Append(gp_UV (uv_el[i].u, uv_el[i].v));
      }
    }
    
    // step2: create faces for right domain
    StdMeshers_Array2OfNode NodesR(1,dr+1,1,nr);
    // add right nodes
    for (j=1; j<=nr; j++) 
      NodesR.SetValue(1,j,uv_er[nr-j].node);
    if (dr>0) {
      // add top nodes
      for (i=1; i<=dr; i++) 
        NodesR.SetValue(i+1,1,uv_et[nt-1-i].node);
      // create and add needed nodes
      TColgp_SequenceOfXY UVtmp;
      for (i=1; i<=dr; i++) {
        double x0 = npt.Value(nt-i);
        double x1 = x0;
        // diagonal node
        double y0 = npl.Value(i+1);
        double y1 = npr.Value(i+1);
        gp_UV UV = calcUV(x0, x1, y0, y1, quad, a0, a1, a2, a3);
        gp_Pnt P = S->Value(UV.X(),UV.Y());
        SMDS_MeshNode * N = meshDS->AddNode(P.X(), P.Y(), P.Z());
        meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
        NodesR.SetValue(i+1,nr,N);
        if (UVR.Length()<nbv-nnn-1) UVR.Append(UV);
        // internal nodes
        for (j=2; j<nr; j++) {
          double y0 = npl.Value(nbv-j+1);
          double y1 = npr.Value(nbv-j+1);
          gp_UV UV = calcUV(x0, x1, y0, y1, quad, a0, a1, a2, a3);
          gp_Pnt P = S->Value(UV.X(),UV.Y());
          SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
          meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
          NodesR.SetValue(i+1,j,N);
          if (i==dr) UVtmp.Prepend(UV);
        }
      }
      for (i=1; i<=UVtmp.Length() && UVR.Length()<nbv-nnn-1; i++) {
        UVR.Append(UVtmp.Value(i));
      }
      // create faces
      for (i=1; i<=dr; i++) {
        for (j=1; j<nr; j++) {
            SMDS_MeshFace* F =
              myHelper->AddFace(NodesR.Value(i,j), NodesR.Value(i+1,j),
                                NodesR.Value(i+1,j+1), NodesR.Value(i,j+1));
            if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
        }
      }
    }
    else {
      // fill UVR using c2d
      for (i=1; i<npr.Length() && UVR.Length()<nbv-nnn-1; i++) {
        UVR.Append(gp_UV(uv_er[i].u, uv_er[i].v));
      }
    }
    
    // step3: create faces for central domain
    StdMeshers_Array2OfNode NodesC(1,nb,1,nbv);
    // add first line using NodesL
    for (i=1; i<=dl+1; i++)
      NodesC.SetValue(1,i,NodesL(i,1));
    for (i=2; i<=nl; i++)
      NodesC.SetValue(1,dl+i,NodesL(dl+1,i));
    // add last line using NodesR
    for (i=1; i<=dr+1; i++)
      NodesC.SetValue(nb,i,NodesR(i,nr));
    for (i=1; i<nr; i++)
      NodesC.SetValue(nb,dr+i+1,NodesR(dr+1,nr-i));
    // add top nodes (last columns)
    for (i=dl+2; i<nbh-dr; i++) 
      NodesC.SetValue(i-dl,nbv,uv_et[i-1].node);
    // add bottom nodes (first columns)
    for (i=2; i<nb; i++)
      NodesC.SetValue(i,1,uv_eb[i-1].node);

    // create and add needed nodes
    // add linear layers
    for (i=2; i<nb; i++) {
      double x0 = npt.Value(dl+i);
      double x1 = x0;
      for (j=1; j<nnn; j++) {
        double y0 = npl.Value(nbv-nnn+j);
        double y1 = npr.Value(nbv-nnn+j);
        gp_UV UV = calcUV(x0, x1, y0, y1, quad, a0, a1, a2, a3);
        gp_Pnt P = S->Value(UV.X(),UV.Y());
        SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
        meshDS->SetNodeOnFace(N, geomFaceID, UV.X(), UV.Y());
        NodesC.SetValue(i,nbv-nnn+j,N);
      }
    }
    // add diagonal layers
    for (i=1; i<nbv-nnn; i++) {
      double du = UVR.Value(i).X() - UVL.Value(i).X();
      double dv = UVR.Value(i).Y() - UVL.Value(i).Y();
      for (j=2; j<nb; j++) {
        double u = UVL.Value(i).X() + du*npb.Value(j);
        double v = UVL.Value(i).Y() + dv*npb.Value(j);
        gp_Pnt P = S->Value(u,v);
        SMDS_MeshNode* N = meshDS->AddNode(P.X(), P.Y(), P.Z());
        meshDS->SetNodeOnFace(N, geomFaceID, u, v);
        NodesC.SetValue(j,i+1,N);
      }
    }
    // create faces
    for (i=1; i<nb; i++) {
      for (j=1; j<nbv; j++) {
        SMDS_MeshFace* F =
          myHelper->AddFace(NodesC.Value(i,j), NodesC.Value(i+1,j),
                            NodesC.Value(i+1,j+1), NodesC.Value(i,j+1));
        if (F) meshDS->SetMeshElementOnShape(F, geomFaceID);
      }
    }
  } // end Multiple Reduce implementation
  else { // Simple Reduce (!MultipleReduce)
    //=========================================================
    if (nr == nl) {
      if (nt < nb) {
        // it is a base case => not shift quad
        //shiftQuad(quad,0,true);
      }
      else {
        // we have to shift quad on 2
        shiftQuad(quad,2);
      }
    }
    else {
      if (nl > nr) {
        // we have to shift quad on 1
        shiftQuad(quad,1);
      }
      else {
        // we have to shift quad on 3
        shiftQuad(quad,3);
      }
    }

    nb = quad->side[0]->NbPoints();
    nr = quad->side[1]->NbPoints();
    nt = quad->side[2]->NbPoints();
    nl = quad->side[3]->NbPoints();

    // number of rows and columns
    int nrows = nr - 1; // and also == nl - 1
    int ncol_top = nt - 1;
    int ncol_bot = nb - 1;
    int npair_top = ncol_top / 2;
    // maximum number of bottom elements for "linear" simple reduce 4->2
    int max_lin42 = ncol_top + npair_top * 2 * nrows;
    // maximum number of bottom elements for "linear" simple reduce 3->1
    int max_lin31 = ncol_top + ncol_top * 2 * nrows;
    // maximum number of bottom elements for "tree" simple reduce 4->2
    int max_tree42 = 0;
    // number of rows needed to reduce ncol_bot to ncol_top using simple 4->2 "tree"
    int nrows_tree42 = int( log( (double)(ncol_bot / ncol_top) )/log((double)2)  ); // needed to avoid overflow at pow(2) while computing max_tree42
    if (nrows_tree42 < nrows) {
      max_tree42 = npair_top * pow(2.0, nrows + 1);
      if ( ncol_top > npair_top * 2 ) {
        int delta = ncol_bot - max_tree42;
        for (int irow = 1; irow < nrows; irow++) {
          int nfour = delta / 4;
          delta -= nfour * 2;
        }
        if (delta <= (ncol_top - npair_top * 2))
          max_tree42 = ncol_bot;
      }
    }
    // maximum number of bottom elements for "tree" simple reduce 3->1
    //int max_tree31 = ncol_top * pow(3.0, nrows);
    bool is_lin_31 = false;
    bool is_lin_42 = false;
    bool is_tree_31 = false;
    bool is_tree_42 = false;
    int max_lin = max_lin42;
    if (ncol_bot > max_lin42) {
      if (ncol_bot <= max_lin31) {
        is_lin_31 = true;
        max_lin = max_lin31;
      }
    }
    else {
      // if ncol_bot is a 3*n or not 2*n
      if ((ncol_bot/3)*3 == ncol_bot || (ncol_bot/2)*2 != ncol_bot) {
        is_lin_31 = true;
        max_lin = max_lin31;
      }
      else {
        is_lin_42 = true;
      }
    }
    if (ncol_bot > max_lin) { // not "linear"
      is_tree_31 = (ncol_bot > max_tree42);
      if (ncol_bot <= max_tree42) {
        if ((ncol_bot/3)*3 == ncol_bot || (ncol_bot/2)*2 != ncol_bot) {
          is_tree_31 = true;
        }
        else {
          is_tree_42 = true;
        }
      }
    }

    const vector<UVPtStruct>& uv_eb = quad->side[0]->GetUVPtStruct(true,0);
    const vector<UVPtStruct>& uv_er = quad->side[1]->GetUVPtStruct(false,1);
    const vector<UVPtStruct>& uv_et = quad->side[2]->GetUVPtStruct(true,1);
    const vector<UVPtStruct>& uv_el = quad->side[3]->GetUVPtStruct(false,0);

    if (uv_eb.size() != nb || uv_er.size() != nr || uv_et.size() != nt || uv_el.size() != nl)
      return error(COMPERR_BAD_INPUT_MESH);

    myHelper->SetElementsOnShape( true );

    gp_UV uv[ UV_SIZE ];
    uv[ UV_A0 ].SetCoord( uv_eb.front().u, uv_eb.front().v);
    uv[ UV_A1 ].SetCoord( uv_eb.back().u,  uv_eb.back().v );
    uv[ UV_A2 ].SetCoord( uv_et.back().u,  uv_et.back().v );
    uv[ UV_A3 ].SetCoord( uv_et.front().u, uv_et.front().v);

    vector<UVPtStruct> curr_base = uv_eb, next_base;

    UVPtStruct nullUVPtStruct; nullUVPtStruct.node = 0;

    int curr_base_len = nb;
    int next_base_len = 0;

    if ( true )
    { // ------------------------------------------------------------------
      // New algorithm implemented by request of IPAL22856
      // "2D quadrangle mesher of reduced type works wrong"
      // http://bugtracker.opencascade.com/show_bug.cgi?id=22856

      // the algorithm is following: all reduces are centred in horizontal
      // direction and are distributed among all rows

      if (ncol_bot > max_tree42) {
        is_lin_31 = true;
      }
      else {
        if ((ncol_top/3)*3 == ncol_top ) {
          is_lin_31 = true;
        }
        else {
          is_lin_42 = true;
        }
      }

      const int col_top_size  = is_lin_42 ? 2 : 1;
      const int col_base_size = is_lin_42 ? 4 : 3;

      // Compute nb of "columns" (like in "linear" simple reducing) in all rows

      vector<int> nb_col_by_row;

      int delta_all     = nb - nt;
      int delta_one_col = nrows * 2;
      int nb_col        = delta_all / delta_one_col;
      int remainder     = delta_all - nb_col * delta_one_col;
      if (remainder > 0) {
        nb_col++;
      }
      if ( nb_col * col_top_size >= nt ) // == "tree" reducing situation
      {
        // top row is full (all elements reduced), add "columns" one by one
        // in rows below until all bottom elements are reduced
        nb_col = ( nt - 1 ) / col_top_size;
        nb_col_by_row.resize( nrows, nb_col );
        int nbrows_not_full = nrows - 1;
        int cur_top_size    = nt - 1;
        remainder = delta_all - nb_col * delta_one_col;
        while ( remainder > 0 )
        {
          delta_one_col   = nbrows_not_full * 2;
          int nb_col_add  = remainder / delta_one_col;
          cur_top_size   += 2 * nb_col_by_row[ nbrows_not_full ];
          int nb_col_free = cur_top_size / col_top_size - nb_col_by_row[ nbrows_not_full-1 ];
          if ( nb_col_add > nb_col_free )
            nb_col_add = nb_col_free;
          for ( int irow = 0; irow < nbrows_not_full; ++irow )
            nb_col_by_row[ irow ] += nb_col_add;
          nbrows_not_full --;
          remainder -=  nb_col_add * delta_one_col;
        }
      }
      else // == "linear" reducing situation
      {
        nb_col_by_row.resize( nrows, nb_col );
        if (remainder > 0)
          for ( int irow = remainder / 2; irow < nrows; ++irow )
            nb_col_by_row[ irow ]--;
      }

      // Make elements

      PReduceFunction reduceFunction = & ( is_lin_42 ? reduce42 : reduce31 );

      const int reduce_grp_size = is_lin_42 ? 4 : 3;

      for (i = 1; i < nr; i++) // layer by layer
      {
        nb_col = nb_col_by_row[ i-1 ];
        int nb_next = curr_base_len - nb_col * 2;
        if (nb_next < nt) nb_next = nt;

        const double y = uv_el[ i ].normParam;

        if ( i + 1 == nr ) // top
        {
          next_base = uv_et;
        }
        else
        {
          next_base.clear();
          next_base.resize( nb_next, nullUVPtStruct );
          next_base.front() = uv_el[i];
          next_base.back()  = uv_er[i];

          // compute normalized param u
          double du = 1. / ( nb_next - 1 );
          next_base[0].normParam = 0.;
          for ( j = 1; j < nb_next; ++j )
            next_base[j].normParam = next_base[j-1].normParam + du;
        }
        uv[ UV_L ].SetCoord( next_base.front().u, next_base.front().v );
        uv[ UV_R ].SetCoord( next_base.back().u,  next_base.back().v );

        int free_left = ( curr_base_len - 1 - nb_col * col_base_size ) / 2;
        int free_middle = curr_base_len - 1 - nb_col * col_base_size - 2 * free_left;

        // not reduced left elements
        for (j = 0; j < free_left; j++)
        {
          // f (i + 1, j + 1)
          const SMDS_MeshNode*& Nf = next_base[++next_base_len].node;
          if ( !Nf )
            Nf = makeNode( next_base[ next_base_len ], y, quad, uv, myHelper, S );

          myHelper->AddFace(curr_base[ j ].node,
                            curr_base[ j+1 ].node,
                            Nf,
                            next_base[ next_base_len-1 ].node);
        }

        for (int icol = 1; icol <= nb_col; icol++)
        {
          // add "H"
          reduceFunction( curr_base, next_base, j, next_base_len, quad, uv, y, myHelper, S );

          j += reduce_grp_size;

          // elements in the middle of "columns" added for symmetry
          if ( free_middle > 0 && ( nb_col % 2 == 0 ) && icol == nb_col / 2 )
          {
            for (int imiddle = 1; imiddle <= free_middle; imiddle++) {
              // f (i + 1, j + imiddle)
              const SMDS_MeshNode*& Nf = next_base[++next_base_len].node;
              if ( !Nf )
                Nf = makeNode( next_base[ next_base_len ], y, quad, uv, myHelper, S );

              myHelper->AddFace(curr_base[ j-1+imiddle ].node,
                                curr_base[ j  +imiddle ].node,
                                Nf,
                                next_base[ next_base_len-1 ].node);
            }
            j += free_middle;
          }
        }

        // not reduced right elements
        for (; j < curr_base_len-1; j++) {
          // f (i + 1, j + 1)
          const SMDS_MeshNode*& Nf = next_base[++next_base_len].node;
          if ( !Nf )
            Nf = makeNode( next_base[ next_base_len ], y, quad, uv, myHelper, S );

          myHelper->AddFace(curr_base[ j ].node,
                            curr_base[ j+1 ].node,
                            Nf,
                            next_base[ next_base_len-1 ].node);
        }

        curr_base_len = next_base_len + 1;
        next_base_len = 0;
        curr_base.swap( next_base );
      }

    }
    else if ( is_tree_42 || is_tree_31 )
    {
      // "tree" simple reduce "42": 2->4->8->16->32->...
      //
      //  .-------------------------------.-------------------------------. nr
      //  |    \                          |                          /    |
      //  |         \     .---------------.---------------.     /         |
      //  |               |               |               |               |
      //  .---------------.---------------.---------------.---------------.
      //  | \             |             / | \             |             / |
      //  |     \ .-------.-------. /     |     \ .-------.-------. /     |
      //  |       |       |       |       |       |       |       |       |
      //  .-------.-------.-------.-------.-------.-------.-------.-------. i
      //  |\      |      /|\      |      /|\      |      /|\      |      /|
      //  |  \.---.---./  |  \.---.---./  |  \.---.---./  |  \.---.---./  |
      //  |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
      //  .---.---.---.---.---.---.---.---.---.---.---.---.---.---.---.---.
      //  |\  |  /|\  |  /|\  |  /|\  |  /|\  |  /|\  |  /|\  |  /|\  |  /|
      //  | .-.-. | .-.-. | .-.-. | .-.-. | .-.-. | .-.-. | .-.-. | .-.-. |
      //  | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
      //  .-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-. 1
      //  1                               j                               nb

      // "tree" simple reduce "31": 1->3->9->27->...
      //
      //  .-----------------------------------------------------. nr
      //  |        \                                   /        |
      //  |                 .-----------------.                 |
      //  |                 |                 |                 |
      //  .-----------------.-----------------.-----------------.
      //  |   \         /   |   \         /   |   \         /   |
      //  |     .-----.     |     .-----.     |     .-----.     | i
      //  |     |     |     |     |     |     |     |     |     |
      //  .-----.-----.-----.-----.-----.-----.-----.-----.-----.
      //  |\   /|\   /|\   /|\   /|\   /|\   /|\   /|\   /|\   /|
      //  | .-. | .-. | .-. | .-. | .-. | .-. | .-. | .-. | .-. |
      //  | | | | | | | | | | | | | | | | | | | | | | | | | | | |
      //  .-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-. 1
      //  1                          j                          nb

      PReduceFunction reduceFunction = & ( is_tree_42 ? reduce42 : reduce31 );

      const int reduce_grp_size = is_tree_42 ? 4 : 3;

      for (i = 1; i < nr; i++) // layer by layer
      {
        // to stop reducing, if number of nodes reaches nt
        int delta = curr_base_len - nt;

        // to calculate normalized parameter, we must know number of points in next layer
        int nb_reduce_groups = (curr_base_len - 1) / reduce_grp_size;
        int nb_next = nb_reduce_groups * (reduce_grp_size-2) + (curr_base_len - nb_reduce_groups*reduce_grp_size);
        if (nb_next < nt) nb_next = nt;

        const double y = uv_el[ i ].normParam;

        if ( i + 1 == nr ) // top
        {
          next_base = uv_et;
        }
        else
        {
          next_base.clear();
          next_base.resize( nb_next, nullUVPtStruct );
          next_base.front() = uv_el[i];
          next_base.back()  = uv_er[i];

          // compute normalized param u
          double du = 1. / ( nb_next - 1 );
          next_base[0].normParam = 0.;
          for ( j = 1; j < nb_next; ++j )
            next_base[j].normParam = next_base[j-1].normParam + du;
        }
        uv[ UV_L ].SetCoord( next_base.front().u, next_base.front().v );
        uv[ UV_R ].SetCoord( next_base.back().u,  next_base.back().v );

        for (j = 0; j+reduce_grp_size < curr_base_len && delta > 0; j+=reduce_grp_size, delta-=2)
        {
          reduceFunction( curr_base, next_base, j, next_base_len, quad, uv, y, myHelper, S );
        }

        // not reduced side elements (if any)
        for (; j < curr_base_len-1; j++)
        {
          // f (i + 1, j + 1)
          const SMDS_MeshNode*& Nf = next_base[++next_base_len].node;
          if ( !Nf )
            Nf = makeNode( next_base[ next_base_len ], y, quad, uv, myHelper, S );
          
          myHelper->AddFace(curr_base[ j ].node,
                            curr_base[ j+1 ].node,
                            Nf,
                            next_base[ next_base_len-1 ].node);
        }
        curr_base_len = next_base_len + 1;
        next_base_len = 0;
        curr_base.swap( next_base );
      }
    } // end "tree" simple reduce

    else if ( is_lin_42 || is_lin_31 ) {
      // "linear" simple reduce "31": 2->6->10->14
      //
      //  .-----------------------------.-----------------------------. nr
      //  |     \                 /     |     \                 /     |
      //  |         .---------.         |         .---------.         |
      //  |         |         |         |         |         |         |
      //  .---------.---------.---------.---------.---------.---------.
      //  |        / \       / \        |        / \       / \        |
      //  |       /   .-----.   \       |       /   .-----.   \       | i
      //  |      /    |     |    \      |      /    |     |    \      |
      //  .-----.-----.-----.-----.-----.-----.-----.-----.-----.-----.
      //  |    /     / \   / \     \    |    /     / \   / \     \    |
      //  |   /     /   .-.   \     \   |   /     /   .-.   \     \   |
      //  |  /     /   /   \   \     \  |  /     /   /   \   \     \  |
      //  .--.----.---.-----.---.-----.-.--.----.---.-----.---.-----.-. 1
      //  1                             j                             nb

      // "linear" simple reduce "42": 4->8->12->16
      //
      //  .---------------.---------------.---------------.---------------. nr
      //  | \             |             / | \             |             / |
      //  |     \ .-------.-------. /     |     \ .-------.-------. /     |
      //  |       |       |       |       |       |       |       |       |
      //  .-------.-------.-------.-------.-------.-------.-------.-------.
      //  |      / \      |      / \      |      / \      |      / \      |
      //  |     /   \.----.----./   \     |     /   \.----.----./   \     | i
      //  |     /    |    |    |    \     |     /    |    |    |    \     |
      //  .-----.----.----.----.----.-----.-----.----.----.----.----.-----.
      //  |     /   / \   |  /  \   \     |     /   / \   |  /  \   \     |
      //  |    /   /    .-.-.    \   \    |    /   /    .-.-.    \   \    |
      //  |   /   /    /  |  \    \   \   |   /   /    /  |  \    \   \   |
      //  .---.---.---.---.---.---.---.---.---.---.---.---.---.---.---.---. 1
      //  1                               j                               nb

      // nt = 5, nb = 7, nr = 4
      //int delta_all = 2;
      //int delta_one_col = 6;
      //int nb_col = 0;
      //int remainder = 2;
      //if (remainder > 0) nb_col++;
      //nb_col = 1;
      //int free_left = 1;
      //free_left += 2;
      //int free_middle = 4;

      int delta_all = nb - nt;
      int delta_one_col = (nr - 1) * 2;
      int nb_col = delta_all / delta_one_col;
      int remainder = delta_all - nb_col * delta_one_col;
      if (remainder > 0) {
        nb_col++;
      }
      const int col_top_size = is_lin_42 ? 2 : 1;
      int free_left = ((nt - 1) - nb_col * col_top_size) / 2;
      free_left += nr - 2;
      int free_middle = (nr - 2) * 2;
      if (remainder > 0 && nb_col == 1) {
        int nb_rows_short_col = remainder / 2;
        int nb_rows_thrown = (nr - 1) - nb_rows_short_col;
        free_left -= nb_rows_thrown;
      }

      // nt = 5, nb = 17, nr = 4
      //int delta_all = 12;
      //int delta_one_col = 6;
      //int nb_col = 2;
      //int remainder = 0;
      //int free_left = 2;
      //int free_middle = 4;

      PReduceFunction reduceFunction = & ( is_lin_42 ? reduce42 : reduce31 );

      const int reduce_grp_size = is_lin_42 ? 4 : 3;

      for (i = 1; i < nr; i++, free_middle -= 2, free_left -= 1) // layer by layer
      {
        // to calculate normalized parameter, we must know number of points in next layer
        int nb_next = curr_base_len - nb_col * 2;
        if (remainder > 0 && i > remainder / 2)
          // take into account short "column"
          nb_next += 2;
        if (nb_next < nt) nb_next = nt;

        const double y = uv_el[ i ].normParam;

        if ( i + 1 == nr ) // top
        {
          next_base = uv_et;
        }
        else
        {
          next_base.clear();
          next_base.resize( nb_next, nullUVPtStruct );
          next_base.front() = uv_el[i];
          next_base.back()  = uv_er[i];

          // compute normalized param u
          double du = 1. / ( nb_next - 1 );
          next_base[0].normParam = 0.;
          for ( j = 1; j < nb_next; ++j )
            next_base[j].normParam = next_base[j-1].normParam + du;
        }
        uv[ UV_L ].SetCoord( next_base.front().u, next_base.front().v );
        uv[ UV_R ].SetCoord( next_base.back().u,  next_base.back().v );

        // not reduced left elements
        for (j = 0; j < free_left; j++)
        {
          // f (i + 1, j + 1)
          const SMDS_MeshNode*& Nf = next_base[++next_base_len].node;
          if ( !Nf )
            Nf = makeNode( next_base[ next_base_len ], y, quad, uv, myHelper, S );

          myHelper->AddFace(curr_base[ j ].node,
                            curr_base[ j+1 ].node,
                            Nf,
                            next_base[ next_base_len-1 ].node);
        }

        for (int icol = 1; icol <= nb_col; icol++) {

          if (remainder > 0 && icol == nb_col && i > remainder / 2)
            // stop short "column"
            break;

          // add "H"
          reduceFunction( curr_base, next_base, j, next_base_len, quad, uv, y, myHelper, S );

          j += reduce_grp_size;

          // not reduced middle elements
          if (icol < nb_col) {
            if (remainder > 0 && icol == nb_col - 1 && i > remainder / 2)
              // pass middle elements before stopped short "column"
              break;

            int free_add = free_middle;
            if (remainder > 0 && icol == nb_col - 1)
              // next "column" is short
              free_add -= (nr - 1) - (remainder / 2);

            for (int imiddle = 1; imiddle <= free_add; imiddle++) {
              // f (i + 1, j + imiddle)
              const SMDS_MeshNode*& Nf = next_base[++next_base_len].node;
              if ( !Nf )
                Nf = makeNode( next_base[ next_base_len ], y, quad, uv, myHelper, S );

              myHelper->AddFace(curr_base[ j-1+imiddle ].node,
                                curr_base[ j  +imiddle ].node,
                                Nf,
                                next_base[ next_base_len-1 ].node);
            }
            j += free_add;
          }
        }

        // not reduced right elements
        for (; j < curr_base_len-1; j++) {
          // f (i + 1, j + 1)
          const SMDS_MeshNode*& Nf = next_base[++next_base_len].node;
          if ( !Nf )
            Nf = makeNode( next_base[ next_base_len ], y, quad, uv, myHelper, S );

          myHelper->AddFace(curr_base[ j ].node,
                            curr_base[ j+1 ].node,
                            Nf,
                            next_base[ next_base_len-1 ].node);
        }

        curr_base_len = next_base_len + 1;
        next_base_len = 0;
        curr_base.swap( next_base );
      }

    } // end "linear" simple reduce

    else {
      return false;
    }
  } // end Simple Reduce implementation

  bool isOk = true;
  return isOk;
}

//================================================================================
namespace // data for smoothing
{
  struct TSmoothNode;
  // --------------------------------------------------------------------------------
  /*!
   * \brief Structure used to check validity of node position after smoothing.
   *        It holds two nodes connected to a smoothed node and belonging to
   *        one mesh face
   */
  struct TTriangle
  {
    TSmoothNode* _n1;
    TSmoothNode* _n2;
    TTriangle( TSmoothNode* n1=0, TSmoothNode* n2=0 ): _n1(n1), _n2(n2) {}

    inline bool IsForward( gp_UV uv ) const;
  };
  // --------------------------------------------------------------------------------
  /*!
   * \brief Data of a smoothed node
   */
  struct TSmoothNode
  {
    gp_XY  _uv;
    gp_XYZ _xyz;
    vector< TTriangle > _triangles; // if empty, then node is not movable
  };
  // --------------------------------------------------------------------------------
  inline bool TTriangle::IsForward( gp_UV uv ) const
  {
    gp_Vec2d v1( uv, _n1->_uv ), v2( uv, _n2->_uv );
    double d = v1 ^ v2;
    return d > 1e-100;
  }
}

//================================================================================
/*!
 * \brief Set UV of nodes on degenerated VERTEXes in the middle of degenerated EDGE
 *
 * WARNING: this method must be called AFTER retrieving UVPtStruct's from quad
 */
//================================================================================

void StdMeshers_Quadrangle_2D::updateDegenUV(FaceQuadStruct::Ptr quad)
{
  if ( myNeedSmooth )

    // Set UV of nodes on degenerated VERTEXes in the middle of degenerated EDGE
    // --------------------------------------------------------------------------
    for ( unsigned i = 0; i < quad->side.size(); ++i )
    {
      StdMeshers_FaceSide* side = quad->side[i];
      const vector<UVPtStruct>& uvVec = side->GetUVPtStruct();

      // find which end of the side is on degenerated shape
      int degenInd = -1;
      if ( myHelper->IsDegenShape( uvVec[0].node->getshapeId() ))
        degenInd = 0;
      else if ( myHelper->IsDegenShape( uvVec.back().node->getshapeId() ))
        degenInd = uvVec.size() - 1;
      else
        continue;

      // find another side sharing the degenerated shape
      bool isPrev = ( degenInd == 0 );
      if ( i >= QUAD_TOP_SIDE )
        isPrev = !isPrev;
      int i2 = ( isPrev ? ( i + 3 ) : ( i + 1 )) % 4;
      StdMeshers_FaceSide* side2 = quad->side[ i2 ];
      const vector<UVPtStruct>& uvVec2 = side2->GetUVPtStruct();
      int degenInd2 = -1;
      if ( uvVec[ degenInd ].node == uvVec2[0].node )
        degenInd2 = 0;
      else if ( uvVec[ degenInd ].node == uvVec2.back().node )
        degenInd2 = uvVec2.size() - 1;
      else
        throw SALOME_Exception( LOCALIZED( "Logical error" ));

      // move UV in the middle
      uvPtStruct& uv1 = const_cast<uvPtStruct&>( uvVec [ degenInd  ]);
      uvPtStruct& uv2 = const_cast<uvPtStruct&>( uvVec2[ degenInd2 ]);
      uv1.u = uv2.u = 0.5 * ( uv1.u + uv2.u );
      uv1.v = uv2.v = 0.5 * ( uv1.v + uv2.v );
    }

  else if ( quad->side.size() == 4 )

    // Set number of nodes on a degenerated side to be same as on an opposite side
    // ----------------------------------------------------------------------------
    for ( unsigned i = 0; i < quad->side.size(); ++i )
    {
      StdMeshers_FaceSide* degSide = quad->side[i];
      if ( !myHelper->IsDegenShape( degSide->EdgeID(0) ))
        continue;
      StdMeshers_FaceSide* oppSide = quad->side[( i+2 ) % quad->side.size() ];
      if ( degSide->NbSegments() == oppSide->NbSegments() )
        continue;

      // make new side data
      const vector<UVPtStruct>& uvVecDegOld = degSide->GetUVPtStruct();
      const SMDS_MeshNode*   n = uvVecDegOld[0].node;
      Handle(Geom2d_Curve) c2d = degSide->Curve2d(0);
      double f = degSide->FirstU(0), l = degSide->LastU(0);
      gp_Pnt2d p1( uvVecDegOld.front().u, uvVecDegOld.front().v );
      gp_Pnt2d p2( uvVecDegOld.back().u,  uvVecDegOld.back().v );

      delete degSide;
      quad->side[i] = new StdMeshers_FaceSide( oppSide, n, &p1, &p2, c2d, f, l );
    }
}

//================================================================================
/*!
 * \brief Perform smoothing of 2D elements on a FACE with ignored degenerated EDGE
 */
//================================================================================

void StdMeshers_Quadrangle_2D::smooth (FaceQuadStruct::Ptr quad)
{
  if ( !myNeedSmooth ) return;

  // Get nodes to smooth

  typedef map< const SMDS_MeshNode*, TSmoothNode, TIDCompare > TNo2SmooNoMap;
  TNo2SmooNoMap smooNoMap;

  const TopoDS_Face&  geomFace = TopoDS::Face( myHelper->GetSubShape() );
  Handle(Geom_Surface) surface = BRep_Tool::Surface( geomFace );
  double U1, U2, V1, V2;
  surface->Bounds(U1, U2, V1, V2);
  GeomAPI_ProjectPointOnSurf proj;
  proj.Init( surface, U1, U2, V1, V2, BRep_Tool::Tolerance( geomFace ) );

  SMESHDS_Mesh*        meshDS = myHelper->GetMeshDS();
  SMESHDS_SubMesh*   fSubMesh = meshDS->MeshElements( geomFace );
  SMDS_NodeIteratorPtr    nIt = fSubMesh->GetNodes();
  while ( nIt->more() ) // loop on nodes bound to a FACE
  {
    const SMDS_MeshNode* node = nIt->next();
    TSmoothNode & sNode = smooNoMap[ node ];
    sNode._uv  = myHelper->GetNodeUV( geomFace, node );
    sNode._xyz = SMESH_TNodeXYZ( node );

    // set sNode._triangles
    SMDS_ElemIteratorPtr fIt = node->GetInverseElementIterator( SMDSAbs_Face );
    while ( fIt->more() )
    {
      const SMDS_MeshElement* face = fIt->next();
      const int nbN = face->NbCornerNodes();
      const int nInd = face->GetNodeIndex( node );
      const int prevInd = myHelper->WrapIndex( nInd - 1, nbN );
      const int nextInd = myHelper->WrapIndex( nInd + 1, nbN );
      const SMDS_MeshNode* prevNode = face->GetNode( prevInd );
      const SMDS_MeshNode* nextNode = face->GetNode( nextInd );
      sNode._triangles.push_back( TTriangle( & smooNoMap[ prevNode ],
                                             & smooNoMap[ nextNode ]));
    }
  }
  // set _uv of smooth nodes on FACE boundary
  for ( unsigned i = 0; i < quad->side.size(); ++i )
  {
    const vector<UVPtStruct>& uvVec = quad->side[i]->GetUVPtStruct();
    for ( unsigned j = 0; j < uvVec.size(); ++j )
    {
      TSmoothNode & sNode = smooNoMap[ uvVec[j].node ];
      sNode._uv.SetCoord( uvVec[j].u, uvVec[j].v );
      sNode._xyz = SMESH_TNodeXYZ( uvVec[j].node );
    }
  }

  // define refernce orientation in 2D
  TNo2SmooNoMap::iterator n2sn = smooNoMap.begin();
  for ( ; n2sn != smooNoMap.end(); ++n2sn )
    if ( !n2sn->second._triangles.empty() )
      break;
  if ( n2sn == smooNoMap.end() ) return;
  const TSmoothNode & sampleNode = n2sn->second;
  const bool refForward = ( sampleNode._triangles[0].IsForward( sampleNode._uv ));

  // Smoothing

  for ( int iLoop = 0; iLoop < 5; ++iLoop )
  {
    for ( n2sn = smooNoMap.begin(); n2sn != smooNoMap.end(); ++n2sn )
    {
      TSmoothNode& sNode = n2sn->second;
      if ( sNode._triangles.empty() )
        continue; // not movable node

      // compute a new XYZ
      gp_XYZ newXYZ (0,0,0);
      for ( unsigned i = 0; i < sNode._triangles.size(); ++i )
        newXYZ += sNode._triangles[i]._n1->_xyz;
      newXYZ /= sNode._triangles.size();

      // compute a new UV by projection
      gp_XY newUV;
      proj.Perform( newXYZ );
      bool isValid = ( proj.IsDone() && proj.NbPoints() > 0 );
      if ( isValid )
      {
        // check validity of the newUV
        Quantity_Parameter u,v;
        proj.LowerDistanceParameters( u, v );
        newUV.SetCoord( u, v );
        for ( unsigned i = 0; i < sNode._triangles.size() && isValid; ++i )
          isValid = ( sNode._triangles[i].IsForward( newUV ) == refForward );
      }
      if ( !isValid )
      {
        // compute a new UV by averaging
        newUV.SetCoord(0.,0.);
        for ( unsigned i = 0; i < sNode._triangles.size(); ++i )
          newUV += sNode._triangles[i]._n1->_uv;
        newUV /= sNode._triangles.size();

        // check validity of the newUV
        isValid = true;
        for ( unsigned i = 0; i < sNode._triangles.size() && isValid; ++i )
          isValid = ( sNode._triangles[i].IsForward( newUV ) == refForward );
      }
      if ( isValid )
      {
        sNode._uv = newUV;
        sNode._xyz = surface->Value( newUV.X(), newUV.Y() ).XYZ();
      }
    }
  }

  // Set new XYZ to the smoothed nodes

  for ( n2sn = smooNoMap.begin(); n2sn != smooNoMap.end(); ++n2sn )
  {
    TSmoothNode& sNode = n2sn->second;
    if ( sNode._triangles.empty() )
      continue; // not movable node

    SMDS_MeshNode* node = const_cast< SMDS_MeshNode*>( n2sn->first );
    gp_Pnt xyz = surface->Value( sNode._uv.X(), sNode._uv.Y() );
    meshDS->MoveNode( node, xyz.X(), xyz.Y(), xyz.Z() );

    // store the new UV
    node->SetPosition( SMDS_PositionPtr( new SMDS_FacePosition( sNode._uv.X(), sNode._uv.Y() )));
  }

  // Move medium nodes in quadratic mesh
  if ( _quadraticMesh )
  {
    const TLinkNodeMap& links = myHelper->GetTLinkNodeMap();
    TLinkNodeMap::const_iterator linkIt = links.begin();
    for ( ; linkIt != links.end(); ++linkIt )
    {
      const SMESH_TLink& link = linkIt->first;
      SMDS_MeshNode*     node = const_cast< SMDS_MeshNode*>( linkIt->second );

      if ( node->getshapeId() != myHelper->GetSubShapeID() )
        continue; // medium node is on EDGE or VERTEX

      gp_XY uv1 = myHelper->GetNodeUV( geomFace, link.node1(), node );
      gp_XY uv2 = myHelper->GetNodeUV( geomFace, link.node2(), node );

      gp_XY uv  = myHelper->GetMiddleUV( surface, uv1, uv2 );
      node->SetPosition( SMDS_PositionPtr( new SMDS_FacePosition( uv.X(), uv.Y() )));
      
      gp_Pnt xyz = surface->Value( uv.X(), uv.Y() );
      meshDS->MoveNode( node, xyz.X(), xyz.Y(), xyz.Z() );
    }
  }
}

/*//================================================================================
/*!
 * \brief Finds vertices at the most sharp face corners
 *  \param [in] theFace - the FACE
 *  \param [in,out] theWire - the ordered edges of the face. It can be modified to
 *         have the first VERTEX of the first EDGE in \a vertices
 *  \param [out] theVertices - the found corner vertices in the order corresponding to
 *         the order of EDGEs in \a theWire
 *  \param [out] theNbDegenEdges - nb of degenerated EDGEs in theFace
 *  \param [in] theConsiderMesh - if \c true, only meshed VERTEXes are considered
 *         as possible corners
 *  \return int - number of quad sides found: 0, 3 or 4
 */
//================================================================================

int StdMeshers_Quadrangle_2D::getCorners(const TopoDS_Face&          theFace,
                                         SMESH_Mesh &                theMesh,
                                         std::list<TopoDS_Edge>&     theWire,
                                         std::vector<TopoDS_Vertex>& theVertices,
                                         int &                       theNbDegenEdges,
                                         const bool                  theConsiderMesh)
{
  theNbDegenEdges = 0;

  SMESH_MesherHelper helper( theMesh );

  // sort theVertices by angle
  multimap<double, TopoDS_Vertex> vertexByAngle;
  TopTools_DataMapOfShapeReal angleByVertex;
  TopoDS_Edge prevE = theWire.back();
  if ( SMESH_Algo::isDegenerated( prevE ))
  {
    list<TopoDS_Edge>::reverse_iterator edge = ++theWire.rbegin();
    while ( SMESH_Algo::isDegenerated( *edge ))
      ++edge;
    if ( edge == theWire.rend() )
      return false;
    prevE = *edge;
  }
  list<TopoDS_Edge>::iterator edge = theWire.begin();
  for ( ; edge != theWire.end(); ++edge )
  {
    if ( SMESH_Algo::isDegenerated( *edge ))
    {
      ++theNbDegenEdges;
      continue;
    }
    TopoDS_Vertex v = helper.IthVertex( 0, *edge );
    if ( !theConsiderMesh || SMESH_Algo::VertexNode( v, helper.GetMeshDS() ))
    {
      double angle = SMESH_MesherHelper::GetAngle( prevE, *edge, theFace );
      vertexByAngle.insert( make_pair( angle, v ));
      angleByVertex.Bind( v, angle );
    }
    prevE = *edge;
  }

  // find out required nb of corners (3 or 4)
  int nbCorners = 4;
  TopoDS_Shape triaVertex = helper.GetMeshDS()->IndexToShape( myTriaVertexID );
  if ( !triaVertex.IsNull() &&
       triaVertex.ShapeType() == TopAbs_VERTEX &&
       helper.IsSubShape( triaVertex, theFace ))
    nbCorners = 3;
  else
    triaVertex.Nullify();

  // check nb of available corners
  if ( nbCorners == 3 )
  {
    if ( vertexByAngle.size() < 3 )
      return error(COMPERR_BAD_SHAPE,
                   TComm("Face must have 3 sides but not ") << vertexByAngle.size() );
  }
  else
  {
    if ( vertexByAngle.size() == 3 && theNbDegenEdges == 0 )
    {
      if ( myTriaVertexID < 1 )
        return error(COMPERR_BAD_PARMETERS,
                     "No Base vertex provided for a trilateral geometrical face");
        
      TComm comment("Invalid Base vertex: ");
      comment << myTriaVertexID << " its ID is not among [ ";
      multimap<double, TopoDS_Vertex>::iterator a2v = vertexByAngle.begin();
      comment << helper.GetMeshDS()->ShapeToIndex( a2v->second ) << ", "; a2v++;
      comment << helper.GetMeshDS()->ShapeToIndex( a2v->second ) << ", "; a2v++;
      comment << helper.GetMeshDS()->ShapeToIndex( a2v->second ) << " ]";
      return error(COMPERR_BAD_PARMETERS, comment );
    }
    if ( vertexByAngle.size() + ( theNbDegenEdges > 0 ) < 4 &&
         vertexByAngle.size() + theNbDegenEdges != 4 )
      return error(COMPERR_BAD_SHAPE,
                   TComm("Face must have 4 sides but not ") << vertexByAngle.size() );
  }

  // put all corner vertices in a map
  TopTools_MapOfShape vMap;
  if ( nbCorners == 3 )
    vMap.Add( triaVertex );
  multimap<double, TopoDS_Vertex>::reverse_iterator a2v = vertexByAngle.rbegin();
  for ( ; a2v != vertexByAngle.rend() && vMap.Extent() < nbCorners; ++a2v )
    vMap.Add( (*a2v).second );

  // check if there are possible variations in choosing corners
  bool isThereVariants = false;
  if ( vertexByAngle.size() > nbCorners )
  {
    double lostAngle = a2v->first;
    double lastAngle = ( --a2v, a2v->first );
    isThereVariants  = ( lostAngle * 1.1 >= lastAngle );
  }

  // make theWire begin from a corner vertex or triaVertex
  if ( nbCorners == 3 )
    while ( !triaVertex.IsSame( ( helper.IthVertex( 0, theWire.front() ))) ||
            SMESH_Algo::isDegenerated( theWire.front() ))
      theWire.splice( theWire.end(), theWire, theWire.begin() );
  else
    while ( !vMap.Contains( helper.IthVertex( 0, theWire.front() )) ||
            SMESH_Algo::isDegenerated( theWire.front() ))
      theWire.splice( theWire.end(), theWire, theWire.begin() );

  // fill the result vector and prepare for its refinement
  theVertices.clear();
  vector< double >      angles;
  vector< TopoDS_Edge > edgeVec;
  vector< int >         cornerInd;
  angles.reserve( vertexByAngle.size() );
  edgeVec.reserve( vertexByAngle.size() );
  cornerInd.reserve( nbCorners );
  for ( edge = theWire.begin(); edge != theWire.end(); ++edge )
  {
    if ( SMESH_Algo::isDegenerated( *edge ))
      continue;
    TopoDS_Vertex v = helper.IthVertex( 0, *edge );
    bool   isCorner = vMap.Contains( v );
    if ( isCorner )
    {
      theVertices.push_back( v );
      cornerInd.push_back( angles.size() );
    }
    angles.push_back( angleByVertex.IsBound( v ) ? angleByVertex( v ) : -M_PI );
    edgeVec.push_back( *edge );
  }

  // refine the result vector - make sides elual by length if
  // there are several equal angles
  if ( isThereVariants )
  {
    if ( nbCorners == 3 )
      angles[0] = 2 * M_PI; // not to move the base triangle VERTEX

    set< int > refinedCorners;
    for ( size_t iC = 0; iC < cornerInd.size(); ++iC )
    {
      int iV = cornerInd[iC];
      if ( !refinedCorners.insert( iV ).second )
        continue;
      list< int > equalVertices;
      equalVertices.push_back( iV );
      int nbC[2] = { 0, 0 };
      // find equal angles backward and forward from the iV-th corner vertex
      for ( int isFwd = 0; isFwd < 2; ++isFwd )
      {
        int     dV = isFwd ? +1 : -1;
        int iCNext = helper.WrapIndex( iC + dV, cornerInd.size() );
        int iVNext = helper.WrapIndex( iV + dV, angles.size() );
        while ( iVNext != iV )
        {
          bool equal = Abs( angles[iV] - angles[iVNext] ) < 0.1 * angles[iV];
          if ( equal )
            equalVertices.insert( isFwd ? equalVertices.end() : equalVertices.begin(), iVNext );
          if ( iVNext == cornerInd[ iCNext ])
          {
            if ( !equal )
              break;
            nbC[ isFwd ]++;
            refinedCorners.insert( cornerInd[ iCNext ] );
            iCNext = helper.WrapIndex( iCNext + dV, cornerInd.size() );
          }
          iVNext = helper.WrapIndex( iVNext + dV, angles.size() );
        }
      }
      // move corners to make sides equal by length
      int nbEqualV  = equalVertices.size();
      int nbExcessV = nbEqualV - ( 1 + nbC[0] + nbC[1] );
      if ( nbExcessV > 0 )
      {
        // calculate normalized length of each side enclosed between neighbor equalVertices
        vector< double > curLengths;
        double totalLen = 0;
        vector< int > evVec( equalVertices.begin(), equalVertices.end() );
        int   iEV = 0;
        int    iE = cornerInd[ helper.WrapIndex( iC - nbC[0] - 1, cornerInd.size() )];
        int iEEnd = cornerInd[ helper.WrapIndex( iC + nbC[1] + 1, cornerInd.size() )];
        while ( curLengths.size() < nbEqualV + 1 )
        {
          curLengths.push_back( totalLen );
          do {
            curLengths.back() += SMESH_Algo::EdgeLength( edgeVec[ iE ]);
            iE = helper.WrapIndex( iE + 1, edgeVec.size());
            if ( iEV < evVec.size() && iE == evVec[ iEV++ ] )
              break;
          }
          while( iE != iEEnd );
          totalLen = curLengths.back();
        }
        curLengths.resize( equalVertices.size() );
        for ( size_t iS = 0; iS < curLengths.size(); ++iS )
          curLengths[ iS ] /= totalLen;

        // find equalVertices most close to the ideal sub-division of all sides
        int iBestEV = 0;
        int iCorner = helper.WrapIndex( iC - nbC[0], cornerInd.size() );
        int nbSides = 2 + nbC[0] + nbC[1];
        for ( int iS = 1; iS < nbSides; ++iS, ++iBestEV )
        {
          double idealLen = iS / double( nbSides );
          double d, bestDist = 1.;
          for ( iEV = iBestEV; iEV < curLengths.size(); ++iEV )
            if (( d = Abs( idealLen - curLengths[ iEV ])) < bestDist )
            {
              bestDist = d;
              iBestEV  = iEV;
            }
          if ( iBestEV > iS-1 + nbExcessV )
            iBestEV = iS-1 + nbExcessV;
          theVertices[ iCorner ] = helper.IthVertex( 0, edgeVec[ evVec[ iBestEV ]]);
          iCorner = helper.WrapIndex( iCorner + 1, cornerInd.size() );
        }
      }
    }
  }

  return nbCorners;
}
