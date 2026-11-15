add_executable(gateway
    main.cpp
    fix_parser.cpp
    session_mgr.cpp
)

target_include_directories(gateway PRIVATE
    .
    ${AERON_INCLUDE_DIR}
    ${AERON_INCLUDE_C_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(gateway PRIVATE
    ${AERON_CLIENT_LIB}
    pthread
    bsd
)