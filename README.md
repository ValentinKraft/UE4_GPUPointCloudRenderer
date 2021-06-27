**Unreal Engine GPU Point Cloud Renderer**

This is a GPU-based Plugin for Real-Time rendering of dynamic and massive point cloud data in Unreal.

__This plugin is only for rendering point clouds and does NOT load point cloud files or visualise Kinect data.__
__For loading PCD files, processing point clouds with PCL or fetching data from a Kinect, another plugin will follow soon.__

__Installation__

Currently supported/tested Unreal Versions:
* __UE4.26__ (master branch)

Newer versions of the engine should probably work as well. For installation, copy the plugin to your Engine' or Project's Plugins folder.

__Usage__

The Point Cloud Renderer is implemented as a component you can add to Unreal actors/objects. For rendering point clouds, simply use the *PCR Set/Stream Input* nodes. The rendering properties can be changed by the *PCR Set Dynamic Properties* node.

Please mind that the depth-ordering of the points is not correct. For proper depth ordering, change the Blend Mode of the *DynPCMat* material to "Masked" or use my Sorting Compute Shader for in-place depth-ordering of the points: https://github.com/ValentinKraft/UE4_SortingComputeShader (and use the "WithComputeShaderSort" branch of this repository).

__License__

The plugin is free for personal and academic use. Commercial use has to be negotiated. For more information, send me an E-Mail or read the LICENSE.md.

The plugin was created in the context of a master thesis at the [CGVR institute of the University of Bremen](http://cgvr.cs.uni-bremen.de/).

__Basics Tutorial:__

[![Tutorial](https://img.youtube.com/vi/95rdEG5H8sI/0.jpg)](https://www.youtube.com/watch?v=95rdEG5H8sI)

__Demo - Huge static point cloud:__

[![Demo](https://img.youtube.com/vi/5LH6IZdmxK4/0.jpg)](https://www.youtube.com/watch?v=5LH6IZdmxK4)

__Demo - Kinect live stream:__

[![Demo](https://img.youtube.com/vi/LZwG054LC4A/0.jpg)](https://www.youtube.com/watch?v=LZwG054LC4A)
