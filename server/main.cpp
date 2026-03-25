/*
* 即时通讯系统的服务器端设计
我先说一些我的简单设计理念:
1.初始化类，用来初始化整个系统，包括网络，数据库，线程池等等。
2.用户类，这个类是存储用户信息的，包括用户名，密码，等等。
3.子线程类，统筹管理子线程，包括创建，销毁，启动，停止等。
有n个接收线程，n是块的数量，一个块有一个线程处理，然后在有n个发送子线程，n是块的数量，
n/2个消息转发进程，上取整，用来把接受到的数据，先分流，分成msg和其他，
n/4个系统消息处理的子线程，上取整，
用来专门处理系统消息的，包括登录，注册和添加好友
//消息格式如下种类
//1、  [msg]     from|原用户ID|to|目标用户ID:mesg_id |内容																		    常规数据包，一般是用户与用户之间的数据包,通用格式
//2、  [register]from|原用户ID|to|目标用户ID:用户名  |密码		|套接字																注册消息，分为发送时和接收时两种，这是发送时，到对方后，要加套接字
//3、  [register]from|原用户ID|to|目标用户ID:用户名  |用户ID	|register_success													注册消息，分为发送时和接收时两种，这是接收时,注册成功
//4、  [register]from|原用户ID|to|目标用户ID:用户名  |用户ID	|register_fail														注册消息，分为发送时和接收时两种，这是接收时,注册失败
//5、  [login]   from|原用户ID|to|目标用户ID:用户名  |密码		|type				|套接字											登录消息，分为发送时和接收时两种，这是发送时，到对方后，要加套接字,type表示是否需要从服务器获取用户好友；列表
//6、  [login]   from|原用户ID|to|目标用户ID:用户名  |用户ID	|login_success														登录消息，分为发送时和接收时两种，这是接收时,登录成功,如果本次是初次登录，可能有联系人，因此在之后，需要把联系人的相关信息加入
//7、  [login]   from|原用户ID|to|目标用户ID:用户名  |用户ID	|login_fail															登录消息，分为发送时和接收时两种，这是接收时,登录失败
//8、  [add_user]from|原用户ID|to|目标用户ID:原用户名|原用户ID	|目标用户名			|目标用户ID	|add_user			|备注			添加用户消息，分为发送时和接收时两种，这是发送时
//9、  [add_user]from|原用户ID|to|目标用户ID:原用户名|原用户ID	|目标用户名			|目标用户ID	|add_user_success	|备注			添加用户消息，分为发送时和接收时两种，这是接收时,添加成功
//10、 [add_user]from|原用户ID|to|目标用户ID:原用户名|原用户ID	|目标用户名			|目标用户ID	|add_user_fail		|备注			添加用户消息，分为发送时和接收时两种，这是接收时,添加失败
//11、 [add_user]from|原用户ID|to|目标用户ID:原用户名|原用户ID	|目标用户名			|目标用户ID	|add_user_not		|备注			添加用户消息，分为发送时和接收时两种，这是接收时,添加错误
//12、 [ack]from|原用户ID|to|目标用户ID:mesg_id                                                                         			确认包，目前只有服务器才有接收的包，未来可以做到服务器发送客户端接收的
//13、 [heartbeat]from|原用户ID|to|目标用户ID:are you ok?                                                                         	心跳包，这是服务器发送的
//14、 [heartbeat]from|原用户ID|to|目标用户ID:i am ok.                                                                         		心跳包，这是客户端发送的
//15、 [have_user]from|原用户ID|to|目标用户ID:i want user list.                                                                		心跳包，这是客户端发送的
//16、 [have_user]from|原用户ID|to|目标用户ID:count|user_id|user_name|...	                                                        拉取联系人数据，这是服务器端发送的
//17、 [login]	  from|原用户ID|to|目标用户ID:login_out.					                                                        手动退出登录
*/
#define _CRT_SECURE_NO_WARNINGS 1
#define USER_LIST_BLOCK_COUNT_SIZE 12//块最大长度
#define USER_LIST_BLOCK_MAX_SIZE 512//块的大小
#define FD_SETSIZE ((USER_LIST_BLOCK_COUNT_SIZE*USER_LIST_BLOCK_MAX_SIZE)*2)//最大连接数
#define USER_TOTAL ((USER_LIST_BLOCK_COUNT_SIZE*USER_LIST_BLOCK_MAX_SIZE)+10)
#include <iostream>
#include <string>
#include <winsock2.h>
#include<windows.h>//windows系统库
#pragma comment(lib, "ws2_32.lib")
#include<queue>
#include <condition_variable>//条件变量
#include<mutex>
#include<atomic>
#include<map>
#include<ctime>
#include<vector>
#include<list>
#include<fstream>
#include <memory>
#include<cstring>
#include<thread>
#include<algorithm>
#include<array>
#include<chrono>
#include<unordered_map>
//宏控制开关，一旦被注释，就开启了该功能
#define STRESS_TESTING//压力测试开关

//宏定义
#define USER_LIST_FILE "user_list.tn"
#define USER_NAME_MAX_LEN 20
#define USER_NAME_MIN_LEN 8
#define USER_PASSWORD_MAX_LEN 20
#define USER_PASSWORD_MIN_LEN 8
#define OPERATION_LOG_FILE L"operator_log.txt"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888
#define BEIJING_MUSIC L"BEIJING_MUSIC.mp3"
#define AGAIN_COUNT 5//重复操作的次数
#define FROME "from|"//从哪里来的
#define TO "|to|"//到哪里去
#define INFO_MSG ":"//分割点
#define SOMEBODY_USER_INDEX 0//所有人的用户索引，表示任意人，用于没登陆时的
#define SERVER_USER_INDEX 1//服务器的用户索引
#define INITIAL_INDEX 2//唯一编号的起始，0表示任意，1表示服务器
#define MESEGE_MAX_LEN 1024*1024*10//消息的最大长度
#define MSG 1//发送的是消息
#define REGISTER 2//注册消息
#define LOGIN 3//登录消息
#define ADD_USER 4//添加用户
#define MSG_ACK 5//确认包类型
#define MSG_HEARTBEAT 6//心跳包类型
#define MSG_HAVE_USER 7//主动向服务器拉取联系人数据
#define ADD_USER_SUCCESS "add_user_success"//添加用户成功
#define ADD_USER_FAIL "add_user_fail"//添加用户失败
#define LOGIN_SUCCESS "login_success"//登录成功
#define LOGIN_FAIL "login_fail"//登录失败
#define LOGIN_OUT "login_out"//退出登录
#define REGISTER_SUCCESS "register_success"//注册成功
#define REGISTER_FAIL "register_fail"//注册失败
#define SEPARATEOR "|"//分隔符
#define USER_LIST_FILE_NAME "user_list.tn"//用户列表文件名
#define INITIAL_SOCKET_TIME_OUT (60*10) //初次连接时，给出了10分钟的超时时间，也就是说，当一个人初次连接时，10分钟内没有发来注册或者登录信息，自动丢失
#define FIRST_LEN 10//消息的长度，占用固定的字节数
#define USER_LIST_MAX 2500//最多2500个好友
#define ADD_USER_ADD "add"
#define ADD_USER_OK "ok"
#define ADD_USER_DEL "del"
#define ADD_USER_NOT "not"
#define ADD_USER_CANCEL "cancel"
#define ADD_USER_HAVED "haved"
#define ADD_USER_DEL_NOT "del_not"
#define SERVER_HEARTBEAT "are you ok?"
#define CLIENT_HEARTBEAT "i am ok."
#define MSG_HAVE_USER_MSG "i want user list."//客户端主动拉取的数据内容
//1.初始化类，用来初始化整个系统，包括网络，数据库，线程池等等。
class default_internet_server;
//2.用户类，这个类是存储用户信息的，包括用户名，密码，等等。
class user_member;
//3.子线程类，统筹管理子线程，包括创建，销毁，启动，停止等。
class san_thread;
//发送消息类，负责把消息发送出去
class send_message;
//8.接收的消息类，解析接收的消息
class recv_message_set;

