cmake_minimum_required(VERSION 4.0)
project(finances)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(nvl)

# Shared library of finance logic for linking into both main and tests
add_library(finances_lib STATIC
        invest/account/Accounts.h
        invest/account/Accounts.cpp
        invest/Day.cpp
        invest/Day.h
        invest/Inflation.h
        invest/Inflation.cpp
        invest/Interest.h
        invest/Interest.cpp
        invest/account/Options.h
        invest/USD.h
        invest/USD.cpp
        invest/account/Options.cpp
        invest/Parsing.h
        invest/Parsing.cpp
        invest/account/Account.h
        invest/account/Account.cpp
        invest/event/Event.cpp
        invest/event/Event.h
        invest/account/Company.cpp
        invest/account/Company.h
        invest/Taxes.cpp
        invest/Taxes.h
        invest/XYSeries.h
)
target_link_libraries(finances_lib PUBLIC nvl)
target_include_directories(finances_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(finances_lib PRIVATE -O0 -Wall -Wextra -Wpedantic -Wshadow -Werror)

add_executable(main
        invest/Main.cpp
        invest/Plotting.h
        invest/Plotting.cpp
)
target_link_libraries(main PRIVATE finances_lib)
target_include_directories(main PRIVATE invest)
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
    target_link_libraries(${test_name} PRIVATE finances_lib nvl-test)
    target_include_directories(${test_name} PRIVATE invest)
    target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
    gtest_discover_tests(${test_name})
    message("Added test ${test_name}")
endfunction()

add_finance_test(test/TestAccount.cpp)
add_finance_test(test/TestDay.cpp)
add_finance_test(test/TestInterest.cpp)
add_finance_test(test/TestUSD.cpp)
