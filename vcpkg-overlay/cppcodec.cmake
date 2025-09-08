find_path(CPPCODEC_INCLUDE_DIRS "cppcodec/base32_crockford.hpp")
add_library(cppcodec INTERFACE)
target_include_directories(cppcodec INTERFACE ${CPPCODEC_INCLUDE_DIRS})
