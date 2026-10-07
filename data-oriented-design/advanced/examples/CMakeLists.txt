design_target(optional_tests tests.cpp)
design_target(optional_demo demo.cpp)
add_test(NAME optional_generations COMMAND optional_tests generations)
add_test(NAME optional_storage COMMAND optional_tests storage)
