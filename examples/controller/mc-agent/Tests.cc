#include "MCGroup.hh"

void MCGroupLessThanOperatorKeyTest(void);
int main(int argc, char **argv) {

     MCGroupLessThanOperatorKeyTest();

    return 0;
}

void MCGroupLessThanOperatorKeyTest(void) {
    std::map<MCGroup, int> mdb;

    MCGroup mg1(30, 0, 6633, true);
    MCGroup mg2(31, 0, 6633, true);
    MCGroup mg3(30, 0, 6634, true);
    MCGroup mg4(31, 0, 6634, true);
    MCGroup mg5(30, 0, 6633, false);
    MCGroup mg6(31, 0, 6633, false);
    MCGroup mg7(30, 0, 6634, false);
    MCGroup mg8(31, 0, 6634, false);
    MCGroup mg9(32, 0, 6634, false);
    MCGroup mg(32, 0, 6634, true);

    mdb[mg1] = 1;
    mdb[mg2] = 2;
    mdb[mg3] = 3;
    mdb[mg4] = 4;
    mdb[mg5] = 5;
    mdb[mg6] = 6;
    mdb[mg7] = 7;
    mdb[mg8] = 8;
    mdb[mg9] = 9;

/*
    for (std::map<MCGroup,int>::iterator it=mdb.begin(); it!=mdb.end(); it++)
        std::cout <<std::dec<< it->second << '\n';
*/
    if (mdb.size() != 7) {
        std::cout<<"UT0: size = "<< mdb.size() <<"Find on a map with MCGroup as a key failed\n";
    }

    if (mdb.find(mg1)->second != 1 || mdb.find(mg2)->second != 2 ||
        mdb.find(mg3)->second != 3 || mdb.find(mg4)->second != 4 ||
        mdb.find(mg7)->second != 7 || mdb.find(mg8)->second != 8 ) {
        std::cout<<"UT1: Find on a map with MCGroup as a key failed\n";
    }

    if (mdb.find(mg) != mdb.end()) {
        std::cout<<"UT2: Find on a map with MCGroup as a key failed\n";
    }
}
