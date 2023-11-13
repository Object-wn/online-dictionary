/********************
 * @File Name: main.c
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

#define R 1 //用户注册
#define L 2 //用户登录
#define Q 3 //用户查询
#define H 4 //用户历史记录


#define N 32

//通信双方的信息结构体
typedef struct 
{
    /* data */
    int type;
    char name[N];
    char data[256];
}MSG;

/// @brief 用户注册
/// @param sockfd 套接字
/// @param msg 通信结构体
/// @return -1失败 0成功
int do_regist(int sockfd, MSG *msg)
{
     msg->type = R;

    printf("Input Name:"); //输入用户名
    scanf("%s", msg->name);
    getchar();

    printf("Input passwd:"); //输入密码
    scanf("%s", msg->data);
    getchar();

    //打印调试信息
    printf("name:%s\npasswd:%s\n", msg->name, msg->data);

    //发送数据
    if (send(sockfd, msg, sizeof(MSG), 0) < 0)
    {
        printf("fail to send \n"); //注册失败
        return -1;
    }

    //接收数据
    if (recv(sockfd, msg, sizeof(MSG), 0) < 0)
    {
        printf("fail to recv\n"); //接收数据失败
        return -1;
    }
    
    /*
    打印返回值
    ok or usr alread exist
    */
    printf("YES\n%s\n", msg->data);

    return 0;
}

/// @brief 用户登录
/// @param sockfd 套接字
/// @param msg 通信结构体
/// @return -1登录和接收数据失败 1传输成功 0无
int do_login(int sockfd, MSG *msg)
{
    msg->type = L;

    printf("Input Name:"); //输入用户名
    scanf("%s", msg->name);
    getchar();

    printf("Input passwd:"); //输入密码
    scanf("%s", msg->data);
    getchar();

     //发送数据
    if (send(sockfd, msg, sizeof(MSG), 0) < 0)
    {
        printf("fail to send \n"); //登录失败
        return -1;
    }

    //接收数据
    if (recv(sockfd, msg, sizeof(MSG), 0) < 0)
    {
        printf("fail to recv\n"); //接收数据失败
        return -1;
    }

    //判断接收的信息是否是OK， 是OK就是传输成功
    if (strncmp(msg->data, "OK", 3) == 0)
    {
        printf("login OK\n");
        return 1;
    }
    else
    {
        printf("data error\n");
        printf("%s\n", msg->data);
    }

    return 0;
}

/// @brief 查找单词
/// @param sockfd 套接字
/// @param msg 通信结构体
/// @return -1失败 0成功 #号退出
int do_query(int sockfd, MSG *msg)
{
    msg->type = Q;  //msg中已经存储了名字，只需要给一个类型就可以
    puts("---------");

    while (1)
    {
        printf("Input word:");
        scanf("%s", msg->data);
        getchar(); //清一下缓冲

        //输入#号退出
        if (strncmp(msg->data, "#", 1) == 0)
            break; //跳回二级菜单

        //讲要查询的单词发送给服务器
        if (send(sockfd, msg, sizeof(MSG), 0) == 0)
        {
            printf("do_query send error\n");
            return -1;
        }

        //等待接收服务器返回的单词注释信息
        if (recv(sockfd, msg, sizeof(MSG), 0) < 0)
        {
            printf("do_query recv error\n");
            return -1;
        }

        //打印传回来的信息
        printf("return Message:%s\n", msg->data);

    }
    return 0;
}

/// @brief 查找历史命令
/// @param sockfd 套接字
/// @param msg 通信结构体
/// @return 成功为0
int do_history(int sockfd, MSG *msg)
{
    msg->type = H;  //msg中已经存储了名字，只需要给一个类型就可以
    puts("---------");

    send(sockfd, msg, sizeof(MSG), 0);

    //接收信息
    while(1)
    {
        recv(sockfd, msg, sizeof(MSG), 0);

        if (msg->data[0] == '\0')
        {
            break;
        }

        //输出历史记录信息
        printf("%s\n", msg->data);
    }

    return 0;
}


int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in serveraddr;
    int n; //接收输入，选择模式
    MSG msg;
    if (argc != 3)
    {
        // ./main.o 192.168.0.1 10000
        printf("usage: %s  IP Error\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //套接字
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socker error\n");
        return -1;
    }

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
    serveraddr.sin_port = htons(atoi(argv[2]));
    
    ///连接服务器
    if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    {
        perror("connect error");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        printf("***************************************************************\n");
        printf("*                1.register    2.login     3.quit             *\n");
        printf("***************************************************************\n");
        printf("Please choose:");
        scanf("%d", &n);
        getchar();//去除垃圾字符

        switch (n) //功能选择
        {
            case 1:
                do_regist(sockfd, &msg);
                break;
            case 2:
                //do_login(sockfd, &msg);
                if (do_login(sockfd, &msg) == 1) //二级菜单
                {
                    goto next;
                }
                break;
            case 3:
                close(sockfd);
                exit(EXIT_FAILURE);
                break;
            default:
                printf("Invalid data cmd\n");
        }
    }

    next: //二级菜单
        while(1)
        {
            printf("*******************************************************\n");
            printf("*       1.query_word  2.history_record   3.quit       *\n");
            printf("*******************************************************\n");
            printf("Please choose:");
            scanf("%d", &n);
            getchar();

            switch (n) 
            {
                case 1:
                    do_query(sockfd, &msg);
                    break;
                case 2:
                    do_history(sockfd, &msg);
                    break;
                case 3:
                    close(sockfd);
                    exit(EXIT_SUCCESS);
                    break;
                default:
                    printf("Invalid data cmd\n");
            }
        }

    return 0;
}


