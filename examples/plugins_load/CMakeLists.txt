file(GLOB_RECURSE CPPS ./*.cpp)
file(GLOB_RECURSE HPPS ./*.hpp)

add_executable(plugins_load ${CPPS} ${HPPS})

target_link_libraries(plugins_load QtNodes)
