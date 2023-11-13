/********************
 * @File Name: server.c
 * @Autuor: wn
 * @Version: 1.0
 * @Email:wn418@outlook.com
 * Creat Time: 2023.10.20 12:28
*********************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sqlite3.h"
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define R 1 //用户注册
#define L 2 //用户登录
#define Q 3 //用户查询
#define H 4 //用户历史记录
#define DATBASE "my.db"
#define N 32


//通信双方的信息结构体
typedef struct 
{
    /* data */
    int type;
    char name[N];
    char data[256];
}MSG;

//处理客户端具体信息
int do_client(int acceptfd, sqlite3 *db); 
//注册
void do_register(int acceptfd, MSG *msg, sqlite3 *db);
//登录
int do_login(int acceptfd, MSG *msg, sqlite3 *db); 
//查词
int do_query(int acceptfd, MSG *msg, sqlite3 *db); 
 //历史记录
int do_history(int acceptfd, MSG *mag, sqlite3 *db); 
//历史记录回调函数
int history_callback(void *arg, int f_num, char **f_value, char **f_name);

int do_searchword(int acceptfd, MSG *msg, char word[]);
//获取日期
int get_date(char *date);


int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in serveraddr;
    int n; //接收输入，选择模式
    MSG msg;
    sqlite3 *db; //数据库
    int acceptfd;//接收客户端的连接请求
    pid_t pid;

    if (argc != 3)
    {
        // ./main.o 192.168.0.1 10000
        printf("usage: %s  IP Error\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //打开数据库
    if (sqlite3_open(DATBASE, &db) != SQLITE_OK)
    {
        printf("%s\n", sqlite3_errmsg(db)); //错误消息
        return -1;
    }
    else
    {
        printf("open sql\n");
    }

    //套接字
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socker error\n");
        return -1;
    }

    //清零前sizeof(serveraddr)个字节的空间
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; //指定IP版本为IPV4
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]); //互联网地址  IP
    serveraddr.sin_port = htons(atoi(argv[2]));   //端口
    
   //连接服务器   套接字与IP绑定
   if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
   {
        perror("fail to bind");
        return -1;
   }

   //将套接字设置为监听模式 用来监听客户端请求
   if (listen(sockfd, 5) < 0)
   {
        printf("fail to listen\n");
        return -1;
   }

    //处理僵尸进程
    //子进程状态发生变化，就回收子进程
    signal(SIGCHLD, SIG_IGN); //SIG_IGN忽略信号

    while (1)
    {
        //获取连接套接字 端口号与这个套接字相连
        if ((acceptfd = accept(sockfd, NULL, NULL)) < 0) //获取客户端的连接请求并建立连接
        {
            perror("fail to accept");
            return -1;
        }

        if ((pid = fork()) < 0)
        {
            perror("fork error");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) //子进程
        {
            //子进程不需要监听，关闭监听套接字sockfd
            close(sockfd);

            //处理客户端具体的消息
            do_client(acceptfd, db);
        }
        else //父进程 接收客户端的请求
        {
            //父进程不需要客户端的接收套接字acceptfd,节省资源，直接关闭掉
            close(acceptfd);

        }
    }

    return 0;
}

/// @brief 子进程执行 处理客户端消息
/// @param acceptfd 套接字
/// @param db 数据库
/// @return 正确0
int do_client(int acceptfd, sqlite3 *db)
{
    MSG msg;
    while (recv(acceptfd, &msg, sizeof(msg), 0) > 0)
    {
        //打印数据类型
        printf("type:%d\n", msg.type); 
        switch (msg.type)
        {
            case R:
                printf("R\n");
                do_register(acceptfd, &msg, db);
                break;
            case L:
                do_login(acceptfd, &msg, db);
                break;
            case Q:
                do_query(acceptfd, &msg, db);
                break;
            case H:
                do_history(acceptfd, &msg, db);
                break;
            default:
                printf("Invalid data msg\n");
        }
    }
    printf("client exit\n");
    close(acceptfd);
    exit(EXIT_SUCCESS);

    return 0;
}

