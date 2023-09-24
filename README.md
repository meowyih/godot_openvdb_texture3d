# How to convert OpenVDB into Texture3D and Access it in Godot (4.1) Spatial Shader

This project is a memo of how to convert OpenVDB data into Godot for myself.

## Background Story

Godot does not support OpenVDB, at least before version 4.2. To display the VDB data in Godot, there are two steps we need to achieve:

* Write Tool to Convert VDB file into Godot's Texture3D
* Use Godot Spatial Shader to Access and Display Texture3D

You can find some code to demostrate both steps. The VDB to Texture3D convertor is under *\cpp\\* folder. It is just a simple main.cpp with no makefile. In main directory, there are two shader files, *local_space_vdb.gdshader* and *world_space_vdb.gdshader*. Both are spatial type shows how to display a texture3D inside box mesh using ray marching. The different is one is using local coordinate and the other is using world coordinate.

## Fog Shader vs Spatial Shader?

Godot 4.0 introduced **Fog Shader**, which is also a volumetric shader type. It provides vec3 (UVW) position in space, and allow us to assign ALBEDO, ALPHA and EMISSION to material output. It looks like a VDB solution. Unfortunately it is not. The **Fog Shader** is optimized for blurred fog. It cannot render clear image. But when using VDB, we usually want some clear result. So... **Fog Shader** is not general volumetric shader. We still need to implement the spatial shader using raymarching by ourselves. 

## OpenVDB Library

I use OpenVDB library to read the VDB file in my C++ tool. In order to compile that standalone, colleage homework style *cpp_tool/main.cpp*, you need to build the OpenVDB first. 

* OpenVDB Libray Github: https://github.com/AcademySoftwareFoundation/openvdb
* OpenVDB API cookbook: https://www.openvdb.org/documentation/doxygen/codeExamples.html

NOTE: If you want to link the OpenVDB in your Visual Studio project, you might need to disable the *Properties -> C/C++ -> SDL Check*.

## OpenVDB to Texture3D tool

The source code *cpp_tool/main.cpp* has two major functions. One is reading voxel data from OpenVDB file, the other is writing voxel data into 24bits/pixel BMP file. It is a small file with only 2xx lines. As you can image, it has many precondition and does not have good error handling. Here are something you need to know before use/rewrite it.


