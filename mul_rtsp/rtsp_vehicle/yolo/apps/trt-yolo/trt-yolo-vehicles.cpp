#include "ds_image.h"
#include "trt_utils.h"
#include "yolo.h"
#include "yolo_config_parser.h"
#include "yolov3.h"

#include <experimental/filesystem>
#include <cstring>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include "opencv2/highgui.hpp"

#include "zmq.hpp"
//#include "zhelpers.hpp"

#include "thread"
#include "mutex"
#include <condition_variable>

using namespace std;
using namespace cv;

#define pbREQ_IP_PORT "tcp://192.168.1.102:5555"
#define PUB_IP "tcp://192.168.1.200:"
//#define PUB_IP_local "tcp://127.0.0.1:6557"
#define REQ_IP_PORT "tcp://192.168.1.210:5530"

Mat recv_img(608, 608, CV_8UC3);
Mat yuv_img(608*3/2, 608, CV_8UC1);

void get_rgb_img();

string get_Ntp();
void detect_highway_person(int argc, char **argv);
void send_info();
void get_imginfo(pair <Mat, string> &info);
void send_imginfo(pair <Mat, string> &info);

pair <Mat, string> g_imginfo;
pair <Mat, string> g_imginfoT;

pair <Mat, string> s_imginfo;
pair <Mat, string> s_imginfoT;

string req_port(zmq::socket_t &reqer);

string global_send_msg = "";
string stSub_ip_port;

mutex mutex_imginfo;
bool g_imginfo_flag = false;
condition_variable cond_imginfo;

mutex mutex_send_img;
bool g_send_flag = false;
condition_variable cond_send_img;


struct Content {
    char timestamp[64];
    char imagelength[24];
    uchar img_data[608*608*3/2];
};

struct Ai_Ui_Content {
    char chStr_info[1024];
    // uchar img_data[500000];
    unsigned char img_data[500000];
} msg_content;


void get_imginfo(pair <Mat, string> &info) {
    g_imginfoT = info;
    info = g_imginfo;
    g_imginfo = g_imginfoT;
}


void send_imginfo(pair <Mat, string> &info) {
    s_imginfoT = info;
    info = s_imginfo;
    s_imginfo = s_imginfoT;
}


string get_Ntp() {
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    int milli = curTime.tv_usec / 1000;

    char buffer[80] = {0};
    struct tm nowTime;
    localtime_r(&curTime.tv_sec, &nowTime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &nowTime);

    char currentTime[84] = {0};
    snprintf(currentTime, sizeof(currentTime), "%s:%03d", buffer, milli);
    //cv::putText(matImage,currentTime,cv::Point(100,200),cv::FONT_HERSHEY_PLAIN,2.0,cv::Scalar(255,255,255),4,8);

//    string stNtpTime(buffer);
    string stNtpTime(currentTime);
    return stNtpTime;
}


string req_port(zmq::socket_t &reqer) {
    string name = "name=detector6;action=getport";
    zmq::message_t message(strlen(name.c_str()));
    memcpy((void *) message.data(), name.c_str(), strlen(name.c_str()));
    reqer.send(message);
    zmq::message_t reply;
    reqer.recv(&reply);
    string remsg = string(static_cast<char *>(reply.data()), reply.size());
    if (reply.size() < 2) {
        cout << "NO RECV PORT !" << endl;
        exit(0);
    } else if (remsg == "novideo") {
        cout << "NO VIDEO !" << endl;
        exit(0);
    } else {
        cout << "video recv port:" << remsg << endl;
    }
    return remsg;
}


