#include <stdio.h>
#include <iostream>
#include <opencv2/video.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <bitset>
#include <string>
#include <unistd.h>
#include <time.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <locale.h>
#include <wchar.h> 
#include <fstream>
#include <string>
#include <stdlib.h>
#include <sys/time.h>
//thread
#include "thread"
#include "mutex"
#include <condition_variable>

//zqm files
#include <zmq.hpp>
//#include "zhelpers.hpp"

//tensorRT_yolo
#include "ds_image_zmq.h"
#include "trt_utils.h"
#include "yolo.h"
#include "yolo_config_parser.h"
#include "yolov2.h"
#include "yolov3.h"
#include <experimental/filesystem>

#define TCP_PORT "5563"    
#define STAM_SIZE 16  
#define ZMQ
#define TENSORRT_YOLO
#define IMGWIDTH 320
#define IMGHEIGHT 180
#define SUB_IP "tcp://192.168.3.244:"
//#define stSub_ip_port "tcp://192.168.3.244:5558"

//#define SUB_IP "epgm://192.168.3.244;239.0.1.2:20480"
#define REQ_IP "tcp://192.168.3.244:5553"
using namespace cv;
using namespace std;
//string stSub_ip_port ="tcp://192.168.3.244:5553";
string stSub_ip_port;
bool sendimg = true;
void get_img();
void detect_trafficcone(int argc, char** argv);
void get_imginfo(pair<Mat,string>& info);
void send_info();
string req_port(zmq::socket_t& reqer);
pair<Mat,string> g_imginfo;
pair<Mat,string> g_imginfoT;
mutex mutex_imginfo;
condition_variable cond_imginfo;
bool g_imginfo_flag = false;
string g_send_img = "";
bool g_send_flag = false;
mutex mutex_send_img;
condition_variable cond_send_img;

struct Content 
{
   char timestamp[24];
   char imglength[24];
   uchar img_data[500000];//根据数据量修改，图片的字节=width*heigh*channel
   
};

int main(int argc, char** argv)
{
	
	zmq::context_t context(1);
	zmq::socket_t reqer(context, ZMQ_REQ);	
	reqer.setsockopt(ZMQ_RCVTIMEO,2000);
    reqer.connect("tcp://192.168.3.244:5553");
	string stPort = " ";
	while(stPort == " ")
	{
		stPort = req_port(reqer);
	}
	stSub_ip_port = SUB_IP + stPort;
	
	thread getobjinfo(detect_trafficcone,argc, argv);
	sleep(5);
	thread sendImg(send_info);
	sleep(3);	
	thread getframe(get_img);		
	
	sendImg.join();
	getobjinfo.join();
	getframe.join();
	return 0;
}

string req_port(zmq::socket_t& reqer)
{
	string name = "name=detector4;action=getport";
	zmq::message_t message(strlen(name.c_str()));
	memcpy((void*)message.data(),name.c_str(),strlen(name.c_str()));
	reqer.send(message);
	zmq::message_t reply;
	reqer.recv(&reply);
	string remsg=string(static_cast<char*>(reply.data()),reply.size());
	if(reply.size() < 2)
	{
		cout<<"NO RECV PORT !"<<endl;
		exit(0);
	}
	else if(remsg == "novideo")
	{
		cout<<"NO VIDEO !"<<endl;
		exit(0);
	}
	else
	{
		cout<<"video recv port:"<<remsg<<endl;
	}
	return remsg;

}
void send_info()
{
	zmq::context_t contextr(1); //创建类
	zmq::socket_t REQsocket(contextr, ZMQ_REQ);//绑定套节字		
	//zmq::socket_t REQsocket(contextr, ZMQ_PUB);	
	int catchNum = 1;
	REQsocket.setsockopt(ZMQ_SNDHWM,&catchNum, sizeof(catchNum));
	REQsocket.connect (REQ_IP);	
	double times = static_cast<double>(cvGetTickCount());
	int n = 0;
	while(true)
	{
		n=n+1;
		usleep(10000);		
		unique_lock<mutex> lock_send_img(mutex_send_img);
		while(!g_send_flag)
		{
		     cond_send_img.wait(lock_send_img);
		}		
		//cout<<"send:"<<g_send_img<<endl;
		zmq::message_t req_message(strlen(g_send_img.c_str()));
		memcpy((void*)req_message.data(),g_send_img.c_str(), strlen(g_send_img.c_str()));
		REQsocket.send(req_message);
		g_send_flag = false;
		lock_send_img.unlock();
		
		zmq::message_t reply;			
		REQsocket.recv(&reply);
		string remsgr = std::string(static_cast<char*>(reply.data()), reply.size());	
		//cout<<"***********remsgr:"<<remsgr<<endl;
		
		double timee = static_cast<double>(cvGetTickCount());
		double T = (timee-times)/(cvGetTickFrequency()*1000);
		if(T>5000)
		{
			double f = (n*1000)/T;
			cout<<"send FPS:"<<f<<endl;
			times = static_cast<double>(cvGetTickCount());
			n = 0;			
		}
	}
	REQsocket.close();
}

