#include <iostream>

void run_fixed_string_unit_tests();
void run_fix_parser_unit_tests();
void run_session_manager_unit_tests();
void run_sbe_encoder_unit_tests();
void run_tcp_receiver_unit_tests();
void run_data_pipeline_unit_tests();
void run_order_book_unit_tests();

int main() {

    std::cout << "Running fix parser unit tests..." << std::endl;
    run_fix_parser_unit_tests();
    std::cout << "Fix parser unit tests passed." << std::endl;

    std::cout << "Running FixedString unit tests..." << std::endl;
    run_fixed_string_unit_tests();
    std::cout << "FixedString unit tests passed." << std::endl;

    std::cout << "Running session manager unit tests..." << std::endl;
    run_session_manager_unit_tests();
    std::cout << "Session manager unit tests passed." << std::endl;

    std::cout << "Running sbe encoder unit tests..." << std::endl;
    run_sbe_encoder_unit_tests();
    std::cout << "Sbe encoder unit tests passed." << std::endl;

    std::cout << "Running tcp receiver unit tests..." << std::endl;
    run_tcp_receiver_unit_tests();
    std::cout << "Tcp receiver unit tests passed." << std::endl;

    std::cout << "Running data pipeline unit tests..." << std::endl;
    run_data_pipeline_unit_tests();
    std::cout << "Data pipeline  unit tests passed." << std::endl;

    std::cout << "Running order_book unit tests..." << std::endl;
    run_order_book_unit_tests();
    std::cout << "Order_book unit tests passed." << std::endl;

    std::cout << "Unit tests complete." << std::endl;

    return 0;
}