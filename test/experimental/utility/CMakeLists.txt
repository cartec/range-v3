set(CMAKE_FOLDER "${CMAKE_FOLDER}/utility")

if (RANGE_V3_COROUTINE_PROBE OR RANGE_V3_COROUTINE_FLAG_PROBE)
  rv3_add_test(test.generator generator generator.cpp)
endif()
