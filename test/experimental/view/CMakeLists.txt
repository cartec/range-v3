add_executable(view.shared shared.cpp)
add_test(test.view.shared, view.shared)

add_executable(view.sorted sorted.cpp)
add_test(test.view.sorted, view.sorted)
