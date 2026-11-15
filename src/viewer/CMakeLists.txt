add_executable(market_viewer
    main.cpp
)

target_include_directories(market_viewer PRIVATE
    .
    ${AERON_INCLUDE_DIR}
    ${AERON_INCLUDE_C_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${BOOST_INCLUDE_DIR}
)

target_link_libraries(market_viewer PRIVATE
    ${AERON_CLIENT_LIB}
    pthread
    bsd
)