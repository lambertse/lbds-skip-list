include(FetchContent)
  FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# Prevent GoogleTest from modifying the parent project's compiler settings.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "Do not build GoogleMock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Do not install GoogleTest" FORCE)

FetchContent_MakeAvailable(googletest)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/test/*_test.cpp)

foreach(test_src ${TEST_SOURCES})
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/test)
  target_link_libraries(
    ${test_name} PRIVATE skiplist skiplist::skiplist
    GTest::gtest_main
  )
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

