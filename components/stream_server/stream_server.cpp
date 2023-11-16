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

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esphome/components/network/util.h"
#include "lwip/sockets.h"


static const char *TAG = "streamserver";
struct sockaddr_in client_addr;
  
  
char read_buf[128];    
uint8_t write_buf[128];

using namespace esphome;

void StreamServerComponent::setup() {
    ESP_LOGCONFIG(TAG, "Setting up stream server...");

    struct sockaddr_in bind_addr = {
        .sin_len = sizeof(struct sockaddr_in),
        .sin_family = AF_INET,
        .sin_port = htons(this->port_),
        .sin_addr = {
            .s_addr = ESPHOME_INADDR_ANY,
        }
    };

    this->serverSocket = lwip_socket(AF_INET, SOCK_STREAM, PF_INET);
    
    
    int opt = 1;
    setsockopt(this->serverSocket,SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    setsockopt(this->serverSocket,SOL_SOCKET, TCP_NODELAY, &opt, sizeof(int));
        
    bind(this->serverSocket, reinterpret_cast<struct sockaddr *>(&bind_addr), sizeof(struct sockaddr_in));
  
    listen(this->serverSocket,8);
    
    fcntl(this->serverSocket,F_SETFL, O_NONBLOCK);
    this->socketstatus = -1;
}

void StreamServerComponent::loop() {
    this->read();
    this->write();
}

void StreamServerComponent::cleanup() {
   
}

void StreamServerComponent::read() {
    int len;
    int err = 0;
    while ((hasclient()) && (len = this->stream_->available()) > 0) {
        len = std::min(len, 128);
        this->stream_->read_array(reinterpret_cast<uint8_t*>(read_buf), len);

        if (len > 0){
            err = send(this->socketstatus, read_buf, len, MSG_DONTWAIT);
        }
    }
 }

void StreamServerComponent::write() {
    int len;
    if(hasclient()){
        len = recv(this->socketstatus, write_buf, sizeof(write_buf) - 1, MSG_DONTWAIT);
        len = std::min(len, 128);
        if (len > 0) {
            this->stream_->write_array(write_buf, len);
        }
        if (len == 0){
            socklen_t client_addrlen = sizeof(client_addr);
            getpeername(this->serverSocket,(struct sockaddr *)&client_addr,&client_addrlen);
            ESP_LOGD(TAG, "Client %s disconnected", inet_ntoa(client_addr.sin_addr));
            shutdown(this->socketstatus, 0);
            close(this->socketstatus);
            this->socketstatus = -1; 
        }
    }    
}

void StreamServerComponent::dump_config() {
    ESP_LOGI(TAG, "Stream Server:");
    ESP_LOGI(TAG, "  Address: %s:%u",
                  esphome::network::get_ip_address().str().c_str(),
                 this->port_);
}

void StreamServerComponent::on_shutdown() {
    shutdown(this->socketstatus, 0);
    close(this->socketstatus);
    this->socketstatus = -1; 
}

bool StreamServerComponent::hasclient() {
    if(this->socketstatus >=0) {
        return true; 
    }
    socklen_t client_addrlen = sizeof(client_addr);
    this->socketstatus = accept(this->serverSocket, (struct sockaddr *)&client_addr, &client_addrlen);
    fcntl(this->socketstatus, F_SETFL, O_NONBLOCK);
    int opt = 1; 
    setsockopt(this->socketstatus, SOL_SOCKET, TCP_NODELAY, &opt, sizeof(int));

    if(this->socketstatus >= 0){
        getpeername(this->serverSocket,(struct sockaddr *)&client_addr,&client_addrlen);
        ESP_LOGD(TAG, "New client connected from %s", inet_ntoa(client_addr.sin_addr));    
        return true;
    }
    return false;
}


StreamServerComponent::Client::Client(std::unique_ptr<esphome::socket::Socket> socket, std::string identifier)
    : socket(std::move(socket)), identifier{identifier}
{
}
