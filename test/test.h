#include "emp-ot/emp-ot.h"
#include <emp-tool/emp-tool.h>
#include <iostream>
#include <vector>
#include <utility>
using namespace emp;

template <typename IO, template <typename> class T>
double test_ot(IO *io, int party, int length) {
  block *b0 = new block[length], *b1 = new block[length],
        *r = new block[length];
  PRG prg(fix_key);
  prg.random_block(b0, length);
  prg.random_block(b1, length);
  bool *b = new bool[length];
  prg.random_bool(b, length);

  io->sync();
  auto start = clock_start();
  T<IO> *ot = new T<IO>(io);
  if (party == ALICE) {
    ot->send(b0, b1, length);
  } else {
    ot->recv(r, b, length);
  }
  io->flush();
  long long t = time_from(start);
  if (party == BOB) {
    for (int i = 0; i < length; ++i) {
      if (b[i])
        assert(block_cmp(&r[i], &b1[i], 1));
      else
        assert(block_cmp(&r[i], &b0[i], 1));
    }
  }
  std::cout << "Tests passed.\n";
  delete ot;
  delete[] b0;
  delete[] b1;
  delete[] r;
  delete[] b;
  return t;
}

// test BIT ot - only expecting to receive back same *bit* sent
// length is the number of OT's, *not* the length of each message
// (each message is usually one block--but, in this case, each
// is a single bit: the LSB of each block)
template <typename IO, template <typename, int> class T, int N_BITS>
double test_bit_ot(IO *io, int party, int length) {
  if (party == ALICE)
    std::cout << "Testing: " << length << " many " << N_BITS << "-bit OT's\n";
  block *b0 = new block[length], *b1 = new block[length],
        *r = new block[length];
  PRG prg(fix_key);
  prg.random_block(b0, length);
  prg.random_block(b1, length);
  bool *b = new bool[length];
  prg.random_bool(b, length);

  io->sync();
  auto start = clock_start();
  T<IO, N_BITS> *ot = new T<IO, N_BITS>(io);
  if (party == ALICE) {
    ot->send(b0, b1, length);
  } else {
    ot->recv(r, b, length);
  }
  io->flush();
  long long t = time_from(start);
  if (party == BOB) {
    int failures = 0;
    int i = 0;
  nextcmp:
    for (; i < length; ++i) {
      int received[4];
      received[0] = _mm_extract_epi32(r[i], 0);
      received[1] = _mm_extract_epi32(r[i], 1);
      received[2] = _mm_extract_epi32(r[i], 2);
      received[3] = _mm_extract_epi32(r[i], 3);
      int expected[4];
      if (b[i] == 0) { // requested item 0
        expected[0] = _mm_extract_epi32(b0[i], 0);
        expected[1] = _mm_extract_epi32(b0[i], 1);
        expected[2] = _mm_extract_epi32(b0[i], 2);
        expected[3] = _mm_extract_epi32(b0[i], 3);
      } else { // b[i] == 1
        expected[0] = _mm_extract_epi32(b1[i], 0);
        expected[1] = _mm_extract_epi32(b1[i], 1);
        expected[2] = _mm_extract_epi32(b1[i], 2);
        expected[3] = _mm_extract_epi32(b1[i], 3);
      }

      std::cout << std::hex;

      for (int subblk = 0; subblk < std::ceil(N_BITS / 32.0); ++subblk) {
        int n_remaining_bits = N_BITS - (subblk * 32);
        if (n_remaining_bits >= 32) {
          if (received[subblk] != expected[subblk]) {
            std::cout << "Failure: Got quarter-block " << received[subblk]
                      << " on branch " << b[i] << " but expected "
                      << expected[subblk] << std::endl;
            ++failures;
            ++i;
            goto nextcmp;
          }
        } else {
          unsigned mask;
          if (n_remaining_bits == 31)
            mask = -1;
          else
            mask = (1 << n_remaining_bits) - 1;
          if ((received[subblk] & mask) != (expected[subblk] & mask)) {
            std::cout << "Failure: Got quarter-block "
                      << (received[subblk] & mask) << " on branch " << b[i]
                      << " but expected " << (expected[subblk] & mask)
                      << std::endl;
            ++failures;
          }
        }
      }
    }
    if (!failures) {
      std::cout << "Passed " << std::dec << length << " tests.\n";
    } else {
      std::cout << std::dec << "Failed " << failures << " of " << length
                << " tests.\n";
    }
  }
  delete ot;
  delete[] b0;
  delete[] b1;
  delete[] r;
  delete[] b;
  return t;
}

