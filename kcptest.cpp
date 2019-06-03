#include <stdio.h>
#include <asio/asio.hpp>
#include <kcp/ikcp.h>
#include <thread>
#include <memory>
#include <chrono>

asio::io_context gService;
ikcpcb* kcp = nullptr;
unsigned long conv = 1;
bool isClient = false;
asio::error_code ec;
std::shared_ptr<asio::ip::udp::socket> so;
std::shared_ptr<asio::ip::udp::socket> so2;
std::shared_ptr<std::string> xbuffer(new std::string(1024 * 16, 0));
std::shared_ptr<std::string> xbuffer2(new std::string(1024 * 16, 0));
std::shared_ptr<asio::ip::udp::endpoint> senderEndpoint;
std::shared_ptr<asio::ip::udp::endpoint> senderEndpoint2;
unsigned long long m_kcpUpdateTime = 0;
std::mutex gLock;
unsigned long long getMilliSeconds()
{
	using namespace std::chrono;
	return duration_cast<duration<unsigned long long, std::milli> >(high_resolution_clock::now().time_since_epoch()).count();
}

int kcp_output_cb(const char *buf, int len, ikcpcb *kcp, void *user)
{
	so->async_send_to(
		asio::buffer(buf, len),
		*senderEndpoint,
		[](asio::error_code, std::size_t) {
	});
	return 0;
}

void nlog(const char *msg, ...)
{
	const int reservedLen = 1;//for \n
	va_list args;
	va_start(args, msg);
	vsnprintf(nullptr, 0, msg, args);
	va_end(args);
}

void startReceive()
{
 	so->async_receive_from(asio::buffer(const_cast<char *>(xbuffer->c_str()), xbuffer->length()), *senderEndpoint,
 		[](asio::error_code ec, std::size_t bytesRecvd) 
	{
		gLock.lock();
		ikcp_input(kcp, xbuffer->c_str(), (long)bytesRecvd);
		m_kcpUpdateTime = getMilliSeconds();
		gLock.unlock();

 		startReceive();
 	});
}
bool kcpUpdate()
{
#if 0
	ikcp_update(kcp, (IUINT32)getMilliSeconds());
	return true;
#else
	auto milliSec = getMilliSeconds();
	if (milliSec >= m_kcpUpdateTime)
	{
		IUINT32 current = (IUINT32)milliSec;
		ikcp_update(kcp, current);
		IUINT32 next = ikcp_check(kcp, current);
		m_kcpUpdateTime = milliSec + (next - current);
		return true;
	}
	return false;
#endif
}
extern void onRecv(const char *buffer, int len);
void kcpRoutine()
{
	if (!kcpUpdate())
	{
		return;
	}
	char data_[1024 * 16];
	while (1)
	{
		int len = ikcp_recv(kcp, data_, 1024 * 16);
		if (len <= 0)
		{
			break;
		}
		onRecv(data_,len);
	}
}

extern void client_loop();

void onRecv(const char *buffer, int len)
{
	if (!isClient)
	{
		ikcp_send(kcp, buffer, len);
	}
	else
	{
		client_loop();
	}
}
void kcp_loop()
{
	std::shared_ptr<asio::steady_timer> t = std::make_shared<asio::steady_timer>(gService);
	t->expires_from_now(std::chrono::milliseconds(1));
	t->async_wait([t](std::error_code ec) {
		if (ec)
		{
			nlog("KcpServerImpl::startRoutine error %s", ec.message().c_str());
			return;
		}
		kcpRoutine();
		kcp_loop();
	});
}
void kcp_loop2()
{
	std::thread thr([] {
		while (1)
		{
			gLock.lock();
			kcpRoutine();
			gLock.unlock();
		}
	});
	thr.detach();
}
unsigned long long lastSendTime = 0;
void client_loop()
{
	if (!isClient)
	{
		return;
	}
	unsigned long long nowt = getMilliSeconds();
	if (lastSendTime != 0)
	{
		printf("kcp ping time: %d ms\n", int(nowt - lastSendTime));
	}
	std::shared_ptr<asio::steady_timer> t = std::make_shared<asio::steady_timer>(gService);
	t->expires_from_now(std::chrono::seconds(1));
	t->async_wait([t](std::error_code ec) {
		lastSendTime = getMilliSeconds();
		ikcp_send(kcp, "666", 3);
	});

}

unsigned long long lastSendTime2 = 0;

void client_loop2()
{
	if (!isClient)
	{
		return;
	}
	unsigned long long nowt = getMilliSeconds();
	if (lastSendTime2 != 0)
	{
		printf("\t\t\tpure udp ping time: %d ms\n", int(nowt - lastSendTime2));
	}
	std::shared_ptr<asio::steady_timer> t = std::make_shared<asio::steady_timer>(gService);
	t->expires_from_now(std::chrono::seconds(1));
	t->async_wait([t](std::error_code ec) {
		lastSendTime2 = getMilliSeconds();
		so2->async_send_to(
			asio::buffer("666", 3),
			*senderEndpoint2,
			[](asio::error_code, std::size_t) {
		});
	});
}

void startReceive2()
{
	so2->async_receive_from(asio::buffer(const_cast<char *>(xbuffer2->c_str()), xbuffer2->length()), *senderEndpoint2,
		[](asio::error_code ec, std::size_t bytesRecvd)
	{
		if (!isClient)
		{
			so2->async_send_to(
				asio::buffer(xbuffer2->c_str(), bytesRecvd),
				*senderEndpoint2,
				[](asio::error_code, std::size_t) {
			});
		}
		else
		{
			client_loop2();
		}
		startReceive2();
	});
}

int main(int argc, char* argv[])
{

	if (argc > 1)
	{
		so = std::make_shared<asio::ip::udp::socket>(gService, asio::ip::basic_endpoint<asio::ip::udp>(asio::ip::address::from_string("::", ec), 5061));
		senderEndpoint = std::make_shared<asio::ip::udp::endpoint>(asio::ip::udp::endpoint(asio::ip::address::from_string("::1", ec), 5060));
		senderEndpoint2 = std::make_shared<asio::ip::udp::endpoint>(asio::ip::udp::endpoint(asio::ip::address::from_string("::1", ec), 5064));
		isClient = true;
		so2 = std::make_shared<asio::ip::udp::socket>(gService, asio::ip::basic_endpoint<asio::ip::udp>(asio::ip::address::from_string("::", ec), 5063));
	}
	else
	{
		senderEndpoint = std::make_shared<asio::ip::udp::endpoint>();
		senderEndpoint2 = std::make_shared<asio::ip::udp::endpoint>();
		so = std::make_shared<asio::ip::udp::socket>(gService, asio::ip::basic_endpoint<asio::ip::udp>(asio::ip::address::from_string("::", ec), 5060));
		so2 = std::make_shared<asio::ip::udp::socket>(gService, asio::ip::basic_endpoint<asio::ip::udp>(asio::ip::address::from_string("::", ec), 5064));
	}
	kcp = ikcp_create(conv, nullptr);
	kcp->output = &kcp_output_cb;
	// boot fast
	// param2 nodelay
	// param3 interval 1ms
	// param4 resend
	// param5 disable congestion control
	ikcp_nodelay(kcp, 1, 1, 2, 1);
	ikcp_wndsize(kcp, 256, 256);
	kcp->interval = 1;
	std::thread t([] {
		asio::io_context::work work(gService);
		gService.run();
	});
	startReceive();
	startReceive2();
	kcp_loop();
	client_loop();
	client_loop2();
	t.join();
	return 0;
}
