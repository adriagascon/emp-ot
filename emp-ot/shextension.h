#ifndef OT_SH_EXTENSION_H__
#define OT_SH_EXTENSION_H__
#include "emp-ot/ot.h"
#include "emp-ot/ot_extension.h"
#include "emp-ot/np.h"
/** @addtogroup OT
  @{
 */
namespace emp {
template<typename IO>
class SHOTExtension: public OTExtension<IO, OTNP, emp::SHOTExtension>{ public:
	SHOTExtension(IO * io) : OTExtension<IO, OTNP, emp::SHOTExtension>(io){
	}
	CRH crh;
	using OTExtension<IO, OTNP, emp::SHOTExtension>::send_pre;
	using OTExtension<IO, OTNP, emp::SHOTExtension>::recv_pre;
	using OTExtension<IO, OTNP, emp::SHOTExtension>::qT;
	using OTExtension<IO, OTNP, emp::SHOTExtension>::tT;
	using OTExtension<IO, OTNP, emp::SHOTExtension>::io;
	using OTExtension<IO, OTNP, emp::SHOTExtension>::block_s;

	void got_send_post(const block* data0, const block* data1, int length) {
		const int bsize = AES_BATCH_SIZE/2;
		block pad[2*bsize];
		for(int i = 0; i < length; i+=bsize) {
			for(int j = i; j < i+bsize and j < length; ++j) {
				pad[2*(j-i)] = qT[j];
				pad[2*(j-i)+1] = xorBlocks(qT[j], block_s);
			}
			crh.H<2*bsize>(pad, pad);
			for(int j = i; j < i+bsize and j < length; ++j) {
				pad[2*(j-i)] = xorBlocks(pad[2*(j-i)], data0[j]);
				pad[2*(j-i)+1] = xorBlocks(pad[2*(j-i)+1], data1[j]);
			}
			io->send_data(pad, 2*sizeof(block)*min(bsize,length-i));
		}
		delete[] qT;
	}

	void got_recv_post(block* data, const bool* r, int length) {
		const int bsize = AES_BATCH_SIZE;
		block res[2*bsize];
		for(int i = 0; i < length; i+=bsize) {
			io->recv_data(res, 2*sizeof(block)*min(bsize,length-i));
			if (bsize <= length-i) crh.H<bsize>(tT+i, tT+i);
			else crh.Hn(tT+i, tT+i, length-i);
			for(int j = 0; j < bsize and j < length-i; ++j) {
				data[i+j] = xorBlocks(res[2*j+r[i+j]], tT[i+j]);
			}
		}
		delete[] tT;
	}

  template<typename F>
  void cot_send_post_ft(block* data0, F f, int length) {
    const int bsize = AES_BATCH_SIZE/2;
		block pad[2*bsize];
		block tmp[2*bsize];
		for(int i = 0; i < length; i+=bsize) {
			for(int j = i; j < i+bsize and j < length; ++j) {
				pad[2*(j-i)] = qT[j];
				pad[2*(j-i)+1] = xorBlocks(qT[j], block_s);
			}
			crh.H<2*bsize>(pad, pad);
			for(int j = i; j < i+bsize and j < length; ++j) {
				data0[j] = pad[2*(j-i)];
        pad[2*(j-i)] = f(data0[j], j);
				tmp[j-i] = xorBlocks(pad[2*(j-i)+1], pad[2*(j-i)]);
			}
			io->send_data(tmp, sizeof(block)*min(bsize,length-i));
		}
		delete[] qT;
  }

  void cot_send_post_f(block* data0, std::function<block(block, uint64_t)> f, int length) {
    const int bsize = AES_BATCH_SIZE/2;
		block pad[2*bsize];
		block tmp[2*bsize];
		for(int i = 0; i < length; i+=bsize) {
			for(int j = i; j < i+bsize and j < length; ++j) {
				pad[2*(j-i)] = qT[j];
				pad[2*(j-i)+1] = xorBlocks(qT[j], block_s);
			}
			crh.H<2*bsize>(pad, pad);
			for(int j = i; j < i+bsize and j < length; ++j) {
				data0[j] = pad[2*(j-i)];
        pad[2*(j-i)] = f(data0[j], j);
				tmp[j-i] = xorBlocks(pad[2*(j-i)+1], pad[2*(j-i)]);
			}
			io->send_data(tmp, sizeof(block)*min(bsize,length-i));
		}
		delete[] qT;
  }

  void cot_send_post_fs(block* data0, std::vector<std::function<block(block)>> fs, int length) {
    const int bsize = AES_BATCH_SIZE/2;
		block pad[2*bsize];
		block tmp[2*bsize];
		for(int i = 0; i < length; i+=bsize) {
			for(int j = i; j < i+bsize and j < length; ++j) {
				pad[2*(j-i)] = qT[j];
				pad[2*(j-i)+1] = xorBlocks(qT[j], block_s);
			}
			crh.H<2*bsize>(pad, pad);
			for(int j = i; j < i+bsize and j < length; ++j) {
				data0[j] = pad[2*(j-i)];
        pad[2*(j-i)] = fs[j](data0[j]);
				tmp[j-i] = xorBlocks(pad[2*(j-i)+1], pad[2*(j-i)]);
			}
			io->send_data(tmp, sizeof(block)*min(bsize,length-i));
		}
		delete[] qT;
	}

