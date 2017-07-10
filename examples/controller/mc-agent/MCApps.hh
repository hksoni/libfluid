#ifndef __MCAPPS_HH__
#define __MCAPPS_HH__

#include <endian.h>
#include <netinet/in.h>
#include <iostream>
#include <map>
#include <set>
#include <bitset>
#include "Controller.hh"
#include "MCGroupMember.hh"
#include "MCGroup.hh"
#include <fluid/of10msg.hh>
#include <fluid/of13msg.hh>

#define ETH_HLEN 14
#define ETH_TYPE_IP 0x0800
#define PROTOCOL_TYPE_IGMP 0x02
#define PROTOCOL_TYPE_UDP 0x11
#define GROUP_JOIN_CODE_UDP 0x7070
#define GROUP_LEAVE_CODE_UDP 0xAEAE


using namespace fluid_msg;


class CBench: public Application {
public:
    CBench () {
        pin_count = 0;
    }

    virtual void event_callback(ControllerEvent* ev) {
        uint8_t ofversion = ev->ofconn->get_version();
        if (ev->get_type() == EVENT_PACKET_IN) {
            PacketInEvent* pi = static_cast<PacketInEvent*>(ev);
            if (ofversion == of13::OFP_VERSION) { 
                of13::PacketIn *ofpi = new of13::PacketIn(); 
                ofpi->unpack(pi->data);
                uint64_t dst = 0, src = 0;
                memcpy(((uint8_t*) &dst) + 2, (uint8_t*) ofpi->data(), 6);
                memcpy(((uint8_t*) &src) + 2, (uint8_t*) ofpi->data() + 6, 6);
                int16_t in_port = -1;
                if (ofpi->match().in_port() == NULL) {
                    in_port = ofpi->match().in_port()->value();
                }
                std::cout<<"PacketIn received : "<<++pin_count<<" \n";
                std::cout<<"src mac : "<<std::hex<<src<<" ";
                std::cout<<"dst mac : "<<std::hex<<dst<<"\n";
                std::cout<<"in_port : "<<std::hex<<in_port<<" \n";
                std::cout<<"-------------------------------";
            }
        }
    }
private:
    uint32_t pin_count;
    
};

class MulticastGroupMembershipApp: public Application {

    typedef std::set<MCGroupMember*, MCGroupMemberPointerComparator> MCGroupMemberSet;
    typedef std::map<uint16_t, MCGroupMember*> MCGroupMemberMap;
    typedef std::map<MCGroup*, MCGroupMemberMap*, MCGroupPointerComparator> MCGroupMap;
    OFConnection* ofconn;
    uint64_t dpid;

public:
    MulticastGroupMembershipApp();

    virtual void event_callback(ControllerEvent* ev);

    uint64_t datapath_id() const {
        return dpid;
    }

    /*
     * Standard OpenFlow1.3 does not have IGMPv2 field matching
     * support. two Work arounds...
     * 1. parse the IGMP, get destion IP from IP header or/IGMP
     * multicast address and if session exist but first membership
     * report is received, PACKET_OUT on upstream interface
     * 2. When session initiated inject rule to forward IGMP
     * destined to multicast group to upstrem interface also if
     * source of the packet is not the sender. (hack for controlled
     * environment)
     */
    void handle_IGMP_packet(PacketInDataHeader* pk_data);

    void handle_UDP_packet(PacketInDataHeader* pk_data);

    void handle_packet_in(PacketInDataHeader* pk_data);

    void send_role_request_equal_msg();


    void create_group(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, 
          uint16_t up_intf, uint16_t is_root);

    void create_group(MCGroup* mcg);

    void join_member_to_group(const MCGroup* mcg, PacketInDataHeader* pk_data, 
                              MCGroupMemberMap* mcg_members);
    void leave_member_from_group(const MCGroup* mcg, PacketInDataHeader* pk_data, 
                              MCGroupMemberMap* mcg_members);
    void delete_group();

private:

    MCGroupMap multicast_session_store_;
    MCGroupMap::iterator multicast_session_store_iterator;
};

MulticastGroupMembershipApp::MulticastGroupMembershipApp() {
    dpid = 0;
}

/*
 * the callback originates from Controller.hh
 * As, this App is executed _on_ switch, only one thread would have been
 * allotted to this OFConnection.
 * TODO: To handle high number of packet_in calls from the switch, spawn worker
 * threads from here. 
 * For now one thread is enough..
 */
