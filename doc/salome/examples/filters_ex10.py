# Free edges

# initialize SALOME and modules
import salome, SMESH
salome.salome_init_without_session()
from salome.geom import geomBuilder
geompy = geomBuilder.New()
from salome.smesh import smeshBuilder
smesh =  smeshBuilder.New()

# create mesh
face = geompy.MakeFaceHW(100, 100, 1)
geompy.addToStudy( face, "quadrangle" )
mesh = smesh.Mesh(face)
mesh.Segment().NumberOfSegments(10)
mesh.Triangle().MaxElementArea(25)
mesh.Compute()

# get all faces with free edges
filter = smesh.GetFilter(SMESH.FACE, SMESH.FT_FreeEdges)
ids = mesh.GetIdsFromFilter(filter)
print("Number of faces with free edges:", len(ids))
