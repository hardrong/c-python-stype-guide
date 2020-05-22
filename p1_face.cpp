#include <pthread.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <unistd.h>
#include <jsoncpp/json/json.h>
#include "aip-cpp-sdk-0.8.1/face.h"//引入百度api头文件

using namespace aip;
using namespace std;
using namespace cv;
using namespace Json;

//定义互斥锁
pthread_mutex_t my_mutex;

//检测到的人脸区域
Rect face;
//两个线程要进行操作的图像
Mat image;
string user_id;	
//创建子线程，进行人脸检测和识别
void * face_op(void *arg)
{
	Mat gray;
	//创建级联分类器，并加载人脸检测模型
	//检测次数
	int count =0;
	CascadeClassifier classifier;
	classifier.load("/usr/share/opencv/haarcascades/haarcascade_frontalface_alt2.xml");
	
	//创建人脸识别的客户端
	string app_id ="19966996";
	string api_key ="gbBhAHEVDTQUCfLRfP1mL4Pi";
	string secret_key ="rKHmnvXDHve7V8KVOeKXkZOo2p8hiXKK";

	Face client(app_id,api_key,secret_key);
	while(1)
	{
		//申请互斥锁
		pthread_mutex_lock(&my_mutex);
		
		//将彩色图像变为灰色
		cvtColor(image,gray,COLOR_BGR2GRAY);
		
		//释放互斥锁
		pthread_mutex_unlock(&my_mutex);
		
		//对图像进行均衡化
		equalizeHist(gray,gray);
		//检测人脸
		vector <Rect> faces;
		classifier.detectMultiScale(gray,faces);

		//如果有人脸
		if (faces.size())
		{
			face=faces[0];
			cout<<"已经检测到人脸!!!"<<endl;
			count++;

		}
		else
		{
			count=0;
		}

		//连续检测到3次以上再进行人脸识别
		if(2==count)
		{
			count=0;
		


			//一秒最多检测5次
		//	usleep(200000);

			//创建人脸识别的c++客户端
			//string app_id = "18046240"
			//string api_key = "VLd6Kayr21fdgAzuiad1WrRE"
			//string secret_key = "0m2OybB35EGuV4WezKHegmAfGUFKUvPB"
		
			//将人脸部分截取出来
			Mat faceRect(image,faces[0]);
	
			//将Mat转化为JPG格式
			vector <unsigned char> buf;
			imencode(".jpg",faceRect,buf);

			//将buf中的数据转化为base64格式
			string base_faceimg = base64_encode((char *)buf.data(),buf.size());

			//将人脸图片发给百度云进行识别，并将识别结果保存在json对象中
			Value people_json=client.search (base_faceimg,"BASE64","base",aip::null);

			//显示识别成功结果
			cout<<people_json<<endl;
			float score = people_json["result"]["user_list"][0]["score"].asFloat();

			if (score>90)
			{
				user_id.clear();
				pthread_mutex_lock(&my_mutex);
				user_id=people_json["result"]["user_list"][0]["user_id"].asString();
				user_id.append("punch OK!!");
				pthread_mutex_unlock(&my_mutex);
			}
		}

	}	
}

int main(void)
{

	pthread_t pid1;
	//创建摄像头对象并打开
	VideoCapture vie;
	vie.open(0);

	//创建窗口
	namedWindow("camera");
	
	//初始化互斥锁
	pthread_mutex_init(&my_mutex,nullptr);
	//创建线程
	pthread_create(&pid1,nullptr,face_op,nullptr);

	while(1){
		//读取一帧图像
		//申请互斥锁
		pthread_mutex_lock(&my_mutex);
		vie>>image;
		
		//释放互斥锁
		pthread_mutex_unlock(&my_mutex);
		//将检测到的人脸用矩形框标记出来
		rectangle(image,face,CV_RGB(255,0,0));

		//将识别结果打印在屏幕上
		putText(image,user_id,Point(30,40),FONT_HERSHEY_SIMPLEX,2,Scalar(0,0,255),4,8);

		//将摄像头捕获的图像显示出来
		imshow("camera",image);
		//延时处理
		if(waitKey(40)!=255) break;
		

		}
	//销毁互斥锁
	pthread_mutex_destroy(&my_mutex);
	return 0;
}
