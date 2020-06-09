cmake_minimum_required( VERSION 2.8 )
project( imageBasics )
set( CMAKE_CXX_FLAGS "-std=c++11" )
#set(OpenCV_DIR /home/chaofan/opt/opencv-3.4.4/release)

find_package( OpenCV 3 REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )
add_executable(video_test video_test.cpp )
target_link_libraries(video_test ${OpenCV_LIBS} )
