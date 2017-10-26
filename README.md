# timewhale


###时间结构体

struct itimerval {  

    struct timeval it_interval; // next value 

    struct timeval it_value;    // current value 

};  

  

struct timeval {  

    time_t      tv_sec;         // seconds 

    suseconds_t tv_usec;        // microseconds   微秒

}; 



###编译

g++ -std=c++11 -o mytimerwhale mytimerwhale.cpp   







1>先声明定时任务对象结构体

    MyTimerArgs mta1;



2>初始化定时任务：

    My_Inint(&mta1);



3>设置定时任务时间：

    mta1.msec = 1;  //100毫秒

    mta1.sec = 1;   //1秒

    mta1.min = 0;   //0分钟

    mta1.hour = 0;  //0小时   如果不赋值默认为0

    mta1.MTHandle = SecondHandle_First;  //设置回调函数 定时到期会触发

    timer->sigle = HEART;      //HEART 为心跳模式   SIGLE 为单次定时模式  默认为HEART模式



4>注册定器任务

    My_Set_Timer(&mta1);



5>删除定时任务

    int error = My_Delete_Timer(&mta1); //返回0为删除成功 -1为是失败



6>设置系统定时信号发送频率

    Run_Timer();   //每100ms触发一次信号 驱动时间轮




