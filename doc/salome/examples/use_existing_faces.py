# Usage of "Use Faces to be Created Manually" algorithm


import salome
salome.salome_init_without_session()
import GEOM
from salome.geom import geomBuilder
geompy = geomBuilder.New()

import SMESH, SALOMEDS
from salome.smesh import smeshBuilder
smesh =  smeshBuilder.New()
import salome_notebook

# define my 2D algorithm
def my2DMeshing(geomFace, geom_builder, smesh_builder):
    import numpy as np

    # find gravity center of geomFace
    gcXYZ = geom_builder.PointCoordinates( geom_builder.MakeCDG( geomFace ))

    # define order and orientation of edges
    sortedEdges = []
    geomEdges = geom_builder.SubShapeAll( geomFace, geom_builder.ShapeType["EDGE"])
    sortedEdges.append(( geomEdges.pop(0), True ))
    while geomEdges:
        prevEdge_rev = sortedEdges[ -1 ]
        prevVV = geom_builder.SubShapeAll( prevEdge_rev[0], geom_builder.ShapeType["VERTEX"])
        prevV2 = prevVV[ prevEdge_rev[1] ]
        found = False
        for iE in range( len( geomEdges )):
            v1,v2 = geom_builder.SubShapeAll( geomEdges[ iE ], geom_builder.ShapeType["VERTEX"])
            same1,same2 = [( geom_builder.MinDistance( prevV2, v ) < 1e-7 ) for v in [v1,v2] ]
            if not same1 and not same2: continue
            sortedEdges.append(( geomEdges.pop( iE ), same1 ))
            found = True
            break
        assert found
    sortedEdges.reverse()

    # put nodes on edges in a right order
    nodes = []
    for edge, isForward in sortedEdges:
        v1,v2 = geom_builder.SubShapeAll( edge, geom_builder.ShapeType["VERTEX"])
        edgeNodes = smesh_builder.GetSubMeshNodesId( v2,   all=False ) + \
                    smesh_builder.GetSubMeshNodesId( edge, all=False ) + \
                    smesh_builder.GetSubMeshNodesId( v1,   all=False )
        if not isForward: edgeNodes.reverse()
        nodes.extend( edgeNodes[:-1] )

    # create nodes inside the geomFace
    r1 = 0.6
    r2 = 1 - r1
    nodesInside = []
    for n in nodes:
        nXYZ = smesh_builder.GetNodeXYZ( n )
        newXYZ = np.add( np.multiply( r1, gcXYZ ), np.multiply( r2, nXYZ ))
        nodesInside.append( smesh_builder.AddNode( newXYZ[0], newXYZ[1], newXYZ[2] ))
        smesh_builder.SetNodeOnFace( nodesInside[-1], geomFace, 0, 0 )

    # find out orientation of faces to create
    #    geomFace normal
    faceNorm = geom_builder.GetNormal( geomFace )
    v1,v2 = [ geom_builder.PointCoordinates( v ) \
              for v in geom_builder.SubShapeAll( faceNorm, geom_builder.ShapeType["VERTEX"]) ]
    faceNormXYZ = np.subtract( v2, v1 )
    outDirXYZ   = np.subtract( v1, [ 50, 50, 50 ] )
    if np.dot( faceNormXYZ, outDirXYZ ) < 0: # reversed face
        faceNormXYZ = np.multiply( -1., faceNormXYZ )
    #   mesh face normal
    e1 = np.subtract( smesh_builder.GetNodeXYZ( nodes[0] ), smesh_builder.GetNodeXYZ( nodes[1] ))
    e2 = np.subtract( smesh_builder.GetNodeXYZ( nodes[0] ), smesh_builder.GetNodeXYZ( nodesInside[0] ))
    meshNorm = np.cross( e1, e2 )
    #   faces orientation
    reverse = ( np.dot( faceNormXYZ, meshNorm ) < 0 )

    # create mesh faces
    iN = len( nodes )
    while iN:
        n1, n2, n3, n4 = nodes[iN-1], nodes[iN-2], nodesInside[iN-2], nodesInside[iN-1]
        iN -= 1
        if reverse:
            f = smesh_builder.AddFace( [n1, n2, n3, n4] )
        else:
            f = smesh_builder.AddFace( [n4, n3, n2, n1] )
        # new faces must be assigned to geometry to allow 3D algorithm finding them
        smesh_builder.SetMeshElementOnShape( f, geomFace )

    if reverse:
        nodesInside.reverse()
    polygon = smesh_builder.AddPolygonalFace( nodesInside )
    smesh_builder.SetMeshElementOnShape( polygon, geomFace )

    return

# create geometry and get faces to mesh with my2DMeshing()
box = geompy.MakeBoxDXDYDZ( 100, 100, 100 )
f1 = geompy.SubShapeAll( box, geompy.ShapeType["FACE"])[0]
f2 = geompy.GetOppositeFace( box, f1 )
geompy.addToStudy( box, "box" )
geompy.addToStudy( f1, "f1" )
geompy.addToStudy( f2, "f2" )

# compute 1D mesh
mesh = smesh.Mesh( box )
mesh.Segment().NumberOfSegments( 5 )
mesh.Compute()

# compute 2D mesh
mesh.Quadrangle()
mesh.UseExistingFaces(f1) # UseExistingFaces() allows using my2DMeshing();
mesh.UseExistingFaces(f2) # assign UseExistingFaces() BEFORE calling my2DMeshing()!
my2DMeshing(f1, geom_builder=geompy, smesh_builder=mesh)
my2DMeshing(f2, geom_builder=geompy, smesh_builder=mesh)
assert mesh.Compute()

# compute 3D mesh
mesh.Prism()
assert mesh.Compute()
