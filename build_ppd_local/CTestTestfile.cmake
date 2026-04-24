# CMake generated Testfile for 
# Source directory: /workspace
# Build directory: /workspace/build_ppd_local
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/workspace/build_ppd_local/splix_gtest[1]_include.cmake")
add_test(rastertoqpdl_usage "/workspace/build_ppd_local/rastertoqpdl")
set_tests_properties(rastertoqpdl_usage PROPERTIES  PASS_REGULAR_EXPRESSION "Usage:" _BACKTRACE_TRIPLES "/workspace/CMakeLists.txt;395;add_test;/workspace/CMakeLists.txt;0;")
add_test(pstoqpdl_usage "/workspace/build_ppd_local/pstoqpdl")
set_tests_properties(pstoqpdl_usage PROPERTIES  PASS_REGULAR_EXPRESSION "Usage:" _BACKTRACE_TRIPLES "/workspace/CMakeLists.txt;403;add_test;/workspace/CMakeLists.txt;0;")
add_test(driver_functional_raster "bash" "/workspace/tests/functional_test.sh" "/workspace/build_ppd_local")
set_tests_properties(driver_functional_raster PROPERTIES  _BACKTRACE_TRIPLES "/workspace/CMakeLists.txt;413;add_test;/workspace/CMakeLists.txt;0;")
subdirs("_deps/googletest-build")
