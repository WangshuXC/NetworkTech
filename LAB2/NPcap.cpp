#include "pcap.h"
#include<iostream>
#include<WinSock2.h>
#include<iomanip>
#include<cstring>
#include<format>

#pragma comment(lib,"wpcap.lib")
#pragma comment(lib,"Packet.lib")
#pragma comment(lib,"WS2_32.lib")
using namespace std;

#pragma pack(1)
typedef struct FrameHeader_t {
	BYTE DesMAC[6];		// 目标MAC地址
	BYTE SrcMAC[6];		// 源MAC地址
	WORD FrameType;		// 帧类型
} FrameHeader_t;

typedef struct IPHeader_t {
	BYTE Ver_HLen;		// 版本和首部长度
	BYTE TOS;			// 服务类型
	WORD TotalLen;		// 总长度
	WORD ID;			// 标识符
	WORD Flag_Segment;	// 标识和分段偏移
	BYTE TTL;			// 存活时间
	BYTE Protocol;		// 协议类型
	WORD Checksum;		// 首部校验和
	ULONG SrcIP;		// 源IP地址
	ULONG DstIP;		// 目的IP地址
} IPHeader_t;

typedef struct Data_t {
	FrameHeader_t FrameHeader;
	IPHeader_t IPHeader;
}Data_t;

int main() {
	Data_t* IPPacket;				// 数据包
	pcap_if_t* alldevs;				// 所有设备
	pcap_if_t* d;					// 选择的设备
	int inum;						// 设备总数
	int i = 0;
	pcap_t* adhandle;
	struct pcap_pkthdr* header;		// 数据包头部信息
	const u_char* pkt_data;			// 数据包
	char errbuf[PCAP_ERRBUF_SIZE];	// 错误缓冲区
	

	if (pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		cout << "error in findalldevs: " << errbuf << endl;
		return 0;
	}
	for (d = alldevs; d; d = d->next)
	{
		cout << ++i << ". " << d->name << endl;
		if (d->description)
			cout << d->description << endl;
	}

	cout << "选择设备：" << endl;
	cin >> inum;
	for (d = alldevs, i = 0; i < inum - 1; d = d->next, i++);

	adhandle = pcap_open_live(d->name, 65536, 1, 1000, errbuf);

	pcap_freealldevs(alldevs);
	int res;
	while ((res = pcap_next_ex(adhandle, &header, &pkt_data)) >= 0)
	{
		if (res == 0)
			continue;

		IPPacket = (Data_t*)pkt_data;
		BYTE* desMac = IPPacket->FrameHeader.DesMAC;
		BYTE* srcMac = IPPacket->FrameHeader.SrcMAC;
		string DesMAC = format("desMAC: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", desMac[0], desMac[1], desMac[2], desMac[3], desMac[4], desMac[5]);
		string SrcMAC = format("srcMAC: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", srcMac[0], srcMac[1], srcMac[2], srcMac[3], srcMac[4], srcMac[5]);
		cout << SrcMAC << "\t" << DesMAC
			<< "\ttype: 0x" << hex << ntohs(IPPacket->FrameHeader.FrameType)
			<< "\t\tlen: " << dec << ntohs(IPPacket->IPHeader.TotalLen)
			<< "caplen: " << header->caplen << endl;
	}

	pcap_close(adhandle);
}