  void cot_send_post_add_delta(block * data0, std::pair<uint64_t, uint64_t> * deltas, int length) {
		const int bsize = AES_BATCH_SIZE/2;
		block pad[2*bsize];
		block tmp[2*bsize];
		for(int i = 0; i < length; i+=bsize) {
			for(int j = i; j < i+bsize and j < length; ++j) {
				pad[2*(j-i)] = qT[j];
				pad[2*(j-i)+1] = xorBlocks(qT[j], block_s);
			}
			crh.H<2*bsize>(pad, pad);
			for(int j = i; j < i+bsize and j < length; ++j) {
				data0[j] = pad[2*(j-i)];
        pad[2*(j-i)] = makeBlock((uint64_t)(pad[2*(j-i)][1]) + deltas[j].second,
          (uint64_t)(pad[2*(j-i)][0]) + deltas[j].first);
				tmp[j-i] = xorBlocks(pad[2*(j-i)+1], pad[2*(j-i)]);
			}
			io->send_data(tmp, sizeof(block)*min(bsize,length-i));
		}
		delete[] qT;
	}

  void cot_send_post(block* data0, block delta, int length) {
		const int bsize = AES_BATCH_SIZE/2;
		block pad[2*bsize];
		block tmp[2*bsize];
		for(int i = 0; i < length; i+=bsize) {
			for(int j = i; j < i+bsize and j < length; ++j) {
				pad[2*(j-i)] = qT[j];
				pad[2*(j-i)+1] = xorBlocks(qT[j], block_s);
			}
			crh.H<2*bsize>(pad, pad);
			for(int j = i; j < i+bsize and j < length; ++j) {
				data0[j] = pad[2*(j-i)];
				pad[2*(j-i)] = xorBlocks(pad[2*(j-i)], delta);
				tmp[j-i] = xorBlocks(pad[2*(j-i)+1], pad[2*(j-i)]);
			}
			io->send_data(tmp, sizeof(block)*min(bsize,length-i));
		}
		delete[] qT;
	}

	void cot_recv_post(block* data, const bool* r, int length) {
		const int bsize = AES_BATCH_SIZE;
		block res[bsize];
		for(int i = 0; i < length; i+=bsize) {
			io->recv_data(res, sizeof(block)*min(bsize,length-i));
			if (bsize <= length-i) crh.H<bsize>(data+i, tT+i);
			else crh.Hn(data+i, tT+i, length-i);
			for(int j = 0; j < bsize and j < length-i; ++j) {
				if(r[i+j]) data[i+j] = xorBlocks(res[j], data[i+j]);
			}
		}
		delete[] tT;
	}

	void rot_send_post(block* data0, block* data1, int length) {
		const int bsize = AES_BATCH_SIZE/2;
		block pad[2*bsize];
		for(int i = 0; i < length; i+=bsize) {
			for(int j = i; j < i+bsize and j < length; ++j) {
				pad[2*(j-i)] = qT[j];
				pad[2*(j-i)+1] = xorBlocks(qT[j], block_s);
			}
			crh.H<2*bsize>(pad, pad);
			for(int j = i; j < i+bsize and j < length; ++j) {
				data0[j] = pad[2*(j-i)];
				data1[j] = pad[2*(j-i)+1];
			}
		}
		delete[] qT;
	}

	void rot_recv_post(block* data, const bool* r, int length) {
		const int bsize = AES_BATCH_SIZE;
		for(int i = 0; i < length; i+=bsize) {
			if (bsize <= length-i) crh.H<bsize>(data+i, tT+i);
			else crh.Hn(data+i, tT+i, length-i);
		}
		delete[] tT;
	}

	void send_impl(const block* data0, const block* data1, int length) {
		send_pre(length);
		got_send_post(data0, data1, length);
	}

	void recv_impl(block* data, const bool* b, int length) {
		recv_pre(b, length);
		got_recv_post(data, b, length);
	}

  void send_cot(block * data0, block delta, int length) {
		send_pre(length);
		cot_send_post(data0, delta, length);
	}
  void send_cot_fs(block * data0, std::vector<std::function<block(block)>> fs, int length) {
		send_pre(length);
		cot_send_post_fs(data0, fs, length);
	}
  void send_cot_f(block * data0, std::function<block(block, uint64_t)> f, int length) {
		send_pre(length);
		cot_send_post_f(data0, f, length);
	}
  template <typename F>
  void send_cot_ft(block * data0, F f, int length) {
    send_pre(length);
    cot_send_post_ft(data0, f, length);
  }
  void send_cot_add_delta(block * data0, std::pair<uint64_t, uint64_t> * deltas, int length) {
    send_pre(length);
    cot_send_post_add_delta(data0, deltas, length);
  }
	void recv_cot(block* data, const bool* b, int length) {
		recv_pre(b, length);
		cot_recv_post(data, b, length);
	}
	void send_rot(block * data0, block * data1, int length) {
		send_pre(length);
		rot_send_post(data0, data1, length);
	}
	void recv_rot(block* data, const bool* b, int length) {
		recv_pre(b, length);
		rot_recv_post(data, b, length);
	}
};
/**@}*/
}
#endif// OT_EXTENSION_H__
