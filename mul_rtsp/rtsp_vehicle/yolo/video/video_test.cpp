#include<iostream>
#include<opencv2/opencv.hpp>
using namespace cv;

int main()
{
  VideoCapture capture;
  Mat frame;
  frame= capture.open("rtsp://192.168.1.211/live2");
  if(!capture.isOpened())
  {
    printf("can not open ...\n");
    return -1;
  }
  namedWindow("output", CV_WINDOW_AUTOSIZE);

  while (capture.read(frame))
  {
    imshow("output", frame);
    waitKey(10);
  }
  capture.release();
  return 0;
}
