cmake_minimum_required(VERSION 4.0)
project(finances)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(FetchContent)
FetchContent_Declare(
    googletest
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(googletest)

# Shared library of finance logic for linking into both main and tests
add_library(finances_lib STATIC
        canni/account/Portfolio.h
        canni/account/Portfolio.cpp
        canni/data/Day.cpp
        canni/data/Day.h
        canni/data/Inflation.h
        canni/data/Inflation.cpp
        canni/data/Interest.h
        canni/data/Interest.cpp
        canni/account/Options.h
        canni/data/USD.h
        canni/data/USD.cpp
        canni/account/Options.cpp
        canni/data/Parsing.h
        canni/data/Parsing.cpp
        canni/account/Account.h
        canni/account/Account.cpp
        canni/event/Event.cpp
        canni/event/Event.h
        canni/account/Company.cpp
        canni/account/Company.h
        canni/account/Taxes.cpp
        canni/account/Taxes.h
        canni/plot/Series.h
        canni/account/MarketReturns.cpp
        canni/account/MarketReturns.h
        canni/account/Scenario.h
        canni/account/Scenario.cpp
        canni/tools/MonteCarlo.h
        canni/tools/Experiment.h
        canni/tools/Retirement.h
        canni/tools/Retirement.cpp
        canni/tools/Compare.h
)
target_link_libraries(finances_lib PUBLIC)
target_include_directories(finances_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(finances_lib PRIVATE -O0 -Wall -Wextra -Wpedantic -Wshadow -Werror)

add_executable(main
        canni/Main.cpp
        canni/plot/Plotting.h
        canni/plot/Plotting.cpp
)
target_link_libraries(main PRIVATE finances_lib)
target_include_directories(main PRIVATE canni)
target_compile_options(main PRIVATE -O0 -Wall -Wextra -Wpedantic -Wshadow -Werror)

# Tests
include(GoogleTest)
enable_testing()

# gtest-all.cc triggers -Wcharacter-conversion under C++23; suppress -Werror for gtest targets only.
foreach(gtest_target gtest gtest_main gmock gmock_main)
    if(TARGET ${gtest_target})
        target_compile_options(${gtest_target} PRIVATE -Wno-error)
    endif()
endforeach()

function(add_finance_test test_file)
    get_filename_component(test_name "${test_file}" NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE finances_lib gtest_main)
    target_include_directories(${test_name} PRIVATE canni)
    target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
    gtest_discover_tests(${test_name})
    message("Added test ${test_name}")
endfunction()

add_finance_test(test/TestAccount.cpp)
add_finance_test(test/TestDay.cpp)
add_finance_test(test/TestInterest.cpp)
add_finance_test(test/TestOptions.cpp)
add_finance_test(test/TestUSD.cpp)