//取消内存对齐
#pragma pack(push, 1)
struct RoutePocketHeader
{
    uint16_t type;
    uint64_t from_id;
    uint64_t to_id;
};
#pragma pack(pop)
class send_message
{
public:
    //把发送的消息转化为[info]from|A|to|B：message的格式
    static std::string send_message_to_from_A_to_B(const unsigned short& info, const size_t& A, const size_t& B, const std::string& message)
    {
        RoutePocketHeader rph;
        rph.type = htons(info);
        rph.from_id = htonll(A);
        rph.to_id = htonll(B);
        std::string msg((char*)&rph, sizeof(rph));
        msg.append(message);
        return msg;
        return info + FROME + std::to_string(A) + TO + std::to_string(B) + INFO_MSG + message;
    }
    static std::string add_user_mesege(std::string& yuan_user_name, size_t& yuan_user_id, std::string& mu_user_name, size_t& mu_user_id, std::string add_user_info, std::string beizhu)
    {
        return yuan_user_name + SEPARATEOR + std::to_string(yuan_user_id) + SEPARATEOR + mu_user_name + SEPARATEOR + std::to_string(mu_user_id) + SEPARATEOR + add_user_info + SEPARATEOR + beizhu;
    }
    static bool send_full(SOCKET socket, const std::string& message)
    {
        if (message.size() > MESEGE_MAX_LEN - 1)
        {
            std::cout << "消息过长，请重新输入" << std::endl;
            return false;
        }
        int message_len = message.length();
        std::string num_to_str = std::to_string(message_len);
        while (num_to_str.length() < FIRST_LEN)
            num_to_str = "0" + num_to_str;
        std::string send_message = num_to_str + message;
        int total_send_len = message_len + FIRST_LEN;
        int curr_send_len = 0;
        int tmp_len = 0;
        //std::cout << "发" << message << std::endl;
        while ((tmp_len = send(socket, send_message.c_str() + curr_send_len, total_send_len - curr_send_len, 0)) > 0)
        {
            curr_send_len += tmp_len;
            if (curr_send_len == total_send_len)
                return true;
            if (tmp_len == SOCKET_ERROR)
                return false;
        }
        return curr_send_len == total_send_len;
    }
};
class user
{
public:
    std::string name;
    std::string password;
    std::mutex user_mutex;
    size_t user_index;
    SOCKET client_socket;
    std::atomic<time_t>last_login;
    std::mutex recv_message_queue_mutex;//接收消息队列锁
    std::queue<std::string> recv_message_queue;
    std::atomic<bool> is_login;
    std::vector<std::shared_ptr<user>> friend_list;//好友列表
    std::mutex friend_list_mutex;//好友列表锁
    user() : name(""), password(""), user_index(0),
        client_socket(INVALID_SOCKET), is_login(false) {
    }
    user(const std::string& user_name, const std::string& password,
        const size_t user_id, const SOCKET client_socket)
        : name(user_name), password(password), user_index(user_id),
        client_socket(client_socket), is_login(false) {
    }
    user(const user& u)
        : name(u.name), password(u.password), user_index(u.user_index),
        client_socket(u.client_socket), is_login(u.is_login.load()) {
    }
    user& operator=(const user& u)
    {
        if (this == &u)
            return *this;
        this->name = u.name;
        this->password = u.password;
        this->user_index = u.user_index;
        this->client_socket = u.client_socket;
        this->is_login.store(u.is_login.load());
        return *this;
    }
    user(user&& u) noexcept
        : name(std::move(u.name)),
        password(std::move(u.password)),
        user_index(u.user_index),
        client_socket(u.client_socket),
        is_login(u.is_login.load())
    {
        u.client_socket = INVALID_SOCKET;
    }
    user& operator=(user&& u) noexcept
    {
        if (this == &u)
            return *this;
        if (client_socket != INVALID_SOCKET)
        {
            closesocket(client_socket);
            client_socket = INVALID_SOCKET;
        }
        name = std::move(u.name);
        password = std::move(u.password);
        user_index = u.user_index;
        client_socket = u.client_socket;
        is_login.store(u.is_login.load());
        u.client_socket = INVALID_SOCKET;
        return *this;
    }
};
std::istream& operator>>(std::istream& is, user& u)
{
    is >> u.name >> u.password >> u.user_index;
    return is;
}
std::ostream& operator<<(std::ostream& os, user& u)
{
    os << u.name << " " << u.password << " " << u.user_index;
    return os;
}
class recv_message_set
{
public:
    RoutePocketHeader rph;
    size_t yuan_user_id;
    size_t mu_user_id;
    size_t mesg_id;
    unsigned short info;
    //std::string info;//协议
    std::string yuan_user_name;
    std::string mu_user_name;
    std::string yuan_user_password;
    std::string end_message_string;
    std::vector<user>user_list;
    std::string add_user_info;
    int is_new_user = 0;
    recv_message_set(std::string& message, int floot = 0)
    {
        recv_message_to_from_A_to_B(message);
        if (info == MSG && floot)
            recv_msg(end_message_string);
        else if (info == LOGIN && floot)
            recv_message_login(end_message_string);
        else if (info == REGISTER && floot)
            recv_message_register(end_message_string);
        else if (info == ADD_USER && floot)
            recv_mesg_to_add_user(end_message_string);
        else if (info == MSG_ACK && floot)
            recv_message_ack(end_message_string);
        else if (info == MSG_HAVE_USER && floot)
            recv_message_have_user(end_message_string);
    }
    void recv_message_ack(std::string message)
    {
        mesg_id = (size_t)std::stoll(message);
    }
    void recv_message_have_user(std::string message)
    {
        size_t len = strlen(SEPARATEOR);
        std::vector<size_t>fenge_index;
        size_t index = -1;
        while ((index = message.find(SEPARATEOR, index + 1)) != std::string::npos)
            fenge_index.push_back(index);
        if (fenge_index.size() == 0)
            end_message_string = message;
        else
        {
            size_t count = std::stoll(&message[0]);
            if (count == 0)
                return;
            user_list.resize(count);
            for (size_t i = 0; i < count; i++)
            {
                user_list[i].user_index = (size_t)std::stoll(&message[fenge_index[i * 2] + len]);
                user_list[i].name = std::string(message.begin() + fenge_index[i * 2 + 1] + len, message.begin() + fenge_index[i * 2 + 2]);
            }
        }
    }
    void recv_message_to_from_A_to_B(std::string message)
    {
        memcpy(&rph, message.data(), sizeof(RoutePocketHeader));
        yuan_user_id = ntohll(rph.from_id);
        mu_user_id = ntohll(rph.to_id);
        info = ntohs(rph.type);
        end_message_string = std::string(message.begin() + sizeof(RoutePocketHeader), message.end());
    }
    void recv_msg(std::string mssage)
    {
        mesg_id = (size_t)std::stoll(mssage);
        end_message_string = mssage.substr(mssage.find(SEPARATEOR) + strlen(SEPARATEOR));
    }
    void recv_message_register(std::string message)
    {
        size_t len = strlen(SEPARATEOR);
        std::vector<size_t>fenge_index;
        size_t index = -1;
        while ((index = message.find(SEPARATEOR, index + 1)) != std::string::npos)
            fenge_index.push_back(index);
        if (fenge_index.size() == 1)//这个只有服务器端解析的分支
        {
            yuan_user_name = std::string(message.begin(), message.begin() + fenge_index[0]);
            yuan_user_password = std::string(message.begin() + fenge_index[0] + len, message.end());
        }
        else if (fenge_index.size() == 2)//这个可能会有服务器和客户端解析的分支
        {
            yuan_user_name = std::string(message.begin(), message.begin() + fenge_index[0]);
            yuan_user_password = std::string(message.begin() + fenge_index[0] + len, message.begin() + fenge_index[1]);
            yuan_user_id = (size_t)atoll(&message[fenge_index[0] + len]);
            end_message_string = std::string(message.begin() + fenge_index[1] + len, message.end());
        }
    }
    void recv_message_login(std::string message)
    {
        size_t len = strlen(SEPARATEOR);
        std::vector<size_t>fenge_index;
        size_t index = -1;
        while ((index = message.find(SEPARATEOR, index + 1)) != std::string::npos)
            fenge_index.push_back(index);
        if (fenge_index.size() == 0)
        {
            end_message_string = message;
            return;
        }
        yuan_user_name = std::string(message.begin(), message.begin() + fenge_index[0]);
        yuan_user_password = std::string(message.begin() + fenge_index[0] + len, message.begin() + fenge_index[1]);
        yuan_user_id = (size_t)std::stoll(&message[fenge_index[0] + len]);
        is_new_user = atoi(&message[fenge_index[1] + len]);
        end_message_string = std::string(message.begin() + fenge_index[1] + len, message.end());
        if (fenge_index.size() == 3)//服务器解析，主要处理socket
            end_message_string = std::string(message.begin() + fenge_index[2] + len, message.end());
    }
    void recv_mesg_to_add_user(std::string message)
    {
        size_t len = strlen(SEPARATEOR);
        std::vector<size_t>fenge_index;
        size_t index = -1;
        while ((index = message.find(SEPARATEOR, index + 1)) != std::string::npos)
            fenge_index.push_back(index);
        yuan_user_name = std::string(message.begin(), message.begin() + fenge_index[0]);
        yuan_user_id = (size_t)std::stoll(&message[fenge_index[0] + len]);
        mu_user_name = std::string(message.begin() + fenge_index[1] + len, message.begin() + fenge_index[2]);
        mu_user_id = (size_t)std::stoll(&message[fenge_index[2] + len]);
        add_user_info = std::string(message.begin() + fenge_index[3] + len, message.begin() + fenge_index[4]);
        end_message_string = std::string(message.begin() + fenge_index[4] + len, message.end());
    }
    static int recv_full(SOCKET socket, std::string& mesg)
    {
        char first_len_char[FIRST_LEN + 1];
        int first_len = 0;
        int curr_first_len = 0;
        while (first_len < FIRST_LEN)
        {
            int	ret = recv(socket, first_len_char + curr_first_len, FIRST_LEN - curr_first_len, 0);
            if (ret <= 0)
                return ret;
            first_len += ret;
            curr_first_len += ret;
        }
        first_len_char[FIRST_LEN] = '\0';
        long long len = atoll(first_len_char);//不能使用stoll,使用atoll，因为会出错
        if (len > MESEGE_MAX_LEN || len == 0)
        {
            std::cout << "消息错误，断开连接" << std::endl;
            return 0;
        }
        mesg.resize(len);
        int mesg_len = 0;
        while (mesg_len < len)
        {
            int ret = recv(socket, &mesg[mesg_len], len - mesg_len, 0);
            if (ret <= 0)
                return ret;
            mesg_len += ret;
        }
        //std::cout << "收"<<mesg << std::endl;
        return 1;
    }
};
class default_internet_server
{
private:
    SOCKET server_socket;
    bool win_init()
    {
        WSADATA w;
        bool flage = false;
        int count = AGAIN_COUNT;//尝试次数
        while (!flage && count--)
        {
            flage = (WSAStartup(MAKEWORD(2, 2), &w) == 0);
            if (flage == false)
            {
                std::cout << "初始化失败，正在第" << AGAIN_COUNT - count << "次尝试" << std::endl;
                Sleep(1000);
            }
        }
        if (count == 0)
            return false;
        return true;
    }
    void win_end()
    {
        WSACleanup();
    }
    bool creat_socket()
    {
        int count = AGAIN_COUNT;
        while ((((server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)) && count--)//创建套接字,TCP协议
        {
            Sleep(1000);
            std::cout << "创建套接字失败，正在第" << AGAIN_COUNT - count << "次尝试" << std::endl;
        }
        if (count == 0 && server_socket == INVALID_SOCKET)
            return false;
        return true;
    }
    bool bind_socket()
    {
        int count = AGAIN_COUNT;
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;                // IPv4协议
        serverAddr.sin_addr.s_addr = INADDR_ANY;        // 监听本机所有网卡
        serverAddr.sin_port = htons(SERVER_PORT);              // 端口转换为网络字节序
        while ((bind(server_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) && count--)
        {
            Sleep(1000);
            serverAddr.sin_family = AF_INET;                // IPv4协议
            serverAddr.sin_addr.s_addr = INADDR_ANY;        // 监听本机所有网卡
            serverAddr.sin_port = htons(SERVER_PORT);              // 端口转换为网络字节序
            std::cerr << "绑定端口失败，错误码：" << WSAGetLastError() << std::endl;
        }
        if (count == 0)
            return false;
        return true;
    }
    bool listen_socket()
    {
        int count = AGAIN_COUNT;
        while ((listen(server_socket, SOMAXCONN) == SOCKET_ERROR) && count--)
        {
            std::cerr << "监听失败，错误码：" << WSAGetLastError() << std::endl;
            Sleep(1000);
        }
        if (count == 0)
            return false;
        return true;
    }
public:
    SOCKET get_server_socket()
    {
        return server_socket;
    }
    SOCKET accept_socket()
    {
        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(server_socket, (sockaddr*)&clientAddr, &clientAddrLen);
        //std::cout << "接收到了一个套接字" << std::endl;
        return clientSocket;
    }
    bool server_enterd()
    {
        if (!creat_socket())
            return false;
        if (!bind_socket())
            return false;
        if (!listen_socket())
            return false;
        return true;
    }
    default_internet_server()
    {
        win_init();
        server_enterd();
    }
    ~default_internet_server()
    {
        if (server_socket != INVALID_SOCKET)
        {
            closesocket(server_socket);
            server_socket = INVALID_SOCKET;
        }
        win_end();
    }
};
class user_member
{
public:
    std::atomic<size_t>user_index = INITIAL_INDEX;
    std::list<std::pair<std::vector<std::shared_ptr<user>>, std::mutex>>user_list;
    std::mutex user_vector_mutex;
    std::vector<std::shared_ptr<user>>user_vector;
    std::mutex user_map_mutex;
    std::unordered_map<std::string, std::shared_ptr<user>>user_map;
    void read_user_friend(std::shared_ptr<user> us)
    {
        std::ifstream read(us->name + std::to_string(us->user_index) + ".tn", std::ios::binary);
        if (!read.is_open())
            return;
        size_t friend_count = 0;
        read >> friend_count;
        std::unique_lock<std::mutex> lock(us->friend_list_mutex);
        us->friend_list.reserve(friend_count);
        for (size_t i = 0; i < friend_count; i++)
        {
            size_t tmp = 0;
            read >> tmp;
            std::unique_lock<std::mutex>lock_vector(user_vector_mutex);
            std::shared_ptr<user> friend_user = user_vector[tmp];
            lock_vector.unlock();
            us->friend_list.push_back(friend_user);
        }
        lock.unlock();
        read.close();
    }
    void write_user_friend(std::shared_ptr<user> us)
    {
        std::ofstream write(us->name + std::to_string(us->user_index) + ".tn", std::ios::binary);
        if (!write.is_open())
            return;
        std::unique_lock<std::mutex> lock(us->friend_list_mutex);
        write << us->friend_list.size() << std::endl;
        for (auto i : us->friend_list)
            write << i->user_index << std::endl;
        write.close();
    }
    void read_user_list_file()
    {
        std::ifstream read(USER_LIST_FILE, std::ios::binary);
        if (!read.is_open())
            return;
        size_t user_count = 0;
        read >> user_count;
        user_index = user_count;
        auto j = user_list.begin();
        for (size_t i = INITIAL_INDEX; i < user_count; i++)
        {
            user tmp;
            read >> tmp;
            if (j->first.size() < USER_LIST_BLOCK_MAX_SIZE)
                j->first.push_back(std::make_shared<user>(tmp));
            else
            {
                j++;
                j->first.push_back(std::make_shared<user>(tmp));
            }
        }
        read.close();
    }
    void write_user_list_file()
    {
        if (user_list.empty())
            return;
        std::ofstream write(USER_LIST_FILE, std::ios::binary);
        if (!write.is_open())
        {
            std::cout << "打开用户文件失败，此操作不进行完全，可能会导致数据丢失" << std::endl;
            return;
        }
        write << user_index << std::endl;
        for (auto& user_vec : user_list)
        {
            std::unique_lock<std::mutex> lock(user_vec.second);
            if (user_vec.first.empty())
                continue;
            for (auto i = user_vec.first.begin(); i != user_vec.first.end(); i++)
                write << **i << std::endl;
        }
        write.close();
    }
    user_member()
    {
        user_list.resize(USER_LIST_BLOCK_COUNT_SIZE);
        user_vector.resize(USER_TOTAL);
        for (auto& i : user_vector)
            i = NULL;
        for (auto& user_vec : user_list)
            user_vec.first.reserve(USER_LIST_BLOCK_MAX_SIZE + 5);//加5是为了防止出现被扩容，导致指针悬空
        read_user_list_file();
        std::unique_lock<std::mutex>lock_vector(user_vector_mutex);
        std::unique_lock<std::mutex>lock_map(user_map_mutex);
        for (auto& i : user_list)
        {
            std::unique_lock<std::mutex> lock(i.second);
            if (i.first.empty())
                continue;
            for (auto& j : i.first)
            {
                user_map[j->name] = j;
                user_vector[j->user_index] = j;
            }
        }
        lock_vector.unlock();
        lock_map.unlock();
        for (auto& i : user_list)
        {
            std::unique_lock<std::mutex> lock(i.second);
            if (i.first.empty())
                continue;
            for (auto& j : i.first)
                read_user_friend(j);
        }
    }
    ~user_member()
    {
        write_user_list_file();
    }
};
class san_thread
{
public:
    default_internet_server server_socket;
    user_member user_list;
    struct recv_message_thread_info
    {
        std::pair<std::vector<std::shared_ptr<user>>, std::mutex>* user_list_block;
        bool* is_start_thread;
        bool* is_end_thread;
        std::mutex* recv_block_queue_mesg_mutex;
        std::queue<std::string>* recv_block_queue_mesg;
        std::condition_variable* recv_block_queue_cv;
        std::mutex* system_mesg_mutex;
        std::queue<std::string>* system_mesg_queue;
        std::condition_variable* system_mesg_cv;
        std::map<size_t, std::pair<time_t, std::string>>* recv_mesg_map_ack;//接收确认包误
        std::mutex* recv_mesg_map_ack_mutex;
        san_thread* find_user;
        int index;
    };
    struct send_message_thread_info
    {
        std::pair<std::vector<std::shared_ptr<user>>, std::mutex>* user_list_block;
        std::atomic<size_t>* mesg_index;
        bool* is_start_thread;
        bool* is_end_thread;
        std::map<size_t, std::pair<time_t, std::string>>* send_mesg_map_ack;
        std::mutex* send_mesg_map_ack_mutex;
        san_thread* find_user;
        int index;
    };
    struct system_message_thread_info
    {
        std::queue<std::string>* system_mesg_queue;
        std::mutex* system_mesg_mutex;
        std::condition_variable* system_mesg_cv;
        bool* is_start_thread;
        bool* is_end_thread;
        san_thread* find_user;
        int index;
    };
    struct recv_message_set_type_info
    {
        std::pair<std::vector<std::shared_ptr<user>>, std::mutex>* user_list_block;
        bool* is_start_thread;
        bool* is_end_thread;
        std::mutex* recv_block_queue_mesg_mutex;
        std::queue<std::string>* recv_block_queue_mesg;
        std::condition_variable* recv_block_queue_cv;
        std::queue<std::string>* system_mesg_queue;
        std::mutex* system_mesg_mutex;
        std::condition_variable* system_mesg_cv;
        san_thread* find_user;
        int index;
    };
    std::mutex mutex_initial_map_socket;//初次连接的锁
    std::map<SOCKET, time_t>initial_socket_map;//初次连接的映射表
    std::atomic<bool>is_accept_inital_socket_start_thread = true;//是否开始接收初始连接的线程
    std::atomic<bool>is_accept_inital_socket_end_thread = false;//是否停止接收初始连接的线程
    std::atomic<bool>is_recv_inital_map_socket_start_thread = true;//是否开始处理初次连接的线程
    std::atomic<bool>is_recv_inital_map_socket_end_thread = false;//是否停止处理初次连接的线程
    std::array<bool, USER_LIST_BLOCK_COUNT_SIZE> recv_blocks_thread_start;
    std::array<bool, USER_LIST_BLOCK_COUNT_SIZE> recv_blocks_thread_end;
    std::array<std::mutex, USER_LIST_BLOCK_COUNT_SIZE / 2>recv_block_queue_mesg_mutexs;
    std::vector < std::queue<std::string>>recv_block_queues_mesg;
    std::array<std::condition_variable, USER_LIST_BLOCK_COUNT_SIZE / 2>recv_block_queue_cv;
    std::vector< san_thread::recv_message_thread_info> recv_message_san_thread_info;
    std::array<bool, USER_LIST_BLOCK_COUNT_SIZE> send_blocks_thread_start;
    std::array<bool, USER_LIST_BLOCK_COUNT_SIZE> send_blocks_thread_end;
    std::vector< san_thread::send_message_thread_info> send_message_san_thread_info;
    std::atomic<size_t>send_mesg_index = 0;//发送消息的时候，会有一个唯一确认码。
    std::queue<std::string>system_mesg_queue;//系统需要处理的消息
    std::mutex mutex_system_mesg;
    std::condition_variable system_mesg_cv;
    std::vector<san_thread::system_message_thread_info> system_message_san_thread_info;
    std::array<bool, USER_LIST_BLOCK_COUNT_SIZE / 4> system_message_thread_start;
    std::array<bool, USER_LIST_BLOCK_COUNT_SIZE / 4> system_message_thread_end;
    std::vector<san_thread::recv_message_set_type_info> recv_message_set_type_san_thread_info;
    std::array<bool, USER_LIST_BLOCK_COUNT_SIZE / 2> recv_message_set_type_thread_start;
    std::array<bool, USER_LIST_BLOCK_COUNT_SIZE / 2> recv_message_set_type_thread_end;
    std::vector< std::map<size_t, std::pair<time_t, std::string>>>mesg_ack_map;//确认包机制
    std::array<std::mutex, USER_LIST_BLOCK_COUNT_SIZE>mesg_ack_map_mutex;
    std::atomic<bool>production_heartbeat_msg_start = true;
    std::atomic<bool>production_heartbeat_msg_end = false;
    std::atomic<bool>ack_mesg_map_to_send_mesg_start = true;
    std::atomic<bool>ack_mesg_map_to_send_mesg_end = false;
    std::atomic<bool>login_out_start = true;
    std::atomic<bool>login_out_end = false;
    //接收初次连接的子线程
    static void accept_inital_socket(san_thread* thread)
    {
        //std::cout << "接收初次连接的子线程开始了" << std::endl;
        SOCKET server_socket = thread->server_socket.get_server_socket();
        while (thread->is_accept_inital_socket_start_thread)
        {
            fd_set accept_fds;
            FD_ZERO(&accept_fds);
            FD_SET(server_socket, &accept_fds);
            timeval time_over_load;
            time_over_load.tv_sec = 1;//超时1秒
            time_over_load.tv_usec = 0;
            int ret = select(server_socket + (SOCKET)1, &accept_fds, NULL, NULL, &time_over_load);//select模式，暂时用这种形式了，未来扩展
            if (ret == 0)
                continue;
            else if (ret < 0)
            {
                std::cerr << "select出错，请稍后再试3" << std::endl;
                std::cerr << "错误码：" << WSAGetLastError() << std::endl;
                continue;
            }
            SOCKET client_socket = thread->server_socket.accept_socket();
            if (client_socket == INVALID_SOCKET)
            {
                std::cerr << "错误码：" << WSAGetLastError() << std::endl;
                continue;
            }
            std::unique_lock<std::mutex> lock(thread->mutex_initial_map_socket);
            thread->initial_socket_map[client_socket] = time(nullptr);//把客户端socket和时间戳放入映射表
            lock.unlock();
        }
        //std::cout << "接收初次连接的子线程结束了" << std::endl;
        thread->is_accept_inital_socket_end_thread = true;
    }
    //处理初次连接,登录和注册
    static void recv_inital_map_socket(san_thread* thread)
    {
        //std::cout<<"处理初次连接的子线程开始了"<<std::endl;
        while (thread->is_recv_inital_map_socket_start_thread)
        {
            std::vector<SOCKET> not_del_sockets;
            std::vector<SOCKET> del_sockets;//需要从映射表里删除的socket，因为时间长了，映射表多了就要爆炸了
            if (!thread->is_recv_inital_map_socket_start_thread)
                break;
            std::unique_lock<std::mutex> lock(thread->mutex_initial_map_socket);
            if (thread->initial_socket_map.empty())
            {
                lock.unlock();
                Sleep(10);//减少cpu的空转
                continue;
            }
            for (auto& client_socket : thread->initial_socket_map)
            {
                if (client_socket.second + INITIAL_SOCKET_TIME_OUT < time(nullptr))
                    del_sockets.push_back(client_socket.first);
                else
                    not_del_sockets.push_back(client_socket.first);
            }
            for (auto& del_socket : del_sockets)
                thread->initial_socket_map.erase(del_socket);
            lock.unlock();
            SOCKET max_socket = 0;
            fd_set read_fds;
            FD_ZERO(&read_fds);
            for (auto& client_socket : not_del_sockets)
            {
                max_socket = max(max_socket, client_socket);
                FD_SET(client_socket, &read_fds);
            }
            timeval time_over_load;//一秒的超时时间
            time_over_load.tv_sec = 1;
            time_over_load.tv_usec = 0;
            int ret = select(max_socket + (SOCKET)1, &read_fds, NULL, NULL, &time_over_load);//select模式，暂时用这种形式了，未来扩展
            if (ret == 0)//没有连接
            {
                Sleep(10);
                continue;
            }
            else if (ret < 0)//出错
            {
                int err = WSAGetLastError();
                if (err == 10038)//
                {

                    lock.lock();
                    for (auto& i : thread->initial_socket_map)
                    {
                        int optval;
                        int optlen = sizeof(optval);
                        if (!(getsockopt(i.first, SOL_SOCKET, SO_TYPE, (char*)&optval, &optlen) != SOCKET_ERROR))
                            continue;
                        del_sockets.push_back(i.first);
                    }
                    for (auto& i : del_sockets)
                        thread->initial_socket_map.erase(i);
                    lock.unlock();
                    Sleep(10);
                    continue;
                }
                std::cout << "select出错，请稍后再试4" << std::endl;
                std::cerr << "错误码：" << err << std::endl;
                continue;

            }
            for (auto& client_socket : not_del_sockets)
            {
                if (!FD_ISSET(client_socket, &read_fds))
                    continue;
                std::string mesg_str;
                int flage = recv_message_set::recv_full(client_socket, mesg_str);
                if (flage == 0)
                {
                    lock.lock();
                    thread->initial_socket_map.erase(client_socket);
                    lock.unlock();
                    int err = WSAGetLastError();
                    if (!thread->SOCKET_fail(client_socket, err))
                    {
                        std::cout << "接收消息失败" << std::endl;
                        std::cerr << "错误码：" << err << std::endl;
                    }
                    continue;
                }
                else if (flage < 0)
                {
                    int err = WSAGetLastError();
                    if (thread->SOCKET_fail(client_socket, err))
                        continue;
                    std::cout << "接收消息失败" << std::endl;
                    std::cerr << "错误码：" << err << std::endl;
                    continue;
                }
                recv_message_set r_m_s(mesg_str);
                if (r_m_s.info != LOGIN && r_m_s.info != REGISTER)//这里不处理非注册和登录的信息
                    continue;
                mesg_str += SEPARATEOR + std::to_string(client_socket);
                std::unique_lock<std::mutex> lock_system(thread->mutex_system_mesg);
                thread->system_mesg_queue.push(mesg_str);
                lock_system.unlock();
                thread->system_mesg_cv.notify_all();
            }
        }
        thread->is_recv_inital_map_socket_end_thread = true;
        //std::cout << "处理初次连接的子线程结束了" << std::endl;
    }
    //用户块接收的子线程
    static void recv_block_thread(recv_message_thread_info* info)
    {
        //std::cout << "第" << info->index+1 << "个块接收的子线程开始了" << std::endl;
        while (*(info->is_start_thread))
        {
            std::vector<std::shared_ptr<user>>can_recv_user;
            fd_set read_fds;
            FD_ZERO(&read_fds);
            SOCKET max_sucket = 0;
            bool is_use_login = false;
            std::unique_lock<std::mutex> lock(info->user_list_block->second);
            for (auto& i : info->user_list_block->first)
                if (i->is_login)
                {
                    is_use_login = true;
                    can_recv_user.push_back(i);
                }
            lock.unlock();
            if (!is_use_login)//这个块没人登录
            {
                Sleep(100);
                continue;
            }
            for (auto& i : can_recv_user)
            {
                std::unique_lock<std::mutex>lock_user(i->user_mutex);
                FD_SET(i->client_socket, &read_fds);
                max_sucket = max(max_sucket, i->client_socket);
            }
            timeval time_over_load;
            time_over_load.tv_sec = 1;
            time_over_load.tv_usec = 0;
            int ret = select(max_sucket + 1, &read_fds, NULL, NULL, &time_over_load);
            if (ret == 0)
                continue;
            else if (ret < 0)
            {
                int err = WSAGetLastError();
                if (info->find_user->SELECT_fail(can_recv_user, err))
                    continue;
                std::cout << "select出错，请稍后再试1" << std::endl;
                std::cerr << "错误码：" << err << std::endl;
                continue;
            }
            for (auto& i : can_recv_user)
            {
                std::unique_lock<std::mutex>lock_user(i->user_mutex);
                if (!FD_ISSET(i->client_socket, &read_fds))
                    continue;
                SOCKET sock = i->client_socket;
                lock_user.unlock();
                std::string mesg_str;
                int flage = recv_message_set::recv_full(sock, mesg_str);
                if (flage == 0)
                {
                    for (auto& j : can_recv_user)
                    {
                        std::unique_lock<std::mutex>lock_user(i->user_mutex);
                        if (j->is_login && j->client_socket == i->client_socket)
                        {
                            j->is_login = false;
                            if (j->client_socket != INVALID_SOCKET)
                            {
                                closesocket(j->client_socket);
                                j->client_socket = INVALID_SOCKET;
                            }
                            break;
                        }
                    }
                    continue;
                }
                else if (flage < 0)
                {
                    int err = WSAGetLastError();
                    if (info->find_user->SOCKET_fail(i->client_socket, err))
                        continue;
                    std::cout << "接收消息失败" << std::endl;
                    std::cerr << "错误码：" << err << std::endl;
                    continue;
                }
                recv_message_set r_m_s(mesg_str, 1);
                if (r_m_s.info == MSG)
                {
                    std::string ack_msg = std::to_string(r_m_s.mesg_id);
                    ack_msg = send_message::send_message_to_from_A_to_B(MSG_ACK, SERVER_USER_INDEX, i->user_index, ack_msg);
#ifndef STRESS_TESTING
                    send_message::send_full(i->client_socket, ack_msg);
#endif
                    std::unique_lock<std::mutex> lock(*(info->recv_block_queue_mesg_mutex));
                    info->recv_block_queue_mesg->push(mesg_str);
                    lock.unlock();
                    info->recv_block_queue_cv->notify_one();
                }
                else if (r_m_s.info == MSG_ACK)
                {
                    std::unique_lock<std::mutex>lock(*(info->recv_mesg_map_ack_mutex));
                    if (info->recv_mesg_map_ack->find(r_m_s.mesg_id) != info->recv_mesg_map_ack->end())
                        info->recv_mesg_map_ack->erase(r_m_s.mesg_id);
                    //处理确认包
                }
                else if (r_m_s.info == MSG_HEARTBEAT)
                {
                    i->last_login = time(nullptr);
                    //处理心跳包
                }
                else if (r_m_s.info == LOGIN && r_m_s.end_message_string == LOGIN_OUT)
                {
                    std::unique_lock<std::mutex>lock_user(i->user_mutex);
                    if (i->client_socket != INVALID_SOCKET)
                    {
                        closesocket(i->client_socket);
                        i->client_socket = INVALID_SOCKET;
                    }
                    i->last_login = 0;
                    i->is_login = false;
                }
                else
                {
                    std::unique_lock<std::mutex> lock(*(info->system_mesg_mutex));
                    info->system_mesg_queue->push(mesg_str);
                    lock.unlock();
                    info->system_mesg_cv->notify_all();
                }
            }
        }
        *(info->is_end_thread) = true;
        //std::cout << "第" << info->index+1 << "个块接收的子线程结束了" << std::endl;
    }
    //用户块发送的子线程
    static void send_block_thread(send_message_thread_info* info)
    {
        //std::cout << "第" << info->index + 1 << "个块发送的子线程开始了" << std::endl;
        while (*(info->is_start_thread))
        {
            std::vector<std::shared_ptr<user>>can_send_user;
            fd_set write_fds;
            timeval time_over_load;
            time_over_load.tv_sec = 1;
            time_over_load.tv_usec = 0;
            std::unique_lock<std::mutex> lock(info->user_list_block->second);
            for (auto& i : info->user_list_block->first)
            {
                if (!i->is_login || i->recv_message_queue.empty())
                    continue;
                can_send_user.push_back(i);
            }
            lock.unlock();
            if (can_send_user.empty())
            {
                Sleep(10);
                continue;
            }
            for (auto i : can_send_user)
            {
                std::unique_lock<std::mutex>tmp_lock(i->user_mutex);
                if (!i->is_login)
                    continue;
                tmp_lock.unlock();
                FD_ZERO(&write_fds);
                FD_SET(i->client_socket, &write_fds);
                if (!FD_ISSET(i->client_socket, &write_fds))
                    continue;
                int ret = select(i->client_socket + 1, NULL, &write_fds, NULL, &time_over_load);
                if (ret == 0)
                    continue;
                else if (ret < 0)
                {
                    int err = WSAGetLastError();
                    if (info->find_user->SELECT_fail(can_send_user, err))
                        continue;
                    std::cout << "select出错，请稍后再试3" << std::endl;
                    std::cerr << "错误码：" << err << std::endl;
                    continue;
                }
                std::unique_lock<std::mutex> lock_recv_message(i->recv_message_queue_mutex);
                std::string mesg = i->recv_message_queue.front();
                i->recv_message_queue.pop();
                lock_recv_message.unlock();
                size_t msg_id = (*(info->mesg_index))++;
                recv_message_set r_m_s(mesg, 1);
                if (r_m_s.info == MSG)
                {
                    mesg = std::to_string(msg_id) + SEPARATEOR + r_m_s.end_message_string;
                    mesg = send_message::send_message_to_from_A_to_B(MSG, r_m_s.yuan_user_id, r_m_s.mu_user_id, mesg);
#ifndef STRESS_TESTING
                    std::unique_lock<std::mutex> lock_tmp(*(info->send_mesg_map_ack_mutex));
                    (*(info->send_mesg_map_ack))[msg_id] = std::pair<time_t, std::string>(time(nullptr), mesg);
#endif
                }
                if (!send_message::send_full(i->client_socket, mesg))
                {
                    int err = WSAGetLastError();
                    lock_recv_message.lock();
                    i->recv_message_queue.push(mesg);
                    lock_recv_message.unlock();
                    if (r_m_s.info == MSG)
                    {
#ifndef STRESS_TESTING
                        std::unique_lock<std::mutex> lock_tmp(*(info->send_mesg_map_ack_mutex));
                        (*(info->send_mesg_map_ack)).erase(msg_id);
#endif
                    }
                    tmp_lock.lock();
                    i->is_login = false;
                    if (info->find_user->SOCKET_fail(i->client_socket, err))
                        continue;
                    tmp_lock.unlock();
                    std::cout << "发送消息失败" << std::endl;
                    std::cerr << "错误码：" << err << std::endl;
                    continue;
                }
                //需要把发送成功的数据，放入确认机制的队列中，等待确认
            }
        }
        *(info->is_end_thread) = true;
        //std::cout << "第" << info->index + 1 << "个块发送的子线程结束了" << std::endl;
    }
    //把块接收的消息做分流，分发到系统消息处理线程和具体的用户发送列表里
    static void recv_message_set_type_thread(recv_message_set_type_info* info)
    {
        //std::cout << "第" << info->index + 1 << "个消息处理线程开始" << std::endl;
        while (*(info->is_start_thread))
        {
            std::unique_lock<std::mutex>lock(*info->recv_block_queue_mesg_mutex);
            info->recv_block_queue_cv->wait(lock, [&]() {return !info->recv_block_queue_mesg->empty() || !*(info->is_start_thread); });
            if (!*(info->is_start_thread))
                break;
            std::string mesg_str = info->recv_block_queue_mesg->front();
            info->recv_block_queue_mesg->pop();
            lock.unlock();
            recv_message_set r_m_s(mesg_str);
            if (r_m_s.info == MSG)//后期处理一下，当如果自己没有好友的但是又收到消息，需要服务器返回提示信息
            {
#ifdef STRESS_TESTING
                std::unique_lock<std::mutex>lock_vector(info->find_user->user_list.user_vector_mutex);
                std::shared_ptr<user> mu_user = info->find_user->user_list.user_vector[r_m_s.mu_user_id];
                lock_vector.unlock();
                if (mu_user == nullptr)
                {
                    std::cout << "目标的下标错误，检查" << r_m_s.mu_user_id << std::endl;
                    continue;
                }
                std::unique_lock < std::mutex> lock_tmp(mu_user->recv_message_queue_mutex);
                mu_user->recv_message_queue.push(mesg_str);
#else
                std::unique_lock<std::mutex>lock_vector(info->find_user->user_list.user_vector_mutex);
                std::shared_ptr<user> yuan_user = info->find_user->user_list.user_vector[r_m_s.yuan_user_id];
                std::shared_ptr<user> mu_user = info->find_user->user_list.user_vector[r_m_s.mu_user_id];
                lock_vector.unlock();
                if (info->find_user->is_user_friend_list_haved(mu_user, yuan_user))
                {
                    std::unique_lock < std::mutex> lock_tmp(mu_user->recv_message_queue_mutex);
                    mu_user->recv_message_queue.push(mesg_str);
                }
                else
                {
                    std::string msg = std::to_string(info->find_user->send_mesg_index++) + SEPARATEOR + "你还不是对方的好友，不能发/收消息，请手动删除该好友后，才能再次申请添加好友。该消息：“" + r_m_s.end_message_string + "”无法被对方收到";
                    msg = send_message::send_message_to_from_A_to_B(MSG, r_m_s.mu_user_id, r_m_s.yuan_user_id, msg);
                    std::unique_lock<std::mutex> lock_tmp(yuan_user->recv_message_queue_mutex);
                    yuan_user->recv_message_queue.push(msg);
                }
#endif
            }
            else
            {
                std::unique_lock<std::mutex> lock_tmp(*(info->system_mesg_mutex));
                info->system_mesg_queue->push(mesg_str);
                lock_tmp.unlock();
                info->system_mesg_cv->notify_all();
            }
        }
        *(info->is_end_thread) = true;
        //std::cout << "第" << info->index + 1 << "个消息处理线程结束" << std::endl;
    }
    bool is_user_friend_list_haved(std::shared_ptr<user> yuan, std::shared_ptr<user> mu)//该用户是否已经存在这个用户
    {
        std::unique_lock<std::mutex>lock_us_friend_list(yuan->friend_list_mutex);
        for (auto& i : yuan->friend_list)
            if (i == mu)
                return true;
        return false;
    }
    bool is_user_friend_list_haved(std::shared_ptr<user>us, std::string& name, size_t index)//该用户是否已经存在这个用户
    {
        std::unique_lock<std::mutex>lock_us_friend_list(us->friend_list_mutex);
        for (auto& i : us->friend_list)
            if (i->name == name && i->user_index == index)
                return true;
        return false;
    }
    void user_have_user_list(size_t suer_index)
    {
        std::unique_lock<std::mutex>lock_vector(user_list.user_vector_mutex);
        std::shared_ptr<user> user_tmp = user_list.user_vector[suer_index];
        lock_vector.unlock();
        std::string msg;
        std::unique_lock<std::mutex>lock_user_friend_list(user_tmp->friend_list_mutex);
        msg += (user_tmp->friend_list.size() == 0) ? ("0" SEPARATEOR) : (std::to_string(user_tmp->friend_list.size()) + SEPARATEOR);
        for (auto& i : user_tmp->friend_list)
            msg += std::to_string(i->user_index) + SEPARATEOR + i->name + SEPARATEOR;
        lock_user_friend_list.unlock();
        msg = send_message::send_message_to_from_A_to_B(MSG_HAVE_USER, SERVER_USER_INDEX, suer_index, msg);
        std::unique_lock<std::mutex>lock_user_recv_msg(user_tmp->recv_message_queue_mutex);
        user_tmp->recv_message_queue.push(msg);
    }
    //系统消息处理线程
    static void system_mesg_thread(system_message_thread_info* info)
    {
        //std::cout << "第" << info->index + 1 << "个系统消息处理开始" << std::endl;
        while (*(info->is_start_thread))
        {
            std::unique_lock<std::mutex> lock(*info->system_mesg_mutex);
            info->system_mesg_cv->wait(lock, [&]() {return !info->system_mesg_queue->empty() || !*(info->is_start_thread); });
            if (!*(info->is_start_thread))
                break;
            std::string mesg_str = info->system_mesg_queue->front();
            info->system_mesg_queue->pop();
            lock.unlock();
            user* user_tmp = NULL;
            recv_message_set r_m_s(mesg_str, 1);
            if (r_m_s.info == LOGIN)
                info->find_user->login_mesg(r_m_s.yuan_user_name, r_m_s.yuan_user_password, std::stoll(r_m_s.end_message_string), r_m_s.is_new_user);
            else if (r_m_s.info == REGISTER)
                info->find_user->register_mesg(r_m_s.yuan_user_name, r_m_s.yuan_user_password, std::stoll(r_m_s.end_message_string));
            else if (r_m_s.info == MSG_HAVE_USER && r_m_s.end_message_string == MSG_HAVE_USER_MSG)
                info->find_user->user_have_user_list(r_m_s.yuan_user_id);
            else if (r_m_s.info == ADD_USER)
            {
                std::shared_ptr<user> find_mu_id, find_mu_name, find_yuan_id;
                if (!(r_m_s.mu_user_id > info->find_user->user_list.user_index))
                {
                    std::unique_lock<std::mutex>lock_vector(info->find_user->user_list.user_vector_mutex);
                    find_mu_id = info->find_user->user_list.user_vector[r_m_s.mu_user_id];
                    find_yuan_id = info->find_user->user_list.user_vector[r_m_s.yuan_user_id];
                    lock_vector.unlock();
                    std::unique_lock<std::mutex>lock_map(info->find_user->user_list.user_map_mutex);
                    find_mu_name = info->find_user->user_list.user_map[r_m_s.mu_user_name];
                    lock_map.unlock();
                }
                std::string mesg;
                if (find_mu_id != find_mu_name || find_mu_id == nullptr)//这俩查找的，必须是同一个人，否则就是没找到
                {
                    std::unique_lock<std::mutex>lock_vector(info->find_user->user_list.user_vector_mutex);
                    find_yuan_id = info->find_user->user_list.user_vector[r_m_s.yuan_user_id];
                    lock_vector.unlock();
                    mesg = r_m_s.yuan_user_name + SEPARATEOR + std::to_string(r_m_s.yuan_user_id) + SEPARATEOR + r_m_s.mu_user_name + SEPARATEOR + std::to_string(r_m_s.mu_user_id) + SEPARATEOR + ADD_USER_NOT + SEPARATEOR + "用户不存在";
                    mesg = send_message::send_message_to_from_A_to_B(ADD_USER, SERVER_USER_INDEX, r_m_s.yuan_user_id, mesg);
                    std::unique_lock<std::mutex>lock_yuan_user_recv_msg(find_yuan_id->recv_message_queue_mutex);
                    find_yuan_id->recv_message_queue.push(mesg);
                    continue;
                }
                if (r_m_s.add_user_info == ADD_USER_ADD)//主动添加
                {
                    if (info->find_user->is_user_friend_list_haved(find_yuan_id, r_m_s.mu_user_name, r_m_s.mu_user_id))//服务器端的原用户是否存在
                    {
                        mesg = send_message::add_user_mesege(r_m_s.yuan_user_name, r_m_s.yuan_user_id, r_m_s.mu_user_name, r_m_s.mu_user_id, ADD_USER_HAVED, "该用户已经添加，如果本地没有，请重新拉取");
                        mesg = send_message::send_message_to_from_A_to_B(ADD_USER, SERVER_USER_INDEX, r_m_s.yuan_user_id, mesg);
                        std::unique_lock<std::mutex>lock_yuan_recv_mesg(find_yuan_id->recv_message_queue_mutex);
                        find_yuan_id->recv_message_queue.push(mesg);
                        continue;
                    }
                    std::unique_lock<std::mutex>lock_us_friend_list(find_yuan_id->friend_list_mutex);
                    if (find_yuan_id->friend_list.size() >= USER_LIST_MAX)//在服务器端显示列表已经满了，直接拒绝
                    {
                        mesg = send_message::add_user_mesege(r_m_s.yuan_user_name, r_m_s.yuan_user_id, r_m_s.mu_user_name, r_m_s.mu_user_id, ADD_USER_NOT, "你在服务器端的好友列表已经满了，请删除一些好友后再试，如果本地没满，请重新拉取好友数据");
                        mesg = send_message::send_message_to_from_A_to_B(ADD_USER, SERVER_USER_INDEX, r_m_s.yuan_user_id, mesg);
                        std::unique_lock<std::mutex>lock_yuan_recv_mesg(find_yuan_id->recv_message_queue_mutex);
                        find_yuan_id->recv_message_queue.push(mesg);
                        continue;
                    }
                    lock_us_friend_list.unlock();
                    if (info->find_user->is_user_friend_list_haved(find_mu_id, r_m_s.yuan_user_name, r_m_s.yuan_user_id))//服务器端的目标还有该用户
                    {
                        mesg = send_message::add_user_mesege(r_m_s.mu_user_name, r_m_s.mu_user_id, r_m_s.yuan_user_name, r_m_s.yuan_user_id, ADD_USER_OK, "你删过他");
                        mesg = send_message::send_message_to_from_A_to_B(ADD_USER, SERVER_USER_INDEX, r_m_s.yuan_user_id, mesg);
                        std::unique_lock<std::mutex>lock_yuan_friend_list(find_yuan_id->friend_list_mutex);
                        find_yuan_id->friend_list.push_back(find_mu_id);
                        lock_yuan_friend_list.unlock();
                        info->find_user->user_list.write_user_friend(find_yuan_id);//重新写入列表
                        std::unique_lock<std::mutex>lock_yuan_recv_mesg(find_yuan_id->recv_message_queue_mutex);
                        find_yuan_id->recv_message_queue.push(mesg);
                        continue;
                    }
                    std::unique_lock<std::mutex>lock_mu_friend_list(find_mu_id->friend_list_mutex);
                    if (find_mu_id->friend_list.size() >= USER_LIST_MAX)//在服务器端显示列表已经满了，直接拒绝
                    {
                        mesg = send_message::add_user_mesege(r_m_s.mu_user_name, r_m_s.mu_user_id, r_m_s.yuan_user_name, r_m_s.yuan_user_id, ADD_USER_NOT, "对方在服务器端的好友列表已经满了");
                        mesg = send_message::send_message_to_from_A_to_B(ADD_USER, SERVER_USER_INDEX, r_m_s.yuan_user_id, mesg);
                        std::unique_lock<std::mutex>lock_mu_recv_mesg(find_mu_id->recv_message_queue_mutex);
                        find_mu_id->recv_message_queue.push(mesg);
                        continue;
                    }
                    lock_mu_friend_list.unlock();
                    std::unique_lock<std::mutex>lock_yuan_recv_mesg(find_mu_id->recv_message_queue_mutex);
                    find_mu_id->recv_message_queue.push(mesg_str);
                }
                else if (r_m_s.add_user_info == ADD_USER_OK)//主动同意
                {
                    if (!info->find_user->is_user_friend_list_haved(find_yuan_id, r_m_s.mu_user_name, r_m_s.mu_user_id))
                    {
                        std::unique_lock<std::mutex>lock_yuan_friend_list(find_yuan_id->friend_list_mutex);
                        find_yuan_id->friend_list.push_back(find_mu_id);
                        lock_yuan_friend_list.unlock();
                        info->find_user->user_list.write_user_friend(find_yuan_id);
                    }
                    if (!info->find_user->is_user_friend_list_haved(find_mu_id, r_m_s.yuan_user_name, r_m_s.yuan_user_id))
                    {
                        std::unique_lock<std::mutex>lock_mu_friend_list(find_mu_id->friend_list_mutex);
                        find_mu_id->friend_list.push_back(find_yuan_id);
                        lock_mu_friend_list.unlock();
                        info->find_user->user_list.write_user_friend(find_mu_id);
                    }
                    std::unique_lock<std::mutex>lock_yuan_recv_mesg(find_mu_id->recv_message_queue_mutex);
                    find_mu_id->recv_message_queue.push(mesg_str);
                }
                else if (r_m_s.add_user_info == ADD_USER_DEL)//主动删除
                {
                    if (!info->find_user->is_user_friend_list_haved(find_yuan_id, r_m_s.mu_user_name, r_m_s.mu_user_id))
                    {
                        mesg = r_m_s.yuan_user_name + SEPARATEOR + std::to_string(r_m_s.yuan_user_id) + SEPARATEOR + r_m_s.mu_user_name + SEPARATEOR + std::to_string(r_m_s.mu_user_id) + SEPARATEOR + ADD_USER_NOT + SEPARATEOR + "你还没有添加好友，如果本地存在，请重新拉取用户好友数据";
                        mesg = send_message::send_message_to_from_A_to_B(ADD_USER, SERVER_USER_INDEX, r_m_s.yuan_user_id, mesg);
                        std::unique_lock<std::mutex>lock_yuan_user_recv_msg(find_yuan_id->recv_message_queue_mutex);
                        find_yuan_id->recv_message_queue.push(mesg);
                        continue;
                    }
                    std::unique_lock<std::mutex>lcok_yuan_friend_list(find_yuan_id->friend_list_mutex);
                    for (size_t index = 0; index < find_yuan_id->friend_list.size(); index++)
                    {
                        if (find_mu_id->name == find_yuan_id->friend_list[index]->name && find_mu_id->user_index == find_yuan_id->friend_list[index]->user_index)
                        {
                            find_yuan_id->friend_list.erase(find_yuan_id->friend_list.begin() + index);
                            lcok_yuan_friend_list.unlock();
                            info->find_user->user_list.write_user_friend(find_yuan_id);
                            break;
                        }
                    }
                }
                else if (r_m_s.add_user_info == ADD_USER_CANCEL)//主动拒绝
                {
                    std::unique_lock<std::mutex>lock_yuan_user_recv_msg(find_mu_id->recv_message_queue_mutex);
                    find_mu_id->recv_message_queue.push(mesg_str);
                    continue;
                }
            }
        }
        *(info->is_end_thread) = true;
        //std::cout << "第" << info->index + 1 << "个系统消息处理结束" << std::endl;
    }
    //生成心跳包
    static void production_heartbeat_msg(san_thread* info)
    {
        //std::cout << "生成心跳包1" << std::endl;
        std::string mesg = SERVER_HEARTBEAT;
        while (info->production_heartbeat_msg_start)
        {
            for (auto& i : info->user_list.user_list)
            {
                if (!info->production_heartbeat_msg_start)
                    break;
                std::unique_lock<std::mutex>lock(i.second);
                if (i.first.empty())
                    continue;
                for (auto& j : i.first)
                {
                    if (!j->is_login)
                        continue;
                    mesg = send_message::send_message_to_from_A_to_B(MSG_HEARTBEAT, SERVER_USER_INDEX, j->user_index, mesg);
                    std::unique_lock<std::mutex>lock_tmp(j->recv_message_queue_mutex);
#ifndef STRESS_TESTING 
                    j->recv_message_queue.push(mesg);
#endif
                }
            }
            Sleep(10000);
        }
        info->production_heartbeat_msg_end = true;
        //std::cout << "生成心跳包2" << std::endl;
    }
    //登录逻辑
    bool login_mesg(std::string  user_name, std::string user_passerword, SOCKET client_socket, int type)
    {
        std::unique_lock<std::mutex>lock_map(user_list.user_map_mutex);
        auto it = user_list.user_map.find(user_name);
        auto it_end = user_list.user_map.end();
        if (it != it_end && it->second->name == user_name && it->second->password == user_passerword)
        {
            lock_map.unlock();
            std::unique_lock<std::mutex> lock_tmp(mutex_initial_map_socket);
            initial_socket_map.erase(client_socket);
            lock_tmp.unlock();
            std::unique_lock<std::mutex> lock_user(it->second->user_mutex);
            if (it->second->client_socket != INVALID_SOCKET)
            {
                closesocket(it->second->client_socket);
                it->second->client_socket = INVALID_SOCKET;
            }
            it->second->client_socket = client_socket;
            it->second->last_login = time(nullptr);
            it->second->is_login = true;
            std::string mesg = user_name + SEPARATEOR + std::to_string(it->second->user_index) + SEPARATEOR + LOGIN_SUCCESS;
            mesg = send_message::send_message_to_from_A_to_B(LOGIN, SERVER_USER_INDEX, it->second->user_index, mesg);
            if (!send_message::send_full(it->second->client_socket, mesg))
            {
                it->second->is_login = false;
                int err = WSAGetLastError();
                if (SOCKET_fail(it->second->client_socket, err))
                    return false;
                std::cout << "发送消息失败" << std::endl;
                std::cerr << "错误码：" << WSAGetLastError() << std::endl;
            }
            lock_user.unlock();
            if (type)
                user_have_user_list(it->second->user_index);
            return true;
        }
        else
            lock_map.unlock();
        std::string mesg = user_name + SEPARATEOR + "0" + SEPARATEOR + LOGIN_FAIL;
        mesg = send_message::send_message_to_from_A_to_B(LOGIN, SERVER_USER_INDEX, 0, mesg);
        if (!send_message::send_full(client_socket, mesg))
        {
            int err = WSAGetLastError();
            if (SOCKET_fail(it->second->client_socket, err))
            {
                std::unique_lock<std::mutex> lock_tmp(mutex_initial_map_socket);
                initial_socket_map.erase(client_socket);
                lock_tmp.unlock();
                return false;
            }
            std::cout << "发送消息失败" << std::endl;
            std::cerr << "错误码：" << err << std::endl;
        }
        return false;
    }
    //注册逻辑
    bool register_mesg(std::string  user_name, std::string user_passerword, SOCKET client_socket)
    {
        std::string mesg;
        mesg = user_name + SEPARATEOR + std::to_string(0) + SEPARATEOR + REGISTER_FAIL;
        mesg = send_message::send_message_to_from_A_to_B(REGISTER, SERVER_USER_INDEX, 0, mesg);
        std::unique_lock<std::mutex>lock_map(user_list.user_map_mutex);
        int register_success = 0;//初始
        auto it = user_list.user_map.find(user_name);
        auto it_end = user_list.user_map.end();
        if (it != it_end)
        {
            lock_map.unlock();
            register_success = 1;//注册失败，已存在用户
        }
        else
        {
            lock_map.unlock();
            for (auto& i : user_list.user_list)
            {
                std::unique_lock<std::mutex>lock(i.second);
                if (i.first.size() < USER_LIST_BLOCK_MAX_SIZE)
                {
                    size_t index = user_list.user_index++;
                    register_success = 2;//注册成功，用户信息存入用户列表
                    std::unique_lock<std::mutex> lock_tmp(mutex_initial_map_socket);
                    initial_socket_map.erase(client_socket);
                    lock_tmp.unlock();
                    i.first.push_back(std::make_shared<user>());
                    i.first[i.first.size() - 1]->name = user_name;
                    i.first[i.first.size() - 1]->password = user_passerword;
                    i.first[i.first.size() - 1]->last_login = time(nullptr);
                    i.first[i.first.size() - 1]->client_socket = client_socket;
                    i.first[i.first.size() - 1]->is_login = true;
                    i.first[i.first.size() - 1]->user_index = index;
                    lock_map.lock();
                    user_list.user_map[user_name] = i.first[i.first.size() - 1];
                    lock_map.unlock();
                    std::unique_lock<std::mutex>lock_vector(user_list.user_vector_mutex);
                    user_list.user_vector[index] = i.first[i.first.size() - 1];
                    lock_vector.unlock();
                    lock.unlock();
                    mesg = user_name + SEPARATEOR + std::to_string(index) + SEPARATEOR + REGISTER_SUCCESS;
                    mesg = send_message::send_message_to_from_A_to_B(REGISTER, SERVER_USER_INDEX, index, mesg);
                    if (!send_message::send_full(client_socket, mesg))
                    {
                        int err = WSAGetLastError();
                        if (SOCKET_fail(it->second->client_socket, err))
                        {
                            std::unique_lock<std::mutex> lock_tmp(mutex_initial_map_socket);
                            initial_socket_map.erase(client_socket);
                            lock_tmp.unlock();
                            return false;
                        }
                        std::cout << "发送消息失败" << std::endl;
                        std::cerr << "错误码：" << err << std::endl;
                    }
                    return register_success == 2 ? true : false;
                }
            }
        }
        if (!send_message::send_full(client_socket, mesg))
        {
            std::unique_lock<std::mutex> lock_tmp(mutex_initial_map_socket);
            initial_socket_map.erase(client_socket);
            lock_tmp.unlock();
            int err = WSAGetLastError();
            if (SOCKET_fail(it->second->client_socket, err))
            {
                std::unique_lock<std::mutex> lock_tmp(mutex_initial_map_socket);
                initial_socket_map.erase(client_socket);
                lock_tmp.unlock();
                return false;
            }
            std::cout << "发送消息失败" << std::endl;
            std::cerr << "错误码：" << err << std::endl;
        }
        return register_success == 2 ? true : false;
    }
    //把没有收到确认信息的消息放到消息队列中
    static void ack_mesg_map_to_send_mesg(san_thread* info)
    {
        //std::cout << "把没有收到确认信息的消息放到消息队列中1" << std::endl;
        while (info->ack_mesg_map_to_send_mesg_start)
        {
            if (!info->ack_mesg_map_to_send_mesg_start)
                break;
            for (size_t i = 0; i < info->mesg_ack_map.size(); i++)
            {
                if (!info->ack_mesg_map_to_send_mesg_start)
                    break;
                std::unique_lock<std::mutex>lock(info->mesg_ack_map_mutex[i]);
                if (info->mesg_ack_map[i].empty())
                    continue;
                time_t old_time = time(nullptr);
                std::vector<size_t> del_ack;
                for (auto& j : info->mesg_ack_map[i])
                {
                    recv_message_set r_m_s(j.second.second, 1);
                    if (old_time - j.second.first > 60 && old_time - j.second.first < 5 * 60)
                    {
                        std::unique_lock<std::mutex>lock_vector(info->user_list.user_vector_mutex);
                        std::shared_ptr<user> mu_user = info->user_list.user_vector[r_m_s.mu_user_id];
                        lock_vector.unlock();
                        if (mu_user != nullptr)
                        {
                            std::unique_lock<std::mutex>lock_tmp(mu_user->recv_message_queue_mutex);
                            mu_user->recv_message_queue.push(j.second.second);
                        }
                        del_ack.push_back(r_m_s.mesg_id);
                    }
                    else if (old_time - j.second.first > 60 * 5)
                        del_ack.push_back(r_m_s.mesg_id);
                }
                for (auto& j : del_ack)
                    info->mesg_ack_map[i].erase(j);
            }
        }
        info->ack_mesg_map_to_send_mesg_end = true;
        //std::cout << "把没有收到确认信息的消息放到消息队列中2" << std::endl;
    }
    static void login_out(san_thread* info)
    {
        while (info->login_out_start)
        {
            if (!info->login_out_start)
                break;
            for (auto& i : info->user_list.user_list)
            {
                if (!info->login_out_start)
                    break;
                std::unique_lock<std::mutex>lock(i.second);
                if (i.first.empty())
                    continue;
                for (auto& j : i.first)
                {
                    std::unique_lock<std::mutex>lock_user(j->user_mutex);
                    if (!j->is_login)
                        continue;
                    if (time(nullptr) - j->last_login > 5 * 60)
                    {
                        if (j->client_socket != INVALID_SOCKET)
                            closesocket(j->client_socket);
                        j->is_login = false;
                        j->client_socket = INVALID_SOCKET;
                    }
                }
            }
        }
        info->login_out_end = true;
    }
    bool SELECT_fail(std::vector<std::shared_ptr<user>>& user_vector, int err)
    {
        switch (err)
        {
        case 10038:
            for (auto& i : user_vector)
            {
                int optval;
                int optlen = sizeof(optval);
                std::unique_lock<std::mutex>lock(i->user_mutex);
                if (!(getsockopt(i->client_socket, SOL_SOCKET, SO_TYPE, (char*)&optval, &optlen) != SOCKET_ERROR))
                    continue;
                if (i->client_socket == INVALID_SOCKET)
                {
                    closesocket(i->client_socket);
                    i->client_socket = INVALID_SOCKET;
                }
            }
            return true;
        }
        return false;
    }
    bool SOCKET_fail(SOCKET& user_socket, int err)
    {
        if (user_socket == INVALID_SOCKET)
            return true;
        switch (err)
        {
        case 10054:
            if (user_socket != INVALID_SOCKET)
            {
                closesocket(user_socket);
                user_socket = INVALID_SOCKET;
                return true;
            }
            break;
        }
        return false;
    }
    san_thread()
    {
        std::thread accept_inital_socket_thread(accept_inital_socket, this);
        accept_inital_socket_thread.detach();
        std::thread recv_inital_map_socket_thread(recv_inital_map_socket, this);
        recv_inital_map_socket_thread.detach();
        recv_message_san_thread_info.resize(USER_LIST_BLOCK_COUNT_SIZE);
        send_message_san_thread_info.resize(USER_LIST_BLOCK_COUNT_SIZE);
        mesg_ack_map.resize(USER_LIST_BLOCK_COUNT_SIZE);
        system_message_san_thread_info.resize(USER_LIST_BLOCK_COUNT_SIZE / 4);
        recv_block_queues_mesg.resize(USER_LIST_BLOCK_COUNT_SIZE / 2);
        recv_message_set_type_san_thread_info.resize(USER_LIST_BLOCK_COUNT_SIZE / 2);
        size_t index = 0;
        for (auto& i : user_list.user_list)
        {
            recv_blocks_thread_start[index] = true;
            recv_blocks_thread_end[index] = false;
            send_blocks_thread_start[index] = true;
            send_blocks_thread_end[index] = false;
            if (index % 2 == 0)
            {
                recv_message_set_type_thread_start[index / 2] = true;
                recv_message_set_type_thread_end[index / 2] = false;
                recv_message_set_type_san_thread_info[index / 2].recv_block_queue_cv = &recv_block_queue_cv[index / 2];
                recv_message_set_type_san_thread_info[index / 2].recv_block_queue_mesg_mutex = &recv_block_queue_mesg_mutexs[index / 2];
                recv_message_set_type_san_thread_info[index / 2].is_start_thread = &recv_message_set_type_thread_start[index / 2];
                recv_message_set_type_san_thread_info[index / 2].is_end_thread = &recv_message_set_type_thread_end[index / 2];
                recv_message_set_type_san_thread_info[index / 2].user_list_block = &i;
                recv_message_set_type_san_thread_info[index / 2].recv_block_queue_mesg = &recv_block_queues_mesg[index / 2];
                recv_message_set_type_san_thread_info[index / 2].system_mesg_cv = &system_mesg_cv;
                recv_message_set_type_san_thread_info[index / 2].system_mesg_queue = &system_mesg_queue;
                recv_message_set_type_san_thread_info[index / 2].system_mesg_mutex = &mutex_system_mesg;
                recv_message_set_type_san_thread_info[index / 2].find_user = this;
                recv_message_set_type_san_thread_info[index / 2].index = index / 2;
                std::thread recv_message_set_type_san_thread(recv_message_set_type_thread, &recv_message_set_type_san_thread_info[index / 2]);
                recv_message_set_type_san_thread.detach();
                if (index % 4 == 0)
                {
                    system_message_thread_start[index / 4] = true;
                    system_message_thread_end[index / 4] = false;
                    system_message_san_thread_info[index / 4].system_mesg_cv = &system_mesg_cv;
                    system_message_san_thread_info[index / 4].system_mesg_mutex = &mutex_system_mesg;
                    system_message_san_thread_info[index / 4].system_mesg_queue = &system_mesg_queue;
                    system_message_san_thread_info[index / 4].is_start_thread = &system_message_thread_start[index / 4];
                    system_message_san_thread_info[index / 4].is_end_thread = &system_message_thread_end[index / 4];
                    system_message_san_thread_info[index / 4].index = index / 4;
                    system_message_san_thread_info[index / 4].find_user = this;
                    std::thread system_san_thread(system_mesg_thread, &system_message_san_thread_info[index / 4]);
                    system_san_thread.detach();
                }
            }
            recv_message_san_thread_info[index].user_list_block = &i;
            recv_message_san_thread_info[index].is_start_thread = &recv_blocks_thread_start[index];
            recv_message_san_thread_info[index].is_end_thread = &(recv_blocks_thread_end[index]);
            recv_message_san_thread_info[index].recv_block_queue_mesg_mutex = &recv_block_queue_mesg_mutexs[index / 2];
            recv_message_san_thread_info[index].recv_block_queue_mesg = &recv_block_queues_mesg[index / 2];
            recv_message_san_thread_info[index].recv_block_queue_cv = &recv_block_queue_cv[index / 2];
            recv_message_san_thread_info[index].system_mesg_cv = &system_mesg_cv;
            recv_message_san_thread_info[index].system_mesg_mutex = &mutex_system_mesg;
            recv_message_san_thread_info[index].system_mesg_queue = &system_mesg_queue;
            recv_message_san_thread_info[index].find_user = this;
            recv_message_san_thread_info[index].recv_mesg_map_ack = &mesg_ack_map[index];
            recv_message_san_thread_info[index].recv_mesg_map_ack_mutex = &mesg_ack_map_mutex[index];
            recv_message_san_thread_info[index].index = index;
            std::thread recv_block_san_threads(recv_block_thread, &recv_message_san_thread_info[index]);
            recv_block_san_threads.detach();
            send_message_san_thread_info[index].user_list_block = &i;
            send_message_san_thread_info[index].is_start_thread = &send_blocks_thread_start[index];
            send_message_san_thread_info[index].is_end_thread = &(send_blocks_thread_end[index]);
            send_message_san_thread_info[index].mesg_index = &send_mesg_index;
            send_message_san_thread_info[index].send_mesg_map_ack = &mesg_ack_map[index];//这个是给确认机制预留的接口
            send_message_san_thread_info[index].send_mesg_map_ack_mutex = &mesg_ack_map_mutex[index];//这个是给确认机制预留的接口
            send_message_san_thread_info[index].find_user = this;
            send_message_san_thread_info[index].index = index;
            std::thread send_block_san_threads(send_block_thread, &send_message_san_thread_info[index]);
            send_block_san_threads.detach();
            index++;
        }
        std::thread production_heartbeat_msg_thread(production_heartbeat_msg, this);
        production_heartbeat_msg_thread.detach();
        std::thread ack_mesg_map_to_send_mesg_thread(ack_mesg_map_to_send_mesg, this);
        ack_mesg_map_to_send_mesg_thread.detach();
        std::thread login_out_thread(login_out, this);
        login_out_thread.detach();
    }
    ~san_thread()
    {
        is_accept_inital_socket_start_thread = false;
        while (!is_accept_inital_socket_end_thread)
            Sleep(100);
        is_recv_inital_map_socket_start_thread = false;
        while (!is_recv_inital_map_socket_end_thread)
            Sleep(100);
        for (size_t i = 0; i < USER_LIST_BLOCK_COUNT_SIZE; i++)
        {
            recv_blocks_thread_start[i] = false;
            send_blocks_thread_start[i] = false;
            system_message_thread_start[i / 4] = false;
            recv_message_set_type_thread_start[i / 2] = false;

            while (!recv_blocks_thread_end[i])
                Sleep(100);
            while (!send_blocks_thread_end[i])
                Sleep(100);
            while (!system_message_thread_end[i / 4])
            {
                system_mesg_cv.notify_all();
                Sleep(100);
            }
            while (!recv_message_set_type_thread_end[i / 2])
            {
                recv_block_queue_cv[i / 2].notify_all();
                Sleep(100);
            }
        }
        production_heartbeat_msg_start = false;
        while (!production_heartbeat_msg_end)
            Sleep(100);
        ack_mesg_map_to_send_mesg_start = false;
        while (!ack_mesg_map_to_send_mesg_end)
            Sleep(100);
        login_out_start = false;
        while (!login_out_end)
            Sleep(100);
    }
};
int main()
{
    san_thread t;
    std::string order;
    /* while (1)
     {
         Sleep(10000);
     }*/
    while (1)
    {
        //system("cls");
        std::cout << "save user member：保存用户数据" << std::endl;
        std::cout << "show user register count：显示注册人数" << std::endl;
        std::cout << "exit：退出程序" << std::endl;
        std::getline(std::cin, order);
        if ("save user member" == order)
        {
            t.user_list.write_user_list_file();
            std::cout << "保存成功" << std::endl;
        }
        else if ("show user register count" == order)
            std::cout << t.user_list.user_index - INITIAL_INDEX << std::endl;
        else if ("exit" == order)
            break;
        Sleep(10000);
    }
    return 0;
}