include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

add_library(string_refcount STATIC lib/string_handle.cpp)
target_include_directories(string_refcount PRIVATE lib)


enable_testing()

add_executable(
  string_test
  test/test.cpp
)
target_link_libraries(
  string_test
  GTest::gtest_main
  string_refcount
)
target_include_directories(
  string_test
  PRIVATE
  lib
)

include(GoogleTest)
gtest_discover_tests(string_test)