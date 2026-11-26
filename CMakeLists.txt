cmake_minimum_required(VERSION 3.20)
project(Programming_C_Summative C)

set(CMAKE_C_STANDARD 11)

add_executable(student_performance student_performance/main.c)
add_executable(sales_analysis sales_analysis/main.c)
add_executable(document_analyzer document_analyzer/main.c)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/sales_analysis/products.txt
        ${CMAKE_CURRENT_BINARY_DIR}/products.txt
        COPYONLY
)
