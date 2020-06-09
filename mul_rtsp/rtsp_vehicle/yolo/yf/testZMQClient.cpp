#include "liveMedia.hh"
//#include "RTSPClient.hh"
#include "BasicUsageEnvironment.hh"
#include "stdio.h"
#include <stdio.h>
//#include <iostream>
#include <opencv2/video.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
//#include <bitset>
#include <string>
#include <pthread.h>
#include <sched.h>
//#include <unistd.h>
#include <time.h>
//#include <fstream>
//#include <sys/stat.h>
//#include <sys/types.h>
//#include <dirent.h>

//*********************
#include "thread"
#include "mutex"
//#include "condition_variable"
//#include <locale.h>
//#include <wchar.h> 
//#include <fstream>
//#include <codecvt>
//#include <string>
//*********************
#include "lisence_plate_proceess.hpp"
#include "trackpro.hpp"
//
//#include <stdlib.h>
//#include <sys/time.h>
//#include "rkdrm.h"
#include "mppdecoder.h"
//#include "arm_neon.h"

//rga head files
//#include <RockchipRga.h>
//#include <RockchipFileOps.h>
//#include "omp.h"
//*********************
//#include <object.h>
//********************
#include <zmq.hpp>
#include "zhelpers.hpp"
#include "zmq_req.hpp"
#include "ntp_time.hpp"
//*********************
#include "function_utils.hpp"
//#define Is_Record_Datas 1
//#define Is_Record_H264 1
// Forward function definitions:

#define SUB_IP "tcp://192.168.1.102:"//"tcp://192.168.3.244:"
#define REQ_IP "tcp://192.168.1.102:5555"
std::string stSub_ip_port = "";

zmq::context_t context_req(1);
zmq::socket_t Reqer(context_req, ZMQ_REQ);

zmq::context_t context_sub(1);
zmq::socket_t subscriber(context_sub,ZMQ_SUB);

int init_mpp = 0;
/* FPS calculator */
RK_U64 fps_ms2,fps_ms3;
RK_U32 fps_counter2,fps_counter3;
float fps2 = 0.0,fps3 = 0.0;


int nIsRecog = 1;

std::mutex muLock;
std::mutex muSearchLock;

using namespace cv;
using namespace std;
using namespace chrono;



std::pair<cv::Mat,std::string> paGetmsg(cv::Mat::zeros(720, 1280, CV_8UC3),"");
std::vector<std::string > veTarget;
std::pair<bool,bool> boIssearch(false,false); // 1:current 2:history
std::string stIsendImg = "0";


void thread_getSearchDemand()
{
    std::string stUI_demand_ip_ = "";
    zmq::context_t context_Search_sub_(1);
    zmq::socket_t subscriber(context_Search_sub_,ZMQ_SUB);
    subscriber.connect(stUI_demand_ip_);
    while(1)
    {
        std::vector<std::string > veTarget_temp;
        int ret = get_searchId(subscriber, veTarget_temp);
      
        muSearchLock.lock();
        veTarget = veTarget_temp;
        if (ret==0)boIssearch.first = true;
        if (ret==1)boIssearch.second = true;
        muSearchLock.unlock();
    }

}

std::vector<std::string> vePlateTarget_ids;
std::string rtmp_address;

std::deque<std::tuple<std::string,int,int,std::string,std::string,cv::Rect,cv::Mat>> deSend_buff;
std::mutex muLock_t;

