@echo off
REM Windows Shader Compilation Script

echo Compiling shaders for Windows...

REM Compile all shaders with correct SPIR-V versions (shaders in src/Shaders/)
glslangValidator.exe -V --target-env spirv1.3 ../Shaders/debug.comp -o ../Shaders/debug.spv
glslangValidator.exe -V --target-env spirv1.3 ../Shaders/preprocess.comp -o ../Shaders/preprocess.spv
glslangValidator.exe -V --target-env spirv1.3 ../Shaders/debugGaussians.comp -o ../Shaders/nearest.spv
glslangValidator.exe -V --target-env spirv1.3 ../Shaders/prefixsum.comp -o ../Shaders/sum.spv
glslangValidator.exe -V --target-env spirv1.5 ../Shaders/radix_sort/radixsort.comp -o ../Shaders/sort.spv
glslangValidator.exe -V --target-env spirv1.5 ../Shaders/radix_sort/histogram.comp -o ../Shaders/histogram.spv
glslangValidator.exe -V --target-env spirv1.5 ../Shaders/idkeys.comp -o ../Shaders/idkeys.spv
glslangValidator.exe -V --target-env spirv1.3 ../Shaders/tile_boundaries.comp -o ../Shaders/boundaries.spv
glslangValidator.exe -V --target-env spirv1.5 ../Shaders/render_shared_mem.comp -o ../Shaders/render_shared.spv
glslangValidator.exe -V --target-env spirv1.5 ../Shaders/render.comp -o ../Shaders/render.spv
glslangValidator.exe -V --target-env spirv1.5 ../Shaders/frag_axis.frag -o ../Shaders/axis_frag.spv
glslangValidator.exe -V --target-env spirv1.5 ../Shaders/vertex_axis.vert -o ../Shaders/axis_vert.spv

echo Windows shader compilation complete!