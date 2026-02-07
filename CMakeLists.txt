cmake_minimum_required(VERSION 3.10)
project(grids-jack VERSION 1.0 LANGUAGES CXX)

# Set C++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags for realtime safety warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Find JACK library
find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED jack)

# Find libsndfile
pkg_check_modules(SNDFILE REQUIRED sndfile)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${JACK_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    main.cpp
    sample_bank.cpp
    sample_player.cpp
    pattern_generator_wrapper.cpp
    grids/pattern_generator.cc
    grids/resources.cc
    avrlib/random.cc
)

# Create executable
add_executable(grids-jack ${SOURCES})

# Link libraries
target_link_libraries(grids-jack
    ${JACK_LIBRARIES}
    ${SNDFILE_LIBRARIES}
)

# Add compiler flags from pkg-config
target_compile_options(grids-jack PUBLIC ${JACK_CFLAGS_OTHER} ${SNDFILE_CFLAGS_OTHER})

# Test executables (don't require JACK server to be running)
add_executable(test_sample_bank test_sample_bank.cpp sample_bank.cpp)
target_link_libraries(test_sample_bank ${SNDFILE_LIBRARIES})

add_executable(test_sample_player test_sample_player.cpp sample_player.cpp sample_bank.cpp)
target_link_libraries(test_sample_player ${SNDFILE_LIBRARIES})

add_executable(test_sample_player_integration test_sample_player_integration.cpp sample_player.cpp sample_bank.cpp)
target_link_libraries(test_sample_player_integration ${SNDFILE_LIBRARIES})

add_executable(test_pattern_generator test_pattern_generator.cpp sample_player.cpp sample_bank.cpp pattern_generator_wrapper.cpp grids/pattern_generator.cc grids/resources.cc avrlib/random.cc)
target_link_libraries(test_pattern_generator ${SNDFILE_LIBRARIES})

add_executable(test_velocity test_velocity.cpp sample_player.cpp sample_bank.cpp pattern_generator_wrapper.cpp grids/pattern_generator.cc grids/resources.cc avrlib/random.cc)
target_link_libraries(test_velocity ${SNDFILE_LIBRARIES})

add_executable(test_velocity_integration test_velocity_integration.cpp sample_player.cpp sample_bank.cpp pattern_generator_wrapper.cpp grids/pattern_generator.cc grids/resources.cc avrlib/random.cc)
target_link_libraries(test_velocity_integration ${SNDFILE_LIBRARIES} m)

# Enable testing with CTest
enable_testing()

# Add tests to CTest with proper working directory
add_test(NAME sample_bank COMMAND test_sample_bank)
set_tests_properties(sample_bank PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME sample_player COMMAND test_sample_player)

add_test(NAME sample_player_integration COMMAND test_sample_player_integration)

add_test(NAME pattern_generator COMMAND test_pattern_generator)
set_tests_properties(pattern_generator PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME velocity COMMAND test_velocity)
set_tests_properties(velocity PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME velocity_integration COMMAND test_velocity_integration)
set_tests_properties(velocity_integration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