void get_imginfo(pair<Mat,string>& info)
{
	g_imginfoT = info;
	info = g_imginfo;
	g_imginfo = g_imginfoT;

}

void detect_highway_person(int argc, char** argv)
{

	cout << "initialized tensorRT yolo engine ..."<<endl;
	 // Flag set in the command line overrides the value in the flagfile
	gflags::SetUsageMessage(
	"Usage : trt-yolo-app --flagfile=</path/to/config_file.txt> --<flag>=value ...");
	// parse config params
	yoloConfigParserInit(argc, argv);
	NetworkInfo yoloInfo = getYoloNetworkInfo();
	InferParams yoloInferParams = getYoloInferParams();
	uint64_t seed = getSeed();
	string networkType = getNetworkType();	
	string saveDetectionsPath = getSaveDetectionsPath();
	uint batchSize = getBatchSize();	
	srand(unsigned(seed));
	
	unique_ptr<Yolo> inferNet{nullptr};
	if ((networkType == "yolov2") || (networkType == "yolov2-tiny"))
	{
		inferNet = std::unique_ptr<Yolo>{new YoloV2(batchSize, yoloInfo, yoloInferParams)};
	}
	else if ((networkType == "yolov3") || (networkType == "yolov3-tiny"))
	{
		inferNet = std::unique_ptr<Yolo>{new YoloV3(batchSize, yoloInfo, yoloInferParams)};
	}
	else
	{
		assert(false && "Unrecognised network_type. Network Type has to be one among the following : yolov2, yolov2-tiny, yolov3 and yolov3-tiny");
	}
	cout << "initialized tensorRT yolo engine success !"<<endl;
 
	//double times = static_cast<double>(cvGetTickCount());
	int n =0;
	std::vector<DsImage> dsImages;	
	string old_timestamp = "0";
	while(true)
	{
		n=n+1;
		usleep(5000);
	#ifdef TENSORRT_YOLO
		//double times = static_cast<double>(cvGetTickCount());					
		pair<Mat,string> imginfo;
		unique_lock<mutex> lock_imginfo(mutex_imginfo);
		while(!g_imginfo_flag)
		{
		     cond_imginfo.wait(lock_imginfo);
		}		
		get_imginfo(imginfo);
		g_imginfo_flag = false;
		lock_imginfo.unlock();
		Mat recv_img = imginfo.first;

		string timestamp = imginfo.second;
		dsImages.clear();
		dsImages.emplace_back(recv_img, inferNet->getInputH(),inferNet->getInputW());
		cv::Mat trtInput = blobFromDsImages(dsImages, inferNet->getInputH(), inferNet->getInputW());		
		inferNet->doInference(trtInput.data, dsImages.size());
	#endif
				
		//for (uint imageIdx = 0; imageIdx < dsImages.size(); ++imageIdx)
		//{
			uint imageIdx = 0;
			auto curImage = dsImages.at(imageIdx);
			auto binfo = inferNet->decodeDetections(imageIdx, curImage.getImageHeight(),curImage.getImageWidth());
			auto remaining = nmsAllClasses(inferNet->getNMSThresh(), binfo, inferNet->getNumClasses());		

			string posboxs;
			int box_num = 0;
			for (auto b : remaining)
			{
				 float area = (b.box.x2-b.box.x1)*(b.box.y2-b.box.y1);
		    		if((area<9000)&&(area>400)&&(b.prob>0.6))
				{   					   
					string posbox = "trafficcone,"+to_string(int(b.box.x1))+","+to_string(int(b.box.y1))+","+to_string(int(b.box.x2-b.box.x1))+
							","+to_string(int(b.box.y2-b.box.y1))+",";
					posboxs = posboxs+posbox;
					box_num += 1;
					rectangle(recv_img,Point(int(b.box.x1),int(b.box.y1)),Point(int(b.box.x2),int(b.box.y2)),Scalar(0,0,255));
				}
			}

			string send_img = "name=detector4;" + timestamp;
			if(box_num == 0)
			{
				send_img = send_img + "event=withouttrafficcone;";
			}else
			{
				send_img = send_img + "event=withtrafficcone;";
			}
			send_img = send_img + "trafficcone_num=" +to_string(box_num)+","+posboxs;
			unique_lock<mutex> lock_send_img(mutex_send_img);
			g_send_img = send_img;	
			g_send_flag = true;	
			lock_send_img.unlock();
			cond_send_img.notify_all();
			//double timee = static_cast<double>(cvGetTickCount());
		//double T = (timee-times)/(cvGetTickFrequency()*1000);
			//cout<<"detect_cost_time:"<<T<<endl;
			//cout<<"send_img:"<<send_img<<endl;	
		//}
		
		//imshow("detect-img",recv_img);
		//waitKey(1);
		/*double timee = static_cast<double>(cvGetTickCount());
		double T = (timee-times)/(cvGetTickFrequency()*1000);
		if(T>2000)
		{
			double f = (n*1000)/T;
			cout<<"detect FPS:"<<f<<endl;
			times = static_cast<double>(cvGetTickCount());
			n = 0;		
		}*/
	}
}