void MulticastGroupMembershipApp::event_callback(ControllerEvent* ev) {
    static uint8_t pin_count = 0;
    uint8_t ofversion = ev->ofconn->get_version();
    uint64_t dmac = 0, smac = 0;
    uint16_t eth_type = 0, protocol = 0, buf16 = 0;
    uint32_t sip = 0, dip = 0, membership_code = 0;
    uint16_t in_intf = 0, ip_header_len = 0, usrc_port = 0, udst_port = 0;

    if (ev->get_type() == EVENT_SWITCH_UP) {
        SwitchUpEvent* sue = static_cast<SwitchUpEvent*>(ev);
        if (ofversion == of13::OFP_VERSION) {
            if (ofconn ==  NULL) {
                ofconn = ev->ofconn;
                of13::FeaturesReply *offr = new of13::FeaturesReply(); 
                offr->unpack(sue->data);
                dpid = offr->datapath_id();
                send_role_request_equal_msg();
            }
        }
    }
    else if (ev->get_type() == EVENT_SWITCH_DOWN) {
        std::cout<<"Connection Down for switch ofconn id = "<<ev->ofconn->get_id();
    }

    if (ev->get_type() == EVENT_PACKET_IN) {
        if (ofconn->get_id() !=  ev->ofconn->get_id())
            return;
        PacketInEvent* pi = static_cast<PacketInEvent*>(ev);
        if (ofversion == of13::OFP_VERSION) { 
            of13::PacketIn *ofpi = new of13::PacketIn(); 
            ofpi->unpack(pi->data);
            
            memcpy(((uint8_t*) &dmac) + 2, (uint8_t*) ofpi->data(), 6);
            memcpy(((uint8_t*) &smac) + 2, (uint8_t*) ofpi->data() + 6, 6);
            //check if IP frame else return
            memcpy(((uint8_t*) &eth_type), (uint8_t*) ofpi->data() + 12, 2);
            //std::cout<<__FUNCTION__<<":"<<__LINE__<<" Eth type "<<ntohs(eth_type)<<"\n";
            if (ntohs(eth_type) != ETH_TYPE_IP) 
                return;

            buf16 = 0;
            memcpy(((uint8_t*) &buf16) + 1, (uint8_t*) ofpi->data() + 14, 1);
            ip_header_len = (ntohs(buf16) & 0x0f ) * 4;
            //check if IGMP or UDP protocol else return
            memcpy(((uint8_t*) &buf16) + 1, (uint8_t*) ofpi->data() + 23, 1);
            protocol = ntohs(buf16);
            //std::cout<<__FUNCTION__<<":"<<__LINE__<<" Proto Type "<<protocol<<"\n";
            if (!(protocol == PROTOCOL_TYPE_IGMP || protocol == PROTOCOL_TYPE_UDP))
                return;

            memcpy(((uint8_t*) &sip), (uint8_t*) ofpi->data() + 26, 4);
            memcpy(((uint8_t*) &dip), (uint8_t*) ofpi->data() + 30, 4);
            //std::cout<<__FUNCTION__<<":"<<__LINE__<<" Sip : "<<sip<<"\n";
            //std::cout<<__FUNCTION__<<":"<<__LINE__<<" dip : "<<dip<<"\n";

            if (protocol == PROTOCOL_TYPE_UDP) {
                memcpy(((uint8_t*) & usrc_port), (uint8_t*) ofpi->data() + ETH_HLEN + ip_header_len, 2);
                memcpy(((uint8_t*) & udst_port), (uint8_t*) ofpi->data() + ETH_HLEN + ip_header_len + 2, 2);
                memcpy(((uint8_t*) & membership_code), (uint8_t*) ofpi->data() + ETH_HLEN + ip_header_len + 8, 4);
                membership_code = be64toh(membership_code);
            }
            
            if (protocol == PROTOCOL_TYPE_IGMP) {
                //TODO: get membership code
            }

            if (ofpi->match().in_port() != NULL) {
                in_intf = ofpi->match().in_port()->value();
            }

//--------------TODO: give it to worker thread from here--------

            PacketInDataHeader* pin_data = new PacketInDataHeader(dmac, smac, dip, sip, 
                in_intf, ntohs(udst_port), ntohs(usrc_port), protocol, membership_code, ofpi);
            handle_packet_in(pin_data);
        }
    }
}


