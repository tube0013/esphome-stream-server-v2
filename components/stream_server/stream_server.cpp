/* Copyright (C) 2020-2022 Oxan van Leeuwen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "stream_server.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include "esphome/components/network/util.h"
#include "esphome/components/socket/socket.h"

static const char *TAG = "streamserver";

using namespace esphome;

void StreamServerComponent::setup() {
    ESP_LOGCONFIG(TAG, "Setting up stream server...");

    // Match the IP version ESPHome itself was built for. socket_ip() returns an
    // AF_INET6 socket when USE_NETWORK_IPV6 is set -- dual-stack, so IPv4 clients
    // still work -- and falls back to AF_INET otherwise.
    this->socket_ = socket::socket_ip(SOCK_STREAM, 0);
    if (this->socket_ == nullptr) {
        ESP_LOGE(TAG, "Could not create socket");
        this->mark_failed();
        return;
    }

    struct sockaddr_storage bind_addr;
    socklen_t bind_addrlen = socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&bind_addr),
                                                      sizeof(bind_addr), this->port_);
    if (bind_addrlen == 0) {
        ESP_LOGE(TAG, "Could not set bind address");
        this->mark_failed();
        return;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 20000; // ESPHome recommends 20-30 ms max for timeouts

    this->socket_->setsockopt(SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    this->socket_->setsockopt(SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    if (this->socket_->bind(reinterpret_cast<struct sockaddr *>(&bind_addr), bind_addrlen) != 0) {
        ESP_LOGE(TAG, "Could not bind socket to port %u", this->port_);
        this->mark_failed();
        return;
    }
    this->socket_->listen(8);


}

void StreamServerComponent::loop() {
    this->accept();
    this->read();
    this->write();
    this->cleanup();
}

void StreamServerComponent::accept() {
    // Must fit a sockaddr_in6: the listener is AF_INET6 when IPv6 is enabled, and
    // a sockaddr_in buffer would truncate an IPv6 peer address.
    struct sockaddr_storage client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    std::unique_ptr<socket::Socket> socket = this->socket_->accept(reinterpret_cast<struct sockaddr *>(&client_addr), &client_addrlen);
    if (!socket)
        return;

    socket->setblocking(false);
    // inet_ntoa() only understands sockaddr_in, so format by family instead.
    // Fixed 64-byte buffer rather than INET6_ADDRSTRLEN, which is not defined
    // on builds without IPv6.
    char addr_buf[64];
    const char *addr_str = nullptr;
#if USE_NETWORK_IPV6
    if (client_addr.ss_family == AF_INET6) {
        auto *addr6 = reinterpret_cast<struct sockaddr_in6 *>(&client_addr);
        addr_str = inet_ntop(AF_INET6, &addr6->sin6_addr, addr_buf, sizeof(addr_buf));
    }
#endif
    if (addr_str == nullptr) {
        auto *addr4 = reinterpret_cast<struct sockaddr_in *>(&client_addr);
        addr_str = inet_ntop(AF_INET, &addr4->sin_addr, addr_buf, sizeof(addr_buf));
    }
    std::string identifier = addr_str != nullptr ? addr_str : "unknown";
    this->clients_.emplace_back(std::move(socket), identifier);
    ESP_LOGD(TAG, "New client connected from %s", identifier.c_str());
}

void StreamServerComponent::cleanup() {
    auto discriminator = [](const Client &client) { return !client.disconnected; };
    auto last_client = std::partition(this->clients_.begin(), this->clients_.end(), discriminator);
    this->clients_.erase(last_client, this->clients_.end());
}

void StreamServerComponent::read() {
    int len;
    while ((len = this->stream_->available()) > 0) {
        char buf[128];
        len = std::min(len, 128);
        this->stream_->read_array(reinterpret_cast<uint8_t*>(buf), len);
        for (const Client &client : this->clients_)
            client.socket->write(buf, len);
    }
}

void StreamServerComponent::write() {
    uint8_t buf[128];
    ssize_t len;
    for (Client &client : this->clients_) {
        while ((len = client.socket->read(&buf, sizeof(buf))) > 0){
            this->stream_->write_array(buf, len);
		}
        if (len == 0) {
            ESP_LOGD(TAG, "Client %s disconnected", client.identifier.c_str());
            client.disconnected = true;
            continue;
        }
    }
}

void StreamServerComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "Stream Server:");
    std::string ip_str = "";
    for (auto &ip : network::get_ip_addresses()) {
      if (ip.is_set()) {
    	char buf[network::IP_ADDRESS_BUFFER_SIZE];
        ip_str += " " + std::string(ip.str_to(buf));
	  }
    }
    ESP_LOGCONFIG(TAG, "  Address:%s", ip_str.c_str());
    ESP_LOGCONFIG(TAG, "  Port: %u", this->port_);
}

void StreamServerComponent::on_shutdown() {
    for (const Client &client : this->clients_)
        client.socket->shutdown(SHUT_RDWR);
}

StreamServerComponent::Client::Client(std::unique_ptr<esphome::socket::Socket> socket, std::string identifier)
    : socket(std::move(socket)), identifier{identifier}
{
}
