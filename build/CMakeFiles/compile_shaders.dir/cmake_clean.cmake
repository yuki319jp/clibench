file(REMOVE_RECURSE
  "CMakeFiles/compile_shaders"
  "shaders/compute.comp.spv"
  "shaders/compute_comp.h"
  "shaders/texture_fill.frag.spv"
  "shaders/texture_fill_frag.h"
  "shaders/triangle.frag.spv"
  "shaders/triangle.vert.spv"
  "shaders/triangle_frag.h"
  "shaders/triangle_vert.h"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/compile_shaders.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