int main(int argc, char **argv) {


    //get port for yuv
    zmq::context_t context_port(1);
    zmq::socket_t reqer(context_port, ZMQ_REQ);
//    reqer.setsockopt(ZMQ_RCVTIMEO,2000);
    reqer.connect(pbREQ_IP_PORT);
//
    // socket_sub.setsockopt(ZMQ_SUBSCRIBE, "", 0);
    cout << "The ZMQ_req client is connect...." << endl;

    string stPort = " ";
    while (stPort == " ") {
        stPort = req_port(reqer);
    }
    stSub_ip_port = PUB_IP + stPort;
    cout << "The stSub_ip_port...." << stSub_ip_port << endl;

    thread get_obj_info(detect_highway_person, argc, argv);
    sleep(1);
    thread send_msg(send_info);
    sleep(1);
    thread get_pub_frame(get_rgb_img);
//    sleep(2);
//    get_rgb_img();
//
    get_obj_info.join();
    send_msg.join();
    get_pub_frame.join();
    return 0;

}


void get_rgb_img() {

    zmq::context_t context_sub(1);
    zmq::socket_t socket_sub(context_sub, ZMQ_SUB);
    // socket_sub.connect(PUB_IP);

//    int cacheNum = 2;
//    socket_sub.setsockopt(ZMQ_RCVHWM, &cacheNum, sizeof(cacheNum));

    socket_sub.connect(stSub_ip_port);
//    socket_sub.connect(PUB_IP_local);
    socket_sub.setsockopt(ZMQ_SUBSCRIBE, "", 0);
//    socket_sub.setsockopt(ZMQ_SUBSCRIBE, "Image", 1);
    cout << "Come from pub_server..." << endl;

    while (true) {
//        cout << "enter into the get_rgb_img while..." << endl;
        double timecost1 = (double) getTickCount();
//        usleep(5000);
        zmq::message_t message_sub;
        socket_sub.recv(&message_sub);
        socket_sub.setsockopt(ZMQ_RCVTIMEO, 10);
//        cout << "message_sub.size: " << sizeof(message_sub) << endl;
        if (message_sub.size() < 100) continue;
        Content *input = reinterpret_cast<Content *>(message_sub.data());
//        for(int i=0;i < 65;i++) input->timestamp[i] = {0};

        string times = input->timestamp;
//        cout << "times size :" << sizeof(times) << endl;
//        cout << "timestamp: " << times << endl;

        string length = input->imagelength;
        cout << "imagelength: " << length << endl;

//        int img_len = stoi(length.substr(length.find("=")+1));
//        cout << "img_len: " << img_len << endl;

        // buffer.clear();
        // for(int i =0; i<img_len; i++){
        //     buffer.push_back(static_cast<uchar *>(input->img_data)[i]);
        // }

        // memcpy(yuv_img.data,(input)->img_data, 1382400*sizeof(uchar));
        memcpy(yuv_img.data, input->img_data, 554496 * sizeof(uchar));

        //recv_img = imdecode(buffer, IMREAD_COLOR);
//        for (int i; i < frTmp.size(); i++) msg_content.img_data[i] = frTmp[i];
//        for(int j; j<64; j++) msg

        cvtColor(yuv_img, recv_img, COLOR_YUV2BGR_I420);


//        imshow("yuv_img", yuv_img);
        //imshow("recv_img", recv_img);
        //waitKey(30);

        unique_lock <mutex> lock_imginfo(mutex_imginfo);
        g_imginfo.first = recv_img;
        g_imginfo.second = times;
        g_imginfo_flag = true;
        lock_imginfo.unlock();
        cond_imginfo.notify_all();
        double timecost2 = (double) getTickCount();
        cout << "timecost get_yuv: " << (timecost2 - timecost1) / getTickFrequency() << "ms" << endl;

    }
    socket_sub.close();
}


