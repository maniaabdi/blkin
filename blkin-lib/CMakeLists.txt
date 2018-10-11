include_directories(.)

#blkin
set(blkin_srcs
    zipkin_c.c
    tp.c
    jtracer.cc
)
add_library(blkin ${blkin_srcs})
set_target_properties(blkin PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(blkin dl ${LTTNG_LIBRARIES})

set(blkin_headers
  zipkin_c.h
  zipkin_trace.h
  ztracer.hpp
  jtracer.hpp
)

add_subdirectory(tests)
