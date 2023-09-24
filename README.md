# How to Convert OpenVDB into Texture3D and Display it in Godot (4.1) Spatial Shader

![Alt text](screenshot.png?raw=true "Title")

This project is a memo of how to convert OpenVDB data into Godot. 

The Godot project might work if the large texture3D (~70MB) upload correcly. But the C++ definitely won't work because it depends on OpenVDB library. Check the **OpenVDB Library** section for detail.

## Background Story

Godot does not support OpenVDB importing, at least in the latest 4.2 version. To display the VDB data in Godot, there are two steps we need to achieve:

* Create Tool to Convert VDB file into Godot's Texture3D
* Use Godot Spatial Shader to Access and Display Texture3D

In this project, you can find some code to demostrate both steps. The VDB to Texture3D convertor is under *\cpp\\* folder. It is just a simple main.cpp with no makefile. In main directory, there are two shader files, *local_space_vdb.gdshader* and *world_space_vdb.gdshader*. Both are spatial type shows how to display a texture3D inside box mesh using ray marching. The different is one is using local coordinate and the other is using world coordinate.

## Fog Shader vs Spatial Shader?

Godot 4.0 introduced **Fog Shader**, which is also a volumetric shader type. It provides vec3 (UVW) position in space, and allow us to assign ALBEDO, ALPHA and EMISSION to material output. It looks like a VDB solution. Unfortunately it is not. The **Fog Shader** is optimized for blurred fog. It cannot render clear image. But when using VDB, we usually want some clear result. So... **Fog Shader** is not general volumetric shader. We still need to implement the spatial shader using raymarching by ourselves. 

## OpenVDB Library

I use OpenVDB library to read the VDB file in my C++ tool. In order to compile that standalone, colleage homework style *cpp_tool/main.cpp*, you need to build the OpenVDB first. 

* OpenVDB Libray Github: https://github.com/AcademySoftwareFoundation/openvdb
* OpenVDB API cookbook: https://www.openvdb.org/documentation/doxygen/codeExamples.html

## OpenVDB to Texture3D tool

The source code *cpp_tool/main.cpp* has two major functions. One is reading voxel data from OpenVDB file, the other is writing voxel data into 24bits/pixel BMP file. It is a small file with only 2xx lines. It has many precondition and does not have good error handling. Here are something you need to know before use/rewrite it.

* The code assumes the input VDB file contains both 'density' and 'flames' grid.
* The code assumes the input VDB file contains 'file_bbox_max' metadata, which is Vec3i type. If you export the VDB from Embergen, then it is not a problem. Otherwise you might need to calculate the simulation bounary by yourself.
* The output file use 24 bits/pixel BMP format. For each pixel, the RED value is density and the GREEN is flames. Since each data has only one byte length, it is imprecise. Check the comment inside *cpp_tool/main.cpp* if you need high precise value.
* Godot Texture3D uses single BMP to represents 3D data. In order to do that, it slices one image just like 2D animation sprites. The maximum slice is H:256 V:256. In other words, if the maximum high of youe VDB model is larger than 65,535, it causes problem in Godot.

Here are somthing that might help to to integrate openVDB library and compile that *cpp_tool/main.cpp* using Visual Studio IDE:

* Disable the *Properties -> C/C++ -> SDL Check*.
* Choose C++17 standard in *Properties -> C/C++ -> Language*.

## Spatial Shader for VDB display

The shader *local_space_vdb.gdshader* and *world_space_vdb.gdshader* show two things:

* How to get certain 3D point's (UVW) 'density' from Texture3D?
* How to draw the volumetric density inside Box Mesh using ray-marching.

The first part is straight-forward. The second one might confusing some people who don't familar with ray-marcging. Just google 'ray marching' or 'SDF' to get some idea.

## Further Work

* To save the memory, we should implement the Texture3D with GDExtension/C++Module, which read the voxel data from openVDB grid directly.
* Implement Texture3DArray to animate the VDB data, and then check the performance.
* Use compilcate color mapping to see the 'real' result, instead of density. 