void detect_highway_person(int argc, char **argv) {
    cout << "initialized tensorRT yolo engine ..." << endl;

    // parse config params
    yoloConfigParserInit(argc, argv);
    uint batchSize = getBatchSize();
    NetworkInfo yoloInfo = getYoloNetworkInfo();
    InferParams yoloInferParams = getYoloInferParams();

    string networkType = getNetworkType();
    uint64_t seed = getSeed();
    srand(unsigned(seed));

    unique_ptr <Yolo> inferNet{nullptr};
    if (networkType == "yolov3") {
        inferNet = std::unique_ptr < Yolo > {new YoloV3(batchSize, yoloInfo, yoloInferParams)};
    } else { cout << "Unrecognised network_type." << endl; }

    cout << "initialized tensorRT yolo engine success !" << endl;
    vector <DsImage> dsImages;
    VideoCapture cap;
    Mat frame;
//    bool ret = cap.open(preVideoPath);
//    bool ret = cap.open("/home/l/rtmp_record/colletion.mp4");
    int detectIndex = 0;
    // int no_person_frame = 0;
    string state = "keep";
//    string state ="";
    while (1) {

        double timecost3 = (double) getTickCount();
//        frame = yuv_rgb_img();
//        cout << "enter into the detect while..." << endl;
        pair <Mat, string> imginfo;
        unique_lock <mutex> lock_imginfo(mutex_imginfo);

        while (!g_imginfo_flag) {
            cond_imginfo.wait(lock_imginfo);
        }
        get_imginfo(imginfo);
//        send_imginfo(imginfo);

        g_imginfo_flag = false;
        lock_imginfo.unlock();
        Mat frame = imginfo.first;
//        string timestamp = imginfo.second;

        dsImages.clear();
        dsImages.emplace_back(frame, inferNet->getInputH(), inferNet->getInputW());
        cv::Mat trtInput = blobFromDsImages(dsImages, inferNet->getInputH(), inferNet->getInputW());
        inferNet->doInference(trtInput.data, dsImages.size());
        uint imageIdx = 0;
        auto curImage = dsImages.at(imageIdx);
        auto binfo = inferNet->decodeDetections(imageIdx, curImage.getImageHeight(), curImage.getImageWidth());
        auto remaining = nmsAllClasses(inferNet->getNMSThresh(), binfo, inferNet->getNumClasses());

        int vehicles_num = 0;
        int frameIndex = 5;
        string name = "name=AI;event=6;para=";

//        {"0":"truck"; "1", "car"; "2", "bus"; "3", "van"; "4", "person";}

        string objBoxesInfo;
        string text_laber;
        objBoxesInfo.clear();
        // bool person_flag = false;

        double t1 = (double) getTickCount();
        for (auto b : remaining) {
            if (to_string(b.label) != "4") {
                vehicles_num += 1;
                // person_flag = true;

                int left = (int) (b.box.x1);
                int top = (int) (b.box.y1);
                int width = (int) (b.box.x2 - b.box.x1);
                int height = (int) (b.box.y2 - b.box.y1);
                string objInfo;
                objInfo.clear();
                objInfo = "vehicles," + to_string(left) + "," + to_string(top) + "," +
                          to_string(width) + "," + to_string(height) + ",";
                objBoxesInfo = objBoxesInfo + objInfo;
                text_laber = "vehicles_num:" +to_string(vehicles_num);
                rectangle(frame, Rect(left, top, width, height), cv::Scalar(0, 0, 255), 1);
                // cv::putText(frame,text_laber,cv::Point(5,80),cv::FONT_HERSHEY_SIMPLEX,2.0,cv::Scalar(0,0,255),2,4);
            }
        }
        cv::putText(frame,text_laber,cv::Point(4,80),cv::FONT_HERSHEY_COMPLEX_SMALL,1.0,cv::Scalar(0,0,255),2,4);

        double t2 = (double) getTickCount();
        cout << "FPS: " << (getTickFrequency()) / (t2 - t1) * 1000 << endl;

//        imwrite("recframe.jpg", frame);
//        imginfo.first=frame;
        imshow("vehicles_check", frame);
        waitKey(10);

//        unique_lock<mutex> lock_send_img(mutex_send_img);
//         if (person_flag == true) {
//             no_person_frame = 0;
//             cout << "state: " << state << endl;
//             if (state == "start") state = "keep";
//             else if (state == "end") {
//                 state = "start";
//                 detectIndex += 1;
//                 if (detectIndex >= 10000) detectIndex = 0;
//             }
// //            else if(state == "start") state="keep";
//         } else {
//             no_person_frame++;
//             if (no_person_frame > 50) state = "end";
//             if (state == "start") state = "keep";
//         }

        string reTime = get_Ntp();
        if (vehicles_num > 0) objBoxesInfo.pop_back();

        string zmq_send_msg = name + to_string(frameIndex) + ","
                              + to_string(detectIndex) + ","
                              + state + "," + reTime + ",["
                              + to_string(vehicles_num) + ","
                              + objBoxesInfo + "];";

        cout << "zmq_send_msg: " << zmq_send_msg << endl;
        // cout << "global_send_msg: " << global_send_msg << endl;
        unique_lock <mutex> lock_send_img(mutex_send_img);
        global_send_msg = zmq_send_msg;

        imginfo.first=frame;
//        imwrite("imginfo_frame.jpg", frame);
//        imwrite("imginfo_first.jpg", imginfo.first);
        send_imginfo(imginfo);
//        s_imginfoT.first=frame;

//        cout << "global_send_msg: " << global_send_msg << endl;
        g_send_flag = true;
        lock_send_img.unlock();
        cond_send_img.notify_all();
        double timecost4 = (double) getTickCount();
        cout << "timecost detection: " << (timecost4 - timecost3) / getTickFrequency() << "ms" << endl;

    }

}


