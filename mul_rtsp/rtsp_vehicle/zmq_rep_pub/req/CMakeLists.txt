cmake_minimum_required(VERSION 3.5)
 
project(video)
set(CMAKE_CXX_STANDARD 11)
find_package(OpenCV REQUIRED)
 
message(STATUS "OpenCV library status:")
message(STATUS "    version: ${OpenCV_VERSION}")
# message(STATUS "    libraries: ${OpenCV_LIBS}")
# message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")

include_directories( ${OpenCV_INCLUDE_DIRS} /home/l/CLionProjects/zmq/zeromq-4.1.4/zmq/include)
add_executable(zmq_yuv_req_server zmq_yuv_req_server.cpp)
# add_executable(zmq_struct_yuv_client zmq_struct_yuv_client.cpp)
target_link_libraries(zmq_yuv_req_server ${OpenCV_LIBS} libzmq.so)
#target_link_libraries(zmq_struct_yuv_client ${OpenCV_LIBS} /home/l/CLionProjects/zmq/zeromq-4.1.4/zmq/lib zmq)
