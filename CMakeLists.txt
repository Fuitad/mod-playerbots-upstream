if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.20)
  project(mod_playerbots_extension_seams LANGUAGES CXX)

  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

  include(CTest)

  add_library(playerbots_extension_registry STATIC
    src/Bot/Extension/PlayerbotExtension.cpp)
  target_include_directories(playerbots_extension_registry PUBLIC src)
  target_compile_options(playerbots_extension_registry PRIVATE
    -Wall -Wextra -Wpedantic -Werror)

  add_executable(playerbots_extension_registry_tests
    tests/cpp/PlayerbotExtensionRegistryTests.cpp)
  target_link_libraries(playerbots_extension_registry_tests PRIVATE
    playerbots_extension_registry)
  target_compile_options(playerbots_extension_registry_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror)

  add_test(NAME playerbots_extension_registry_tests
    COMMAND playerbots_extension_registry_tests)
endif()