template <typename IO, template <typename> class T>
double test_cot(IO *io, int party, int length) {
  block *b0 = new block[length], *r = new block[length];
  bool *b = new bool[length];
  block delta;
  PRG prg(fix_key);
  prg.random_block(&delta, 1);
  prg.random_bool(b, length);

  io->sync();
  auto start = clock_start();
  T<IO> *ot = new T<IO>(io);
  if (party == ALICE) {
    ot->send_cot(b0, delta, length);
  } else {
    ot->recv_cot(r, b, length);
  }
  io->flush();
  long long t = time_from(start);
  if (party == ALICE)
    io->send_block(b0, length);
  else if (party == BOB) {
    io->recv_block(b0, length);
    for (int i = 0; i < length; ++i) {
      block b1 = xorBlocks(b0[i], delta);
      if (b[i]) {
        if (!block_cmp(&r[i], &b1, 1))
          error("COT failed!");
      } else {
        if (!block_cmp(&r[i], &b0[i], 1))
          error("COT failed!");
      }
    }
  }
  io->flush();
  delete ot;
  delete[] b0;
  delete[] r;
  delete[] b;
  return t;
}

template <typename IO, template <typename> class T>
double test_cot_fs(NetIO *io, int party, int length) {
  block *b0 = new block[length], *r = new block[length];
  bool *b = new bool[length];
  std::vector<std::function<block(block)>> fs(length);
  for (int i = 0; i < length; ++i) {
    uint64_t x = 42;
    auto f = [x](block s0) {
      uint64_t v1 = (uint64_t) s0[1];
      uint64_t v2 = (uint64_t) s0[0];
      return makeBlock(v1 + x, v2 - x);
    };
    fs[i] = f;
  }
  block delta;
  PRG prg(fix_key);
  prg.random_block(&delta, 1);
  prg.random_bool(b, length);

  io->sync();
  auto start = clock_start();
  T<IO> *ot = new T<IO>(io);
  if (party == ALICE) {
    ot->send_cot_fs(b0, fs, length);
  } else {
    ot->recv_cot(r, b, length);
  }
  io->flush();
  long long t = time_from(start);
  if (party == ALICE){
    io->send_block(b0, length);
  } else if (party == BOB) {
    io->recv_block(b0, length);
    for (int i = 0; i < length; ++i) {
      block m1 = fs[i](b0[i]);
      // std::cout << b[i] << std::endl;
      // std::cout << (uint64_t)(m0[0]) << "|" << (uint64_t)(m0[1]) << std::endl;
      // std::cout << (uint64_t)(m1[0]) << "|" << (uint64_t)(m1[1]) << std::endl;
      // std::cout << (uint64_t)(res[0]) << "|" << (uint64_t)(res[1]) << std::endl;
      if (b[i]) {
        if (!block_cmp(&r[i], &m1, 1))
          error("COT failed!\n");
      } else {
        if (!block_cmp(&r[i], &b0[i], 1))
          error("COT failed!\n");
      }
    }
  }
  io->flush();
  delete ot;
  delete[] b0;
  delete[] r;
  delete[] b;
  return t;
}

template <typename IO, template <typename> class T>
double test_cot_ft(NetIO *io, int party, int length) {
  block *b0 = new block[length], *r = new block[length];
  bool *b = new bool[length];
  auto f = [](block s0, uint64_t i) {
    uint64_t v1 = (uint64_t) s0[1];
    uint64_t v2 = (uint64_t) s0[0];
    return makeBlock(v1 + i, v2 - i);
  };
  block delta;
  PRG prg(fix_key);
  prg.random_block(&delta, 1);
  prg.random_bool(b, length);

  io->sync();
  auto start = clock_start();
  T<IO> *ot = new T<IO>(io);
  if (party == ALICE) {
    ot->send_cot_ft(b0, f, length);
  } else {
    ot->recv_cot(r, b, length);
  }
  io->flush();
  long long t = time_from(start);
  if (party == ALICE){
    io->send_block(b0, length);
  } else if (party == BOB) {
    io->recv_block(b0, length);
    for (int i = 0; i < length; ++i) {
      block m1 = f(b0[i], i);
      if (b[i]) {
        if (!block_cmp(&r[i], &m1, 1))
          error("COT failed!\n");
      } else {
        if (!block_cmp(&r[i], &b0[i], 1))
          error("COT failed!\n");
      }
    }
  }
  io->flush();
  delete ot;
  delete[] b0;
  delete[] r;
  delete[] b;
  return t;
}

