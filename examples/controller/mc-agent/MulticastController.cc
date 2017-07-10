#include <stdlib.h>
#include <vector>
#include "MCApps.hh"
#include "MCNWClient.hh"

std::vector<std::string> parse_commandline_args(int argc, char**argv);

int main(int argc, char **argv) {
    
    std::vector<std::string> args = parse_commandline_args(argc, argv);
    if (args.size() == 1) {
        Controller ctrl("0.0.0.0", 6653, 2);
        CBench cbench;
        ctrl.register_for_event(&cbench, EVENT_PACKET_IN);
        printf("cbench (RawCBench) application started\n");
        ctrl.start();
        wait_for_sigint();
        ctrl.stop();
    }
    if (args.size() == 4) {
        Controller ctrl("0.0.0.0", atoi(args[3].c_str()), 2);
        printf("mc-app ...\n");
        MCNWClientConnection* mcnw_cc = new MCNWClientConnection(args[1], (uint16_t)atoi(args[2].c_str()) );
        std::cout<<"contrl reach here\n";
        MulticastGroupMembershipApp* mcapp = new MulticastGroupMembershipApp();
        ctrl.register_for_event(mcapp, EVENT_PACKET_IN);
        ctrl.register_for_event(mcapp, EVENT_SWITCH_UP);
        ctrl.register_for_event(mcapp, EVENT_SWITCH_DOWN);
        mcnw_cc->set_multicast_group_membership_app(mcapp);
        ctrl.start();
        while (mcapp->datapath_id() == 0) {
            sleep(2);
        }
        std::cout<<"Datapath id = "<<std::hex<<mcapp->datapath_id()<<std::dec<<"\n";
        mcnw_cc->run();
        wait_for_sigint();
        ctrl.stop();
        mcnw_cc->stop();
    }
    return 0;
}

std::vector<std::string> parse_commandline_args(int argc, char**argv) {
    std::vector<std::string>  args;
    if(argc < 2) {
        std::cout<<"Choose an application to run (\"mc-app\" or \"cbench\"):\n";
        std::cout<<"  ./multicast_controller mc-app|cbench ...\n";
        exit(1);
    }

    if (!strcmp(argv[1], "cbench")) {
        args.push_back(std::string(argv[1]));
        return args;
    }

    if (!strcmp(argv[1], "mc-app")) {
        if (argc != 5) {
            std::cout<<"./multicast_controller mc-app <network controller server ip> <network controller server port> <OpenFlow port>\n";
            exit(1);
        }
    }
    args.push_back(std::string(argv[1]));

    struct in_addr addr;
    if (inet_aton(argv[2], &addr) == 0) {
        std::cout<<"Invalid address\n";
        exit(EXIT_FAILURE);
    }
    args.push_back(std::string(argv[2]));

    if (atoi(argv[3]) > 65535 || atoi(argv[3]) < 1000 ) {
        std::cout<<"Invalid port\n";
        exit(EXIT_FAILURE);
    }
    args.push_back(std::string(argv[3]));

    if (atoi(argv[4]) > 65535 || atoi(argv[4]) < 1000 ) {
        std::cout<<"Invalid port\n";
        exit(EXIT_FAILURE);
    }
    args.push_back(std::string(argv[4]));

    return args;
}
