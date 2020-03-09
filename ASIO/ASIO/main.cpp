
#include <iostream>
#include <chrono>
#include <atomic>
//#include <memory>
#include <vector>
#include <array>
//#include <thread>
#include "boost/shared_ptr.hpp"
#include "boost/thread.hpp"
#include "boost/asio.hpp"
#include "../../Protocols.h"

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

using atomic_uint16_t = std::atomic_char16_t;

constexpr uint32_t _MAX_THREAD = 8;

atomic_uint16_t _GlobalUserID;

uint16_t GetNewUserID() {
	if (_GlobalUserID == _MAX_USER) {
		std::wcout << _GlobalUserID << "명의 플레이어가 모두 접속중입니다.\n서버를 정지시킵니다.\n";
		while (1) {}
	}

	return _GlobalUserID++;
}
class CSession;
class CPlayer {
public:
	CPlayer() = default;
	~CPlayer() = default;
public:
	volatile bool isconnected_;
	boost::shared_ptr<CSession> psession_;
	std::chrono::system_clock::time_point startTime_;
public:
	void InitPlayer() {
		isconnected_ = false;
	}
	void JoinPlayer(boost::shared_ptr<CSession> psession) {
		isconnected_ = true;
		psession_ = psession;
		startTime_ = std::chrono::system_clock::now();
	}
};

std::array<CPlayer, _MAX_USER> _Players;

class CSession
	: public boost::enable_shared_from_this<CSession>
{
public:
	CSession() = delete;
	CSession(tcp::socket socket) : tcpSocket_(std::move(socket)), currentPacketSize_(0), prevPacketSize_(0), id_() {}
	~CSession() = default;
	CSession(const CSession& o) = delete;
	const CSession& operator=(const CSession& o) = delete;
public:
	enum { MAX_SESSION_BUFFER = 1024 };
	tcp::socket tcpSocket_;
	//udp::socket udpSocket_;
	uint8_t dataBuffer_[MAX_SESSION_BUFFER];
	uint8_t packetBuffer_[MAX_SESSION_BUFFER];
	int32_t currentPacketSize_;
	int32_t prevPacketSize_;
	uint16_t id_;
public:
	void Start() {
#pragma region InitPlayer
		id_ = GetNewUserID();
		_Players[id_].JoinPlayer(shared_from_this());
		std::cout << "Join Player ID [ " << id_ << " ]\n";
#pragma endregion

		DoTCPRead();

		auto sendIDPacket = [this]() {
			PACKET_ID packet{ sizeof(PACKET_ID), EPacketType::id, id_ };
			SendPacket(&packet, id_);
		};
		sendIDPacket();

	}
	void DoTCPRead() {
		auto self(shared_from_this());
		tcpSocket_.async_read_some(boost::asio::buffer(dataBuffer_, MAX_SESSION_BUFFER),
			[this, self](boost::system::error_code ec, std::size_t length)
			{
				if (ec) {
					if (ec.value() == boost::asio::error::operation_aborted) return;
					if (false == _Players[id_].isconnected_) return;
					tcpSocket_.shutdown(tcpSocket_.shutdown_both);
					tcpSocket_.close();
					std::cout << "ID [ " << id_ << " ] Out Player\n";
					//udpSocket_.shutdown(udpSocket_.shutdown_both);
					//udpSocket_.close();
					return;
				}

				int dataToProcess = static_cast<int>(length);
				uint8_t* pbuf = dataBuffer_;

				if (prevPacketSize_ != 0) {
					currentPacketSize_ = packetBuffer_[0];
				}
				while (0 < dataToProcess) {
					if (currentPacketSize_ == 0) {
						currentPacketSize_ = pbuf[0];
					}
					int needSize = currentPacketSize_ - prevPacketSize_;
					if (needSize <= dataToProcess) {
						memcpy(packetBuffer_ + prevPacketSize_, pbuf, needSize);
						PacketProcess(packetBuffer_, id_);
						dataToProcess -= needSize;
						currentPacketSize_ = 0;
						prevPacketSize_ = 0;
						pbuf += needSize;
					}
					else {
						memcpy(packetBuffer_ + prevPacketSize_, pbuf, dataToProcess);
						prevPacketSize_ += dataToProcess;
						pbuf += dataToProcess;
						dataToProcess = 0;
					}
				}
				DoTCPRead();
			});
	}
	void DoTCPWrite(uint8_t* ppacket, std::size_t length) {
		auto self(shared_from_this());
		tcpSocket_.async_write_some(boost::asio::buffer(ppacket, length),
			[this, self, ppacket, length](boost::system::error_code ec, std::size_t ioBytes)
			{
				if (!ec) {
					if (length != ioBytes) {
						delete ppacket;
					}
				}
			});
	}

	void SendPacket(void* ppacket, uint16_t id) {
		int packetSize = reinterpret_cast<uint8_t*>(ppacket)[0];
		uint8_t* pnewPacket = new uint8_t[packetSize]{};
		memcpy(pnewPacket, ppacket, packetSize);
		_Players[id].psession_->DoTCPWrite(pnewPacket, packetSize);
	}

	void PacketProcess(uint8_t* ppacket, uint16_t id) {
		char c = reinterpret_cast<PACKET_SIMPLE*>(ppacket)->c_;
		std::cout << "ID [ " << id << " ] Send '" << c << " '\n";

		PACKET_SIMPLE packet{ sizeof(PACKET_SIMPLE), EPacketType::simplechat, id, c };

		for (int i = 0; i < _MAX_USER; ++i) {
			CPlayer& player = _Players[i];
			if (player.isconnected_ == false) continue;
			//if (i == id) continue;
			SendPacket(&packet, i);
		}
	}
};

class CServer {
public:
	CServer(boost::asio::io_service& ioservice, int port) : acceptor_(ioservice, tcp::endpoint(tcp::v4(), port)), socket_(ioservice) {
		DoAccept();
	};
public:
	tcp::acceptor acceptor_;
	tcp::socket socket_;
public:
	void DoAccept() {
		acceptor_.async_accept(socket_,
			[this](boost::system::error_code ec)
			{
				if (!ec) {
					boost::make_shared<CSession>(std::move(socket_))->Start();
				}
				DoAccept();
			});
	}
};

void WorkerThread(boost::asio::io_service* service) {
	service->run();
}

int main()
{
	boost::asio::io_service service;
	std::vector<boost::thread*> vecThreads;

	CServer server(service, _SERVER_PORT);

#pragma region InitServer
	auto InitServer = []() {
		_wsetlocale(LC_ALL, L"korean");
		for (int i = 0; i < _MAX_USER; ++i) {
			_Players[i].InitPlayer();
		}
	};
	InitServer();
#pragma endregion

#pragma region InitThread
	for (int i = 0; i < _MAX_THREAD; ++i) {
		vecThreads.push_back(new boost::thread{ WorkerThread, &service });
	}
#pragma endregion

#pragma region JoinThread
	for (auto th : vecThreads) {
		th->join();
		delete th;
	}
	vecThreads.clear();
#pragma endregion

}
