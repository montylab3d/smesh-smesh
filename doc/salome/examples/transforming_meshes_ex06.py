# Merging Elements


import salome
salome.salome_init_without_session()
from salome.geom import geomBuilder
geompy = geomBuilder.New()

import SMESH
from salome.smesh import smeshBuilder
smesh =  smeshBuilder.New()

# create a face to be meshed
px = geompy.MakeVertex(100., 0.  , 0.  )
py = geompy.MakeVertex(0.  , 100., 0.  )
pz = geompy.MakeVertex(0.  , 0.  , 100.)

vxy = geompy.MakeVector(px, py)
arc = geompy.MakeArc(py, pz, px)

wire = geompy.MakeWire([vxy, arc])
isPlanarFace = 1

face1 = geompy.MakeFace(wire, isPlanarFace)
id_face1 = geompy.addToStudy(face1, "Face1")

# create a circle to be an extrusion path
px1 = geompy.MakeVertex( 100.,  100.,  0.)
py1 = geompy.MakeVertex(-100., -100.,  0.)
pz1 = geompy.MakeVertex(   0.,    0., 50.)

circle = geompy.MakeCircleThreePnt(py1, pz1, px1)
id_circle = geompy.addToStudy(circle, "Path")
 
# create a 2D mesh on the face
trias = smesh.Mesh(face1, "Face : 2D mesh")

algo1D = trias.Segment()
algo1D.NumberOfSegments(6)
algo2D = trias.Triangle()
algo2D.LengthFromEdges()

trias.Compute()

# create a group of all triangles currently present in the mesh
faceTriGroup = trias.Group( face1, "face triangles" )

# create a path mesh
circlemesh = smesh.Mesh(circle, "Path mesh")
algo = circlemesh.Segment()
algo.NumberOfSegments(10)
circlemesh.Compute()

# extrusion of the mesh
trias.ExtrusionAlongPath([], circlemesh, circle, 1, MakeGroups=True )

# get a group "opposite" to faceTriGroup within the generated prismatic mesh
oppositeGroup = trias.GetGroupByName( faceTriGroup.GetName() + "_top" )[0]

# get edges of the groups
edgeGroup = trias.CreateDimGroup([ faceTriGroup, oppositeGroup ], SMESH.EDGE, "face edges")

# merge nodes of the groups only
print("Number of nodes before MergeNodes:", end=' ') 
trias.NbNodes()
tolerance = 0.001
array_of_nodes_groups = trias.FindCoincidentNodesOnPart([faceTriGroup, oppositeGroup], tolerance)

trias.MergeNodes(array_of_nodes_groups)

print("Number of nodes after MergeNodes:", trias.NbNodes())
print("")
print("Number of elements before MergeEqualElements:")
print("Edges      : ", trias.NbEdges())
print("Faces      : ", trias.NbFaces())
print("Volumes    : ", trias.NbVolumes())

# merge elements of the groups
equalFaces = trias.FindEqualElements( [faceTriGroup, oppositeGroup, edgeGroup] )
trias.MergeElements( equalFaces )
print("Number of elements after MergeEqualElements:")
print("Edges      : ", trias.NbEdges())
print("Faces      : ", trias.NbFaces())
print("Volumes    : ", trias.NbVolumes())

salome.sg.updateObjBrowser()