void send_info() {
    zmq::context_t context_req(1);
    zmq::socket_t socket_req(context_req, ZMQ_REQ);
//    int catchNum = 1;
//    REQsocket.setsockopt(ZMQ_SNDHWM,&catchNum, sizeof(catchNum));
    socket_req.connect(REQ_IP_PORT);
    cout << "The ZMQ_REQ is now connect with server...." << endl;
//    double times = static_cast<double>(cvGetTickCount());
//    int n = 0;
    while (true) {
        double timecost5 = (double) getTickCount();
//        cout << "enter into the send_info while..." << endl;
//        usleep(10000);
        cout << "waiting for the mutex_send_img... " << endl;
        unique_lock <mutex> lock_send_img(mutex_send_img);
        cout << "ready to send info..." << endl;
        while (!g_send_flag) {
            cond_send_img.wait(lock_send_img);
        }
        Mat frame = s_imginfoT.first;
//        imwrite("s_imginfoT.jpg", frame);

//        string pubtimestamp = s_imginfoT.second;
//        cout << "pub_timestamp: " << pubtimestamp << endl;

//        imshow("frame_send", frame);
//        waitKey(30);
//        cout << "global_send_msg: " << global_send_msg << endl;
//        Ai_Ui_Content msg_content;
//        for(int i=0;i<signed(sizeof(msg_content.chStr_info));i++) msg_content.chStr_info[i] = 0;

        strcpy(msg_content.chStr_info, global_send_msg.c_str());
//        cout << "already prepare the send msg..." << endl;
        vector <uchar> frTmp;
        imencode(".jpg", frame, frTmp);
        for (int i=0; i < signed(frTmp.size()); i++) msg_content.img_data[i] = frTmp[i];
        cout << "msg_content.chStr_info: " << msg_content.chStr_info << endl;
//        cout << "msg_content.img_data: " << msg_content.img_data << endl;

        zmq::message_t message(sizeof(msg_content));
        memcpy(message.data(), &msg_content, sizeof(msg_content));

//        s_sendmore(socket_req, "");
        socket_req.send(message);
        cout << "message has already send..." << endl;
        g_send_flag = false;
        lock_send_img.unlock();

//        send_msg.clear();
//        send_msg="";
        for (int i = 0; i < signed(sizeof(msg_content.chStr_info)); i++) msg_content.chStr_info[i] = 0;
//        frTmp.clear();

//        g_send_flag = false;
//        lock_send_img.unlock();

        zmq::message_t reply;
        socket_req.recv(&reply);

        std::string remsg = std::string(static_cast<char *>(reply.data()), reply.size());
        std::cout << "get the reply: " << remsg << std::endl;
        double timecost6 = (double) getTickCount();
        cout << "timecost send msg: " << (timecost6 - timecost5) / getTickFrequency() << "ms" << endl;

    }
    socket_req.close();
}

