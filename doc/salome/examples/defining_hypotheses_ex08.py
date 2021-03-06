# Propagation

import salome
salome.salome_init_without_session()
import GEOM
from salome.geom import geomBuilder
geompy = geomBuilder.New()

import SMESH, SALOMEDS
from salome.smesh import smeshBuilder
smesh =  smeshBuilder.New()

# create a box
base = geompy.MakeSketcher("Sketcher:F 0 0:TT 10 0:TT 20 10:TT 0 10:WF", theName="F")
box  = geompy.MakePrismDXDYDZ( base, 0,0,10 )
geompy.addToStudy(box, "Box")

# get one edge of the box to put local hypothesis on
p5 = geompy.MakeVertex(5., 0., 0.)
EdgeX = geompy.GetEdgeNearPoint(box, p5)
geompy.addToStudyInFather(box, EdgeX, "Edge [0,0,0 - 10,0,0]")

# create a hexahedral mesh on the box
hexa = smesh.Mesh(box, "Propagation of hypothesis")

# set global algorithms and hypotheses
algo1D = hexa.Segment()
hexa.Quadrangle()
hexa.Hexahedron()
algo1D.NumberOfSegments(4)

# create a sub-mesh with local 1D hypothesis and "Propagation of 1D Hypothesis"
algo_local = hexa.Segment(EdgeX)

# define "Arithmetic1D" hypothesis to cut an edge in several segments with increasing length
algo_local.Arithmetic1D(1, 4)

# define "Propagation" hypothesis that propagates "Arithmetic1D" hypothesis
# from 'EdgeX' on opposite sides of all quadilateral faces
algo_local.Propagation()

# compute the mesh which contains prisms
hexa.Compute()


# create another mesh on the box
mesh = smesh.Mesh(box, "Propagation of distribution of nodes")

# set global algorithms and hypotheses
algo1D = mesh.Segment()
mesh.Quadrangle()
mesh.Hexahedron()
algo1D.NumberOfSegments(4)

# create a sub-mesh with local 1D hypothesis and "Propagation of Node Distribution"
algo_local = mesh.Segment(EdgeX)
algo_local.Arithmetic1D(1, 4)

# define "Propagation Of Distribution" hypothesis that propagates
# distribution of nodes generated by "Arithmetic1D" hypothesis
# from 'EdgeX' on opposite sides of all quadilateral faces
algo_local.PropagationOfDistribution()

# compute the mesh which contains hexahedra only
mesh.Compute()
