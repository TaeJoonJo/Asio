
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <thread>
#include <vector>
#include <array>
#include <atomic>
#include <chrono>
#include <WinSock2.h>
#include "../../Protocols.h"

#pragma comment (lib, "ws2_32.lib")

constexpr uint16_t _MAX_DUMMY = 10;
constexpr uint32_t _MAX_WORKERTHREAD = 8;

HANDLE _IOCP;

enum class EIOType {
	send = 0,
	recv
};

struct IOContext {
	WSAOVERLAPPED overlapped_;
	WSABUF wsabuf_;
	uint8_t buf_[_MAX_BUFFER];
	EIOType iotype_;
};

class CSession {
public:
	CSession() = default;
	~CSession() = default;

public:
	volatile bool isrun_;
	IOContext ioContex_;
	uint16_t id_;
	SOCKET socket_;
	uint8_t buf_[_MAX_BUFFER];
	std::chrono::system_clock::time_point lastChatTime_;
};

std::array<CSession, _MAX_DUMMY> _Dummys;

void SendPacket(void* ppacket, CSession* psession) {
	uint8_t* pbuf = reinterpret_cast<uint8_t*>(ppacket);
	
	IOContext* pcontext = new IOContext{};
	pcontext->iotype_ = EIOType::send;
	memcpy(pcontext->buf_, ppacket, pbuf[0]);
	pcontext->wsabuf_.buf = reinterpret_cast<char*>(pcontext->buf_);
	pcontext->wsabuf_.len = pbuf[0];
	WSASend(psession->socket_, &pcontext->wsabuf_, 1, NULL, 0, &pcontext->overlapped_, NULL);
}

void WorkerThread() {
	while (true) {
		IOContext* pcontext = nullptr;
		DWORD ioSize{};
		CSession* psession = nullptr;

		bool ret = GetQueuedCompletionStatus(_IOCP, &ioSize, reinterpret_cast<PULONG_PTR>(&psession), reinterpret_cast<LPOVERLAPPED*>(&pcontext), INFINITE);
		/*if (psession == nullptr) {
			continue;
		}*/
		if (ret == false || 0 == ioSize) {
			psession->isrun_ = false;
			closesocket(psession->socket_);
		}
		

		if (EIOType::recv == pcontext->iotype_) {
			uint8_t* pbuf = psession->buf_;

			EPacketType packetType = static_cast<EPacketType>(pbuf[1]);

			switch (packetType) {
			case EPacketType::id: {
				PACKET_ID* ppacket = reinterpret_cast<PACKET_ID*>(pbuf);
				psession->id_ = ppacket->id_;
				std::cout << "ID [ " << ppacket->id_ << " ] Player Join\n";
			} break;
			case EPacketType::simplechat: {
				PACKET_SIMPLE* ppacket = reinterpret_cast<PACKET_SIMPLE*>(pbuf);
				std::cout << "ID [ " << ppacket->id_ << " ] : ' " << ppacket->c_ << " ]\n";
			}
			}
		}
		else {
			delete pcontext;
		}
	}
}

int main()
{
#pragma region InitNetwork
	WSADATA wsadata;

	if (0 != WSAStartup(MAKEWORD(2, 2), &wsadata)) {
		exit(1);
	}

	_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, NULL, 0);
#pragma endregion

#pragma region InitDummys
	for (auto& dummy : _Dummys) {
		dummy.socket_ = WSASocketW(PF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		dummy.isrun_ = false;
		int ran = rand() % _MAX_DUMMY;
		dummy.lastChatTime_ = std::chrono::system_clock::now() + std::chrono::seconds(ran);
	}
#pragma endregion

	std::vector<std::thread> vecThreads;

	for (int i = 0; i < _MAX_WORKERTHREAD - 1; ++i) {
		vecThreads.push_back(std::thread(WorkerThread));
	}
	auto TestThread = []() {
		while (true) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			for (auto& dummy : _Dummys) {
				if (false == dummy.isrun_) continue;
				if (std::chrono::system_clock::now() - dummy.lastChatTime_ < std::chrono::seconds(10)) continue;
				PACKET_SIMPLE packet{sizeof(PACKET_SIMPLE), EPacketType::simplechat, dummy.id_, rand() % 256};
				SendPacket(&packet, &dummy);
				dummy.lastChatTime_ = std::chrono::system_clock::now();
				break;
			}
		}
	};
	vecThreads.push_back(std::thread(TestThread));

	auto AdjustDummy = []() {
		while (true) {
			Sleep(5000);
			for (auto& dummy : _Dummys) {
			//for(int i = 0 ; i < _MAX_DUMMY; ++i){
				if (true == dummy.isrun_) continue;
				
				dummy.socket_ = WSASocketW(PF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

				SOCKADDR_IN serverAddr{};
				serverAddr.sin_family = AF_INET;
				serverAddr.sin_port = htons(_SERVER_PORT);
				serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	
				if (0 != WSAConnect(dummy.socket_, (sockaddr*)&serverAddr, sizeof(serverAddr), NULL, NULL, NULL, NULL))
					continue;
				dummy.ioContex_.iotype_ = EIOType::recv;
				dummy.ioContex_.wsabuf_.buf = reinterpret_cast<char*>(dummy.buf_);
				dummy.ioContex_.wsabuf_.len = _MAX_BUFFER;
	
				DWORD flag{};
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(dummy.socket_), _IOCP, reinterpret_cast<ULONG_PTR>(&dummy), 0);
				if (SOCKET_ERROR == WSARecv(dummy.socket_, &dummy.ioContex_.wsabuf_, 1, NULL, &flag, &dummy.ioContex_.overlapped_, NULL))
					continue;
	
				dummy.isrun_ = true;
				break;
			}
			
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}
	};
	AdjustDummy();

	for (int i = 0; i < _MAX_WORKERTHREAD; ++i) {
		vecThreads[i].join();
	}

#pragma region Cleanup
	delete _IOCP;
	WSACleanup();
#pragma endregion
}

