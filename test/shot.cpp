#include "test/test.h"
#include <iostream>
using namespace std;

int main(int argc, char** argv) {
	int length = 1<<24, port, party;
  parse_party_and_port(argv, 2, &party, &port);
	NetIO * io = new NetIO(party==ALICE ? nullptr:"127.0.0.1", port);
	cout << "NPOT\t"<<10000.0/test_ot<NetIO, OTNP>(io, party, 10000)*1e6<<" OTps"<<endl;
	cout << "Semi Honest OT Extension\t"<<double(length)/test_ot<NetIO, SHOTExtension>(io, party, length)*1e6<<" OTps"<<endl;
	cout << "Semi Honest COT Extension\t"<<double(length)/test_cot<NetIO, SHOTExtension>(io, party, length)*1e6<<" OTps"<<endl;
  cout << "Semi Honest COT Extension (arbitrary correlation, vector<function>)\t"<<double(length)/test_cot_fs<NetIO, SHOTExtension>(io, party, length)*1e6<<" OTps"<<endl;
  cout << "Semi Honest COT Extension (arbitrary correlation, singlefunction)\t"<<double(length)/test_cot_f<NetIO, SHOTExtension>(io, party, length)*1e6<<" OTps"<<endl;
  cout << "Semi Honest COT Extension (arbitrary correlation, templated-singlefunction)\t"<<double(length)/test_cot_ft<NetIO, SHOTExtension>(io, party, length)*1e6<<" OTps"<<endl;
  cout << "Semi Honest COT Extension (2 x 64-bit Integer addition)\t"<<double(length)/test_cot_add_deltas<NetIO, SHOTExtension>(io, party, length)*1e6<<" OTps"<<endl;
	cout << "Semi Honest ROT Extension\t"<<double(length)/test_rot<NetIO, SHOTExtension>(io, party, length)*1e6<<" OTps"<<endl;
	delete io;
}
