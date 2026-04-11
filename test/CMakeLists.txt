# 测试可执行文件
add_executable(code_runner_tests
    test_data_structures.cpp
    test_languages.cpp
    ${CMAKE_SOURCE_DIR}/src/code_executor.cpp
    ${CMAKE_SOURCE_DIR}/src/data_structures.cpp
    ${CMAKE_SOURCE_DIR}/src/namespace_isolation.cpp
    ${CMAKE_SOURCE_DIR}/src/cgroup_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/seccomp_filter.cpp
)

target_link_libraries(code_runner_tests
    PRIVATE
    GTest::gtest_main
    Threads::Threads
    seccomp
)

include(GoogleTest)
gtest_discover_tests(code_runner_tests)
