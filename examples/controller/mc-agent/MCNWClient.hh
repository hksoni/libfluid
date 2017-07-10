#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <iostream>
#include <string>

#include "MCApps.hh"

class NetworkEvent {
public:
    void* data;
    uint16_t* length;
};

//FUTURE NOTE1: inherit MulticastGroupMembershipApp from MCNWClient handle
//NeworkEvent callbacks there... later
class MCNWClient {
public:
    virtual void network_client_callback(NetworkEvent* nv);
};


class MCNWClientConnection {

public:
    MCNWClientConnection(std::string ip, uint16_t port);

    void set_multicast_group_membership_app(MulticastGroupMembershipApp* app) {
        mgma = app;
    }

    static void event_cb(struct bufferevent *bev, short int events, void *data);
    static void read_cb(struct bufferevent *bev, void *data);
    static void* run_client(void*arg);

    void run();
    void stop();

private:
    void connect_network_controller();

private:
    struct event_base *base;
    struct bufferevent *bev_mcnwcc;
    evutil_socket_t sock;
    struct sockaddr_in sock_addr_in;
    pthread_t thread;

    //NOTE1: remove this
    MulticastGroupMembershipApp* mgma;
};


MCNWClientConnection::MCNWClientConnection(std::string ip, uint16_t port) {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    evutil_make_socket_nonblocking(sock);
    memset(&sock_addr_in, 0, sizeof(sock_addr_in));
    sock_addr_in.sin_family = AF_INET;
    sock_addr_in.sin_addr.s_addr = inet_addr(ip.c_str());
    sock_addr_in.sin_port = htons(port);
    base = event_base_new();
    bev_mcnwcc = bufferevent_socket_new(base, sock, BEV_OPT_CLOSE_ON_FREE);
}


void MCNWClientConnection::connect_network_controller() {
    bufferevent_setcb (bev_mcnwcc, read_cb, NULL, event_cb, (void *)this);
    bufferevent_enable(bev_mcnwcc, EV_READ|EV_WRITE);
    bufferevent_socket_connect(bev_mcnwcc, (struct sockaddr *)&sock_addr_in, sizeof(sock_addr_in));
    event_base_loop(base, 0x04);
}

/*
 * As of now only create group message is expected from read call back.
 * buf[8] is, 1st 4 bytes ip in (32 bit integer format) next 2 bytes are port
 * and last 2 bytes are 
 */
void MCNWClientConnection::read_cb(struct bufferevent *bev, void *data) {
    uint8_t buf[16];
    int n;
    struct evbuffer *input = bufferevent_get_input(bev);
    n = evbuffer_remove(input, buf, sizeof(buf));
    std::cout<<"return of evbuffer_remove: "<<n<<"\n";
    if (n!=sizeof(buf)) {
        std::cout<<"Error in receiving buf\n";
        return; 
    }

    MCNWClientConnection* mcnwcc = static_cast<MCNWClientConnection*> (data);
    uint32_t sip, dip;
    uint16_t sport, dport, up_intf, is_root;
    memcpy(&sip, buf, sizeof(sip));
    memcpy(&dip, buf+4, sizeof(dip));
    memcpy(&sport, buf+8, sizeof(sport));
    memcpy(&dport, buf+10, sizeof(dport));
    memcpy(&up_intf, buf+12, sizeof(up_intf));
    memcpy(&is_root, buf+14, sizeof(is_root));
    mcnwcc->mgma->create_group(sip, dip, ntohs(sport), ntohs(dport), ntohs(up_intf), ntohs(is_root));
}

void MCNWClientConnection::event_cb(struct bufferevent *bev, short int events, void* data) {
    MCNWClientConnection* mcnwcc = static_cast<MCNWClientConnection*> (data);
    struct event_base *base = mcnwcc->base;
    if (events & BEV_EVENT_CONNECTED) {
        std::cout<<"\nConnected to network controller....\n";
        uint64_t dpid = be64toh(mcnwcc->mgma->datapath_id());
        if (bufferevent_write(bev, &dpid, sizeof(dpid)) == 0) {
            std::cout<<std::hex<<mcnwcc->mgma->datapath_id()<<std::dec<<"Datapath Id successfully sent\n";
        } else {
            std::cout<<"Error in sending Datapath id\n";
        }
    } else {
        if (events & BEV_EVENT_EOF) {
            std::cout<<"\nDisconnected from network controller...\n";
        } else if (events & BEV_EVENT_ERROR) {
            std::cout<<"Error in connection with network controller...";
        } else if (events & BEV_EVENT_TIMEOUT) {
            std::cout<<"Timeout with network controller... ";
        } else {
            std::cout<<"Unknow event in connection  with network controller... ";
        }
    }
}

void MCNWClientConnection::stop() {
    bufferevent_free(bev_mcnwcc);
    event_base_loopexit(base, NULL);
}

void* MCNWClientConnection::run_client(void* arg) {
    MCNWClientConnection* mcnwcc = static_cast<MCNWClientConnection*> (arg);
    mcnwcc->connect_network_controller();
    return NULL;
}


void MCNWClientConnection::run() {
    pthread_create(&thread, NULL,run_client, (void*)this);
}