/*
 * Standard OpenFlow1.3 does not have IGMPv2 field matching
 * support. two Work arounds...
 * 1. parse the IGMP, get destion IP from IP header or/IGMP
 * multicast address and if session exist but first membership
 * report is received, PACKET_OUT on upstream interface
 * 2. When session initiated inject rule to forward IGMP
 * destined to multicast group to upstrem interface also if
 * source of the packet is not the sender. (hack for controlled
 * environment)
 */
void MulticastGroupMembershipApp::handle_IGMP_packet(PacketInDataHeader* pk_data) {
    std::cout<<__FUNCTION__<<":"<<__LINE__<<(*pk_data)<<"\n";
    MCGroup group_in_pk(pk_data->dip, 0, pk_data->usrc_port, pk_data->udst_port, true, false);
    multicast_session_store_iterator = multicast_session_store_.find(&group_in_pk);
    if(multicast_session_store_iterator == multicast_session_store_.end())
        return;
    if (multicast_session_store_iterator->second->size() == 0) {
        //CONT
    } else {
        
    }

}

void MulticastGroupMembershipApp::handle_UDP_packet(PacketInDataHeader* pk_data) {

    //group according to udp packet
    MCGroup group_in_pk(pk_data->dip, 0, pk_data->usrc_port, pk_data->udst_port, false, false);
    //is there any entry in mutlicast store, should be.. as default is already
    //inserted for this group.. still just match the ip..
    multicast_session_store_iterator = multicast_session_store_.find(&group_in_pk);

    //std::cout<<__FUNCTION__<<":"<<__LINE__<<" group from packet_in information\n";
    //std::cout<<group_in_pk<<"\n";
    
    // if some other random traffic
    if(multicast_session_store_iterator == multicast_session_store_.end()) {
      //  std::cout<<__FUNCTION__<<":"<<__LINE__<<" Random UDP traffic"<<"\n";
        return;
    }

    std::cout<<__FUNCTION__<<":"<<__LINE__<<"sip: "<<ntohl(pk_data->sip)<<"\n";
    std::cout<<__FUNCTION__<<":"<<__LINE__<<"dip: "<<ntohl(pk_data->dip)<<"\n";
    std::cout<<__FUNCTION__<<":"<<__LINE__<< " pk_data : "<<(*pk_data)<<"\n";

    // Atleast ip matches in multicast store, there is a group with the dest ip
    // specified in UDP packet
    //getting the group object
    const MCGroup* mcg = multicast_session_store_iterator->first;
    uint8_t mflag = 0;
    if (mcg->usrc_port() == pk_data->usrc_port && mcg->udst_port() == pk_data->udst_port)
          mflag = 1;
    if (mcg->usrc_port() == pk_data->udst_port && mcg->udst_port() == pk_data->usrc_port)
          mflag = 2;
    if (mflag == 0) {
        std::cout<<__FUNCTION__<<":"<<__LINE__<<" mflag is 0.. returning \n";
        return;
    }

//    std::cout<<__FUNCTION__<<":"<<__LINE__<<"sip: "<<ntohl(pk_data->sip)<<"\n";
//    std::cout<<__FUNCTION__<<":"<<__LINE__<<"dip: "<<ntohl(pk_data->dip)<<"\n";
    std::cout<<__FUNCTION__<<":"<<__LINE__<<" group infor "<<(*mcg);
    std::cout<<__FUNCTION__<<":"<<__LINE__<<"Indirect  group infor "<<*(mcg->mc_group_of_context->mcgroup);
    //getting group members
    MCGroupMemberMap* mcg_members = multicast_session_store_iterator->second;
    MCGroupOpenFlowContext* mcg_of_context = mcg->mc_group_of_context;
    MCGroupMemberMap::iterator mcgm_iter = mcg_members->find(pk_data->in_intf);
    if (mflag == 1) { //Join request
        join_member_to_group(mcg, pk_data, mcg_members);
    } 
    if (mflag == 2) { //leave request
        leave_member_from_group(mcg, pk_data, mcg_members);
    }
}

void MulticastGroupMembershipApp::handle_packet_in(PacketInDataHeader* pk_data) {
    std::cout<<__FUNCTION__<<":"<<__LINE__<<"\n";
    if (pk_data->protocol == PROTOCOL_TYPE_IGMP) {
         handle_IGMP_packet(pk_data);
    }
    else {
         handle_UDP_packet(pk_data);
    }
}

