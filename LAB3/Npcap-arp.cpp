#include <Winsock2.h>
#include<Windows.h>
#include<iostream>
#include <ws2tcpip.h>
#include "pcap.h"
#include "stdio.h"
#include<time.h>
#include <cstring>
#pragma comment(lib, "Packet.lib")
#pragma comment(lib,"wpcap.lib")
#pragma comment(lib,"ws2_32.lib")//表示链接的时侯找ws2_32.lib
#pragma warning( disable : 4996 )//要使用旧函数
#define _WINSOCK_DEPRECATED_NO_WARNINGS
using namespace std;

void printMAC(BYTE MAC[6])
{
	for (int i = 0; i < 6; i++)
	{
		if (i < 5)
			printf("%02x:", MAC[i]);
		else
			printf("%02x", MAC[i]);
	}

};
void printIP(DWORD IP)
{
	BYTE* p = (BYTE*)&IP;
	for (int i = 0; i < 3; i++)
	{
		cout << dec << (int)*p << ".";
		p++;
	}
	cout << dec << (int)*p;
};
#pragma pack(1)
struct FrameHeader_t //帧首部
{
	BYTE DesMAC[6];  //目的地址
	BYTE SrcMAC[6];  //源地址
	WORD FrameType;  //帧类型
};

struct ARPFrame_t               //ARP帧
{
	FrameHeader_t FrameHeader;
	WORD HardwareType;
	WORD ProtocolType;
	BYTE HLen;
	BYTE PLen;
	WORD Operation;
	BYTE SendHa[6];
	DWORD SendIP;
	BYTE RecvHa[6];
	DWORD RecvIP;
};


void sendARPPacket(pcap_t* pcap_handle, const std::string& srcIP, const BYTE srcMAC[6], const std::string& targetIP, BYTE targetMAC[6]) {
	ARPFrame_t ARPRequest;
	ARPFrame_t* ARPReply;
	struct pcap_pkthdr* pkt_header;
	const u_char* pkt_data;
	char errbuf[PCAP_ERRBUF_SIZE] = {};

	// 组装ARP请求帧
	for (int i = 0; i < 6; i++) {
		ARPRequest.FrameHeader.DesMAC[i] = 0xFF;  // 广播地址
		ARPRequest.FrameHeader.SrcMAC[i] = srcMAC[i];
		ARPRequest.RecvHa[i] = 0;  // 目标MAC（尚未知）
		ARPRequest.SendHa[i] = srcMAC[i];
	}
	ARPRequest.FrameHeader.FrameType = htons(0x0806);  // ARP帧
	ARPRequest.HardwareType = htons(0x0001);  // 以太网
	ARPRequest.ProtocolType = htons(0x0800);  // IP
	ARPRequest.HLen = 6;
	ARPRequest.PLen = 4;
	ARPRequest.Operation = htons(0x0001);  // ARP请求
	DWORD SendIP = ARPRequest.SendIP = inet_addr(srcIP.c_str());
	DWORD TargetIP = ARPRequest.RecvIP = inet_addr(targetIP.c_str());

	// 发送ARP请求
	if (pcap_sendpacket(pcap_handle, (u_char*)&ARPRequest, sizeof(ARPFrame_t)) != -1) {
		cout << "ARP请求发送成功" << endl;
	}

	// 接收ARP回复
	while (true) {
		if (pcap_next_ex(pcap_handle, &pkt_header, &pkt_data) != -1) {
			ARPReply = (ARPFrame_t*)pkt_data;
			if (ARPReply->RecvIP == SendIP && ARPReply->SendIP == TargetIP && ARPReply->Operation == htons(0x0002)) {
				cout << "成功捕获到ARP并解析" << endl;
				printIP(ARPReply->SendIP);
				cout << " -> ";
				printMAC(ARPReply->SendHa);
				cout << endl;

				// 将获取到的目标MAC地址拷贝到传入的 targetMAC
				memcpy(targetMAC, ARPReply->SendHa, 6);
				return;
			}
		}
		else {
			cout << "捕获数据包时发生错误：" << errbuf << endl;
			return;
		}
	}
}

int main()
{
	pcap_if_t* alldevs;//指向设备列表首部的指针
	char errbuf[PCAP_ERRBUF_SIZE];//错误信息缓冲区

	int index = 0;
	const int maxInterfaces = 100;
	pcap_if_t* interfaces[maxInterfaces] = {};

	string CheatIP = "112.112.112.112";
	string LocalIP = {};
	string TargetIP = {};

	BYTE cheatMAC[6] = { 0x66,0x66,0x66,0x66,0x66,0x66 };
	BYTE LocalMAC[6] = {};
	BYTE TargetMAC[6] = {};

	// 获得本机的设备列表
	if (pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		cerr << "获取网络接口时发生错误：" << errbuf << endl;
		return 1;
	}

	// 显示接口列表
	for (pcap_if_t* ptr = alldevs; ptr != nullptr; ptr = ptr->next)
	{
		for (pcap_addr_t* a = ptr->addresses; a != nullptr; a = a->next)
		{
			if (a->addr->sa_family == AF_INET)
			{
				interfaces[index] = ptr;
				cout << index + 1 << ". " << ptr->description << endl;
				cout << "IP地址：" << inet_ntoa(((struct sockaddr_in*)(a->addr))->sin_addr) << endl << endl;
				index++;
				break;
			}
		}
	}

	int num;
	cout << "请选要打开的网络适配器：";
	cin >> num;

	if (num <= 0 || num > index) {
		cerr << "无效序号" << endl;
		return 1;
	}

	pcap_t* pcap_handle = pcap_open(interfaces[num - 1]->name, 1024, PCAP_OPENFLAG_PROMISCUOUS, 1000, nullptr, errbuf);  // 打开选中的网络适配器
	if (pcap_handle == nullptr)
	{
		cerr << "打开网络适配器时发生错误：" << errbuf << endl;
		return 1;
	}
	else
	{
		cout << "成功打开" << interfaces[num - 1]->description << endl << endl;
	}

	

	//编译过滤器，只捕获ARP包
	u_int netmask;
	netmask = ((sockaddr_in*)(interfaces[num - 1]->addresses->netmask))->sin_addr.S_un.S_addr;
	bpf_program fcode;
	char packet_filter[] = "ether proto \\arp";
	if (pcap_compile(pcap_handle, &fcode, packet_filter, 1, netmask) < 0)
	{
		cout << "无法编译数据包过滤器。检查语法";
		pcap_freealldevs(alldevs);
		return 0;
	}
	//设置过滤器
	if (pcap_setfilter(pcap_handle, &fcode) < 0)
	{
		cout << "过滤器设置错误";
		pcap_freealldevs(alldevs);
		return 0;
	}

	LocalIP = inet_ntoa(((struct sockaddr_in*)(interfaces[num - 1]->addresses->addr))->sin_addr);
	sendARPPacket(pcap_handle, CheatIP, cheatMAC, LocalIP, LocalMAC);

	while (true) {
		cout << endl << "请输入请求的IP地址:";
		cin >> TargetIP;
		sendARPPacket(pcap_handle, LocalIP, LocalMAC, TargetIP, TargetMAC);
	}

	pcap_freealldevs(alldevs);
	pcap_close(pcap_handle);
}