template <typename IO, template <typename> class T>
double test_cot_f(NetIO *io, int party, int length) {
  block *b0 = new block[length], *r = new block[length];
  bool *b = new bool[length];
  auto f = [](block s0, uint64_t i) {
    uint64_t v1 = (uint64_t) s0[1];
    uint64_t v2 = (uint64_t) s0[0];
    return makeBlock(v1 + i, v2 - i);
  };
  block delta;
  PRG prg(fix_key);
  prg.random_block(&delta, 1);
  prg.random_bool(b, length);

  io->sync();
  auto start = clock_start();
  T<IO> *ot = new T<IO>(io);
  if (party == ALICE) {
    ot->send_cot_f(b0, f, length);
  } else {
    ot->recv_cot(r, b, length);
  }
  io->flush();
  long long t = time_from(start);
  if (party == ALICE){
    io->send_block(b0, length);
  } else if (party == BOB) {
    io->recv_block(b0, length);
    for (int i = 0; i < length; ++i) {
      block m1 = f(b0[i], i);
      if (b[i]) {
        if (!block_cmp(&r[i], &m1, 1))
          error("COT failed!\n");
      } else {
        if (!block_cmp(&r[i], &b0[i], 1))
          error("COT failed!\n");
      }
    }
  }
  io->flush();
  delete ot;
  delete[] b0;
  delete[] r;
  delete[] b;
  return t;
}

template <typename IO, template <typename> class T>
double test_cot_add_deltas(NetIO *io, int party, int length) {
  block *b0 = new block[length], *r = new block[length];
  bool *b = new bool[length];
  std::pair<uint64_t, uint64_t> *deltas = new std::pair<uint64_t, uint64_t>[length];
  for (int i = 0; i < length; ++i) {
    deltas[i] = std::make_pair(i + 1, i + 2);
  }
  PRG prg(fix_key);
  prg.random_bool(b, length);

  io->sync();
  auto start = clock_start();
  T<IO> *ot = new T<IO>(io);
  if (party == ALICE) {
    ot->send_cot_add_delta(b0, deltas, length);
  } else {
    ot->recv_cot(r, b, length);
  }
  io->flush();
  long long t = time_from(start);
  if (party == ALICE){
    io->send_block(b0, length);
  } else if (party == BOB) {
    io->recv_block(b0, length);
    for (int i = 0; i < length; ++i) {
      block m1 = makeBlock((uint64_t)(b0[i][1]) + deltas[i].second,
        (uint64_t)(b0[i][0]) + deltas[i].first);
      // std::cout << b[i] << std::endl;
      // std::cout << (uint64_t)(b0[i][0]) << "|" << (uint64_t)(b0[i][1]) << std::endl;
      // std::cout << (uint64_t)(m1[0]) << "|" << (uint64_t)(m1[1]) << std::endl;
      // std::cout << (uint64_t)(r[i][0]) << "|" << (uint64_t)(r[i][1]) << std::endl;
      if (b[i]) {
        if (!block_cmp(&r[i], &m1, 1))
          error("COT failed!\n");
      } else {
        if (!block_cmp(&r[i], &b0[i], 1))
          error("COT failed!\n");
      }
    }
  }
  io->flush();
  delete ot;
  delete[] b0;
  delete[] r;
  delete[] b;
  return t;
}

template <typename IO, template <typename> class T>
double test_rot(IO *io, int party, int length) {
  block *b0 = new block[length], *r = new block[length];
  block *b1 = new block[length];
  bool *b = new bool[length];
  PRG prg;
  prg.random_bool(b, length);

  io->sync();
  auto start = clock_start();
  T<IO> *ot = new T<IO>(io);
  if (party == ALICE) {
    ot->send_rot(b0, b1, length);
  } else {
    ot->recv_rot(r, b, length);
  }
  io->flush();
  long long t = time_from(start);
  if (party == ALICE) {
    io->send_block(b0, length);
    io->send_block(b1, length);
  } else if (party == BOB) {
    io->recv_block(b0, length);
    io->recv_block(b1, length);
    for (int i = 0; i < length; ++i) {
      if (b[i])
        assert(block_cmp(&r[i], &b1[i], 1));
      else
        assert(block_cmp(&r[i], &b0[i], 1));
    }
  }
  io->flush();
  delete ot;
  delete[] b0;
  delete[] b1;
  delete[] r;
  delete[] b;
  return t;
}
