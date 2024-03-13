forfiles /m *.glsl /p "assets\shaders" -c "cmd /c %VULKAN_SDK%/Bin/glslangValidator.exe @path -gVS -V -o assets\spriv\@fname.spv"
