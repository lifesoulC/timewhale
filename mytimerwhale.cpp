#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <list>
#include <iostream>
#include <iterator>
#include <mutex>
#include <thread>

#define HOUR 24
#define MIN  60
#define SEC  60
#define MSEC 10

#define KEY_HOUR 1001
#define KEY_MIN  1002
#define KEY_SEC  1003
#define KEY_MSEC 1004

#define HEART true
#define SIGLE fales 


using namespace std;


typedef void (*MyTimerHandle)(void* arg);

typedef struct My_Timer_Args
{
    //是否持续定时
    bool sigle;
    
    //下次分配的位置
    int hour_slot;
    int min_slot;
    int sec_slot;
    int msec_slot;  //单位为100ms发送一次
    
    //定时时间
    int hour;
    int min;
    int sec;
    int msec;

    //当前出位置
    int index_whale;
    int index_slot;

    //回调函数
    MyTimerHandle MTHandle;
}MyTimerArgs;

static int g_Hour = 0;
static int g_Min = 0;
static int g_Sec = 0;
static int g_Msec = 0;

static list<MyTimerArgs*> Hour_Wale[HOUR];
static list<MyTimerArgs*> Min_Wale[MIN];
static list<MyTimerArgs*> Sec_Wale[SEC];
static list<MyTimerArgs*> Msec_Wale[MSEC];

static int TimerCount = 0;
//创建锁 粒度为list 在操作槽中的list时候必须上锁
static mutex Hour_Mutex[HOUR];
static mutex Min_Mutex[MIN];
static mutex Sec_Mutex[SEC];
static mutex Msec_Mutex[MSEC];



void My_PutTiemr_Slot(MyTimerArgs* arg, int Slot_Type);
void My_Set_Timer(MyTimerArgs* arg);
int My_Delete_Timer(MyTimerArgs* arg);
void Checke_Time_Whales(MyTimerArgs* timer, int whale_type);
void My_Inint(MyTimerArgs* timer);
void Run_Timer();  //启动定时任务

void DealeWithSigalrm_Sec();
void DealeWithSigalrm_Min();
void DealeWithSigalrm_Hour();

//callbake func
static void SecondHandle_First(void* arg);


