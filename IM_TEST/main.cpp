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
using namespace std;
class user
{
public:
	size_t user_index;//用户的服务器中唯一编码
	std::string name;//用户的名称
	std::mutex message_mutex;
	std::list<std::string> message_queue;//与该用户的消息队列
	size_t no_read_message_count = 0;//未读消息的数量
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
	void read_file_message(std::string file_name)//全量读取
	{
		std::unique_lock<std::mutex>lock(message_mutex);
		std::ifstream read(file_name + name + std::to_string(user_index));
		if (!read.is_open())
			return;
		std::string line;
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
//取消内存对齐
#pragma pack(push, 1)
struct RoutePocketHeader
{
	uint16_t type;
	uint64_t from_id;
	uint64_t to_id;
};
#pragma pack(pop)
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
class Connect
{
public:
	SOCKET user_socket = INVALID_SOCKET;
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
		while ((((user_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)) && count--)//创建套接字,TCP协议
		{
			Sleep(1000);
			std::cout << "创建套接字失败，正在第" << AGAIN_COUNT - count << "次尝试" << std::endl;
		}
		if (count == 0 && user_socket == INVALID_SOCKET)
			return false;
		return true;//&&set_socket_ioctlsocket();
	}
	Connect()
	{
		if (!win_init())//恶性错误，初始化失败
		{
			std::cout << "初始化失败，请检查网络连接" << std::endl;
			exit(1);
		}
		creat_socket();
		Connect_server();
	}
	~Connect()
	{
		if (user_socket != INVALID_SOCKET)
		{
			closesocket(user_socket);
			user_socket = INVALID_SOCKET;
		}
		win_end();//释放
	}
	bool Connect_server()
	{
		sockaddr_in server = { 0 };
		server.sin_family = AF_INET;
		server.sin_port = htons(SERVER_PORT);
		server.sin_addr.s_addr = inet_addr(SERVER_IP);
		int count = AGAIN_COUNT;//重连次数
		while (count--)
		{
			if (connect(user_socket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
			{
				Sleep(1000);
				continue;
			}
			return true;
		}
		return false;
	}
};
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
void login_user(int total)
{
	size_t user_name = 10000000;
	vector<Connect> user(total);
	int success = 0;
	auto start = chrono::high_resolution_clock::now();
	for (int start_index = 0; start_index < total; start_index++)
	{
		string message;
		message = to_string(user_name) + SEPARATEOR + to_string(user_name) + SEPARATEOR + "0";
		message = send_message::send_message_to_from_A_to_B(LOGIN, SOMEBODY_USER_INDEX, SERVER_USER_INDEX, message);
		if (!send_message::send_full(user[start_index].user_socket, message))
			cout << "发送失败\n" << endl;
		user_name++;
	}
	for (int start_index = 0; start_index < total; start_index++)
	{
		string msg;
		if (recv_message_set::recv_full(user[start_index].user_socket, msg))
		{
			if (msg.find(LOGIN_SUCCESS) != string::npos)
				success++;
		}
	}
	auto end = chrono::high_resolution_clock::now();
	auto cost = chrono::duration_cast<chrono::milliseconds>(end - start).count();
	cout << endl;
	cout << "==========================" << endl;
	cout << "总请求: " << total << endl;
	cout << "成功: " << success << endl;
	cout << "失败: " << total - success << endl;
	cout << "耗时(ms): " << cost << endl;
	double qps = total * 1000.0 / cost;
	cout << "QPS: " << qps << endl;
	cout << "==========================" << endl;
}
void register_user(int total)
{
	size_t user_name = 10000000;
	vector<Connect> user(total);
	int success = 0;
	auto start = chrono::high_resolution_clock::now();
	for (int start_index = 0; start_index < total; start_index++)
	{
		string message;
		message = to_string(user_name) + SEPARATEOR + to_string(user_name);
		message = send_message::send_message_to_from_A_to_B(REGISTER, SOMEBODY_USER_INDEX, SERVER_USER_INDEX, message);
		if (!send_message::send_full(user[start_index].user_socket, message))
			cout << "发送失败\n" << endl;
		user_name++;
	}
	for (int start_index = 0; start_index < total; start_index++)
	{
		string msg;
		if (recv_message_set::recv_full(user[start_index].user_socket, msg))
		{
			if (msg.find(REGISTER_SUCCESS) != string::npos)
				success++;
		}
	}
	auto end = chrono::high_resolution_clock::now();
	auto cost = chrono::duration_cast<chrono::milliseconds>(end - start).count();
	cout << endl;
	cout << "==========================" << endl;
	cout << "总请求: " << total << endl;
	cout << "成功: " << success << endl;
	cout << "失败: " << total - success << endl;
	cout << "耗时(ms): " << cost << endl;
	double qps = total * 1000.0 / cost;
	cout << "QPS: " << qps << endl;
	cout << "==========================" << endl;
}
void send_ans_recv_msg_one(int total, int count)
{
	size_t user_name = 10000000;
	vector<Connect> user(total);
	vector<pair<SOCKET, size_t>> member(total);
	int success = 0;
	for (int i = 0; i < total; i++)
	{
		string message;
		message = to_string(user_name) + SEPARATEOR + to_string(user_name);
		message = send_message::send_message_to_from_A_to_B(REGISTER, SOMEBODY_USER_INDEX, SERVER_USER_INDEX, message);
		if (!send_message::send_full(user[i].user_socket, message))
			cout << "发送失败\n";
		user_name++;
	}
	for (int i = 0; i < total; i++)
	{
		string msg;
		if (recv_message_set::recv_full(user[i].user_socket, msg))
		{
			recv_message_set r_m_s(msg, 1);
			member[i] = pair<SOCKET, size_t>(user[i].user_socket, r_m_s.mu_user_id);
		}
	}
	auto start = chrono::high_resolution_clock::now();
	for (int i = 0; i < total; i++)
	{
		for (int j = 0; j < count; j++)
		{
			string message = to_string(i + 10000 + j) + SEPARATEOR + "你好";
			message = send_message::send_message_to_from_A_to_B(MSG, member[i].second, member[total - 1 - i].second, message);
			if (!send_message::send_full(member[i].first, message))
				cout << "发送失败\n";
		}
	}
	for (int i = 0; i < total; i++)
	{
		for (int j = 0; j < count; j++)
		{
			string msg;
			if (!recv_message_set::recv_full(member[i].first, msg))
			{
				cout << "接收失败\n";
				continue;
			}
			recv_message_set r_m_s(msg, 1);
			if (r_m_s.end_message_string == "你好")
				success++;
		}
	}
	auto end = chrono::high_resolution_clock::now();
	total *= count;
	auto cost = chrono::duration_cast<chrono::milliseconds>(end - start).count();
	cout << endl;
	cout << "==========================" << endl;
	cout << "总请求: " << total << endl;
	cout << "成功: " << success << endl;
	cout << "失败: " << total - success << endl;
	cout << "耗时(ms): " << cost << endl;
	double qps = total * 1000.0 / cost;
	cout << "QPS: " << qps << endl;
	cout << "==========================" << endl;
}
int main()
{
	int total = 12 * 512;
	//login_user(total);
	//register_user(total);
	send_ans_recv_msg_one(total, 100);
	return 0;
}