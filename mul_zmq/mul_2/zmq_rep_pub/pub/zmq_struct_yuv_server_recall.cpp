#include <stdio.h>
#include <iostream>
#include <cstring>
#include <cstdio>
// #include <ctime>
#include <sys/time.h>

#include <zmq.hpp>
// #include <unistd.h>
#include <opencv2/opencv.hpp>
#include <zhelpers.hpp>

using namespace cv;
using namespace std;


struct Content{
    char timestamp[64];
    char imagelength[24];
    uchar img_data[554496];
    // uchar img_data[3110400];
};


struct Ai_Ui_Content
{
   // char chStr_info[256];
   //uchar img_data[2764800];
   uchar img_data[500000];
};

int main() {

    Content pub_content;
    char tbuffer[61] = {0};
    struct timeval curTime;
    // gettimeofday(&curTime, NULL);
    // int milli = curTime.tv_usec / 1000;

    struct tm nowTime;
    // localtime_r(&curTime.tv_sec, &nowTime);
    // strftime(tbuffer, sizeof(tbuffer), "%Y-%m-%d %H:%M:%S:", &nowTime);
    // snprintf(pub_content.timestamp, sizeof(pub_content.timestamp), "%s%03d", tbuffer, milli);
    // cout << "pub_content.timestamp: " << pub_content.timestamp << endl;


	while(1){
        // while(1){
    	    Mat frame;
    	    // VideoCapture capture("/home/l/select_video/car02.mp4");
    	    VideoCapture capture("/home/l/rtmp_record/colletion_608.mp4");

    	    if(!capture.isOpened()){
    		cout << "Please check whether video is exist....." << endl;
    	    }

    	    string imagelenstr;
    	    int nReg_w = capture.get(CAP_PROP_FRAME_WIDTH);
    	    int nReg_h = capture.get(CAP_PROP_FRAME_HEIGHT);

    	    int frmCount = capture.get(CV_CAP_PROP_FRAME_COUNT);
    	    // frmCount -= 5;
    	    printf("frmCount: %d \n", frmCount);


    	    int bufLen = nReg_h*nReg_w*3/2;
    	    // cout << "video width: " << nReg_w << endl;
    	    // cout << "video hight: " << nReg_h << endl;
    	    imagelenstr = "imagelength=" + to_string(bufLen) + ";";
    	    strcpy(pub_content.imagelength, imagelenstr.c_str());
    	    cout << "pub_content.imagelength: " << pub_content.imagelength << endl;

    	    zmq::context_t context_pub(1);
    	    zmq::socket_t socket_pub(context_pub, ZMQ_PUB);
    	    socket_pub.bind("tcp://127.0.0.1:6557");
    	    cout << "The server is starting." << endl;
    	    int i =0;

    //while(1){

            bool ret = capture.read(frame);

        //while(ret){
    	while(1){
    	    //bool ret = capture.read(frame);
                
                //imshow("src",frame);
    	        //waitKey(10);
                // if(frame.empty()) break;
                
                // char tbuffer[61] = {0};
                // struct timeval curTime;
                gettimeofday(&curTime, NULL);
                int milli = curTime.tv_usec / 1000;

                // struct tm nowTime;
                localtime_r(&curTime.tv_sec, &nowTime);
                strftime(tbuffer, sizeof(tbuffer), "%Y-%m-%d %H:%M:%S:", &nowTime);
                snprintf(pub_content.timestamp, sizeof(pub_content.timestamp), "%s%03d", tbuffer, milli);

                // capture >> frame;
                capture.read(frame);
                if(frame.empty()) break;

                Mat yuvImg;
                cvtColor(frame, yuvImg, CV_BGR2YUV_I420);
                //imshow("yuv", yuvImg);
                //waitKey(10);

                vector<uchar> frTmp;
                //imencode(".jpg", yuvImg, frTmp);
                //for (int i=0;i<frTmp.size();i++) pub_content.img_data[i]=frTmp[i];
                for (int i=0;i<bufLen;i++) pub_content.img_data[i]=yuvImg.data[i];
                zmq::message_t message_pub( sizeof(pub_content) );
                memcpy(message_pub.data(),&pub_content,sizeof(pub_content));

                cout << "pub_content.timestamp: " << pub_content.timestamp << endl;
                cout << "pub_content.imagelength: " << pub_content.imagelength << endl;
                //cout << "pub_content.yuvImg: " << pub_content.img_data << endl;


                //cout << "message_pub: " << message_pub.data() << endl;
                // s_sendmore(socket_pub,"Image");
                socket_pub.send(message_pub);
                printf("%d/%d \n", i+1, frmCount);
                i++;
                // cout << "server has already been sent the message......" << endl;

                }
                cout << "video had already been finished ... " << endl;
            //break;
    }
    //socket_pub.close();
    //return 0;
}
