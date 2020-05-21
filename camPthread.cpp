#include<opencv2/opencv.hpp>
#include<pthread.h>
#include<iostream>
#include<unistd.h>
#include <jsoncpp/json/json.h>
#include<time.h>
#include<sqlite3.h>
#include "aip-cpp-sdk-0.8.1/face.h"

using namespace std;
using namespace cv;
using namespace aip;
using namespace Json;

Rect face;//检测到的人脸
Mat image;//摄像机采集的图像
pthread_mutex_t my_mutex;//互斥锁
string user_id;//识别的人脸的姓名
char buff[80] = {'\0'};//存储当前时间


//将打卡信息保存在数据库
void * save_db(void * arg)
{
	sqlite3 * db;
	char * errmsg;
	int rc = 0;
	rc = sqlite3_open("attend.db",&db);
	if(rc)
	{
		cout<<"打开数据库失败！"<<endl;
	}
	else
	{
		cout<<"打开数据库成功！"<<endl;
		string sql = "create table punch1(user_id text not null,punch_time text not null)";
		//执行sql，创建数据库
		if(sqlite3_exec(db,sql.c_str(),nullptr,nullptr,&errmsg)!=SQLITE_OK)
		{
			cout<<errmsg<<endl;
		}
		else
		{
			cout<<"创建成功"<<endl;
		}
		while(1)
		{
			if(user_id.size()==0)
			{
				continue;
			}
			//sql = "insert into punch1 values('me','2020/2/21 11;21;45')";
			sql = "insert into punch1 values(\'"+user_id+"\',\'"+buff+"\')";
			if(sqlite3_exec(db,sql.c_str(),nullptr,nullptr,&errmsg)!=SQLITE_OK)
			{
				cout<<errmsg<<endl;
			}
			else
			{
				cout<<"插入成功"<<endl;
				sleep(1);
				pthread_mutex_lock(&my_mutex);
				user_id.clear();
				pthread_mutex_unlock(&my_mutex);
			}
		}
	}
	//关闭数据库
	sqlite3_close(db);
}


//获取当前时间
void * time_fun(void * arg)
{	
	//获取1970到现在的秒数
	time_t now_time;
	while(1)
	{
		time(&now_time);
		//获取当地时间
		struct tm * ptime;
		ptime = localtime(&now_time);
		//将时间格式化保存到buff中
		pthread_mutex_lock(&my_mutex);
		strftime(buff,80,"%D %H:%M:%S",ptime);	
		pthread_mutex_unlock(&my_mutex);
		sleep(1);
	}
}

void * face_fun(void * arg)
{	
	
	Mat gray,equal;
	int count = 0;
	//创建集连分类器，加载人脸检测模型
	CascadeClassifier classifier;
	classifier.load("/usr/share/opencv/haarcascades/haarcascade_frontalface_alt2.xml");
	//创建人脸识别客户端函数
	string app_id = "18103380";
	string api_key = "b9knYf7YEfCHcN7ai3X5SBm3";
	string secret_key = "ivdxhtSnlt35KFhxAZ0KoWWNnUFGeSCG";
	Face client(app_id,api_key,secret_key);
	while(1)
	{	
		//转换成灰度图
		pthread_mutex_lock(&my_mutex);
		cvtColor(image,gray,COLOR_RGB2GRAY);
		pthread_mutex_unlock(&my_mutex);
		//直方图均衡
		equalizeHist(gray,equal);
		//检测人脸
		vector<Rect> faces;
		classifier.detectMultiScale(equal,faces);//检测人脸
		if(faces.size())
		{
			face = faces[0];
			count++;
			cout<<"检测到人脸"<<endl;
			//imshow("face",face);
	
		}
		else
		{
			count = 0;
		}
		if(3==count)
		{	
			count = 0;
			//连续检测三次，进行人脸识别
			//截取人脸
			Mat faceRect(image,faces[0]);
			//将Mat转换成jpg格式
			vector<unsigned char> buf;
			imencode(".jpg",faceRect,buf);//转换后的代码放在buf中
			//将jpg格式的人脸图片编码转换成Base64格式
			string faceImg = base64_encode((char *)buf.data(),buf.size());
			//将人脸图片发送到百度云进行识别，将识别结果保存到json中
			Value people_json = client.search(faceImg,"BASE64","group1",aip::null);
			cout<<people_json<<endl;
			//访问json中的值
			float score = people_json["result"]["user_list"][0]["score"].asFloat();
			if(score > 50)
			{
				user_id.clear();
				pthread_mutex_lock(&my_mutex);
				user_id = people_json["result"]["user_list"][0]["user_id"].asString();
				user_id.append(" punch ok!");
				pthread_mutex_unlock(&my_mutex);
				cout<<people_json["result"]["user_list"][0]["user_id"]<<buff<<"打卡成功!!!!!!"<<endl;
			}
		}
		usleep(200000);
	}

}

int main(void)
{	
	//线程id
	pthread_t pid1,pid2,pid3;
	//创建摄像头对象，并打开
	VideoCapture cam1;
	cam1.open(0);
	//创建显示窗口
	namedWindow("outImage");
	//初始化锁
	pthread_mutex_init(&my_mutex,nullptr);
	//创建线程1，负责人脸识别
	pthread_create(&pid1,nullptr,face_fun,nullptr);
	//创建线程2，负责获取时间
	pthread_create(&pid2,nullptr,time_fun,nullptr);
	//创建线程3，将打卡信息保存到数据库
	pthread_create(&pid3,nullptr,save_db,nullptr);
	while(1)
	{
		pthread_mutex_lock(&my_mutex);
		cam1>>image;	
		pthread_mutex_unlock(&my_mutex);
		rectangle(image,face,CV_RGB(255,0,0));
		//在图片上显示文字
		putText(image,user_id,Point(30,20),FONT_HERSHEY_SIMPLEX,0.8,Scalar(0,0,255),4,8);
		putText(image,buff,Point(30,80),FONT_HERSHEY_SIMPLEX,0.8,Scalar(0,255,255),4,8);
		imshow("outImage",image);
		if(waitKey(40)!=255)
		{
			break;
		}
	}
	pthread_mutex_destroy(&my_mutex);
	return 0;
}
