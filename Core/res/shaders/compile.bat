%VULKAN_SDK%/Bin/glslc.exe Triangle.vert -o vert.spv
%VULKAN_SDK%/Bin/glslc.exe Triangle.frag -o frag.spv
%VULKAN_SDK%/Bin/glslc.exe Depth.frag -o depth_frag.spv
%VULKAN_SDK%/Bin/glslc.exe Depth.vert -o depth_vert.spv
xcopy .\ ..\..\..\out\build\x64-Debug\Editor\res\shaders /s /e /h /Y > nul
xcopy .\ ..\..\..\out\build\x64-Release\Editor\res\shaders /s /e /h /Y > nul