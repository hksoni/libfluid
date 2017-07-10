#ifndef __MCGROUPMEMBER_HH__
#define __MCGROUPMEMBER_HH__

#include <iostream>
#include <map>
#include <set>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <fluid/of10msg.hh>
#include <fluid/of13msg.hh>

using namespace fluid_msg;
/*
 *Ips are in network order, rest all are in host order
 */

class PacketInDataHeader {
public:
    PacketInDataHeader(uint64_t d_m, uint64_t s_m, uint32_t d_ip, uint32_t s_ip, 
                       uint16_t in_i, uint16_t ud_p, uint16_t us_p, uint16_t proto, 
                       uint32_t mem_code, of13::PacketIn* pi) {
        dmac = d_m;
        smac = s_m;
        dip = d_ip;
        sip = s_ip;
        in_intf = in_i;
        usrc_port = us_p;
        udst_port = ud_p;
        protocol = proto;
        membership_code = mem_code;
        ofpi = pi;
    }

    friend std::ostream& operator<<(std::ostream& os, const PacketInDataHeader& obj);

public:
    uint64_t dmac, smac;
    uint32_t sip, dip;
    uint16_t in_intf, usrc_port, udst_port, protocol;
    uint32_t membership_code;

    of13::PacketIn *ofpi;
};



/*
 * MCGroupMember holds identiy of a receiver or a
 * group member.
 * To support mutlicast without class-D and IGMPv2 match in openflow,
 * source's IP and src UDP port pair will be considered as group identifier.
 * JOIN: member/reciver should send a udp packet with destination ip as group
 * source's(gs's) IP and <src, dst> matching  gs's transport pair in the multicast
 * flow.  Member sends packet with udp_src = gs_src, udp_dst = gs_dst.
 * LEAVE: Member sends packet with udp_src = gs_dst, udp_dst = gs_src.
 * Note: 1 receiver per interface on switch: operator< compares only interface
 * value
 */
class MCGroupMember {

public:
    /* Create a new Multicast Group memeber
     *
     * @param: dmac: destination mac, 0 in case of class-D ip destination
     * @param: dip: can be either unicast dest IP or class-D multicast destination
     * ip.
     * @param: port: udp destination port for application
     * @param: intf: physical interface to which multicast member is attached
     * @param: classd: true if destination ip is of class-D
     */
    MCGroupMember(uint64_t dmac, uint32_t dip, uint16_t port, uint8_t intf, bool classd) {
        destmac_ = dmac;
        destip_ = dip;
        udpport_ = port;
        is_classd_ = classd;
        in_intf_ = intf;
    }

    MCGroupMember& operator=(const MCGroupMember& mtgm) {
        this->destmac_ = mtgm.destmac_;
        this->destip_ = mtgm.destip_;
        this->udpport_ = mtgm.udpport_;  
        this->is_classd_ = mtgm.is_classd_;
        this->in_intf_ = mtgm.in_intf_;
        this->bucket_index_ = mtgm.bucket_index_;
        return *this;
    }



    /*
     * One receiver per interface of the switch at the edge, it is enough to
     * compare only interface value
     */
    bool operator<(const MCGroupMember& mtgm) const {
        return (this->in_intf_ < mtgm.in_intf_);
    }

    uint64_t dest_mac() const {return this->destmac_;}
    uint32_t dest_ip() const {return this->destip_;}
    uint16_t udp_port() const {return this->udpport_;}
    uint8_t intf() const {return this->in_intf_ ;}

    void bucket_index(size_t index) { bucket_index_ = index;}
    size_t bucket_index() const {return this->bucket_index_;}

private:
    uint64_t destmac_;
    uint32_t destip_;
    uint16_t udpport_;
    uint8_t in_intf_;
    bool is_classd_;
    size_t bucket_index_;

};

class MCGroupMemberPointerComparator {
public:
    bool operator()(const MCGroupMember* obj1, const MCGroupMember* obj2) 
    {
        return ((*obj1)<(*obj2));
    }
};

#endif