void thread_sendTrackresult()
{
     zmq::context_t context(1);
     zmq::socket_t Reqer(context, ZMQ_REQ);
     string tcp_s = "tcp://192.168.1.101:5530";
     Reqer.connect(tcp_s);
     while(1)
     {
         //std::cout << deSend_buff.size() << std::endl;
         
         if (deSend_buff.size()>0)
         {
            std::cout << "send the result"<< std::endl;
            std::string track_id = std::get<0>(deSend_buff[0]);
            int nFramIndex = std::get<1>(deSend_buff[0]);
            int nDetectIndex = std::get<2>(deSend_buff[0]);
            std::string stState = std::get<3>(deSend_buff[0]);
            std::string stReTime = std::get<4>(deSend_buff[0]);
            cv::Rect reRectPl = std::get<5>(deSend_buff[0]);
            cv::Mat img = std::get<6>(deSend_buff[0]);
            send_trackResult(
                 Reqer, 
                 track_id, 
                 0,
                 nDetectIndex,
                 stState,
                 stReTime,
                 reRectPl,
                 img,
                 stIsendImg
            );
           muLock_t.lock();
           deSend_buff.pop_front();
           muLock_t.unlock();
         }
         
         usleep(30000);
     }
  


}

void thread_getMp4()
{
    std::cout << rtmp_address << std::endl;
    std::string rtmp_path = rtmp_address;
    cv::Mat frame;
    
    cv::VideoCapture capture;
    capture.open(rtmp_path);
    bool ret = capture.read(frame);
    int nTimeStamp = 0;
    while(ret)
    {
         //cv::imshow("1",frame);
         //cv::waitKey(25);
         ret = capture.read(frame);


          std::string timstamp = get_Ntp();
          muLock.lock();
          paGetmsg.first = frame;
          paGetmsg.second = timstamp;
          muLock.unlock();
          //cout << timstamp << endl;
          //usleep(30000);
          //sleep(3);
          //cv::imshow("123",paGetmsg.first);
          //cv::waitKey(1);
    }
    
    std::cout << "video end" << std::endl;

    capture.release();

}


void thread_getImg()
{


   //zmq::context_t context(1);
   //zmq::socket_t subscriber(context,ZMQ_SUB);

   //subscriber.setsockopt(ZMQ_RCVTIMEO,1000);
   //subscriber.connect(stSub_ip_port);
   //subscriber.setsockopt(ZMQ_SUBSCRIBE,"Image",1);
   
   std::vector<uchar> veImg_buff;
   veImg_buff.reserve(400000);
   std::cout << "go1" << std::endl;

   cv::Mat recvimg(720,1280,CV_8UC3);
   cv::Mat yuvImg(1080,1280,CV_8UC1);
   while(true)
   {
      veImg_buff.clear();

      zmq::message_t reply;
      auto res = subscriber.recv(&reply);
   
      if (res==0)
      {
          std::cout << "get the image time out"<<std::endl;
          continue;
      }
      if (reply.size()<100)continue;

      //Content* input = reinterpret_cast<Content*>(reply.data());

      Content_yuv* input = reinterpret_cast<Content_yuv*>(reply.data());

      //Content* input;
      //int nRelySize = 0;
      //if(!get_img(subscriber,input,nRelySize))continue;

      std::string timstamp = (input)->timestamp;
      //std::string stImglenght = (input)->imagelenght;
      //auto pos = stImglenght.find_first_of("=");
      //std::string stlen = stImglenght.substr(pos+1,strlen(stImglenght.c_str()) - pos - 2);
      //int nLen = std::stoi(stlen);

      //cout << stImglenght << " " << timstamp << endl;
      /* 
      for (int i = 0; i< nLen; i++)veImg_buff.push_back(static_cast<uchar*>((input)->img_data)[i]);
      recvimg = cv::imdecode(veImg_buff,CV_LOAD_IMAGE_COLOR);*/
      memcpy(yuvImg.data,(input)->img_data,1382400*sizeof(uchar));

      cv::cvtColor(yuvImg,recvimg,cv::COLOR_YUV2BGR_NV12);

      if (recvimg.rows <10 || recvimg.cols <10)continue;
   
      muLock.lock();
      paGetmsg.first = recvimg;
      paGetmsg.second = timstamp;
      muLock.unlock();
////  get time current time
          struct timeval curTime;
          gettimeofday(&curTime,NULL);
          int milli = curTime.tv_usec/1000;

          char buffer[80] = {0};
          struct tm nowTime;
          localtime_r(&curTime.tv_sec, &nowTime);
          strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S:",&nowTime);

          char currentTime[84] = {0};
          snprintf(currentTime,sizeof(currentTime),"%s%03d",buffer,milli);
          cv::putText(paGetmsg.first,currentTime,cv::Point(100,150),FONT_HERSHEY_PLAIN,2.0,cv::Scalar(255,255,255),4,8);
          cv::putText(paGetmsg.first,timstamp,cv::Point(100,100),FONT_HERSHEY_PLAIN,2.0,cv::Scalar(255,255,255),4,8);
////

      while(!nIsRecog)
      {
           ;
      }
      
   }


}

