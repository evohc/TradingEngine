add_executable(matching_engine
    main.cpp
)

target_include_directories(matching_engine PRIVATE
    .
    ${AERON_INCLUDE_DIR}
    ${AERON_INCLUDE_C_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${BOOST_INCLUDE_DIR}
)

target_link_libraries(matching_engine PRIVATE
    ${AERON_CLIENT_LIB}
    pthread
    bsd
)