/// @brief 数据库插入数据
/// @param acceptfd 套接字
/// @param msg 通信结构体
/// @param db 数据库
void do_register(int acceptfd, MSG *msg, sqlite3 *db)
{
    printf("YES\n");
   char *errmsg;
   char sql[1024];

   sprintf(sql, "insert into usr values('%s', %s);", msg->name, msg->data); //sql语句拼接
   printf("%s\n", sql); //打印sql信息

    //插入数据
   if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
   {
        //插入失败
        printf("%s\n", errmsg);
        strcpy(msg->data, "usr name already exist");

   }
   else 
   {
        //插入成功
        printf("client register ok!\n");
        strcpy(msg->data, "ok");
   }

   //回复客户端信息
   if (send(acceptfd, msg, sizeof(MSG), 0) < 0)
   {
        perror("fail to send");
        return ;
   }

   return ;
}

/// @brief 查找用户
/// @param acceptfd 
/// @param msg 
/// @param db 
/// @return 1成功 0失败
int do_login(int acceptfd, MSG *msg, sqlite3 *db)
{
    char  sql[1024] = {0};
    char *errmsg;
    int nrow; //查询是否成功
    int ncloumn; 
    char **resultp;

    //sql命令拼接
    sprintf(sql, "select * from usr where name = '%s' and pass = '%s';", msg->name, msg->data);
    printf("%s\n", sql);

    //查询
    if (sqlite3_get_table(db, sql, &resultp, &nrow, &ncloumn, &errmsg) != SQLITE_OK)
    {
        printf("%s\n", errmsg);
        return -1;
    }
    else
    {
        //仅代表if后的语句执行成功 (查询成功) 库中不一定有这个用户
        printf("get table ok\n");
    }

    //查询成功，数据库拥有此用户
    if (nrow == 1)
    {
        //复制数据到msg中
        strcpy(msg->data, "OK");
        //发送结果到客户端
        send(acceptfd, msg, sizeof(MSG), 0);
        return 1;
    }

    //用户名或密码错误
    if (nrow == 0)
    {
        strcpy(msg->data, "usr/passwd wrong");
        //发送结果到客户端
        send(acceptfd, msg, sizeof(MSG), 0);
    }

    return 0;
}

/// @brief 打开文件 读取查找单词 截取单词注释并返回
/// @param acceptfd 套接字
/// @param msg 通信结构体
/// @param word 存放要查找的单词
/// @return 1找到 0没找到
int do_searchword(int acceptfd, MSG *msg, char word[])
{
    FILE *fp;
    int len = 0;
    char temp[512] = {};
    int result;
    char *p;

    //打开文件 读取文件 进行比对
    if ((fp = fopen("dict.txt", "r")) == NULL)
    {
        perror("fail to fopen\n");
        strcpy(msg->data, "Fail to open dict.txt");
        send(acceptfd, msg, sizeof(MSG), 0);
        return -1;
    }

    //打印文件 读取文件 进行比对
    len = strlen(word);
    printf("%s, len = %d\n", word, len);

    //读文件 查询单词
    while(fgets(temp, 512, fp) != NULL)
    {
        //printf("temp:%s\n", temp);
        //判断读取的数据和要查询的是否一致
        result = strncmp(temp, word, len);
        //printf("result:%d temp:%s\n", result, temp);

        /*
        查找aa 找到ab* aa的ascii码大于ab
        所以返回大于0的值
        */
        if (result < 0) 
        {
            continue;
        }

        /*
        @param:查找ab 找到aa*  ab的ascii码小于aa 返回值小于0
        @param:查找aa 找到aab  aab包含aa但是aab不是要找的，
                所以判断找到的单词数组下标为len的地方是否为空格
                如是 则找到了单词   查aa 找到aa
                如不是 则没有找到   查aa 找到aab len为3 下标为3的地方不是空格是b
        */
        if (result > 0 || ((result == 0) && (temp[len] != ' ')))
        {
            break;
        }

        
        //表示找到了 查询的单词
        p = temp + len ; //数组加偏移量 定位到单词注释的首地址
        //printf("*found word:%s\n", p);

        //表示找到了需要的单词
        while (*p == ' ')
        {
            p++;
            //printf("*%c", *p);
        }

       // printf("\n");
        //找到了注释，跳过所有的空格
        //printf("p=%s\n", p);
        strcpy(msg->data, p);
        printf("**found word:%s\n", msg->data);

        //注释拷贝完毕后，关闭文件
        fclose(fp);

        return 1;

    }

    fclose(fp);
    return 0;

}

