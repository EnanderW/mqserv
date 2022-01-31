#include <iostream>
#include <uv.h>

#include "util/buffer.h"

#include <vector>
#include <unordered_map>
#include <algorithm>

#define DEFAULT_PORT 7000
#define DEFAULT_BACKLOG 128

static uv_loop_t loop;
static sockaddr_in address;
static char READ_BUFFER[20000];
static char WRITE_BUFFER[20000];

static std::unordered_map<std::string, std::vector<uv_stream_t*>> registered_clients{};

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = READ_BUFFER;
    buf->len = sizeof(READ_BUFFER);
}

enum Action {
    SUBSCRIBE = 0,
    UNSUBSCRIBE = 1,
    PUBLISH = 2,
};

static void remove_client(std::vector<uv_stream_t*> &clients, std::string &channel, uv_stream_t* client) {
    clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
}

void echo_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* uv_buf) {
    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        else
            std::cout << "End of file\n";

        uv_close((uv_handle_t*)client, nullptr);
    }

    char *end = uv_buf->base + nread;

    Buffer buf(uv_buf->base);

    while (buf.data < end) {
        uint8_t action_id = buf.ubyte();

        if (action_id == SUBSCRIBE) {
            size_t channel_length = buf.ushort();
            std::string channel(buf.data, channel_length);
            buf.data += channel_length;

            if (registered_clients.find(channel) == registered_clients.end())
                registered_clients[channel] = std::vector<uv_stream_t*>();

            auto &clients = registered_clients[channel];
            remove_client(clients, channel, client);
            clients.push_back(client);

        } else if (action_id == UNSUBSCRIBE) {
            size_t channel_length = buf.ushort();
            std::string channel(buf.data, channel_length);
            buf.data += channel_length;

            if (registered_clients.find(channel) == registered_clients.end())
                continue;

            auto &clients = registered_clients[channel];
            remove_client(clients, channel, client);
        } else if (action_id == PUBLISH) {
            char channel[500]{};
            size_t channel_length = buf.ushort();
            buf.string(channel_length, channel);

            char message[500]{};
            size_t message_length = buf.ushort();
            buf.string(message_length, message);

            if (registered_clients.find(channel) != registered_clients.end()) {
                auto &clients = registered_clients[channel];

                Buffer w_buf(WRITE_BUFFER);
                w_buf.utfstring(channel, channel_length);
                w_buf.utfstring(message, message_length);

                uv_buf_t w_uv_buf = { WRITE_BUFFER, (size_t) (w_buf.data - WRITE_BUFFER) };

                for (auto *all : clients)
                    if (all != client)
                        uv_try_write(all, &w_uv_buf, 1);

            }
        }
    }
}

void on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        fprintf(stderr, "New connection error %s\n", uv_strerror(status));
        return;
    }

    auto* client = new uv_tcp_t;
    uv_tcp_init(&loop, client);

    if (uv_accept(server, (uv_stream_t*)client) == 0) {
        uv_read_start((uv_stream_t*)client, alloc_buffer, echo_read);
    } else {
        uv_close((uv_handle_t*)client, nullptr);
    }
}

int main() {
    uv_loop_init(&loop);

    uv_tcp_t server;
    uv_tcp_init(&loop, &server);

    uv_ip4_addr("0.0.0.0", DEFAULT_PORT, &address);

    uv_tcp_bind(&server, (const struct sockaddr*) &address, 0);
    int r = uv_listen((uv_stream_t*)&server, DEFAULT_BACKLOG, on_new_connection);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }
    return uv_run(&loop, UV_RUN_DEFAULT);
}

