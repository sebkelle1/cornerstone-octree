
add_executable(collisions_a2a_test collisions_a2a.cpp test_main.cpp)
target_include_directories(collisions_a2a_test PRIVATE ../../include)
target_link_libraries(collisions_a2a_test gtest_main)
add_test(NAME CollisionReferenceTests COMMAND collisions_a2a_test)
