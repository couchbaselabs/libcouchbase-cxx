ADD_EXECUTABLE(redef_test EXCLUDE_FROM_ALL redef_1.cpp redef_2.cpp)
TARGET_LINK_LIBRARIES(redef_test couchbase)

ADD_TEST(NAME check_redef
    COMMAND
    ${CMAKE_COMMAND} --build "${PROJECT_BINARY_DIR}" --target redef_test)
