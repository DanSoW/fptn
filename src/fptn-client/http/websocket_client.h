#pragma once

#include <mutex>
#include <thread>
#include <string>
#include <iostream>
#include <functional>

#include <hv/WebSocketClient.h>
#include <common/network/ip_packet.h>


namespace fptn::http
{
    class WebSocketClient final
    {
    public:
        using NewIPPacketCallback = std::function<void(fptn::common::network::IPPacketPtr packet)>;
    public:
        WebSocketClient(
            const std::string& vpnServerIP, 
            int vpnServerPort,
            const std::string& tunInterfaceAddress,
            bool useSsl = true,
            const NewIPPacketCallback& newIPPktCallback = nullptr
        );
        bool login(const std::string& username, const std::string& password) noexcept;
        std::string getDns() noexcept;
        bool start() noexcept;
        bool stop() noexcept;
        bool send(fptn::common::network::IPPacketPtr packet) noexcept;
        void setNewIPPacketCallback(const NewIPPacketCallback& callback) noexcept;
    private:
        void run() noexcept;
        http_headers getRealBrowserHeaders() noexcept;
    private:
        void onOpenHandle() noexcept;
        void onMessageHandle(const std::string& msg) noexcept;
        void onCloseHandle() noexcept;
    private:
        std::thread th_;
        hv::WebSocketClient ws_;

        const std::string vpnServerIP_; 
        int vpnServerPort_;

        std::string token_;
        std::string tunInterfaceAddress_;
        NewIPPacketCallback newIPPktCallback_;
    };

    using WebSocketClientPtr = std::unique_ptr<WebSocketClient>;
}