void MulticastGroupMembershipApp::create_group(MCGroup* mcg) {
    std::cout<<"-----------------------------------------------\n";
    std::cout<<__FUNCTION__<<":"<<__LINE__<<"\n";
    
    if (multicast_session_store_.find(mcg) != multicast_session_store_.end()) {
        std::cout<<__FUNCTION__<<":"<<__LINE__<<"Group Already exists : "<<(*mcg)<<"\n";
        return;
    }


    MCGroupOpenFlowContext* mcg_of_context = mcg->create_openflow_context();
    multicast_session_store_[mcg] = new MCGroupMemberMap();

    mcg_of_context->create_downstream_flow_mod_for_group(); 
    if (mcg->is_root()) { //inserting rules to drop packets if the switch is root
        //GroupMod with empty buckets
        mcg_of_context->add_bucket_of13_group(++(MCGroupOpenFlowContext::OFMsgXidGen), NULL, of13::OFPGC_ADD);
        uint8_t* of_group_mod_buffer = mcg_of_context->pack_group_mod_msg();
        ofconn->send(of_group_mod_buffer, mcg_of_context->ofgroupmod.length());
        OFMsg::free_buffer(of_group_mod_buffer);
        std::cout<<"----"<<__FUNCTION__<<"---root switch\n";
        //FlowMod pointinf to the group
        mcg_of_context->insert_downstream_flow_mod_for_group(ofconn);
    }


    //Creating structre not inserting the flow
    //This flow insertion is just a hook to get join/leave message to the
    //controller, so that it doesnt get matched to other rules and pass by
    uint8_t* of_upstream_flow_mod_buffer = mcg_of_context->create_group_hook_flow_mod();
    ofconn->send(of_upstream_flow_mod_buffer, mcg_of_context->upstreamflowmod.length());
    OFMsg::free_buffer(of_upstream_flow_mod_buffer);
    std::cout<<"-----------------------------------------------\n";
}

void MulticastGroupMembershipApp::join_member_to_group(const MCGroup* mcg, 
                                  PacketInDataHeader* pk_data, MCGroupMemberMap* mcg_members) {
    uint8_t command = of13::OFPGC_MODIFY;
    uint8_t* of_group_mod_buffer = NULL;
    MCGroupOpenFlowContext* mcg_of_context = mcg->mc_group_of_context;
    std::cout<<"-----------------------------------------------\n";

    std::cout<<__FUNCTION__<<":"<<__LINE__<<" Join request \n";
    MCGroupMemberMap::iterator mcgm_iter = mcg_members->find(pk_data->in_intf);
    if (mcgm_iter != mcg_members->end()) {// it is already a member
        std::cout<<__FUNCTION__<<":"<<__LINE__<<"already member\n";
        return;
    }
    MCGroupMember* mcgm = new MCGroupMember(pk_data->smac, pk_data->sip, 
                              pk_data->udst_port, pk_data->in_intf, false);
    
    if (mcg_members->size() == 0 && !mcg->is_root()) { //first member
        command = of13::OFPGC_ADD;
        mcg_of_context->send_upstream_packet_out(pk_data, ofconn);
    }

    std::cout<<__FUNCTION__<<":"<<__LINE__<<" bucket-add\n";
    mcg_of_context->add_bucket_of13_group(++(MCGroupOpenFlowContext::OFMsgXidGen), mcgm, command);
    std::cout<<__FUNCTION__<<":"<<__LINE__<<" after insert : "<<mcgm->bucket_index()<<"\n";
    
    of_group_mod_buffer = mcg_of_context->pack_group_mod_msg();
    ofconn->send(of_group_mod_buffer, mcg_of_context->ofgroupmod.length());

    //TODO:might have to send berrier here

    if (mcg_members->size() == 0 && !mcg->is_root()) {
        std::cout<<__FUNCTION__<<":"<<__LINE__<<" First member \n";
        mcg_of_context->insert_downstream_flow_mod_for_group(ofconn);
    }

    (*mcg_members)[pk_data->in_intf] = mcgm;
    OFMsg::free_buffer(of_group_mod_buffer);
    std::cout<<"-----------------------------------------------\n";

}

