//  SMESH SMESHDS : idl implementation based on 'SMESH' unit's classes
//
//  Copyright (C) 2004  CEA
// 
//  This library is free software; you can redistribute it and/or 
//  modify it under the terms of the GNU Lesser General Public 
//  License as published by the Free Software Foundation; either 
//  version 2.1 of the License. 
// 
//  This library is distributed in the hope that it will be useful, 
//  but WITHOUT ANY WARRANTY; without even the implied warranty of 
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
//  Lesser General Public License for more details. 
// 
//  You should have received a copy of the GNU Lesser General Public 
//  License along with this library; if not, write to the Free Software 
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA 
// 
//  See http://www.salome-platform.org or email : webmaster.salome@opencascade.org 
//
//
//
//  File   : SMESHDS_Group.cxx
//  Module : SMESH
//  $Header$

#include "SMESHDS_Group.hxx"
#include "SMESHDS_Mesh.hxx"

using namespace std;

//=============================================================================
/*!
 *  
 */
//=============================================================================

SMESHDS_Group::SMESHDS_Group (const int                 theID,
                              const SMESHDS_Mesh*       theMesh,
                              const SMDSAbs_ElementType theType)
     : SMESHDS_GroupBase(theID,theMesh,theType),
       myGroup(theMesh,theType)
{
}

//=======================================================================
//function : Extent
//purpose  : 
//=======================================================================

int SMESHDS_Group::Extent()
{
  return myGroup.Extent();
}

//=======================================================================
//function : IsEmpty
//purpose  : 
//=======================================================================

bool SMESHDS_Group::IsEmpty()
{
  return myGroup.IsEmpty();
}

//=============================================================================
/*!
 *  
 */
//=============================================================================

bool SMESHDS_Group::Contains (const int theID)
{
  const SMDS_MeshElement* aElem = findInMesh (theID);
  if (aElem)
    return myGroup.Contains(aElem);
  return false;
}

//=============================================================================
/*!
 *  
 */
//=============================================================================

bool SMESHDS_Group::Add (const int theID)
{
  const SMDS_MeshElement* aElem = findInMesh (theID);
  if (!aElem || myGroup.Contains(aElem))
    return false;

  if (myGroup.IsEmpty())
    SetType( aElem->GetType() );

  myGroup.Add (aElem);
  resetIterator();
  return true;
}

//=============================================================================
/*!
 *  
 */
//=============================================================================

bool SMESHDS_Group::Remove (const int theID)
{
  const SMDS_MeshElement* aElem = findInMesh (theID);
  if (!aElem || !myGroup.Contains(aElem))
    return false;
  myGroup.Remove (aElem);
  resetIterator();
  return true;
}

//=======================================================================
//function : Clear
//purpose  : 
//=======================================================================

void SMESHDS_Group::Clear()
{
  myGroup.Clear();
  resetIterator();
}

// =====================
// class MyGroupIterator
// =====================

class MyGroupIterator: public SMDS_ElemIterator
{
  const SMDS_MeshGroup& myGroup;
 public:
  MyGroupIterator(const SMDS_MeshGroup& group): myGroup(group) { myGroup.InitIterator(); }
  bool more() { return myGroup.More(); }
  const SMDS_MeshElement* next() { return myGroup.Next(); }
};
   
//=======================================================================
//function : GetElements
//purpose  : 
//=======================================================================

SMDS_ElemIteratorPtr SMESHDS_Group::GetElements()
{
  return SMDS_ElemIteratorPtr( new MyGroupIterator ( myGroup ));
}

//=======================================================================
//function : SetType
//purpose  : 
//=======================================================================

void SMESHDS_Group::SetType(SMDSAbs_ElementType theType)
{
  if ( myGroup.IsEmpty() || GetType() == SMDSAbs_All ) {
    SMESHDS_GroupBase::SetType( theType );
    myGroup.SetType ( theType );
  }
  else
    SMESHDS_GroupBase::SetType( myGroup.GetType() );
}

