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

#define REQ_IP "tcp://127.0.0.1:5530"

int main(){
    zmq::context_t context_rep(1);
    zmq::socket_t socket_rep(context_rep, ZMQ_REP);
    socket_rep.bind("tcp://*:5530");
    cout << "The rep server is starting." << endl;
    //int i =0;

    while(true){
        zmq::message_t message_req;
        socket_rep.recv(&message_req);
	string remsg = string(static_cast<char*>(message_req.data()),message_req.size());
	cout <<"message_req: " << remsg << endl;

	//zmq_sleep(1);

        zmq::message_t reply(3);
        memcpy(reply.data(), "OK", 3);
        socket_rep.send(reply);
	}
}