void MulticastGroupMembershipApp::leave_member_from_group(const MCGroup* mcg,
                                  PacketInDataHeader* pk_data, MCGroupMemberMap* mcg_members) {
    MCGroupOpenFlowContext* mcg_of_context = mcg->mc_group_of_context;
    MCGroupMemberMap::iterator mcgm_iter = mcg_members->find(pk_data->in_intf);

    std::cout<<"-----------------------------------------------\n";
    std::cout<<__FUNCTION__<<":"<<__LINE__<<" Leave request \n";
   
    //group member does not exist, nothing to leave
    if (mcgm_iter == mcg_members->end()) {
        std::cout<<__FUNCTION__<<":"<<__LINE__<<" group member does not exist.. nothing to leave\n";
        return;
    }
    std::cout<<__FUNCTION__<<":"<<__LINE__<<" before remove : "<<mcgm_iter->second->bucket_index()<<"\n";
    uint8_t command = of13::OFPGC_MODIFY;
    if (mcg_members->size() == 1 && !mcg->is_root()) {
        std::cout<<__FUNCTION__<<":"<<__LINE__<<" Last Member \n";
        command = of13::OFPGC_DELETE;
        //send leave request upstream 
        mcg_of_context->send_upstream_packet_out(pk_data, ofconn);
        //Delete downstream flow from the switch
        mcg_of_context->delete_downstream_flow_mod_for_group(ofconn);
    }

    mcg_of_context->remove_bucket_of13_group(++(MCGroupOpenFlowContext::OFMsgXidGen), mcgm_iter->second, command);
    for (MCGroupMemberMap::iterator iter =mcg_members->begin(); iter!= mcg_members->end(); ++iter) {
        if(iter->second->bucket_index()>mcgm_iter->second->bucket_index())
            iter->second->bucket_index(iter->second->bucket_index()-1);
    }

    mcg_members->erase(mcgm_iter);
    uint8_t* of_group_mod_buffer = mcg_of_context->pack_group_mod_msg();
    ofconn->send(of_group_mod_buffer, mcg_of_context->ofgroupmod.length());
    OFMsg::free_buffer(of_group_mod_buffer);

    delete mcgm_iter->second;
    std::cout<<"-----------------------------------------------\n";
}

void MulticastGroupMembershipApp::create_group(uint32_t sip, uint32_t dip, 
        uint16_t sport, uint16_t dport, uint16_t up_intf, uint16_t is_root) {

    struct in_addr daddr, saddr;
    daddr.s_addr = dip;
    saddr.s_addr = sip;

    std::cout<<"--------"<<__FUNCTION__<<":"<<__LINE__<<"-------\n";
    std::cout<<"Source IP : "<<inet_ntoa(saddr)<<"\n";
    std::cout<<"Destination IP : "<<inet_ntoa(daddr)<<"\n";
    std::cout<<"Source Port : "<<sport<<"\n";
    std::cout<<"dst Port : "<<dport<<"\n";
    std::cout<<"intf : "<<up_intf<<"\n";
    std::cout<<"is_root_switch : "<<is_root<<"\n";
    std::cout<<"---------------------------------\n";

    if ((dip & 0xf0000000) == 0xe0000000) {
        std::cout<<"class-D multicast session\n";
        return;
    }
    else {
         MCGroup *mcg = new MCGroup(sip, up_intf, sport, dport, false, (is_root == 0?false:true));
         create_group(mcg);
    }
}


//implement it later for sake of completion
void MulticastGroupMembershipApp::delete_group() {
}

void MulticastGroupMembershipApp::send_role_request_equal_msg() {
    of13::RoleRequest role_request_msg(++(MCGroupOpenFlowContext::OFMsgXidGen), OFPCR_ROLE_EQUAL, 0);
    uint8_t* buf = role_request_msg.pack();
    ofconn->send(buf, role_request_msg.length());
    OFMsg::free_buffer(buf);
}

std::ostream& operator<<(std::ostream& os, const PacketInDataHeader& obj) {
    struct in_addr saddr;
    struct in_addr daddr;
    daddr.s_addr = obj.dip;
    saddr.s_addr = obj.sip;
    char* src = inet_ntoa(saddr);
    char* dst = inet_ntoa(daddr);

    os<<"srcIP="<<src<<" - "<<obj.sip;
    os<<",dstIP="<<dst<<" - "<<obj.dip;
    os<<",SourcePort="<<obj.usrc_port<<",Dest Port="
      <<obj.udst_port<<",in_intf="<<obj.in_intf<<"\n";
    return os;
}


#endif
