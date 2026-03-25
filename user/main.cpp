/*
* 即时通讯系统的用户端设计
我先说一些我的简单设计理念:
1.开始类，主要是注册和登录等行为，是程序的一开始
2.连接类，子线程，里面给出一个连接成功的接口给外面使用，每次发消息的时候，主动检查就能快速检测出是否在连接，子线程的话，未来也可以给这个做类似心跳包之类的
3.联系人类，这个是存储联系人的，包括一些个人信息什么的，和我那个通讯录类似，但是用户端应该用不到，没有那么多的联系人
4.聊天记录类，这个不应该算是一种类，准过来说前期可以在用户类里用一个简单的队列数据结构就行了，未来如果要处理多种聊天记录(图片什么的)，就可以用到了。
5.界面类，虽然我这不是专门的聊天界面，但是也要简单搞一点界面，让用户使用。
6.管理类，他是整个系统的核心，从这开始，在从这结束，统筹程序的过程。
*/
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
//12、 [ack]	 from|原用户ID|to|目标用户ID:mesg_id                                                                       			确认包，目前只有服务器才有接收的包，未来可以做到服务器发送客户端接收的
//13、 [heartbeat]from|原用户ID|to|目标用户ID:are you ok?                                                                         	心跳包，这是服务器发送的
//14、 [heartbeat]from|原用户ID|to|目标用户ID:i am ok.                                                                         		心跳包，这是客户端发送的
//15、 [have_user]from|原用户ID|to|目标用户ID:i want user list.                                                                		拉取联系人数据，这是客户端发送的
//16、 [have_user]from|原用户ID|to|目标用户ID:count|user_id|user_name|...	                                                        拉取联系人数据，这是服务器端发送的
//17、 [login]	  from|原用户ID|to|目标用户ID:login_out.					                                                        手动退出登录
#define _CRT_SECURE_NO_WARNINGS 1//屏蔽windows的一些安全警告
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#include <iostream>//输入输出流
#include<fstream>//文件流
#include<vector>//动态数组
#include<queue>//队列
#include <winsock2.h>
#include<windows.h>//windows系统库
#pragma comment(lib, "ws2_32.lib")
#include<thread>//通用线程库
#include<string>//字符串
#include<mutex>//互斥锁
#include <condition_variable>//条件变量
#include<atomic>//原子变量
#include<semaphore>//信号量
#include <mmsystem.h>//windows音乐库
#pragma comment(lib,"winmm.lib")//windows动态音乐链接库
#include <chrono>//将时间转换成不同样式
#include<ctime>//时间库
#include <sstream>
#include <iomanip>
#include<map>
#include <memory>
#include<conio.h>
#include<list>
//前置声明
//1.开始类，主要是注册和登录等行为，是程序的一开始
//系统的开始，它只需要接受用户名和密码什么的，后期可以做加密信息，然后直接发到服务器去验证就行了
//并把用户名和账号等非保密的信息保存到本地，密码设为可选保存，这样用户就可以直接登录了。
class start;
//2.连接类，子线程，里面给出一个连接成功的接口给外面使用，每次发消息的时候，
//主动检查就能快速检测出是否在连接，子线程的话，未来也可以给这个做类似心跳包之类的
class Connect;
//3.联系人类，这个是存储联系人的，包括一些个人信息什么的，
//和我那个通讯录类似，但是用户端应该用不到，没有那么多的联系人
//前期不做聊天记录类，先用一个简单的队列数据结构就行了，未来如果要处理多种聊天记录(图片什么的)，就可以用到了。
class user;
//5.界面类，虽然我这不是专门的聊天界面，但是也要简单搞一点界面，让用户使用。
class ui;
//6.管理类，他是整个系统的核心，从这开始，在从这结束，统筹程序的过程。
class manage;
//新增类
//7.日志类，处理日志数据，原因是因为日志导致的全局变量太多，为避免变量名污染，可以用一个类来处理日志。
class operator_log;
//8.接收的消息类，解析接收的消息
class recv_message_set;
//9.全局变量类，全局变量的类，用于存储一些全局变量，比如用户列表，日志等。
class Global_variable;
//10.发送消息类，处理发送的消息
class send_message;
//11.输入的数据类，所有的输入数据先经过这个类，包括数字和字符等所有的，为了方便帮助使用，以及获取特殊命令
class input_mesg;
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
#define MESEGE_MAX_LEN 1024*1024*10//消息的最大长度
#define FROME "from|"//从哪里来的
#define TO "|to|"//到哪里去
#define INFO_MSG ":"//分割点
#define SERVER_USER_INDEX 1//服务器的用户索引
#define SOMEBODY_USER_INDEX 0//所有人的用户索引，表示任意人，用于没登陆时的
//消息的标识符
#define MSG 1//发送的是消息
#define REGISTER 2//注册消息
#define LOGIN 3//登录消息
#define ADD_USER 4//添加用户
#define MSG_ACK 5//确认包类型
#define MSG_HEARTBEAT 6//心跳包类型
#define MSG_HAVE_USER 7//主动向服务器拉取联系人数据
#define LOGIN_SUCCESS "login_success"//登录成功
#define LOGIN_FAIL "login_fail"//登录失败
#define LOGIN_OUT "login_out"//退出登录
#define REGISTER_SUCCESS "register_success"//注册成功
#define REGISTER_FAIL "register_fail"//注册失败
#define SEPARATEOR "|"//分隔符
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
#define MESSAGE_MUSIC_BEIJING_MAX 5//即时通讯的消息背景音乐的最大数量，就算有超多消息轰炸，也不能一直响下去
//全局变量
Global_variable* global_variable_obj = nullptr;//唯一的全局变量对象，里面包括所有需要的全局变量
//通用函数
std::istream& operator>>(std::istream& is, user& u);//重载>>对user类的一个流操作
std::ostream& operator<<(std::ostream& os, const user& u);//重载<<对user类的一个流操作
static void play_message_music_beijing();//播放消息背景音乐
bool operator_log_condition_variable(operator_log* ol);
std::string time_point_to_time_str()
{
	time_t timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	tm local_time = *localtime(&timestamp);
	std::ostringstream oss;
	oss << std::put_time(&local_time, "%Y.%m.%d_%H.%M.%S");
	return oss.str();
}
void recv_message_set_type(std::string mesg);//接收到的消息的类型转换
class Global_variable//全局变量
{
public:
	std::counting_semaphore<INT_MAX>message_semaphore_music_beijing;//消息的背景声音,后期如果可以的话，建议封装到music类里
	std::atomic<int>message_semaphore_music_beijing_count = 0;//消息的背景声音的计数器,避免超出最大数值
	std::atomic<bool> is_play_start_message_music_beijing_thread = true;//是否结束背景音乐的线程的，原子变量
	std::atomic<bool> is_play_end_message_music_beijing_thread = false;//是否结束背景音乐的线程的，原子变量
	operator_log* operator_log_obj;//操作日志对象
	std::mutex mesg_queue_mutex;//消息队列的互斥锁
	std::queue<std::string> user_mesg_queue;//消息队列
	std::condition_variable user_mesg_queue_cv;//消息队列的条件变量
	std::mutex system_mesg_queue_mutex;//系统消息队列的互斥锁
	std::queue<std::string> system_mesg_queue;//服务器消息队列
	std::condition_variable system_mesg_queue_cv;//消息队列的条件变量
	Global_variable() :message_semaphore_music_beijing(0) {}
};
static void play_message_music_beijing()//播放消息背景音乐
{
	//std::cout << "音乐的子线程开始了" << std::endl;
	std::wstring music_path = L"open \"";
	music_path += BEIJING_MUSIC;
	music_path += L"\" alias music";
	mciSendString(music_path.c_str(), NULL, 0, NULL);
	wchar_t music_len_wchar_t[256];
	mciSendString(L"status music length", music_len_wchar_t, 256, NULL);
	int music_len = _wtoi(music_len_wchar_t);
	mciSendString(L"close music", NULL, 0, NULL);
	while (global_variable_obj->is_play_start_message_music_beijing_thread)
	{
		//std::cout << "4";
		global_variable_obj->message_semaphore_music_beijing.acquire();//等待获取信号量
		global_variable_obj->message_semaphore_music_beijing_count--;
		if (!global_variable_obj->is_play_start_message_music_beijing_thread)
			break;
		mciSendString(music_path.c_str(), NULL, 0, NULL);
		mciSendString(L"play music", NULL, 0, NULL);
		Sleep(music_len + 10);//音效的延迟
		mciSendString(L"close music", NULL, 0, NULL);
	}
	global_variable_obj->is_play_end_message_music_beijing_thread = true;
	//std::cout << "音乐的子线程结束了" << std::endl;
}
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
		//std::cout << "发" << send_message << std::endl;
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
class input_mesg
{
public:
	static void del_for_sp(std::string& str)//去除前导空格
	{
		size_t i;
		for (i = 0; i < str.size(); i++)
			if (str[i] != ' ')
				break;
		str = str.substr(i);
	}
	static void del_for_sp(std::wstring& str)//去除前导空格
	{
		size_t i;
		for (i = 0; i < str.size(); i++)
			if (str[i] != L' ')
				break;
		str = str.substr(i);
	}
	static int input(int& index, std::atomic<bool>* crls)
	{
		std::string str_tmp;
		int flag = input(str_tmp, crls);
		if (flag != 0)
			return flag;
		del_for_sp(str_tmp);
		index = atoi(str_tmp.c_str());
		return 0;
	}
	static int input(size_t& index, std::atomic<bool>* crls)
	{
		std::string str_tmp;
		int flag = input(str_tmp, crls);
		if (flag != 0)
			return flag;
		del_for_sp(str_tmp);
		index = atoll(str_tmp.c_str());
		return 0;
	}
	static int input(std::wstring& mseg, std::atomic<bool>* crls)
	{
		while (1)
		{
			if (crls != nullptr && *crls == true)
			{
				*crls = false;
				return 3;
			}
			if (_kbhit())
			{
				std::getline(std::wcin, mseg);
				if (mseg == L"?" || mseg == L"help" || mseg == L"？")
				{
					std::cout << "重新连接服务器：again connect server" << std::endl;
					std::cout << "返回上一步：backword" << std::endl;
					std::cout << "刷新：crls" << std::endl;
					std::cout << "保存数据：save" << std::endl;
					std::cout << "拉取用户数据：to server have user" << std::endl;
					std::cout << "显示我和当前用户所有的聊天记录：have all msg" << std::endl;
					continue;
				}
				else if (mseg == L"again connect server")
					return 1;
				else if (mseg == L"backword")
					return 2;
				else if (mseg == L"crls")
					return 3;
				else if (mseg == L"save")
					return 4;
				else if (mseg == L"to server have user")
					return 5;
				else if (mseg == L"have all msg")
					return 6;
				break;
			}
			Sleep(100);
		}
		return 0;
	}
	static int input(std::string& mseg, std::atomic<bool>* crls)
	{
		while (1)
		{
			if (crls != nullptr && *crls == true)
			{
				*crls = false;
				return 3;
			}
			if (_kbhit())
			{
				std::getline(std::cin, mseg);
				if (mseg == "?" || mseg == "help" || mseg == "？")
				{
					std::cout << "重新连接服务器：again connect server" << std::endl;
					std::cout << "返回上一步：backword" << std::endl;
					std::cout << "刷新：crls" << std::endl;
					std::cout << "保存数据：save" << std::endl;
					std::cout << "拉取用户数据：to server have user" << std::endl;
					std::cout << "显示我和当前用户所有的聊天记录：have all msg" << std::endl;
					continue;
				}
				else if (mseg == "again connect server")
					return 1;
				else if (mseg == "backword")
					return 2;
				else if (mseg == "crls")
					return 3;
				else if (mseg == "save")
					return 4;
				else if (mseg == "to server have user")
					return 5;
				else if (mseg == "have all msg")
					return 6;
				break;
			}
			Sleep(100);
		}
		return 0;
	}
};
class ui
{
public:
	static void login_ui(std::wstring& user_name, std::wstring& password)//登录的界面
	{
		system("cls");
		std::cout << "登录" << std::endl;
		std::cout << "请输入用户名(输入r表示注册)" << std::endl;
		std::getline(std::wcin, user_name);
		if (user_name == L"r" || user_name == L"R")
			return;
		std::cout << "请输入密码(输入r表示注册)" << std::endl;
		std::getline(std::wcin, password);
	}
	static void register_ui(std::wstring& user_name, std::wstring& password)//注册的界面
	{
		//目前其实和登录的界面一样，因为我没法做到手机验证的等能够验证是否为真人的功能，这里就暂时和登录的界面一样
		//后期可以做手机验证码，邮箱等的功能
		system("cls");
		std::cout << "注册(注册成功后自动登录)" << std::endl;
		std::cout << "请输入用户名(输入l表示登录,最多" << USER_NAME_MAX_LEN << "个字符，最少" << USER_NAME_MIN_LEN << "个字符, 必须是英文字母或者数字)" << std::endl;
		std::getline(std::wcin, user_name);
		if (user_name == L"l" || user_name == L"L")
			return;
		std::cout << "请输入密码(输入l表示登录,最多" << USER_PASSWORD_MAX_LEN << "个字符，最少" << USER_PASSWORD_MIN_LEN << "个字符,必须是英文字母或者数字)" << std::endl;
		std::getline(std::wcin, password);
	}
	static void message_queue_ui(size_t index, std::string name, std::list<std::string> message_queue)//消息的队列
	{
		system("cls");
		std::cout << "与" << name << "的聊天记录" << std::endl;
		if (message_queue.empty())
			std::cout << "还没有聊天记录，请先聊天" << std::endl;
		index = ntohll(index);
		for (auto& i : message_queue)
		{
			RoutePocketHeader rph;
			memcpy(&rph, i.data(), sizeof(RoutePocketHeader));
			if (rph.to_id == index)
				std::cout << "他：";
			else
				std::cout << "我：";
			std::cout << i << std::endl;
		}
	}
	static void user_list_ui(int user_index, size_t no_read_message_count, const std::string& user_name)//用户列表的界面
	{
		char tmp[100];
		sprintf(tmp, "%d、%%-%ds ", user_index, USER_NAME_MAX_LEN);
		printf(tmp, user_name.c_str());
		if (no_read_message_count)
			printf("%d条未读消息", no_read_message_count);
		printf("\n");
	}
	static int get_user_nume_index_ui(std::wstring& nume, size_t& index, std::atomic<bool>* crls)
	{
		std::cout << "请输入目标的用户名：" << std::endl;
		int flag = input_mesg::input(nume, crls);
		if (flag != 0)
			return flag;
		std::cout << "请输入目标的用户编号：" << std::endl;
		flag = input_mesg::input(index, crls);
		if (flag != 0)
			return flag;
		return 0;
	}
	static void system_mesg_operation_ui()
	{
		std::cout << "|----------------------|" << std::endl;
		std::cout << "|      1.添加好友      |" << std::endl;
		std::cout << "|      2.删除好友      |" << std::endl;
		std::cout << "|      3.通过好友      |" << std::endl;
		std::cout << "|      4.拒绝好友      |" << std::endl;
		std::cout << "|----------------------|" << std::endl;
	}
};
class user
{
public:
	size_t user_index;//用户的服务器中唯一编码
	std::string name;//用户的名称
	std::mutex message_mutex;
	std::list<std::string> message_queue;//与该用户的消息队列
	size_t no_read_message_count = 0;//未读消息的数量
	std::atomic<bool>new_mesg_flage = false;
	user& operator=(const user& u1)
	{
		if (this == &u1)
			return *this;
		this->user_index = u1.user_index;
		this->name = u1.name;
		this->message_queue = u1.message_queue;
		this->no_read_message_count = u1.no_read_message_count;
		return *this;
	}
	user() {}
	user(const user& u)
	{
		this->user_index = u.user_index;
		this->name = u.name;
		this->message_queue = u.message_queue;
		this->no_read_message_count = u.no_read_message_count;
	}
	void show_message(size_t user_index, bool  type = false)
	{
		//显示聊天记录
		std::unique_lock<std::mutex> lock(message_mutex);
		while (message_queue.size() > 150 && type)//不能有太多消息存在内存中，防止内存占用过多
			message_queue.pop_front();//弹出最早的消息
		ui::message_queue_ui(user_index, name, message_queue);
	}
	void show(int user_index)
	{
		ui::user_list_ui(user_index, no_read_message_count, name);
	}
	void read_file_message(std::string file_name)//全量读取
	{
		std::unique_lock<std::mutex>lock(message_mutex);
		std::ifstream read(file_name + name + std::to_string(user_index));
		if (!read.is_open())
			return;
		std::string line;
		message_queue.clear();
		while (std::getline(read, line))
			message_queue.push_back(line);
		read.close();
	}
	void write_file_message(std::string file_name)//全量写入
	{
		std::unique_lock<std::mutex>lock(message_mutex);
		std::list<std::string> tmp_queue = message_queue;
		lock.unlock();
		std::ofstream write(file_name + name + std::to_string(user_index));
		if (!write.is_open())
			return;
		for (auto& i : tmp_queue)
			write << i << std::endl;
		write.close();
	}
	void write_one_message(std::string file_name, std::string message)//写入一条消息
	{
		std::ofstream write(file_name + name + std::to_string(user_index), std::ios::app);
		if (!write.is_open())
		{
			write.open(file_name + name + std::to_string(user_index));
			if (!write.is_open())
				return;
		}
		write << message << std::endl;
		write.close();
	}
};
std::istream& operator>>(std::istream& is, user& u)
{
	is >> u.user_index >> u.name;
	return is;
}
std::ostream& operator<<(std::ostream& os, const user& u)
{
	os << u.user_index << ' ' << u.name << ' ';
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
	static bool recv_full(SOCKET socket, std::string& mesg)
	{
		char first_len_char[FIRST_LEN + 1];
		int first_len = 0;
		int curr_first_len = 0;
		while (first_len < FIRST_LEN)
		{
			int	ret = recv(socket, first_len_char + curr_first_len, FIRST_LEN - curr_first_len, 0);
			if (ret <= 0)
				return false;
			first_len += ret;
			curr_first_len += ret;
		}
		first_len_char[FIRST_LEN] = '\0';
		long long len = std::stoll(first_len_char);
		if (len > MESEGE_MAX_LEN)
		{
			std::cout << "消息错误，断开连接" << std::endl;
			return false;
		}
		mesg.resize(len);
		int mesg_len = 0;
		while (mesg_len < len)
		{
			int ret = recv(socket, &mesg[mesg_len], len - mesg_len, 0);
			if (ret <= 0)
				return false;
			mesg_len += ret;
		}
		//std::cout << "收" << mesg << std::endl;
		return true;
	}
};
void recv_message_set_type(std::string mesg)
{
	//std::cout << "收到消息：" << mesg << std::endl;//打印消息
	recv_message_set r_m_s(mesg);
	if (r_m_s.info == LOGIN || r_m_s.info == REGISTER || r_m_s.info == ADD_USER || r_m_s.info == MSG_HAVE_USER)//系统消息
	{
		if (r_m_s.info == ADD_USER)
		{
			if (global_variable_obj->message_semaphore_music_beijing_count < MESSAGE_MUSIC_BEIJING_MAX)
			{
				global_variable_obj->message_semaphore_music_beijing_count++;
				global_variable_obj->message_semaphore_music_beijing.release();//信号变量
			}
		}
		std::unique_lock<std::mutex> lock(global_variable_obj->system_mesg_queue_mutex);//互斥锁，不需要等，上锁，防止其他线程修改，并把数据放入消息转发队列就行了
		global_variable_obj->system_mesg_queue.push(mesg);
		global_variable_obj->system_mesg_queue_cv.notify_one();
	}
	else//用户消息
	{
		std::unique_lock<std::mutex> lock(global_variable_obj->mesg_queue_mutex);//互斥锁，不需要等，上锁，防止其他线程修改，并把数据放入消息转发队列就行了
		global_variable_obj->user_mesg_queue.push(mesg);
		global_variable_obj->user_mesg_queue_cv.notify_one();
	}
}
class operator_log
{
private:
	void change_file_name_operator_log(std::ofstream& log_file_num, time_t& time_old)
	{
		time_t time_now = time(nullptr);
		std::string time_str = time_point_to_time_str();
		time_str = time_str.substr(0, time_str.find("_"));
		std::wstring log_file_name = std::wstring(time_str.begin(), time_str.begin() + time_str.find("_")) + OPERATION_LOG_FILE;
		std::ofstream log(log_file_name, std::ios::app | std::ios::binary);//打开文件，追加模式
		if (!log.is_open())
		{
			std::cout << "新的日志文件打开失败，请检查文件是否出现错误" << std::endl;
			return;
		}
		time_old = time_now;
		log_file_num.close();
		log.close();
		log_file_num.open(log_file_name, std::ios::app | std::ios::binary);//重新打开文件，追加模式
	}
	static void write_operator_log(operator_log* ol)//操作日志
	{
		//std::cout << "写日志的子线程开始了" << std::endl;
		time_t time_old = time(nullptr);
		std::string time_str = time_point_to_time_str();
		time_str = time_str.substr(0, time_str.find("_"));
		std::wstring log_file_name = std::wstring(time_str.begin(), time_str.end()) + OPERATION_LOG_FILE;
		std::ofstream log(log_file_name, std::ios::app | std::ios::binary);//打开文件，追加模式
		if (!log.is_open())
		{
			std::cout << "日志文件打开失败，请检查文件是否出现错误" << std::endl;
			return;
		}
		while (ol->is_write_start_operator_log_thread)
		{
			//std::cout << "6";
			std::unique_lock<std::mutex> lock(ol->operator_log_mutex);
			ol->operator_log_mutex_cv.wait(lock, [&] { return operator_log_condition_variable(ol); });
			if (!ol->is_write_start_operator_log_thread && ol->operator_log_queue.empty())
				break;
			if (labs(time(nullptr) - time_old) > (60 * 60 * 24))//超过一天了，打开新的日志文件
				ol->change_file_name_operator_log(log, time_old);
			while (!ol->operator_log_queue.empty())
			{
				std::string op_log_wstr = ol->operator_log_queue.front();
				log << op_log_wstr << std::endl;
				ol->operator_log_queue.pop();
			}
		}
		log.close();
		ol->is_write_end_operator_log_thread = true;
		//std::cout << "写日志的子线程结束了" << std::endl;
	}
	std::mutex operator_log_mutex;//日志的互斥锁
	std::condition_variable operator_log_mutex_cv;//日志的条件变量
	std::atomic<bool> is_write_end_operator_log_thread = false;//是否写入日志的原子变量
public:
	std::queue<std::string> operator_log_queue;//操作日志队列
	std::atomic<bool> is_write_start_operator_log_thread = true;//是否写入日志的原子变量
	~operator_log()
	{
		is_write_start_operator_log_thread = false;
		while (!is_write_end_operator_log_thread)
		{
			operator_log_mutex_cv.notify_one();
			Sleep(100);
		}
	}
	operator_log()
	{
		std::thread t(&operator_log::write_operator_log, this);
		t.detach();//脱离主线程
	}
	void queue_operator_log(std::string ol_str)//操作日志队列
	{
		std::unique_lock<std::mutex> lock(operator_log_mutex);
		operator_log_queue.push(time_point_to_time_str() + " : " + ol_str);
		operator_log_mutex_cv.notify_one();
	}
};
bool operator_log_condition_variable(operator_log* ol)
{
	return!ol->operator_log_queue.empty() || !ol->is_write_start_operator_log_thread;
}
class start
{
private:
	bool is_num_or_english(std::wstring& user_name, std::wstring& password, std::string& user_name_char, std::string& password_char)
	{
		if (!(is_num_or_english(user_name) && is_num_or_english(password)))
			return false;
		user_name_char = std::string(user_name.begin(), user_name.end());
		password_char = std::string(password.begin(), password.end());
		return true;
	}
	bool user_passwerd_check(std::wstring user_name, std::wstring password, std::string& user_name_char, std::string& password_char)
	{
		input_mesg::del_for_sp(user_name);
		input_mesg::del_for_sp(password);
		if (user_name.empty() || password.empty())
		{
			std::cout << "用户名或密码不能为空" << std::endl;
			return false;
		}
		if (user_name.size() > USER_NAME_MAX_LEN || password.size() > USER_PASSWORD_MAX_LEN || user_name.size() < USER_NAME_MIN_LEN || password.size() < USER_PASSWORD_MIN_LEN)
		{
			std::cout << "用户名或密码长度不符合" << std::endl;
			return false;
		}
		if (is_num_or_english(user_name, password, user_name_char, password_char) == false)
		{
			std::cout << "用户名或密码只能是英文字母或者数字" << std::endl;
			return false;
		}
		return true;
	}
public:
	bool is_num_or_english(std::wstring& info)
	{
		for (auto i : info)
			if (!((i >= L'0' && i <= L'9') || (i >= L'a' && i <= L'z') || (i >= L'A' && i <= L'Z')))
				return false;
		return true;
	}
	bool start_run(std::string& user_name_char, std::string& password_char)
	{
		bool flage = true;//默认是登录 
		std::wstring user_name, password;
		while (1)
		{
			if (flage)
			{
				ui::login_ui(user_name, password);
				if (user_name == L"r" || user_name == L"R" || password == L"r" || password == L"R")
				{
					flage = false;
					continue;
				}
			}
			else
			{
				ui::register_ui(user_name, password);
				if (user_name == L"l" || user_name == L"L" || password == L"l" || password == L"L")
				{
					flage = true;
					continue;
				}
			}
			if (!user_passwerd_check(user_name, password, user_name_char, password_char))
			{
				Sleep(1000);
				continue;
			}
			if (!flage)
				global_variable_obj->operator_log_obj->queue_operator_log(" user " + user_name_char + " try Register");
			global_variable_obj->operator_log_obj->queue_operator_log(" user " + user_name_char + " try login");
			break;
		}
		return flage;
	}
};
class Connect
{
private:
	std::atomic<bool> is_connected = false;//原子变量，是否连接上服务器
	std::atomic<bool> is_logind = false;//原子变量，是否登录上服务器
	std::atomic<bool> is_not_logind = false;//原子变量，
	SOCKET user_socket = INVALID_SOCKET;
	std::atomic<bool>recv_start_thread = true;
	std::atomic<bool>recv_end_thread = false;
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
	bool set_socket_ioctlsocket()//采用select模式，暂时不用这种形式了，未来扩展
	{
		int count = AGAIN_COUNT;
		// 如果Mode = 0，则启用阻塞模式；
		// 如果Mode != 0，则启用非阻塞模式。
		u_long mode = 1;
		while (ioctlsocket(user_socket, FIONBIO, &mode) == SOCKET_ERROR && count--)//设置非阻塞模式
		{
			Sleep(1000);
			std::cout << "设置非阻塞模式失败，正在第" << AGAIN_COUNT - count << "次尝试" << std::endl;
		}
		if (count == 0 && user_socket == INVALID_SOCKET)
			return false;
		return true;
	}
	bool creat_socket()
	{
		int count = AGAIN_COUNT;
		while ((((user_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)) && count--)//创建套接字,TCP协议
		{
			Sleep(1000);
			std::cout << "创建套接字失败，正在第" << AGAIN_COUNT - count << "次尝试" << std::endl;
		}
		if (count == 0 && user_socket == INVALID_SOCKET)
			return false;
		return true;//&&set_socket_ioctlsocket();
	}
	static void recv_message(Connect* connect_obj)//接收消息，子线程
	{
		//std::cout << "收消息的子线程开始了" << std::endl;
		while (connect_obj->recv_start_thread && connect_obj->get_is_connected())
		{
			//std::cout << "5";
			fd_set read_fds;
			FD_ZERO(&read_fds);
			FD_SET(connect_obj->user_socket, &read_fds);
			timeval time_over_load;//一秒的超时时间
			time_over_load.tv_sec = 1;
			time_over_load.tv_usec = 0;
			int ret = select(connect_obj->user_socket + 1, &read_fds, NULL, NULL, &time_over_load);//select模式，暂时不用这种形式了，未来扩展
			if (!connect_obj->recv_start_thread)//退出子线程
				break;
			if (ret == 0)//没有连接
				continue;
			else if (ret < 0)//出错
			{
				std::cout << "select错误，请检查相关问题" << std::endl;
				break;
			}
			if (!FD_ISSET(connect_obj->user_socket, &read_fds))
				continue;
			std::string mesg;
			if (!recv_message_set::recv_full(connect_obj->user_socket, mesg))
			{
				closesocket(connect_obj->user_socket);
				connect_obj->user_socket = INVALID_SOCKET;
				connect_obj->set_is_connect(false);
				connect_obj->is_connected_cs.release();
				break;
			}
			recv_message_set_type(mesg);
		}
		connect_obj->recv_end_thread = true;
		//std::cout << "收消息的子线程结束了" << std::endl;
	}
public:
	std::counting_semaphore<1>is_connected_cs;//提醒自动重连
	bool get_is_connected()
	{
		return is_connected;
	}
	bool get_is_logind()
	{
		return is_logind;
	}
	bool get_is_not_logind()
	{
		return is_not_logind;
	}
	void set_is_logind(bool flage)
	{
		this->is_logind = flage;
		this->is_not_logind = !flage;
	}
	void set_is_not_logind(bool flage)
	{
		this->is_logind = !flage;
		this->is_not_logind = flage;
	}
	void set_is_logind_not_logind()
	{
		this->is_logind = false;
		this->is_not_logind = false;
	}
	void set_is_connect(bool flag)
	{
		is_connected = flag;
		set_is_logind(false);
	}
	SOCKET& get_user_socket()
	{
		return user_socket;
	}
	bool revoce_connet_server()//新连接服务器
	{
		if (!creat_socket())//恶行错误，创建套接字失败
		{
			std::cout << "创建套接字失败，请检查网络连接" << std::endl;
			exit(1);
		}
		set_is_connect(Connect_server());//连接服务器
		std::thread t(&Connect::recv_message, this);
		t.detach();
		return is_connected;
	}
	Connect() :is_connected_cs(0)
	{
		if (!win_init())//恶性错误，初始化失败
		{
			std::cout << "初始化失败，请检查网络连接" << std::endl;
			exit(1);
		}
		revoce_connet_server();
	}
	~Connect()
	{
		recv_start_thread = false;
		while (!recv_end_thread)
			Sleep(200);
		if (user_socket != INVALID_SOCKET)
		{
			closesocket(user_socket);
			user_socket = INVALID_SOCKET;
		}
		win_end();//释放
	}
	bool Connect_server()//可以强制手动连接
	{
		sockaddr_in server = { 0 };
		server.sin_family = AF_INET;
		server.sin_port = htons(SERVER_PORT);
		server.sin_addr.s_addr = inet_addr(SERVER_IP);
		int count = AGAIN_COUNT;//重连次数
		while (count--)
		{
			is_connected = false;
			if (connect(user_socket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
			{
				Sleep(1000);
				continue;
			}
			is_connected = true;
			return true;
		}
		if (!is_connected)
			return false;
		return false;
	}
};
class manage
{
private:
	std::mutex user_list_mutex;
	std::vector<std::shared_ptr<user>> user_list;
	size_t user_index = 0;//用户的唯一编码，通过服务器传过来的数据有这个数据，就能通过这个数获取该用户的网络端各种的相关信息,0表示任意id，1表示服务器
	std::mutex user_map_mutex;
	std::map<size_t, std::shared_ptr<user>> user_map;
	std::string user_name;
	std::string user_passerword;
	operator_log ob;
	start start_obj;
	std::atomic<bool>recv_or_send_message_input_user_message_queue_start_thread = true;
	std::atomic<bool>recv_or_send_message_input_user_message_queue_end_thread = false;
	std::atomic<size_t> no_read_message_count = 0;
	std::atomic<bool>new_mesg_flage = false;
	Connect connect_obj;
	std::atomic<bool>global_variable_system_input_manage_system_start_thread = true;
	std::atomic<bool>global_variable_system_input_manage_system_end_thread = false;
	std::atomic<bool>system_mesege_handle_start_thread = true;
	std::atomic<bool>system_mesege_handle_end_thread = false;
	std::mutex system_mesege_mutex;
	std::vector<std::string> system_mesege_vector;
	std::vector<bool> system_mesege_vector_flage;
	std::condition_variable system_mesege_handle_cv;//系统消息的条件变量
	std::atomic<bool> returned = false;
	std::atomic<bool> is_read_user_list_file = false;
	std::atomic<bool>revoce_connet_server_and_login_start = true;
	std::atomic<bool>revoce_connet_server_and_login_end = false;
	std::atomic<bool>mesg_ack_map_re_send_start = true;
	std::atomic<bool>mesg_ack_map_re_send_end = false;
	bool login_mesg(std::string& mesg)
	{
		recv_message_set r_m_s(mesg, 1);
		if (r_m_s.end_message_string == LOGIN_SUCCESS)
		{
			connect_obj.set_is_logind(true);
			//std::cout << r_m_s.yuan_user_name << "登录成功" << std::endl;
			global_variable_obj->operator_log_obj->queue_operator_log(" user " + r_m_s.yuan_user_name + " " + LOGIN_SUCCESS);//登录成功日志
			user_name = r_m_s.yuan_user_name;
			user_index = r_m_s.mu_user_id;
			if (!is_read_user_list_file)
			{
				is_read_user_list_file = true;
				user server_user;
				server_user.name = "服务器";
				server_user.user_index = SERVER_USER_INDEX;
				std::unique_lock<std::mutex>lock_user(user_list_mutex);
				user_list.clear();
				user_list.push_back(std::make_shared<user>(server_user));
				lock_user.unlock();
			}
			write_user_list(user_name + user_passerword);
			return true;
		}
		else if (r_m_s.end_message_string == LOGIN_FAIL)
		{
			//std::cout << "登录失败，请检查用户名或密码" << std::endl;
			global_variable_obj->operator_log_obj->queue_operator_log(" user " + r_m_s.yuan_user_name + " " + LOGIN_FAIL);//登录失败日志
			connect_obj.set_is_not_logind(true);
			return true;
		}
		return false;
	}
	bool register_mesg(std::string& mesg)
	{
		recv_message_set r_m_s(mesg, 1);
		if (r_m_s.end_message_string == REGISTER_SUCCESS)
		{
			connect_obj.set_is_logind(true);
			//std::cout << r_m_s.yuan_user_name << "注册成功" << std::endl;
			//std::cout << r_m_s.yuan_user_name << "登录成功" << std::endl;
			global_variable_obj->operator_log_obj->queue_operator_log(" user " + r_m_s.yuan_user_name + " register success");
			global_variable_obj->operator_log_obj->queue_operator_log(" user " + r_m_s.yuan_user_name + " login success");
			user_name = r_m_s.yuan_user_name;
			user_index = r_m_s.mu_user_id;
			is_read_user_list_file = true;
			user server_user;
			server_user.name = "服务器";
			server_user.user_index = SERVER_USER_INDEX;
			std::unique_lock<std::mutex>lock_user(user_list_mutex);
			user_list.clear();
			user_list.push_back(std::make_shared<user>(server_user));
			lock_user.unlock();
			write_user_list(user_name + user_passerword);
			return true;
		}
		else if (r_m_s.end_message_string == REGISTER_FAIL)
		{
			//std::cout << "注册失败，当前用户名已被使用，请使用其他用户名" << std::endl;
			global_variable_obj->operator_log_obj->queue_operator_log(" user " + r_m_s.yuan_user_name + " " + REGISTER_FAIL);
			global_variable_obj->operator_log_obj->queue_operator_log(" user " + r_m_s.yuan_user_name + " " + LOGIN_FAIL);
			connect_obj.set_is_not_logind(true);
			return true;
		}
		return false;
	}
	static void recv_or_send_message_input_user_message_queue(manage* manage_obj)//把用户的消息放入消息队列，在分散到各个用户的消息队列中，包括发送和接受的消息
	{
		//std::cout << "消息转发的子线程开始了" << std::endl;
		std::string message = CLIENT_HEARTBEAT;
		while (manage_obj->recv_or_send_message_input_user_message_queue_start_thread)
		{
			//std::cout << "3";
			std::unique_lock<std::mutex> lock(global_variable_obj->mesg_queue_mutex);
			global_variable_obj->user_mesg_queue_cv.wait(lock, [&]() {return !global_variable_obj->user_mesg_queue.empty() || !manage_obj->recv_or_send_message_input_user_message_queue_start_thread; });
			if (!manage_obj->recv_or_send_message_input_user_message_queue_start_thread)
				break;
			std::string mesg = global_variable_obj->user_mesg_queue.front();
			global_variable_obj->user_mesg_queue.pop();
			lock.unlock();
			size_t msg_id = manage_obj->msg_id++;
			recv_message_set r_m_s(mesg, 1);
			if (r_m_s.info == MSG_HEARTBEAT)
			{
				message = send_message::send_message_to_from_A_to_B(MSG_HEARTBEAT, manage_obj->user_index, SERVER_USER_INDEX, message);
				send_message::send_full(manage_obj->get_connect_obj().get_user_socket(), message);//把心跳包还回去，不需要考虑是否真正的完全发回去，心跳包的确认时间很长
				continue;
			}
			else if (r_m_s.info == MSG_ACK)
			{
				//std::cout << "收到确认消息" << std::endl;
				std::unique_lock<std::mutex>lock(manage_obj->mesg_ack_mutex);
				if (manage_obj->mesg_ack_map.find(r_m_s.mesg_id) != manage_obj->mesg_ack_map.end())
					manage_obj->mesg_ack_map.erase(r_m_s.mesg_id);
				lock.unlock();
				continue;
			}
			else  if (r_m_s.info == MSG)
			{
				std::string ack_msg = std::to_string(r_m_s.mesg_id);
				ack_msg = send_message::send_message_to_from_A_to_B(MSG_ACK, manage_obj->user_index, SERVER_USER_INDEX, ack_msg);
				send_message::send_full(manage_obj->get_connect_obj().get_user_socket(), ack_msg);
			}
			std::unique_lock<std::mutex>lock_map(manage_obj->user_map_mutex);
			auto it1 = manage_obj->user_map.find(r_m_s.yuan_user_id);
			auto it3 = manage_obj->user_map.end();
			if (it1 == it3)//当前消息不是自己及其好友的消息时，不处理
				continue;
			it1->second->write_one_message(manage_obj->user_name + manage_obj->user_passerword, mesg);
			std::unique_lock<std::mutex>lock_user(it1->second->message_mutex);//互斥锁，防止主线程修改
			manage_obj->user_map[r_m_s.yuan_user_id]->message_queue.push_back(mesg);//加入用户的消息队列
			manage_obj->user_map[r_m_s.yuan_user_id]->no_read_message_count++;
			manage_obj->user_map[r_m_s.yuan_user_id]->new_mesg_flage = true;
			lock_map.unlock();
			manage_obj->all_new_mesg_flage = true;
			if (global_variable_obj->message_semaphore_music_beijing_count < MESSAGE_MUSIC_BEIJING_MAX)
			{
				global_variable_obj->message_semaphore_music_beijing_count++;
				global_variable_obj->message_semaphore_music_beijing.release();//信号变量
			}
			manage_obj->no_read_message_count++;
		}
		manage_obj->recv_or_send_message_input_user_message_queue_end_thread = true;
		//std::cout << "消息转发的子线程结束了" << std::endl;
	}
	static void global_variable_system_input_manage_system(manage* manage_obj)//把全局变量里的数据，转入manage的系统队列里
	{
		//std::cout << "系统消息转发的子线程开始了" << std::endl;
		while (manage_obj->global_variable_system_input_manage_system_start_thread)
		{
			//std::cout << "2";
			std::unique_lock<std::mutex> lock(global_variable_obj->system_mesg_queue_mutex);
			global_variable_obj->system_mesg_queue_cv.wait(lock, [&]() {return !global_variable_obj->system_mesg_queue.empty() || !manage_obj->global_variable_system_input_manage_system_start_thread; });
			if (!manage_obj->global_variable_system_input_manage_system_start_thread)
				break;
			std::unique_lock<std::mutex> tmp_lock(manage_obj->system_mesege_mutex);
			while (!global_variable_obj->system_mesg_queue.empty())
			{
				manage_obj->system_mesege_vector.push_back(global_variable_obj->system_mesg_queue.front());
				manage_obj->system_mesege_vector_flage.push_back(false);
				global_variable_obj->system_mesg_queue.pop();
				manage_obj->system_mesege_handle_cv.notify_one();
				//std::cout << "系统消息提醒了一次" << std::endl;
			}
		}
		manage_obj->global_variable_system_input_manage_system_end_thread = true;
		//std::cout << "系统消息转发的子线程结束了" << std::endl;
	}
	void add_user_mesg(std::string& mesg)
	{
		recv_message_set r_m_s(mesg, 1);
		std::unique_lock<std::mutex>lock(user_list_mutex);
		std::unique_lock<std::mutex>lock_map(user_map_mutex);
		for (size_t i = 0; i < user_list.size(); i++)
		{
			if (user_list[i]->name == r_m_s.yuan_user_name && user_list[i]->user_index == r_m_s.yuan_user_id)
				return;
		}
		user tmp_user;
		tmp_user.name = r_m_s.yuan_user_name;
		tmp_user.user_index = r_m_s.yuan_user_id;
		user_list.push_back(std::make_shared<user>(tmp_user));
		user_map[r_m_s.yuan_user_id] = user_list[user_list.size() - 1];
		user_list.back()->read_file_message(user_name + user_passerword);
		lock_map.unlock();
		lock.unlock();
		write_user_list(user_name + user_passerword);
		return;
	}
	static void system_mesege_handle(manage* manage_obj)
	{
		//std::cout << "系统消息处理线程开始了" << std::endl;
		while (manage_obj->system_mesege_handle_start_thread)
		{
			//std::cout << "1";
			std::unique_lock<std::mutex> lock(manage_obj->system_mesege_mutex);
			size_t tmp_size = manage_obj->system_mesege_vector.size();
			//std::cout << "系统消息处理了一次" << std::endl;
			manage_obj->system_mesege_handle_cv.wait(lock, [&]() {return tmp_size != manage_obj->system_mesege_vector.size() || !manage_obj->system_mesege_handle_start_thread; });
			if (!manage_obj->system_mesege_handle_start_thread)
				break;
			if (manage_obj->system_mesege_vector.empty())
				continue;
			size_t index = manage_obj->system_mesege_vector.size() - 1;
			if (manage_obj->system_mesege_vector_flage[index])
				continue;
			recv_message_set r_m_s(manage_obj->system_mesege_vector[index], 1);
			bool flage = false;
			if (r_m_s.info == LOGIN)
			{
				flage = manage_obj->login_mesg(manage_obj->system_mesege_vector[index]);
				manage_obj->system_mesege_vector_flage[index] = flage;
				lock.unlock();
				manage_obj->write_system_msg_false();
				lock.lock();
			}
			else if (r_m_s.info == REGISTER)
			{
				flage = manage_obj->register_mesg(manage_obj->system_mesege_vector[index]);
				manage_obj->system_mesege_vector_flage[index] = flage;
				lock.unlock();
				manage_obj->write_system_msg_false();
				lock.lock();
			}
			else if (r_m_s.info == MSG_HAVE_USER)
			{
				manage_obj->system_mesege_vector_flage[index] = true;
				lock.unlock();
				manage_obj->write_system_msg_false();
				lock.lock();
				std::unique_lock<std::mutex>lock_user_list(manage_obj->user_list_mutex);
				std::unique_lock<std::mutex>lock_user_map(manage_obj->user_map_mutex);
				manage_obj->user_map.clear();
				manage_obj->user_list.clear();
				manage_obj->user_list.push_back(std::make_shared<user>());
				manage_obj->user_list[0]->name = "服务器";
				manage_obj->user_list[0]->user_index = SERVER_USER_INDEX;
				for (auto& i : r_m_s.user_list)
				{
					manage_obj->user_list.push_back(std::make_shared<user>(i));
					manage_obj->user_map[i.user_index] = manage_obj->user_list[manage_obj->user_list.size() - 1];
					manage_obj->user_list.back()->read_file_message(manage_obj->user_name + manage_obj->user_passerword);
				}
				lock_user_list.unlock();
				manage_obj->write_user_list(manage_obj->user_name + manage_obj->user_passerword);
			}
			else if (r_m_s.info == ADD_USER)
			{
				manage_obj->new_mesg_flage = true;
				manage_obj->all_new_mesg_flage = true;
				if (r_m_s.add_user_info == ADD_USER_OK)
				{
					manage_obj->add_user_mesg(manage_obj->system_mesege_vector[index]);
					manage_obj->system_mesege_vector_flage[index] = true;
					lock.unlock();
					manage_obj->write_system_msg_false();
					lock.lock();
					std::unique_lock<std::mutex>lock_user_list(manage_obj->user_list_mutex);
					manage_obj->user_list[0]->no_read_message_count++;
					lock_user_list.unlock();
					flage = true;
					manage_obj->no_read_message_count++;
				}
				else if (r_m_s.add_user_info == ADD_USER_ADD)
				{
					std::unique_lock<std::mutex>lock_user_list(manage_obj->user_list_mutex);
					if (manage_obj->user_list.size() >= USER_LIST_MAX)//好友列表已经满了，直接拒绝
					{
						std::string msg = send_message::add_user_mesege(r_m_s.mu_user_name, r_m_s.mu_user_id, r_m_s.yuan_user_name, r_m_s.yuan_user_id, ADD_USER_NOT, "对方的好友列表已满");
						msg = send_message::send_message_to_from_A_to_B(ADD_USER, r_m_s.mu_user_id, r_m_s.yuan_user_id, msg);
						send_message::send_full(manage_obj->get_connect_obj().get_user_socket(), msg);
						continue;
					}
					manage_obj->system_mesege_vector_flage[index] = false;
					manage_obj->no_read_message_count++;
					manage_obj->user_list[0]->no_read_message_count++;
					lock.unlock();
					manage_obj->write_system_msg_false();
					lock.lock();
				}
				else if (r_m_s.add_user_info == ADD_USER_HAVED || r_m_s.add_user_info == ADD_USER_NOT)
				{
					manage_obj->system_mesege_vector_flage[index] = true;
					lock.unlock();
					manage_obj->write_system_msg_false();
					lock.lock();
					flage = true;
				}
				else
				{
					manage_obj->system_mesege_vector_flage[index] = true;
					lock.unlock();
					manage_obj->write_system_msg_false();
					lock.lock();
				}
				//处理添好友行为
			}
			if (flage)
			{
				for (size_t i = 0; i < manage_obj->system_mesege_vector.size(); i++)
				{
					if (i == index || manage_obj->system_mesege_vector_flage[i])
						continue;
					recv_message_set r_m_s_tmp(manage_obj->system_mesege_vector[i], 1);
					if (r_m_s.info != r_m_s_tmp.info)
						continue;
					if (r_m_s.info == LOGIN || r_m_s.info == REGISTER)
					{
						if (r_m_s.yuan_user_name == r_m_s_tmp.yuan_user_name)
						{
							manage_obj->system_mesege_vector_flage[i] = true;
							lock.unlock();
							manage_obj->write_system_msg_false();
							lock.lock();
							break;
						}
					}
					else if (r_m_s.info == ADD_USER && r_m_s.add_user_info == ADD_USER_OK && r_m_s_tmp.info == ADD_USER && r_m_s_tmp.add_user_info == ADD_USER_ADD)
					{
						if (r_m_s.yuan_user_name == r_m_s_tmp.mu_user_name &&
							r_m_s.mu_user_name == r_m_s_tmp.yuan_user_name &&
							r_m_s.yuan_user_id == r_m_s_tmp.mu_user_id &&
							r_m_s.mu_user_id == r_m_s_tmp.yuan_user_id)
						{
							manage_obj->system_mesege_vector_flage[i] = true;
							lock.unlock();
							manage_obj->write_system_msg_false();
							lock.lock();
							break;
						}
					}
					else if ((r_m_s.add_user_info == ADD_USER_HAVED || r_m_s.add_user_info == ADD_USER_NOT) && r_m_s_tmp.info == ADD_USER && r_m_s_tmp.add_user_info == ADD_USER_ADD)
					{
						if (r_m_s.yuan_user_id == r_m_s_tmp.yuan_user_id &&
							r_m_s.yuan_user_name == r_m_s_tmp.yuan_user_name &&
							r_m_s.mu_user_id == r_m_s_tmp.mu_user_id &&
							r_m_s.mu_user_name == r_m_s_tmp.mu_user_name)
						{
							manage_obj->system_mesege_vector_flage[i] = true;
							lock.unlock();
							manage_obj->write_system_msg_false();
							lock.lock();
							break;
						}
					}
				}
			}
		}
		manage_obj->system_mesege_handle_end_thread = true;
		//std::cout << "系统消息处理线程结束了" << std::endl;
	}
	static void mesg_ack_map_re_send(manage* manage_obj)
	{
		while (manage_obj->mesg_ack_map_re_send_start)
		{
			if (!manage_obj->mesg_ack_map_re_send_start)
				break;
			time_t old = time(nullptr);
			std::unique_lock<std::mutex>lock(manage_obj->mesg_ack_mutex);
			for (auto& i : manage_obj->mesg_ack_map)
			{
				if (old - i.second.first < 5 * 60 && old - i.second.first>30)
					send_message::send_full(manage_obj->get_connect_obj().get_user_socket(), i.second.second);
				else
				{
					std::cout << "超时包:" << i.second.second << std::endl;
					manage_obj->mesg_ack_map.erase(i.first);
				}
			}
			lock.unlock();
			Sleep(1000);
		}
		manage_obj->mesg_ack_map_re_send_end = true;
	}
public:
	std::atomic<size_t>msg_id = 0;
	std::map<size_t, std::pair<time_t, std::string>>mesg_ack_map;//确认包机制
	std::mutex mesg_ack_mutex;
	std::atomic<bool>all_new_mesg_flage = false;
	void read_user_list(std::string file_name)
	{
		std::ifstream read(file_name + USER_LIST_FILE, std::ios::binary);
		if (!read.is_open())
			return;
		is_read_user_list_file = true;
		std::unique_lock<std::mutex>lock_user(user_list_mutex);
		std::unique_lock<std::mutex>lock_map(user_map_mutex);
		user_map.clear();
		user_list.clear();
		size_t sz = 0;
		read >> sz;
		for (size_t i = 0; i < sz; i++)//用户列表存入管理中
		{
			user tmp;
			read >> tmp;
			user_list.push_back(std::make_shared<user>(tmp));
			user_list[i]->read_file_message(user_name + user_passerword);
			user_map[tmp.user_index] = user_list[user_list.size() - 1];
		}
		lock_map.unlock();
		lock_user.unlock();
		read.close();
	}
	void write_user_list(std::string file_name)
	{
		std::ofstream write(file_name + USER_LIST_FILE, std::ios::binary);
		if (!write.is_open())
		{
			std::cout << "文件打开失败，请检查文件路径" << std::endl;
			exit(1);
		}
		std::unique_lock<std::mutex>lock(user_list_mutex);
		size_t sz = user_list.size();
		if (sz == 0)
			return;
		write << sz << ' ';
		for (size_t i = 0; i < sz; i++)//用户列表存入文件中
			write << *(user_list[i]) << ' ';
		lock.unlock();
		write.close();
	}
	bool get_is_logind()
	{
		return connect_obj.get_is_logind();
	}
	bool get_returned()
	{
		return returned;
	}
	std::string get_user_name()
	{
		return user_name;
	}
	std::string get_user_passerword()
	{
		return user_passerword;
	}
	bool run()
	{
		while (1)
		{
			connect_obj.set_is_logind_not_logind();//初始化
			returned = false;
			int count = AGAIN_COUNT;
			bool flage = start_obj.start_run(user_name, user_passerword);
			while (connect_obj.get_is_connected() == false)
			{
				count--;
				if (count == 0)//没有连接上服务器端
				{
					char ch = 0;
					std::cout << "没有连接服务器，按c键手动连接服务器" << std::endl;
					ch = _getch();
					if (ch == 'c' || ch == 'C')
					{
						std::cout << "正在重新连接，请稍候" << std::endl;
						if (connect_obj.revoce_connet_server())
							std::cout << "连接成功" << std::endl;
						else
						{
							std::cout << "连接失败,请检查网络环境，正在退出本软件" << std::endl;
							exit(1);
						}
					}
					else
					{
						std::cout << "取消连接，正在退出" << std::endl;
						return false;
					}
				}
				Sleep(1000);
			}
			std::string message;
			read_user_list(user_name + user_passerword);
			if (flage)
				message = user_name + SEPARATEOR + user_passerword + SEPARATEOR + (is_read_user_list_file ? "0" : "1");
			else
				message = user_name + SEPARATEOR + user_passerword;
			if (flage)
			{
				message = send_message::send_message_to_from_A_to_B(LOGIN, SOMEBODY_USER_INDEX, SERVER_USER_INDEX, message);//登录
				std::cout << "正在登录中..." << std::endl;
			}
			else
			{
				message = send_message::send_message_to_from_A_to_B(REGISTER, SOMEBODY_USER_INDEX, SERVER_USER_INDEX, message);//注册
				std::cout << "正在注册中..." << std::endl;
			}
			if (!send_message::send_full(connect_obj.get_user_socket(), message))
			{
				std::cout << "发送消息失败，请检查网络连接" << std::endl;
				continue;
			}
			std::unique_lock<std::mutex>lock(system_mesege_mutex);
			system_mesege_vector.push_back(message);
			system_mesege_vector_flage.push_back(false);
			lock.unlock();
			write_system_msg_false();
			Sleep(3000);//等消息
			count = AGAIN_COUNT;
			while (!(connect_obj.get_is_logind() != connect_obj.get_is_not_logind()) && count--)
			{
				if (flage)
					std::cout << "正在登录中..." << "(第" << AGAIN_COUNT - count + 1 << "次尝试)" << std::endl;
				else
					std::cout << "正在注册中..." << "(第" << AGAIN_COUNT - count + 1 << "次尝试)" << std::endl;
				Sleep(3000);//等消息
			}
			if (connect_obj.get_is_logind())
			{
				std::thread t5(&manage::revoce_connet_server_and_login_san_thread, this);
				t5.detach();
				std::cout << "欢迎回来:" << user_name << std::endl;
				Sleep(1000);
				returned = false;
				return connect_obj.get_is_logind();
			}
			else if (connect_obj.get_is_not_logind())
				std::cout << "输入错误" << std::endl;
			char ch = 0;
			std::cout << "按b键返回登录界面，其余键退出程序" << std::endl;
			ch = _getch();
			if (ch == 'b' || ch == 'B')
			{
				returned = true;
				break;
			}
		}
		return connect_obj.get_is_logind();
	}
	void get_user_list()
	{
		system("cls");
		std::cout << "按下对应标头后，回车查看聊天记录(-2为退出程序,-1为退出登录)你是：" << user_name << " 你的ID是：" << user_index << std::endl;
		std::cout << "共有" << no_read_message_count << "条未读消息" << std::endl;
		std::cout << "用户列表" << std::endl;
		std::unique_lock<std::mutex>lock(user_list_mutex);
		for (size_t i = 0; i < user_list.size(); i++)
			user_list[i]->show(i + 1);
	}
	bool get_user_mesege(int index)
	{
		system("cls");
		std::unique_lock<std::mutex>lock(user_list_mutex);
		if (index < 0 || index >= user_list.size())
		{
			std::cout << "没有当前位置的用户" << std::endl;
			Sleep(1000);
			return false;//超标了，显示不了
		}
		lock.unlock();
		return true;
	}
	manage()
	{
		std::unique_lock<std::mutex>lock(user_list_mutex);
		user_list.reserve(USER_LIST_MAX);
		global_variable_obj->operator_log_obj = &ob;
		std::thread t1(&play_message_music_beijing);
		t1.detach();
		std::thread t2(&manage::recv_or_send_message_input_user_message_queue, this);
		t2.detach();
		std::thread t3(&manage::global_variable_system_input_manage_system, this);
		t3.detach();
		std::thread t4(&manage::system_mesege_handle, this);
		t4.detach();
		std::thread t5(&manage::mesg_ack_map_re_send, this);
		t5.detach();
	}
	~manage()
	{
		if (connect_obj.get_is_logind())
			global_variable_obj->operator_log_obj->queue_operator_log(" user " + user_name + " login out");
		global_variable_obj->is_play_start_message_music_beijing_thread = false;//关掉线程
		while (!global_variable_obj->is_play_end_message_music_beijing_thread)
		{
			if (global_variable_obj->message_semaphore_music_beijing_count < MESSAGE_MUSIC_BEIJING_MAX)
			{
				global_variable_obj->message_semaphore_music_beijing_count++;
				global_variable_obj->message_semaphore_music_beijing.release();//信号变量
			}
			Sleep(20);
		}
		recv_or_send_message_input_user_message_queue_start_thread = false;
		while (!recv_or_send_message_input_user_message_queue_end_thread)
		{
			global_variable_obj->user_mesg_queue_cv.notify_one();
			Sleep(20);
		}
		global_variable_system_input_manage_system_start_thread = false;
		while (!global_variable_system_input_manage_system_end_thread)
		{
			global_variable_obj->system_mesg_queue_cv.notify_one();
			Sleep(20);
		}
		system_mesege_handle_start_thread = false;
		while (!system_mesege_handle_end_thread)
		{
			system_mesege_handle_cv.notify_one();
			Sleep(20);
		}
		revoce_connet_server_and_login_start = false;
		while (!revoce_connet_server_and_login_end)
		{
			connect_obj.is_connected_cs.release();
			Sleep(20);
		}
		mesg_ack_map_re_send_start = false;
		while (!mesg_ack_map_re_send_end)
			Sleep(20);

	}
	bool revoce_connet_server_and_login()
	{
		if (!get_connect_obj().get_is_logind() && get_connect_obj().revoce_connet_server())
		{
			std::string message = user_name + SEPARATEOR + user_passerword + SEPARATEOR + (is_read_user_list_file ? "0" : "1");
			message = send_message::send_message_to_from_A_to_B(LOGIN, SOMEBODY_USER_INDEX, SERVER_USER_INDEX, message);//登录
			bool flage = send_message::send_full(get_connect_obj().get_user_socket(), message);
			std::unique_lock<std::mutex>lock(system_mesege_mutex);
			system_mesege_vector.push_back(message);
			system_mesege_vector_flage.push_back(false);
			lock.unlock();
			write_system_msg_false();
			int count = AGAIN_COUNT;
			while (count-- && !get_connect_obj().get_is_logind())
				Sleep(3000);
			//std::cout << "连接成功，并且已经重新登录" << std::endl;
			return true;
		}
		return false;
	}
	static void revoce_connet_server_and_login_san_thread(manage* manage_obj)
	{
		//std::cout << "自动重连线程开始了" << std::endl;
		while (manage_obj->revoce_connet_server_and_login_start)
		{
			manage_obj->get_connect_obj().is_connected_cs.acquire();
			if (manage_obj->get_connect_obj().get_is_logind())
			{
				Sleep(100);
				continue;
			}
			if (!manage_obj->revoce_connet_server_and_login_start)
				break;
			int count = 0;
			bool flage = false;
			while (count != AGAIN_COUNT && (flage = !manage_obj->revoce_connet_server_and_login()))
				Sleep(count++ * 1000);
			if (flage)
				std::cout << "连接失败，请手动尝试连接，如果不知如何操作，请按?或者help" << std::endl;
		}
		manage_obj->revoce_connet_server_and_login_end = true;
		//std::cout << "自动重连线程结束了" << std::endl;
	}
	void write_system_msg_false()//把收到的系统消息没被处理的，写入文件，以便之后可以使用
	{
		std::ofstream write(user_name + user_passerword + "服务器");
		if (!write.is_open())
		{
			std::cout << "服务器消息文件错误，请重新操作" << std::endl;
			return;
		}
		std::unique_lock<std::mutex>lock(system_mesege_mutex);
		write << system_mesege_vector.size() << std::endl;
		for (int i = 0; i < system_mesege_vector.size(); i++)
		{
			if (system_mesege_vector_flage[i])
				continue;
			write << system_mesege_vector[i] << std::endl;
		}
		write.close();
	}
	void read_system_msg_false()//读取系统消息没被处理的
	{
		std::ifstream read(user_name + user_passerword + "服务器");
		if (!read.is_open())
		{
			std::cout << "服务器消息文件错误，请重新操作" << std::endl;
			return;
		}
		size_t count = 0;
		read >> count;
		std::unique_lock<std::mutex>lock(system_mesege_mutex);
		for (size_t i = 0; i < count; i++)
		{
			std::string mesg;
			std::getline(read, mesg);
			system_mesege_vector.push_back(mesg);
			system_mesege_vector_flage.push_back(false);
		}
		read.close();
	}
	bool is_have_apail_add_user_mesg(std::string& user_name, size_t user_index)//判断是否有申请消息
	{
		std::unique_lock<std::mutex>lock_system_messege(system_mesege_mutex);
		for (size_t index = 0; index < system_mesege_vector.size(); index++)
		{
			if (system_mesege_vector_flage[index])
				continue;
			recv_message_set r_m_s(system_mesege_vector[index], 1);
			if (r_m_s.info == ADD_USER && r_m_s.add_user_info == ADD_USER_ADD && r_m_s.yuan_user_id == user_index && r_m_s.yuan_user_name == user_name)
			{
				system_mesege_vector_flage[index] = true;
				lock_system_messege.unlock();
				write_system_msg_false();
				return true;
			}
		}
		return false;
	}
	bool is_have_user_to_friend_list(std::string& user_name, size_t user_index)//判断是否有该好友
	{
		std::unique_lock<std::mutex>lock_user_list(user_list_mutex);
		for (auto& i : user_list)
			if (i->name == user_name && i->user_index == user_index)
				return true;
		return false;
	}
	bool is_haved_apail_add_user_not_login(std::string& user_name, size_t user_index)//判断是否被重复申请，且没被处理的
	{
		std::unique_lock<std::mutex>lock_system_messege(system_mesege_mutex);
		for (size_t index = 0; index < system_mesege_vector.size(); index++)
		{
			if (system_mesege_vector_flage[index])
				continue;
			recv_message_set r_m_s(system_mesege_vector[index], 1);
			if (r_m_s.info == ADD_USER && r_m_s.add_user_info == ADD_USER_ADD && r_m_s.mu_user_id == user_index && r_m_s.mu_user_name == user_name)
				return true;
		}
		return false;
	}
	void system_mesg_operation()//服务器的操作，目前只有添加好友，后期有就增加
	{
		std::string info[5] = { ADD_USER_ADD,ADD_USER_DEL,ADD_USER_OK,ADD_USER_CANCEL };
		bool flage = false;
		int index;
		int flag = 0;
		while (1)
		{
			get_system_mesege();
			ui::system_mesg_operation_ui();
			std::unique_lock<std::mutex>lock_user_list(user_list_mutex);
			no_read_message_count -= user_list[0]->no_read_message_count;
			if (no_read_message_count < 0)
				no_read_message_count = 0;
			user_list[0]->no_read_message_count = 0;
			lock_user_list.unlock();
			flag = input_mesg::input(index, &new_mesg_flage);
			switch (flag)
			{
			case 1:revoce_connet_server_and_login(); continue;
			case 2:return;
			case 3:continue;
			case 4:write_user_list(user_name + user_passerword); continue;
			case 5:have_user_list(); continue;
			case 6:std::cout << "无法操作当前行为" << std::endl; continue;
			}
			if (flag != 0)
				continue;
			if (index < 1 || index > 4)
			{
				std::cout << "输入错误，请重新输入" << std::endl;
				Sleep(1000);
				continue;
			}
			index--;
			break;
		}
		std::wstring mu_user_name_w;
		std::string mu_user_name;
		size_t mu_user_index;
		std::string mesg;
		while (1)
		{
			int info = ui::get_user_nume_index_ui(mu_user_name_w, mu_user_index, &new_mesg_flage);
			switch (flag)
			{
			case 1:revoce_connet_server_and_login(); break;
			case 2:return;
			case 3:continue; break;
			case 4:write_user_list(user_name + user_passerword); break;
			case 5:have_user_list(); break;
			}
			if (info != 0)
				continue;
			if (!start_obj.is_num_or_english(mu_user_name_w))
			{
				std::cout << "用户名错误，用户名只支持数字和英文字符" << std::endl;
				Sleep(1000);
				continue;
			}
			if (mu_user_index == 0)
			{
				std::cout << "格式错误" << std::endl;
				Sleep(1000);
				return;
			}
			mu_user_name = std::string(mu_user_name_w.begin(), mu_user_name_w.end());
			if (user_name == mu_user_name || user_index == mu_user_index)
			{
				std::cout << "目标不能是自己" << std::endl;
				Sleep(1000);
				return;
			}
			break;
		}
		std::string beizhu_info;
		std::unique_lock<std::mutex>lock_user_list(user_list_mutex);
		if (index == 0 && user_list.size() >= USER_LIST_MAX - 1)
		{
			std::cout << "你的好友太多了，没法再添加了" << std::endl;
			Sleep(1000);
			return;
		}
		lock_user_list.unlock();
		switch (index)
		{
		case 0:
			operation_user_info(beizhu_info, "添加", "我是" + user_name + "，我的id是" + std::to_string(user_index));
			if (is_haved_apail_add_user_not_login(mu_user_name, mu_user_index))
			{
				std::cout << "重复申请好友，且还未被处理，请不要重复操作" << std::endl;
				Sleep(1000);
				return;
			}
			if (is_have_user_to_friend_list(mu_user_name, mu_user_index))
			{
				std::cout << "你已经添加该好友，不能再次添加，如果本地不存在，请向服务器重新拉取好友数据" << std::endl;
				Sleep(1000);
				return;
			}
			if (is_have_apail_add_user_mesg(mu_user_name, mu_user_index))
				index = 2;
			break;
		case 1:
			operation_user_info(beizhu_info, "删除", "我的好友数量已达上限，但我需要好友位，暂时删掉，谢谢");
			if (!is_have_user_to_friend_list(mu_user_name, mu_user_index))
			{
				std::cout << "你没有添加该好友，不能删除该好友，如果本地不能存在，请向服务器重新拉取好友数据" << std::endl;
				Sleep(1000);
				return;
			}
			break;
		case 2:
			operation_user_info(beizhu_info, "同意", "我是" + user_name + "，我的id是" + std::to_string(user_index));
			if (is_have_user_to_friend_list(mu_user_name, mu_user_index))
			{
				std::cout << "该操作错误，本地已有该好友数据，如果无法操作，请向服务器重新拉取好友数据" << std::endl;
				Sleep(1000);
				return;
			}
			if (!is_have_apail_add_user_mesg(mu_user_name, mu_user_index))
			{
				std::cout << "没有该用户的申请记录，请先进行添加" << std::endl;
				Sleep(1000);
				return;
			}
			break;
		case 3:
			operation_user_info(beizhu_info, "拒绝", "我不认识你，请你指明姓名");
			if (!is_have_apail_add_user_mesg(mu_user_name, mu_user_index))
			{
				std::cout << "没有该用户的申请记录，请先进行添加" << std::endl;
				Sleep(1000);
				return;
			}
			break;
		}
		mesg = send_message::add_user_mesege(user_name, user_index, mu_user_name, mu_user_index, info[index], beizhu_info);
		mesg = send_message::send_message_to_from_A_to_B(ADD_USER, user_index, mu_user_index, mesg);
		flage = send_message::send_full(connect_obj.get_user_socket(), mesg);
		if (!flage)
		{
			std::cout << "发送消息失败，请检查网络连接" << std::endl;
			return;
		}
		if (index == 1)
		{
			std::unique_lock<std::mutex>lock_user_friend_list(user_list_mutex);
			std::unique_lock<std::mutex>lock_user_map(user_map_mutex);
			for (auto i = user_list.begin(); i != user_list.end(); i++)
				if ((*i)->name == mu_user_name && (*i)->user_index == mu_user_index)
				{
					user_list.erase(i);
					user_map.erase(mu_user_index);
					break;
				}
		}
		else if (index == 2)
		{
			user tmp;
			tmp.name = mu_user_name;
			tmp.user_index = mu_user_index;
			std::unique_lock<std::mutex>lock_user_friend_list(user_list_mutex);
			std::unique_lock<std::mutex>lock_user_friend_map(user_map_mutex);
			user_list.push_back(std::make_shared<user>(tmp));
			user_list.back()->read_file_message(user_name + user_passerword);
			user_map[mu_user_index] = user_list[user_list.size() - 1];
			lock_user_friend_map.unlock();
			lock_user_friend_list.unlock();
			write_user_list(user_name + user_passerword);
		}
		std::unique_lock<std::mutex>lock(global_variable_obj->system_mesg_queue_mutex);
		system_mesege_vector.push_back(mesg);
		if (index == 0)
			system_mesege_vector_flage.push_back(false);
		else
			system_mesege_vector_flage.push_back(true);
		lock.unlock();
		write_system_msg_false();
	}
	void operation_user_info(std::string& beizhu_info, const std::string type_info, const std::string default_info)
	{
		while (1)
		{
			std::cout << "输入" << type_info << "好友的备注信息，不输按回车即可，没有则按空格加回车(默认是:" << default_info << ")" << std::endl;
			int flag = input_mesg::input(beizhu_info, nullptr);
			switch (flag)
			{
			case 1:revoce_connet_server_and_login(); continue;
			case 2:return;
			case 3:continue;
			case 4:write_user_list(user_name + user_passerword); continue;
			case 5:have_user_list(); continue;
			case 6:std::cout << "无法操作当前行为" << std::endl; continue;
			}
			if (flag != 0)
				continue;
			if (beizhu_info.empty())
				beizhu_info = default_info;
			break;
		}
	}
	void user_mesg_operation(int index)
	{
		bool type = true;
		while (1)
		{
			std::unique_lock<std::mutex>lock_user_list(user_list_mutex);
			no_read_message_count -= user_list[index]->no_read_message_count;
			if (no_read_message_count < 0)
				no_read_message_count = 0;
			user_list[index]->show_message(user_index, type);
			user_list[index]->no_read_message_count = 0;
			lock_user_list.unlock();
			std::cout << "请输入消息（?/help寻求别的帮助）" << std::endl;
			std::string mesg;
			size_t mg_id = msg_id++;
			int flag = input_mesg::input(mesg, &user_list[index]->new_mesg_flage);
			if (mesg.empty() && flag == 0)
			{
				std::cout << "不能发送空消息" << std::endl;
				Sleep(200);
				flag = 3;
			}
			switch (flag)
			{
			case 1:revoce_connet_server_and_login(); break;
			case 2:return;
			case 3:continue; break;
			case 4:write_user_list(user_name + user_passerword); break;
			case 5:have_user_list(); break;
			case 6:type = false, user_list[index]->read_file_message(user_name + user_passerword); continue; break;
			}
			if (flag != 0)
				continue;
			mesg = std::to_string(mg_id) + SEPARATEOR + mesg;
			mesg = send_message::send_message_to_from_A_to_B(MSG, user_index, user_list[index]->user_index, mesg);
			std::unique_lock<std::mutex>lock(mesg_ack_mutex);
			mesg_ack_map[mg_id] = std::pair<time_t, std::string>(time(nullptr), mesg);
			lock.unlock();
			if (!send_message::send_full(connect_obj.get_user_socket(), mesg))
			{
				lock.lock();
				mesg_ack_map.erase(mg_id);
				lock.unlock();
				std::cout << "发送失败，请检查网络问题" << std::endl;
				Sleep(1000);
				continue;
			}
			std::unique_lock<std::mutex>lock_msg(user_list[index]->message_mutex);
			user_list[index]->message_queue.push_back(mesg);
			user_list[index]->write_one_message(user_name + user_passerword, mesg);
		}
		//需要处理转发操作
	}
	void get_system_mesege()
	{
		system("cls");
		std::cout << "你与服务器的消息记录：" << std::endl;
		std::unique_lock<std::mutex>lock(global_variable_obj->system_mesg_queue_mutex);
		for (size_t i = 0; i < system_mesege_vector.size(); i++)
		{
			std::cout << i + 1 << '.' << system_mesege_vector[i];
			if (system_mesege_vector_flage[i])
				std::cout << "(已处理)";
			else
				std::cout << "(未处理)";
			std::cout << std::endl;
		}
		std::cout << "请输入消息（?/help寻求别的帮助）" << std::endl;
	}
	Connect& get_connect_obj()
	{
		return connect_obj;
	}
	void  have_user_list()
	{
		std::string msg = MSG_HAVE_USER_MSG;
		msg = send_message::send_message_to_from_A_to_B(MSG_HAVE_USER, user_index, SERVER_USER_INDEX, msg);
		if (!send_message::send_full(connect_obj.get_user_socket(), msg))
			std::cout << "拉取好友数据失败,请检查网络连接" << std::endl;
		else
		{
			std::cout << "正在拉取好友数据" << std::endl;
			Sleep(2000);//等待处理完毕
		}
	}
	void unlogin()//退出登录
	{
		std::string msg = LOGIN_OUT;
		msg = send_message::send_message_to_from_A_to_B(LOGIN, user_index, SERVER_USER_INDEX, msg);
		if (!send_message::send_full(connect_obj.get_user_socket(), msg))
		{
			std::cout << "退出登录失败,请检查网络连接" << std::endl;
			return;
		}
		std::cout << "已经退出登录" << std::endl;
		connect_obj.set_is_logind(false);
		revoce_connet_server_and_login_start = false;
		while (!revoce_connet_server_and_login_end)
		{
			connect_obj.is_connected_cs.release();
			Sleep(20);
		}
		revoce_connet_server_and_login_start = true;
		revoce_connet_server_and_login_end = false;
		if (connect_obj.get_user_socket() != INVALID_SOCKET)
		{
			closesocket(connect_obj.get_user_socket());
			connect_obj.get_user_socket() = INVALID_SOCKET;
		}
		bool flage = connect_obj.revoce_connet_server();
		if (flage == false)
		{
			std::cout << "重连失败，请检查网络连接" << std::endl;
			connect_obj.set_is_connect(false);
			return;
		}
	}
};
int main()
{
	Global_variable global_variable_obj_tmp;
	global_variable_obj = &global_variable_obj_tmp;
	manage m;
	while (1)
	{
		if (m.get_returned())
			break;
		while (m.get_is_logind() == false)
			m.run();
		m.get_user_list();
		int user_index = 0;
		int flag = input_mesg::input(user_index, &m.all_new_mesg_flage);
		switch (flag)
		{
		case 1:m.revoce_connet_server_and_login(); continue;
		case 2:std::cout << "这就是主页，无法再次后退" << std::endl; continue;
		case 3:continue;
		case 4:m.write_user_list(m.get_user_name() + m.get_user_passerword()); continue;
		case 5:m.have_user_list(); continue;
		case 6:std::cout << "无法操作当前行为" << std::endl; continue;
		}
		if (flag != 0)
			continue;
		switch (user_index--)
		{
		case -2:
			global_variable_obj_tmp.operator_log_obj->queue_operator_log(" user: " + m.get_user_name() + " login out");
			return 1;
		case -1:
			//退出登录操作
			global_variable_obj_tmp.operator_log_obj->queue_operator_log(" user: " + m.get_user_name() + " login out");
			m.unlogin();
			Sleep(1000);
			break;
		case 1:
			m.system_mesg_operation();
			break;
		default:
			if (m.get_user_mesege(user_index))
				m.user_mesg_operation(user_index);
			break;
		}
	}
	return 0;
}