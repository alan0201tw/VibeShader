enable_testing()

add_executable(playground_tests
  placeholder_test.cc
)

target_link_libraries(playground_tests PRIVATE
  GTest::gtest_main
  glm::glm
)

if(MSVC)
  target_compile_options(playground_tests PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(playground_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
  )
  if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
    target_compile_options(playground_tests PRIVATE -fsanitize=address,undefined)
    target_link_options(playground_tests PRIVATE -fsanitize=address,undefined)
  endif()
endif()

include(GoogleTest)
gtest_discover_tests(playground_tests)
