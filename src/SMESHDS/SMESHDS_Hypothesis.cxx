//  SMESH SMESHDS : management of mesh data and SMESH document
//
//  Copyright (C) 2003  OPEN CASCADE, EADS/CCR, LIP6, CEA/DEN,
//  CEDRAT, EDF R&D, LEG, PRINCIPIA R&D, BUREAU VERITAS 
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
//  See http://www.opencascade.org/SALOME/ or email : webmaster.salome@opencascade.org 
//
//
//
//  File   : SMESHDS_Hypothesis.cxx
//  Author : Paul RASCLE, EDF
//  Module : SMESH
//  $Header$

using namespace std;
using namespace std;
#include "SMESHDS_Hypothesis.hxx"


//=============================================================================
/*!
 * 
 */
//=============================================================================

SMESHDS_Hypothesis::SMESHDS_Hypothesis(int hypId)
{
//   MESSAGE("SMESHDS_Hypothesis::SMESHDS_Hypothesis");
  _hypId = hypId;
  _name = "generic";
//   SCRUTE(_name);
//   SCRUTE(_hypId);
}

//=============================================================================
/*!
 * 
 */
//=============================================================================

SMESHDS_Hypothesis::~SMESHDS_Hypothesis()
{
//   MESSAGE("SMESHDS_Hypothesis::~SMESHDS_Hypothesis");
}

//=============================================================================
/*!
 * 
 */
//=============================================================================

const char* SMESHDS_Hypothesis::GetName() const
{
//   MESSAGE("SMESHDS_Hypothesis::GetName");
//   SCRUTE(_name);
//   SCRUTE(&_name);
  return _name.c_str();
}

//=============================================================================
/*!
 * 
 */
//=============================================================================

int SMESHDS_Hypothesis::GetID() const
{
//   MESSAGE("SMESHDS_Hypothesis::GetId");
//   SCRUTE(_hypId);
  return _hypId;
}

//=============================================================================
/*!
 * 
 */
//=============================================================================

int SMESHDS_Hypothesis::GetType() const
{
//   MESSAGE("SMESHDS_Hypothesis::GetType");
//   SCRUTE(_type);
  return _type;
}

void SMESHDS_Hypothesis::SetID(int id)
{
	_hypId=id;
}