void thread_Lplate()
{


    //zmq::context_t context(1);
   // zmq::socket_t Reqer(context, ZMQ_REQ);
    //Reqer.setsockopt(ZMQ_RCVTIMEO,2000);
    //Reqer.connect(REQ_IP);

   trackSystem_pro b = trackSystem_pro();
   sleep(1);
   std::cout << "go2" << std::endl;
   std::string last_timestamp = "";
   std::string stTimestamp_save = "";
   std::string stResx = "Null";
   fps_ms2 = current_ms();
   fps_counter2 = 0;
   int nImg_c = 0;
   cv::Mat matCropPl;
   cv::Mat matImg_save(720,1280,CV_8UC3);

   thread send_tr(thread_sendTrackresult);
   //track_plate_id para
   int nDetectIndex = 0;
   std::string stState = "";
   char tmp[64];
   time_t timep;
   time(&timep);
 
  
   

   while(true)
   {
      //cout << paGetmsg.second << endl;
      //std::string current_stamp = paGetmsg.second;   
      //cout << "!!!" << endl;
      usleep(1000);
      if (paGetmsg.second!=last_timestamp)
      {
          nImg_c++;
          //cout << "????" << endl;
          if (nImg_c == 10)
          {
             //auto resSend = send_sy(Reqer, paGetmsg.second); state of recognizition
             //if (!resSend)continue;
             nImg_c= 0;
          }
          muLock.lock();
          cv::Mat img = paGetmsg.first;
          std::string stTimestamp_temp = paGetmsg.second;
          muLock.unlock();

          stResx = b.track_pro(img);

          std::string stReTime = stTimestamp_temp;
          if ((stState == "start" || stState == "keep")&&b.stPlateMax_res!="")
          {
               stState = "keep";
               std::string track_id = b.stPlateMax_res;
               cv::Rect reRectPl = b.reCrop_plate;
               rectangle(img, reRectPl, cv::Scalar(139, 139,   255, 255), 3);
         
               muLock_t.lock();
               deSend_buff.push_back(std::tuple<std::string,int,int,std::string,std::string,cv::Rect,cv::Mat>(
                                      track_id,1,nDetectIndex,stState,stReTime,reRectPl,img));
               muLock_t.unlock();
    
          }




          if (stResx != "Null")
          {
              if (stResx == "save_img")
              {
                  img.copyTo(matImg_save);
                  stTimestamp_save = stTimestamp_temp;
                  //matCropPl = matImg_save(b.reCrop_plate_save);
                  //cout << b.reCrop_plate_save.x << " " << b.reCrop_plate_save.y << " " << b.reCrop_plate_save.width << " " << b.reCrop_plate_save.height << endl;
                  if (stState=="" && b.nAp_max >=1 )//&& check_plateID(b.stPlateMax_res,vePlateTarget_ids))
                  {
                      nDetectIndex++;
                      stState="start";
                      std::string track_id = b.stPlateMax_res;
                      cv::Rect reRectPl = b.reCrop_plate;
                      rectangle(img, reRectPl, cv::Scalar(139, 139,   255, 255), 3);
                     
                      muLock_t.lock();
                      deSend_buff.push_back(std::tuple<std::string,int,int,std::string,std::string,cv::Rect,cv::Mat>(
                                            track_id,1,nDetectIndex,stState,stReTime,reRectPl,img));
                      muLock_t.unlock();

                  }              



              }
              else 
              {
                  //std::cout<<"the result is "<<stResx<<std::endl;
                  //VEHICLE_NUM++;
                  //test();
                  //nIsRecog = send_result(Reqer, stTimestamp_save, stResx);

                  if (stState=="keep")
                  {
                       stState="end";
                       std::string track_id = stResx;
                       cv::Rect reRectPl = b.reCrop_plate;
                       rectangle(img, reRectPl, cv::Scalar(139, 139,   255, 255), 3);
                       
                       muLock_t.lock();
                       deSend_buff.push_back(std::tuple<std::string,int,int,std::string,std::string,cv::Rect,cv::Mat>(
                                             track_id,1,nDetectIndex,stState,stReTime,reRectPl,img));
                       muLock_t.unlock();
                           /*
                           send_trackResult(
                               Reqer, 
                               track_id, 
                               1,
                               nDetectIndex,
                               stState,
                               stReTime,
                               reRectPl,
                               img
                               );*/
                           stState = "";
                   }                        
                         
                  //cv::imshow("result",save_img);
                  //cv::namedWindow("matCropPl",CV_WINDOW_NORMAL);
                  //cv::imshow("matCropPl",matCropPl);
                  //save_Imgresult(save_img,Reqer,chRes,timestamp);     
                  if (boIssearch.first)
                  {
                      //send the current search result 
                      boIssearch.first = false;                 
                  }
              }
          }
          
          if (boIssearch.second) 
          {
                std::vector<std::string> veSresult;
                muSearchLock.lock();
                veSresult = Search_ID(veTarget);
                muSearchLock.unlock();
                boIssearch.second = false;
                //send the history search results
          }



          //cv::imshow("123",paGetmsg.first);
          //cv::waitKey(1);
          last_timestamp = paGetmsg.second;
          fps_counter2 ++;
          RK_U64 diff, now = current_ms();
          if ((diff = now - fps_ms2) >= 1000.0) 
          {
               fps2 = fps_counter2 / (diff / 1000.0);
               fps_counter2 = 0;
               fps_ms2 = now;
               printf("bgr resize FPS = %3.2f\n", fps2);   
          }   
          
      }
   }

   //send_tr.join();
}





