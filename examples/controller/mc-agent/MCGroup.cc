#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <fluid/of10msg.hh>
#include <fluid/of13msg.hh>
#include "MCGroupMember.hh"
#include "MCGroup.hh"

using namespace fluid_msg;

uint32_t MCGroupOpenFlowContext::OFMsgXidGen = 0;
uint32_t MCGroupOpenFlowContext::OFMsgGroupIdGen = 0;
uint64_t MCGroupOpenFlowContext::OFMsgCookieGen = 0;


MCGroupOpenFlowContext::MCGroupOpenFlowContext(MCGroup* mcg) {
   mcgroup = mcg; 
   of_flow_mod_buffer = NULL;
}

void MCGroupOpenFlowContext::add_bucket_of13_group(uint32_t xid, MCGroupMember* mtmg, uint8_t command) {
    ofgroupmod.xid(xid);
    ofgroupmod.commmand(command);
    ofgroupmod.group_type(of13::OFPGT_ALL);
    ofgroupmod.group_id(mcgroup->groupid_);

    if (mtmg != NULL) {
        of13::Bucket member_bucket(0xffff, 0xffffffff, 0xffffffff);
        uint64_t dmac = mtmg->dest_mac();
        of13::EthType *ethtype = new of13::EthType(0x0800);
        of13::EthDst *ethdst = new of13::EthDst(EthAddress((uint8_t*) (&dmac) + 2));
        of13::IPv4Dst *ipv4dst = new of13::IPv4Dst(IPAddress(mtmg->dest_ip()));
        of13::UDPDst *udpdst = new of13::UDPDst(mtmg->udp_port());

        of13::SetFieldAction sfu(udpdst);
        of13::SetFieldAction sfe(ethdst);
        of13::SetFieldAction sfi(ipv4dst);
        of13::OutputAction outact(mtmg->intf(), 0);
        sfu.set_order(161);
        sfi.set_order(162);
        sfe.set_order(163);
    
        member_bucket.add_action(sfu);
        member_bucket.add_action(sfi);
        member_bucket.add_action(sfe);
        member_bucket.add_action(outact);

        ofgroupmod.add_bucket(member_bucket);
        mtmg->bucket_index(member_of13_buckets.size());
        member_of13_buckets.push_back(member_bucket);
        std::cout<<__FUNCTION__<<":"<<__LINE__<<" Group ID : "<<mcgroup->groupid_<<"\n";
        std::cout<<__FUNCTION__<<":"<<__LINE__<<" Adding Group Member IP : "<<mtmg->dest_ip()<<"\n";
        std::cout<<__FUNCTION__<<":"<<__LINE__<<" bucket_index : "<<mtmg->bucket_index()<<"\n";
        std::cout<<__FUNCTION__<<":"<<__LINE__<<" members bucket size : "<<member_of13_buckets.size()<<"\n";
    }
}

void MCGroupOpenFlowContext::remove_bucket_of13_group(uint32_t xid, const MCGroupMember* mtmg, uint8_t command) {
    ofgroupmod.xid(xid);
    ofgroupmod.commmand(command);
    size_t bi = mtmg->bucket_index();
    std::cout<<__FUNCTION__<<":"<<__LINE__<<" Group ID : "<<mcgroup->groupid_<<"\n";
    std::cout<<__FUNCTION__<<":"<<__LINE__<<" Removing Group Member IP : "<<mtmg->dest_ip()<<"\n";
    std::cout<<__FUNCTION__<<":"<<__LINE__<<" bucket_index retrieved : "<<mtmg->bucket_index()<<"\n";
    std::cout<<__FUNCTION__<<":"<<__LINE__<<" size : "<<member_of13_buckets.size()<<"\n";
    if(member_of13_buckets.size() <= bi) {
      std::cout<<__FUNCTION__<<":"<<__LINE__<<" ERROR: removing member : "<<mtmg->dest_ip()<<"\n";
      return;
    }
    member_of13_buckets.erase(member_of13_buckets.begin() + bi);
    ofgroupmod.del_bucket(bi);
}

void MCGroupOpenFlowContext::delete_downstream_flow_mod_for_group(OFConnection* ofconn) {
    offlowmod.command(of13::OFPFC_DELETE);
    of_flow_mod_buffer = offlowmod.pack();
    ofconn->send(of_flow_mod_buffer, offlowmod.length());
    OFMsg::free_buffer(of_flow_mod_buffer);
}

void MCGroupOpenFlowContext::insert_downstream_flow_mod_for_group(OFConnection* ofconn) {
    offlowmod.command(of13::OFPFC_ADD);
    of_flow_mod_buffer = offlowmod.pack();
    ofconn->send(of_flow_mod_buffer, offlowmod.length());
    OFMsg::free_buffer(of_flow_mod_buffer);
}

