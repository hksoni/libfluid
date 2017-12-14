CC=cc
CXX=c++
RUN_FLAGS=-I. -O3 -std=c++0x -lpthread
DBG_FLAGS=-I. -g  -std=c++0x -lpthread

all: msg_controller raw_controller  mc_controller
all_for_reals: all loci_controller

mc_controller:
	cd mc-agent; $(CXX) -c  MCGroup.cc  -I../ $(RUN_FLAGS) ;cd ../
	$(CXX) mc-agent/MulticastController.cc mc-agent/MCGroup.o -levent -lfluid_msg -lfluid_base $(RUN_FLAGS) -o mc_controller
#	$(CXX) mc-agent/Tests.cc mc-agent/MCGroup.o -lfluid_msg -lfluid_base $(RUN_FLAGS) -o utest_mc_controller

msg_controller:
	$(CXX) msg/MsgController.cc -lfluid_msg -lfluid_base $(RUN_FLAGS) -o msg_controller

raw_controller:
	$(CXX) raw/RawController.cc -I./raw -lfluid_base $(RUN_FLAGS) -o raw_controller

secure_controller:
	$(CXX) secure/SecureController.cc -I./raw -lfluid_base $(RUN_FLAGS) -o secure_controller
	
loci_controller:
	cd loci/loci; $(CC) -c src/*.c -Iinc
	$(CXX) loci/LociController.cc loci/loci/*.o -Iloci/loci/inc -lfluid_base $(RUN_FLAGS) -o loci_controller
	
# TODO: don't duplicate the code above
mc_controller_debug:
	cd mc-agent; $(CXX) -c  MCGroup.cc  -I../ $(DBG_FLAGS) ;cd ../
	$(CXX) mc-agent/MulticastController.cc mc-agent/MCGroup.o -levent -lfluid_msg -lfluid_base $(DBG_FLAGS) -o mc_controller
#	$(CXX) mc-agent/Tests.cc mc-agent/MCGroup.o -lfluid_msg -lfluid_base $(DBG_FLAGS) -o utest_mc_controller

msg_controller_debug:
	$(CXX) msg/MsgController.cc -lfluid_msg -lfluid_base $(DBG_FLAGS) -o msg_controller

raw_controller_debug:
	$(CXX) raw/RawController.cc -I./raw -lfluid_base $(DBG_FLAGS) -o raw_controller

secure_controller_debug:
	$(CXX) secure/SecureController.cc -I./raw -lfluid_base $(DBG_FLAGS) -o secure_controller
	
loci_controller_debug:
	$(CXX) loci/LociApps.cc loci/loci/src/*.c -I.loci/loci/inc -lfluid_base $(DBG_FLAGS) -o loci_controller


clean:
	rm -f msg_controller
	rm -f raw_controller
	rm -f secure_controller
	rm -f loci_controller
	rm -f *.gch

.PHONY : mc_controller msg_controller raw_controller secure_controller loci_controller clean
