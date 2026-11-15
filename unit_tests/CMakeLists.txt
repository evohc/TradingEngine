add_executable(unit_tests
    main.cpp
    gateway/test_fixed_string.cpp
    gateway/test_fix_parser.cpp
    gateway/test_session_mgr.cpp
    gateway/test_sbe_encoder.cpp
    gateway/test_tcp_receiver.cpp
    gateway/test_data_pipeline.cpp
    matching/test_order_book.cpp
    ../src/gateway/fix_parser.cpp
    ../src/gateway/session_mgr.cpp
    ../src/gateway/sbe_encoder.hpp
    ../src/gateway/data_pipeline.hpp
)

target_include_directories(unit_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/gateway
    ${BOOST_INCLUDE_DIR}
)