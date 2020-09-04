#include <iostream>
#include <cstring>
#include <zmq.hpp>
#include <opencv2/opencv.hpp>
using namespace cv;
using namespace std;
using namespace zmq;

struct Content{
    char timestamp[64];
    char imagelength[24];
    uchar img_data[554496];
};

struct Ai_Ui_Content
{
   // char chStr_info[256];
   //uchar img_data[2764800];
   uchar img_data[500000];
};


// Mat yuv_img;
// Mat recv_img;
// Mat recv_img(720,1280,CV_8UC3);
// Mat yuv_img(1080,1280,CV_8UC1);

vector<uchar> buffer;

int main() {

    zmq::context_t context_sub(1);
    zmq::socket_t socket_sub(context_sub, ZMQ_SUB);
    // int cacheNum =2;
    // socket_sub.setsockopt(ZMQ_RCVHWM, &cacheNum, sizeof(cacheNum));
    socket_sub.connect("tcp://127.0.0.1:6557");
    socket_sub.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    cout << "The client is connect." << endl;
    cv::Mat recv_img(720,1280,CV_8UC3);
    cv::Mat yuv_img(1080,1280,CV_8UC1);

    while (true) {
        cout << "Come from server..." << endl;

        zmq::message_t message_sub;
        socket_sub.recv(&message_sub);

        if(message_sub.size()<100) continue;
        Content* input = reinterpret_cast<Content*>(message_sub.data());
        string times = input->timestamp;
        cout << "timestamp: " << times << endl;
        
        string length = input->imagelength;
        cout << "imagelength: " << length << endl;

        int img_len = stoi(length.substr(length.find("=")+1));
        cout << "img_len: " << img_len << endl;
        
        // buffer.clear();
        // for(int i =0; i<img_len; i++){
        //     buffer.push_back(static_cast<uchar *>(input->img_data)[i]);
        // }

        memcpy(yuv_img.data,(input)->img_data, 554496*sizeof(uchar));
        //yuv_img = imdecode(buffer, IMREAD_COLOR);
        // for (int i; i < frTmp.size(); i++) msg_content.img_data[i] = frTmp[i];


        cvtColor(yuv_img, recv_img, COLOR_YUV2BGR_I420);
        // printf("%s, %d\n",  __FILE__ , __LINE__);
        imshow("yuv_img", yuv_img);
        // waitKey(10);
        imshow("recv_img", recv_img);
        waitKey(10);

        // cout << "get the frame...." << endl;

        // do some work
        // zmq_sleep(1);

        // // 发送信息给客户端[应答]
        // zmq::message_t recv_msg;
        // socket.recv(&recv_msg);
        
        // zmq::message_t reply(10);
        // memcpy(reply.data(), "frame_get", 10);
        // socket.send(reply);
    }
    socket_sub.close();
    return 0;
}