//初始化定时任务对象
void My_Inint(MyTimerArgs* timer)
{
    timer->hour = 0;
    timer->min = 0;
    timer->sec = 0;
    timer->msec = 0;

    timer->hour_slot = 0;
    timer->min_slot = 0;
    timer->sec_slot = 0;
    timer->msec_slot = 0;

    timer->index_slot = 0;   //当前时间槽
    timer->index_whale = 0;  //当前时间轮

    timer->sigle = HEART;
}
//计算定时事件对应的时间槽
void My_Set_Timer(MyTimerArgs* arg)
{
    arg->hour_slot = 0;
    arg->min_slot = 0;
    arg->sec_slot = 0;
    arg->msec_slot = 0;

    if (arg->hour != 0){
        arg->msec_slot = (arg->msec + g_Msec)%MSEC;
        
        int tmp_msec = (arg->msec + g_Msec)/MSEC;
        arg->sec_slot = (arg->sec + g_Sec + tmp_msec)%SEC;
        
        int tmp_sec = (arg->sec + g_Sec + tmp_msec)/SEC;
        arg->min_slot = (arg->min + g_Min + tmp_sec)%MIN;
    
        int tmp_min = (arg->min + g_Min + tmp_sec)/MIN;
        arg->hour_slot = (arg->hour + g_Hour + tmp_min)%HOUR;

        //放置hour槽
        My_PutTiemr_Slot(arg, KEY_HOUR);
    
    } else if (arg->min != 0){

        arg->msec_slot = (arg->msec + g_Msec)%MSEC;

        int tmp_msec = (arg->msec + g_Msec)/MSEC;
        arg->sec_slot = (arg->sec + g_Sec + tmp_msec)%SEC;
        
        int tmp_sec = (arg->sec + g_Sec + tmp_msec)/SEC;
        arg->min_slot = (arg->min + g_Min + tmp_sec)%MIN;    

        //放置min槽
        My_PutTiemr_Slot(arg, KEY_MIN);

    } else if (arg->sec != 0){

        arg->msec_slot = (arg->msec + g_Msec)%MSEC;

        int tmp_msec = (arg->msec + g_Msec)/MSEC;
        arg->sec_slot = (arg->sec + g_Sec + tmp_msec)%SEC;   

      //  printf("sec_slot %d \n", arg->sec_slot );
        My_PutTiemr_Slot(arg, KEY_SEC);
        
        //放置sec槽
    } else if (arg->msec != 0){
        arg->msec_slot = (arg->msec + g_Msec)%MSEC;
         //放置msec槽
        My_PutTiemr_Slot(arg, KEY_MSEC);
    }

}
//将定时时间加入相应的槽队列中
void My_PutTiemr_Slot(MyTimerArgs* arg, int Slot_Type)
{
    if (Slot_Type == KEY_HOUR){

        arg->index_whale = KEY_HOUR;
        arg->index_slot = arg->hour_slot;
       
        {   //加锁
            std::unique_lock<std::mutex> ul(Hour_Mutex[arg->hour_slot]);
            Hour_Wale[arg->hour_slot].push_front(arg);
        }while(0);

    }

    if (Slot_Type == KEY_MIN){
   
       arg->index_whale = KEY_MIN;
       arg->index_slot = arg->min_slot;

       {
            std::unique_lock<std::mutex> ul(Min_Mutex[arg->min_slot]);
            Min_Wale[arg->min_slot].push_front(arg);
       }while(0);
       
    }

    if (Slot_Type == KEY_SEC){

        arg->index_whale = KEY_SEC;
        arg->index_slot = arg->sec_slot;
        {
            std::unique_lock<std::mutex> ul(Sec_Mutex[arg->sec_slot]);
            Sec_Wale[arg->sec_slot].push_front(arg);
        }while(0);

    }

    if (Slot_Type == KEY_MSEC){

        arg->index_whale = KEY_MSEC;
        arg->index_slot = arg->msec_slot;
        
        {
            std::unique_lock<std::mutex> ul(Msec_Mutex[arg->msec_slot]);
            Msec_Wale[arg->msec_slot].push_front(arg);
        }while(0);

    }

}
//删除定时任务
//判断当前定时任务所在的时间轮和槽号进行删除
int My_Delete_Timer(MyTimerArgs* arg)
{
    list<MyTimerArgs*>::iterator Itor;
    std::cout << arg->index_whale << std::endl << arg->index_slot <<std::endl;
    if (arg->index_whale == KEY_HOUR) {
        if (Hour_Wale[arg->index_slot].empty()){
            return -1;
        }else{
            {
                std::unique_lock<std::mutex> ul(Hour_Mutex[arg->index_slot]);
                for(Itor = Hour_Wale[arg->index_slot].begin(); Itor != Hour_Wale[arg->index_slot].end();){
                    if(*Itor == arg){
                        Hour_Wale[arg->index_slot].erase(Itor++);
                    }else{
                        ++Itor;
                    }
                }
            }while(0);
        }

    } else if (arg->index_whale == KEY_MIN) {
        if (Min_Wale[arg->index_slot].empty()){
            return -1;
        }else{
            {
                std::unique_lock<std::mutex> ul(Min_Mutex[arg->index_slot]);
                for(Itor = Min_Wale[arg->index_slot].begin(); Itor != Min_Wale[arg->index_slot].end();){
                    if(*Itor == arg){
                        Min_Wale[arg->index_slot].erase(Itor++);
                    }else{
                        ++Itor;
                    }
                }
            }while(0);
        }

    } else if (arg->index_whale == KEY_SEC) {
        if (Sec_Wale[arg->index_slot].empty()){
            return -1;
        }else{
            {
                std::unique_lock<std::mutex> ul(Sec_Mutex[arg->index_slot]);
                for(Itor = Sec_Wale[arg->index_slot].begin(); Itor != Sec_Wale[arg->index_slot].end();){
                    if(*Itor == arg){
                        Sec_Wale[arg->index_slot].erase(Itor++);
                    }else{
                        ++Itor;
                    }
                }
            }while(0);

        }

    } else if (arg->index_whale == KEY_MSEC) {
        if (Msec_Wale[arg->index_slot].empty()){
            return -1;
        }else{
            {
                std::unique_lock<std::mutex> ul(Msec_Mutex[arg->index_slot]);
                for(Itor = Msec_Wale[arg->index_slot].begin(); Itor != Msec_Wale[arg->index_slot].end();){
                    if(*Itor == arg){
                        Msec_Wale[arg->index_slot].erase(Itor++);
                    }else{
                        ++Itor;
                    }
                }
            }while(0);
        }
    }
    return 0;
}