void MCGroupOpenFlowContext::create_downstream_flow_mod_for_group() {
    offlowmod.xid(++OFMsgXidGen);
    offlowmod.cookie(++OFMsgCookieGen);
    offlowmod.cookie_mask(0xffffffffffffffff);
    offlowmod.table_id(0);
    offlowmod.command(of13::OFPFC_ADD);
    offlowmod.idle_timeout(0); 
    offlowmod.hard_timeout(0);
    
    offlowmod.priority(10);
    offlowmod.buffer_id(0xffffffff);
    offlowmod.out_port(0);
    offlowmod.out_group(mcgroup->groupid_);
    
    //For unicast source IP and UDP port match, 
    if (!mcgroup->is_classd_) {
        of13::EthType etype(0x0800);
        of13::IPProto ipp(17);
        of13::IPv4Src ipsrcm(IPAddress (mcgroup->groupip_));
        of13::UDPSrc udpsrcm(mcgroup->sudpport_);
        of13::UDPDst udpdstm(mcgroup->dudpport_);
        offlowmod.add_oxm_field(etype);
        offlowmod.add_oxm_field(ipp);
        offlowmod.add_oxm_field(ipsrcm);
        offlowmod.add_oxm_field(udpsrcm);
        offlowmod.add_oxm_field(udpdstm);
    } else { //group is using class-D ip for multicast
        of13::IPv4Dst ipdstm(IPAddress (mcgroup->groupip_));
        offlowmod.add_oxm_field(ipdstm);
    }

    of13::GroupAction groupact(mcgroup->groupid_);
    of13::ApplyActions inst;
    inst.add_action(groupact);
    offlowmod.add_instruction(inst);
}

/*
 * This function installs upstream flow for join/leave mechanism.
 * For multicasting without class-D, packet with destination IP matching gs's
 * IP and <src, dst> udp pair matching gs's pair is considered as join
 * request,
 * packet <src, dst>  = gs's <dst, src> for leave
 */
uint8_t* MCGroupOpenFlowContext::create_group_hook_flow_mod() {
    uint8_t* buffer;
    upstreamflowmod.xid(++OFMsgXidGen);
    upstreamflowmod.cookie(++OFMsgCookieGen);
    upstreamflowmod.cookie_mask(0xffffffffffffffff);
    upstreamflowmod.table_id(0);
    upstreamflowmod.command(of13::OFPFC_ADD);
    upstreamflowmod.idle_timeout(0); 
    upstreamflowmod.hard_timeout(0);
    
    upstreamflowmod.priority(1000);
    upstreamflowmod.buffer_id(0xffffffff);
    upstreamflowmod.out_port(0);
    upstreamflowmod.out_group(0);
    
    if (!mcgroup->is_classd_) {
        of13::EthType etype(0x0800);
        of13::IPProto ipp(17);
        of13::IPv4Dst ipdstm(IPAddress (mcgroup->groupip_));
        //of13::UDPSrc udpsrcm(mcgroup->sudpport_);
        //of13::UDPDst udpdstm(mcgroup->dudpport_);
        upstreamflowmod.add_oxm_field(etype);
        upstreamflowmod.add_oxm_field(ipp);
        upstreamflowmod.add_oxm_field(ipdstm);
        //upstreamflowmod.add_oxm_field(udpsrcm);
        //upstreamflowmod.add_oxm_field(udpdstm);
    } else { //group is using class-D ip for multicast
        of13::IPv4Dst ipdstm(IPAddress (mcgroup->groupip_));
        upstreamflowmod.add_oxm_field(ipdstm);
    }

    //of13::OutputAction up_stream_outact(mcgroup->upstream_intf(), of13::OFPCML_NO_BUFFER);
    of13::OutputAction up_controller_outact(of13::OFPP_CONTROLLER, 512);
    of13::GroupAction groupact(mcgroup->groupid_);
    of13::ApplyActions inst;
    //inst.add_action(up_stream_outact);
    inst.add_action(up_controller_outact);
    upstreamflowmod.add_instruction(inst);
    buffer = upstreamflowmod.pack();
    return buffer;

}

void  MCGroupOpenFlowContext::send_upstream_packet_out(PacketInDataHeader *pk, OFConnection* ofconn) {

    of13::PacketOut po(pk->ofpi->xid(), pk->ofpi->buffer_id(), pk->in_intf);
    if (pk->ofpi->buffer_id() == -1) {
        po.data(pk->ofpi->data(), pk->ofpi->data_len());
    }
    of13::OutputAction act(mcgroup->upstream_intf(), 1024);
    po.add_action(act);
    uint8_t *buf = po.pack();
    ofconn->send(buf, po.length());
    OFMsg::free_buffer(buf);
}

MCGroupOpenFlowContext* MCGroup::create_openflow_context() {
    mc_group_of_context = new  MCGroupOpenFlowContext(this);
    return mc_group_of_context;
}


std::ostream& operator<<(std::ostream& os, const MCGroup& obj)
{
    struct in_addr addr;
    addr.s_addr = obj.groupip_;
    os<<"GroupIP="<<inet_ntoa(addr)<<",groupID="<<obj.groupid_;
    os<<",SourcePort="<<obj.sudpport_<<",DestPort="<<obj.dudpport_;
    os<<",up_stream_intf="<<obj.upstream_intf_<<"\n";
    return os;
}
 