int main(int argc, char** argv)
{
    //zmq::context_t context(1);
    //zmq::socket_t Reqer(context, ZMQ_REQ);
    
    std::string stIsSend(argv[1]);
    stIsendImg = stIsSend;
    Reqer.setsockopt(ZMQ_RCVTIMEO,2000);
    Reqer.connect(REQ_IP);

    //req_begin(Reqer);

    std::string stInfo ="";
    while(1)
    {
        stInfo = req_info(Reqer,vePlateTarget_ids,rtmp_address);
        if (stInfo!="" && stInfo!="noinfo")break;

        Reqer.close();
        Reqer = zmq::socket_t (context_req, ZMQ_REQ);
        Reqer.setsockopt(ZMQ_RCVTIMEO,2000);
        Reqer.connect(REQ_IP);
        sleep(1);

        std::cout << "123123" << std::endl;
    }
/*
    return 0;
    stSub_ip_port = SUB_IP + stInfo	;
    subscriber.setsockopt(ZMQ_RCVTIMEO,1000);
    subscriber.connect(stSub_ip_port);
    subscriber.setsockopt(ZMQ_SUBSCRIBE,"Image",1);


    thread getImg(thread_getImg);
    thread Lplate(thread_Lplate);


    while(true)
    {
      sleep(1);
    }
    getImg.join();
    Lplate.join();
*/

    thread GetMp4(thread_getMp4);
    thread Lplate(thread_Lplate);
    while(true)
    {
      sleep(10);

    }
    
    return 0;
}













