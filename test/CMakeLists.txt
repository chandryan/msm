# include(CTest)

file(GLOB SOURCES "*.cpp")
if(CMAKE_CXX_STANDARD LESS 20)
    message(STATUS "MSM: CXX standard is below 20, excluding puml tests")
    list(FILTER SOURCES EXCLUDE REGEX "puml|Puml")
endif()

add_executable(msm_test
    # ${SOURCES}
    AnonymousAndGuard.cpp
    main.cpp
)
target_include_directories(msm_test PRIVATE ../include)
find_package(boost_serialization)
target_link_libraries(msm_test Boost::serialization)
target_compile_definitions(msm_test PRIVATE "BOOST_MSM_NONSTANDALONE_TEST")

add_test(msm_test msm_test)