/// @brief 获取系统时间
/// @param date 存放时间信息
/// @return 0成功
int get_date(char *date)
{
    time_t t;
    struct tm *tp;

    //获取时间秒数
    time(&t);

    //时间格式转换
    tp = localtime(&t);

    //年-月-日 时:分:秒
    sprintf(date, "%d-%d-%d %d:%d:%d", tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday,
                                        tp->tm_hour, tp->tm_min, tp->tm_sec);

    return 0;
}

/// @brief 查找单词 并发送客户端
/// @param acceptfd 套接字
/// @param msg 通信结构体
/// @param db 数据库
/// @return 1成功 -1打开数据库失败
int do_query(int acceptfd, MSG *msg, sqlite3 *db)
{
    char word[64];
    int found = 0;  //1找到单词或者0没有找到单词
    char date[128] = {}; //保存时间
    char sql[1024] = {}; //历史命令拼接 时间+单词
    char *errmsg;

    //取出msg结构体中 要查询的单词
    strcpy(word, msg->data);

    //查询单词
    found = do_searchword(acceptfd, msg, word);

    //表示找到了单词 此时应该讲用户名 时间 单词 插入历史记录
    if (found == 1)
    {
        //获取系统时间
        get_date(date);
        printf("date:%s\n", date);

        //拼接
        sprintf(sql, "insert into record values('%s', '%s', '%s')", msg->name, date, word);
        printf("%s\n", sql);

        if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
        {
            printf("%s\n", errmsg);
            return -1;
        }
        else
        {
            printf("errmsg--%s\n", errmsg);
        }
    }
    else //没有找到
    {
        strcpy(msg->data, "Not found");
    }
    
    //将查询的结果 发送给客户端
    send(acceptfd, msg, sizeof(MSG), 0);
    printf("send:%s\n", msg->data);

    return 1;
}

/// @brief 历史命令回调函数 
/// @param arg 
/// @param f_num 
/// @param f_value 
/// @param f_name 
/// @return 
int history_callback(void *arg, int f_num, char **f_value, char **f_name)
{
    //read name date word
    int acceptfd;
    MSG msg;

    acceptfd = *((int *)arg);

    sprintf(msg.data, "%s , %s", f_value[1], f_value[2]);

    send(acceptfd, &msg, sizeof(MSG), 0);

    return 0;

}

/// @brief 查找历史命令
/// @param acceptfd 套接字
/// @param msg 通信结构体
/// @param db 数据库
/// @return 0成功 -1失败
int do_history(int acceptfd, MSG *msg, sqlite3 *db)
{
    char sql[128] = {};
    char *errmsg;

    ///sql命令拼接
    sprintf(sql, "select * from record where name = '%s'", msg->name);

    //查询数据库
    if (sqlite3_exec(db, sql, history_callback, (void *)&acceptfd, &errmsg) != SQLITE_OK)
    {
        printf("history sqlite3_exec error:%s\n", errmsg);
        return -1;
    }
    else
    {
        printf("history OK\n");
    }

    //所有记录查询发送完毕后 给客户端发送结束标志 '\0'
    msg->data[0] = '\0';
    send(acceptfd, msg, sizeof(MSG), 0);

    return 0;
}