//毫秒时间轮
void DealWithSigalrm_Msec(int sig)
{
    g_Msec++;
    //每100ms触发一次 10次为1s
    if (g_Msec ==  MSEC) {
        g_Msec = 0;
        DealeWithSigalrm_Sec();
    }

    if (Msec_Wale[g_Msec].empty()){
      //  std::cout << "Msec list is NULL " << std::endl;
        
    } else {
        {   
            std::unique_lock<std::mutex> ul(Msec_Mutex[g_Msec]);

            list<MyTimerArgs*>::iterator iter = Msec_Wale[g_Msec].begin();       
            while(iter != Msec_Wale[g_Msec].end())
            {
                Checke_Time_Whales(*iter, KEY_MSEC);
                Msec_Wale[g_Msec].erase(iter++);
            }
        }while(0);
    }
}

//秒时间轮
void DealeWithSigalrm_Sec()
{
    g_Sec++;
    //每100ms触发一次 10次为1s
    if (g_Sec ==  SEC) {
        g_Sec = 0;
        DealeWithSigalrm_Min();
    }

    if (Sec_Wale[g_Sec].empty()){
        
    } else {
        {
            std::unique_lock<std::mutex> ul(Sec_Mutex[g_Sec]);

            list<MyTimerArgs*>::iterator iter = Sec_Wale[g_Sec].begin();
            while(iter != Sec_Wale[g_Sec].end())
            {
                Checke_Time_Whales(*iter, KEY_SEC);
                Sec_Wale[g_Sec].erase(iter++);
            }
        }while(0);
    }
}
//分钟时间轮
void DealeWithSigalrm_Min()
{
    g_Min++;
    if (g_Min ==  60) {
        g_Min = 0;
        DealeWithSigalrm_Hour();
    }

    if (Min_Wale[g_Min].empty()){
       // std::cout << "Min list is NULL " << std::endl;
    } else {
        {
            std::unique_lock<std::mutex> ul(Min_Mutex[g_Min]);

            list<MyTimerArgs*>::iterator iter = Min_Wale[g_Min].begin();
            while(iter != Min_Wale[g_Min].end())
            {
                Checke_Time_Whales(*iter, KEY_MIN);
                Min_Wale[g_Min].erase(iter++);
            }
        }while(0);
    }
}
//小时时间轮
void DealeWithSigalrm_Hour()
{
    g_Hour++; 
    if (g_Hour ==  HOUR) {
        g_Hour = 0;
    }

    if (Hour_Wale[g_Hour].empty()){
        //std::cout << "Hour list is NULL " << std::endl;   
    } else {
        {
            std::unique_lock<std::mutex> ul(Hour_Mutex[g_Hour]);

            list<MyTimerArgs*>::iterator iter = Hour_Wale[g_Hour].begin(); 
            while(iter != Hour_Wale[g_Hour].end())
            {
                Checke_Time_Whales(*iter, KEY_HOUR);
                Hour_Wale[g_Hour].erase(iter++);
            }
        }while(0);
    }
}
//检查timer下次应加入的时间轮
void Checke_Time_Whales(MyTimerArgs* timer, int whale_type)
{
    if (whale_type == KEY_HOUR){
        if (timer->min_slot != 0) {
            //加入分钟时间轮
            My_PutTiemr_Slot(timer,KEY_MIN);
        } else if (timer->sec_slot != 0) {
            //加入秒时间轮
            My_PutTiemr_Slot(timer,KEY_SEC);
        } else if (timer->msec_slot != 0) {
            //加入毫秒时间轮
            My_PutTiemr_Slot(timer,KEY_MSEC);
        } else if (timer->sigle) {
            My_Set_Timer(timer);
            //触发定时任务
            timer->MTHandle((void*)timer);
        } else {
            timer->MTHandle(NULL);
        }

    } else if (whale_type == KEY_MIN){
        if (timer->sec_slot != 0) {
            My_PutTiemr_Slot(timer,KEY_SEC);
        } else if (timer->sec_slot != 0) {
            My_PutTiemr_Slot(timer,KEY_MSEC);
        } else if (timer->sigle) {
            My_Set_Timer(timer);
            timer->MTHandle((void*)timer);
        } else {
            timer->MTHandle(NULL);
        }

    } else if (whale_type == KEY_SEC){
        if (timer->msec_slot != 0) {
            My_PutTiemr_Slot(timer,KEY_MSEC);
        } else if (timer->sigle) {

            My_Set_Timer(timer);
            timer->MTHandle((void*)timer);    
        } else {
            timer->MTHandle(NULL);
        }

    } else if (whale_type == KEY_MSEC) {
        if (timer->sigle) {
            My_Set_Timer(timer);
           // printf("MSec_Wale g_min:%d    g_sec:%d   g_msec:%d msec_slot:%d:\n",g_Min, g_Sec, g_Msec, timer->msec_slot);
            timer->MTHandle((void*)timer);  
        } else {
            timer->MTHandle(NULL);
        }
    }
}