void get_img()
{
	cout<<"initlize zmq..."<<endl;
	zmq::context_t context(1);
	zmq::socket_t subscriber (context, ZMQ_SUB); //zqm tcp subscriber
	int cacheNum = 2;
	subscriber.setsockopt( ZMQ_RCVHWM, &cacheNum, sizeof(cacheNum));
	subscriber.connect(stSub_ip_port);
	subscriber.setsockopt( ZMQ_SUBSCRIBE, "Image", 1);
	cout<<"initlize zmq success!"<<endl;
	vector<uchar> buffer;	
	//double times = static_cast<double>(cvGetTickCount());
	int n =0;
	
	while(true)
	{
		buffer.clear();	
		zmq::message_t reply;				
		subscriber.recv(&reply);
		subscriber.setsockopt(ZMQ_RCVTIMEO, 10);
		//cout<<"reply.size():"<<reply.size()<<endl;						
		if(reply.size()<100) continue;		
		n=n+1;
		if(n%9==0) continue;
		Content* input = reinterpret_cast<Content*>(reply.data());
		string timestamp = (input)->timestamp;
		string imglength = (input)->imglength;
		int imgstrlength = imglength.length();
		//cout<<"imaglength:"<<imglength<<endl;
		imglength = imglength.substr(12,(imgstrlength-12));
		int imageNum = stoi(imglength);
		for(int i=0; i<imageNum; i++)
		{
			buffer.push_back(((input)->img_data)[i]);
		}	
		Mat recvimg = imdecode(buffer,CV_LOAD_IMAGE_COLOR);
		//double timee = static_cast<double>(cvGetTickCount());
		//double T = (timee-times)/(cvGetTickFrequency()*1000);
		//cout<<"recv_costtime:"<<T<<", imglength:"<<imglength<<endl;
		//times = timee;	
		//double timee = static_cast<double>(cvGetTickCount());
		//double T = (timee-times)/(cvGetTickFrequency()*1000);
		/*if(T>5000)
		{
			double f = (n*1000)/T;
			cout<<"recv FPS:"<<f<<endl;
			times = static_cast<double>(cvGetTickCount());
			n = 0;
			
		}*/		
		//imshow("recv_img",recvimg);
		//waitKey(1);

		unique_lock<mutex> lock_imginfo(mutex_imginfo);
		g_imginfo.first = recvimg;
		g_imginfo.second = timestamp;
		g_imginfo_flag = true;
		lock_imginfo.unlock();
		cond_imginfo.notify_all();				
	}
	subscriber.close();
}


