cmake_minimum_required(VERSION 3.25)
project(reeot_extracted_disc_tests LANGUAGES CXX)

add_executable(extracted_disc_tests
    extracted_disc_tests.cpp
    ../../src/installer/extracted_disc.cpp)
target_include_directories(extracted_disc_tests PRIVATE ../../src)
target_compile_features(extracted_disc_tests PRIVATE cxx_std_23)
if(MSVC)
    target_compile_options(extracted_disc_tests PRIVATE /W4)
else()
    target_compile_options(extracted_disc_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME extracted_disc_folders COMMAND extracted_disc_tests)