void Run_Timer(){
    struct itimerval value, ovalue, value2;

    if(signal(SIGALRM,DealWithSigalrm_Msec) == SIG_ERR){
        puts("error regist sigalrm");
        return;
    }
    value.it_value.tv_sec = 0;                //timer start after some seconds later
    value.it_value.tv_usec = 1;                 //usec
    value.it_interval.tv_sec = 0;               //every sec seconds send the signale
    value.it_interval.tv_usec = 100000;           //every 100msec seconds send the signale
    setitimer(ITIMER_REAL, &value, &ovalue);    //ITIMER 以系统真实的时间来计算，它推送出SIGALRM信号 &ovalue用来存储上一次setitimer调用时设置的new_value值
}



////////////////////回调函数////////////////////////////////////////////
static void SecondHandle_First(void* arg)
{
    printf("MY_TIMER      hour:%d min:%d sec:%d    msec:%d \n",((MyTimerArgs*)arg)->hour, ((MyTimerArgs*)arg)->min, ((MyTimerArgs*)arg)->sec, ((MyTimerArgs*)arg)->msec);
    printf("TIME_NOW           %d    :%d    :%d       :%d \n",g_Hour,g_Min, g_Sec, g_Msec, g_Msec); 
    printf("NEXT_SET_SLOT hour:%d min:%d sec:%d   msec:%d \n\n",((MyTimerArgs*)arg)->hour_slot, ((MyTimerArgs*)arg)->min_slot, ((MyTimerArgs*)arg)->sec_slot, ((MyTimerArgs*)arg)->msec_slot);
    
}
static void SecondHandle_Second(void* arg)
{
    printf("MY_TIMER      hour:%d min:%d sec:%d    msec:%d \n",((MyTimerArgs*)arg)->hour, ((MyTimerArgs*)arg)->min, ((MyTimerArgs*)arg)->sec, ((MyTimerArgs*)arg)->msec);
    printf("TIME_NOW           %d    :%d    :%d       :%d \n",g_Hour,g_Min, g_Sec, g_Msec, g_Msec); 
    printf("NEXT_SET_SLOT hour:%d min:%d sec:%d   msec:%d \n\n",((MyTimerArgs*)arg)->hour_slot, ((MyTimerArgs*)arg)->min_slot, ((MyTimerArgs*)arg)->sec_slot, ((MyTimerArgs*)arg)->msec_slot);
}

static void SecondHandle_Everysecond(void* arg)
{
   // std::cout << "timer sec:" <<((MyTimerArgs*)arg)->sec << std::endl;
    printf(" Every Second:\n\n\n\n");
}




int main()
{
    MyTimerArgs mta1, mta2, mta3, mta4, mta5;

    //初始化定时器
    My_Inint(&mta1);
    My_Inint(&mta2);
    My_Inint(&mta3);
    My_Inint(&mta4);
    My_Inint(&mta5);


    //设置时间
    mta1.msec = 0;
    mta1.sec = 1;
    mta1.min = 0;
    mta1.MTHandle = SecondHandle_First;

    mta2.msec = 0;
    mta2.sec = 1;
    mta2.MTHandle = SecondHandle_Everysecond;

    mta3.msec = 0;
    mta3.sec = 4;
    mta3.min = 2;
    mta3.MTHandle = SecondHandle_First;
    
    
    mta4.msec = 0;
    mta4.sec = 4;
    mta4.min = 1;
    mta4.MTHandle = SecondHandle_Second;

    mta5.msec = 0;
    mta5.sec = 3;
    mta5.min = 0;
    mta5.MTHandle = SecondHandle_Second;
    //注册定时器
    My_Set_Timer(&mta1);
    My_Set_Timer(&mta2);
    My_Set_Timer(&mta3);
    My_Set_Timer(&mta4);

    My_Set_Timer(&mta5);




    int error = My_Delete_Timer(&mta5);
    if (error != 0) {
        std::cout << "delet error " << endl;
    }


    //设置系统定时信号发送频率
    Run_Timer();  //每100ms触发一次信号 驱动时间轮

    while(1){
        usleep(1000);
    }
    return 0;
}

/*****************READ ME***********************
时间结构体
struct itimerval {  
    struct timeval it_interval; // next value 
    struct timeval it_value;    // current value 
};  
  
struct timeval {  
    time_t      tv_sec;         // seconds 
    suseconds_t tv_usec;        // microseconds   微秒
}; 

编译
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

*/
