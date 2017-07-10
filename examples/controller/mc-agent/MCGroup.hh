#ifndef __MSGAPPS_HH__
#define __MSGAPPS_HH__

#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <fluid/OFConnection.hh>
#include <fluid/of10msg.hh>
#include <fluid/of13msg.hh>
#include "MCGroupMember.hh"


using namespace fluid_msg;
using namespace fluid_base;

class MCGroup;

class MCGroupOpenFlowContext { 
public:
    MCGroupOpenFlowContext (MCGroup* mcg);

    void create_of13_group_table_msg(uint32_t xid);
    void add_bucket_of13_group(uint32_t xid, MCGroupMember* mtmg, uint8_t command);
    void remove_bucket_of13_group(uint32_t xid, const MCGroupMember* mtmg, uint8_t command);

    void create_downstream_flow_mod_for_group();
    void insert_downstream_flow_mod_for_group(OFConnection* ofconn);
    void delete_downstream_flow_mod_for_group(OFConnection* ofconn);

    uint8_t* create_group_hook_flow_mod();

    void send_upstream_packet_out(PacketInDataHeader *pk, OFConnection* ofconn); 

    uint8_t* pack_group_mod_msg() {
        return ofgroupmod.pack();
    }

    static uint32_t OFMsgXidGen;
    static uint32_t OFMsgGroupIdGen;
    static uint64_t OFMsgCookieGen;
public:

    //TODO: convert this to read only access
    const MCGroup *mcgroup;

    of13::FlowMod upstreamflowmod;
    of13::FlowMod offlowmod;
    of13::GroupMod ofgroupmod;
    std::vector<of13::Bucket> member_of13_buckets;
private:
    uint8_t *of_flow_mod_buffer;

};

/* 
 * MCGroup holds information to uniquely identify group.
 * It needs destination class-D ip value as groupip.
 * To support multicast without class-D IP, groupip should hold
 * source IP. Only SSM is allowed, as of now
 */
class MCGroup {
public:
    /* Create a new Multicast Group
     *
     * @param: ip: can either unicast source IP or class-D multicast destination
     * ip
     * @param: sport: udp source port of the sender
     *         member should send messages to this port to join
     * @param: dport: udp dest port of the sender 
     * @param: up_intf: upstream interface for the group
     * @param: classd: true if  multicast class-D ip is used
     *                 false, otherwise
     * @param: is_root: true if the switch is root for the source else false
     */
    MCGroup(uint32_t ip, uint16_t up_intf, uint16_t sport, uint16_t dport, bool classd, bool is_root) {
        /*
         * IPs are stored in network order and ports are stored in host order.
         * to adjust with inet_ntoa calls
         */
        groupip_ = ip;
        sudpport_ = sport;
        upstream_intf_ = up_intf;
        is_classd_ = classd;
        groupid_ = ++MCGroupOpenFlowContext::OFMsgGroupIdGen;
        mc_group_of_context = NULL;

        dudpport_ = dport;
        is_root_ = is_root;
    }

    MCGroup(const MCGroup &mtg) {
        this->groupip_ = mtg.groupip_;  
        this->sudpport_ = mtg.sudpport_;  
        this->dudpport_ = mtg.dudpport_;  
        this->upstream_intf_  = mtg.upstream_intf_;  
        this->is_classd_ = mtg.is_classd_;  
        this->groupid_ = mtg.groupid_;
        this->is_root_ = mtg.is_root_;
        mc_group_of_context = mtg.mc_group_of_context;
    }

   
    /*
     * TODO: reminder, if ever introduce pointers in the class add copy constructor,
     * destructor and also modified below.
     */
    MCGroup& operator=(const MCGroup& mtg) {
        if (this == &mtg)
            return *this;
        this->groupip_ = mtg.groupip_;  
        this->sudpport_ = mtg.sudpport_;  
        this->dudpport_ = mtg.dudpport_;  
        this->upstream_intf_  = mtg.upstream_intf_;  
        this->is_classd_ = mtg.is_classd_;  
        this->groupid_ = mtg.groupid_;
        this->is_root_ = mtg.is_root_;
        if (mc_group_of_context != NULL)
            delete mc_group_of_context;
        mc_group_of_context = mtg.mc_group_of_context;
        return *this;
    }

    /*
     * less than operator is for std::map. MCGroup class is
     * used as key in a std::map object.
     */
    bool operator<(const MCGroup& mtg) const {
        return (this->groupip_ < mtg.groupip_);
    }

/*
    bool operator<(const MCGroup& mtg) const {

        if (is_classd_ == mtg.is_classd_ ) {
            if (this->groupip_ != mtg.groupip_)
                return (this->groupip_ < mtg.groupip_);
            if (is_classd_)
                if (this->sudpport_ != mtg.sudpport_)
                  return (this->sudpport_ < mtg.sudpport_);
        } else {
            return is_classd_;
        }
    }
*/

    MCGroupOpenFlowContext* create_openflow_context();

    uint16_t upstream_intf(void) const {
        return upstream_intf_;
    }

    uint16_t usrc_port(void) const {
        return sudpport_;
    }

    uint16_t udst_port(void) const {
        return dudpport_;
    }

    bool is_root(void) const {
        return is_root_;
    }
    
    uint32_t group_id(void) const {
        return groupid_;
    }

    ~MCGroup() {
        delete mc_group_of_context;
    }

    MCGroupOpenFlowContext *mc_group_of_context;

    friend class MCGroupOpenFlowContext;
    friend std::ostream& operator<<(std::ostream& os, const MCGroup& obj);
private:
    uint32_t groupip_;
    uint16_t sudpport_;
    uint16_t dudpport_;
    uint16_t upstream_intf_;
    bool is_classd_;
    uint32_t groupid_;
    bool is_root_;
};


class MCGroupPointerComparator {
public:
    bool operator()(const MCGroup* obj1, const MCGroup* obj2) 
    {
        return ((*obj1)<(*obj2));
    }
};

#endif
