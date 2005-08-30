//  SMESH SMDS : implementaion of Salome mesh data structure
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
//  File   : SMDS_Position.hxx
//  Module : SMESH

#ifndef _SMDS_Position_HeaderFile
#define _SMDS_Position_HeaderFile

#include "SMDS_TypeOfPosition.hxx"
#include <boost/shared_ptr.hpp>

//#ifdef WNT
//#include <SALOME_WNT.hxx>
//#else
//#define SALOME_WNT_EXPORT
//#endif

#if defined WNT && defined WIN32 && defined SMDS_EXPORTS
#define SMDS_WNT_EXPORT __declspec( dllexport )
#else
#define SMDS_WNT_EXPORT
#endif

class SMDS_Position;
typedef boost::shared_ptr<SMDS_Position> SMDS_PositionPtr;


class SMDS_WNT_EXPORT SMDS_Position
{

  public:
	const virtual double * Coords() const = 0;
	virtual SMDS_TypeOfPosition GetTypeOfPosition() const = 0;
	virtual int GetDim() const;
	void SetShapeId(int aShapeId);
	int GetShapeId() const;
	virtual ~SMDS_Position() {}

  protected:
	  SMDS_Position(int aShapeId);

  private:
	int myShapeId;
};


#endif
