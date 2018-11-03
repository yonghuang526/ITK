# Contact: Davis Marc Vigneault <davis.vigneault@gmail.com>
itk_fetch_module(DVMeshNoise
  "Perturb itk::Mesh and itk::QuadEdgeMesh coordinates with Gaussian noise.
  Please see the following Insight Journal article for an introduction to this module:
  Vigneault, DM.  Perturbing Mesh Vertices with Additive Gaussian Noise.
  http://hdl.handle.net/10380/3567
  "
  GIT_REPOSITORY ${git_protocol}://github.com/InsightSoftwareConsortium/DVMeshNoise
  GIT_TAG 88aa6efb1fde9009c92ba86bb8107dcfff38ec03
  )
