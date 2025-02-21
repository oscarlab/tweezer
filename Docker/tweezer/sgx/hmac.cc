#include <string>
#include <map>
#include "rocksdb/slice.h"
#include "openssl/hmac.h"
#include "openssl/evp.h"
#include "sgx/enc_dec.h"
#include "sgx/hmac.h"
#include "file/filename.h"

namespace ROCKSDB_NAMESPACE{

extern std::map<uint64_t, std::string> KeyList;
extern pthread_rwlock_t key_lock;

static std::atomic<uint64_t>  num_hashes;
static std::atomic<uint64_t>  num_hashed_bytes;

void digest(unsigned char* hmac, const Slice block,std::string sst_key){
	const unsigned char* key = (const unsigned char*)sst_key.data();
//Solving Memory corruption
	unsigned int n;
	HMAC(EVP_sha3_384(), key, 32,(const unsigned char*) block.data(), block.size(), hmac,&n);
        num_hashes += 1;
        num_hashed_bytes += block.size();
}

uint64_t get_num_hashes() {
  uint64_t rv = num_hashes;
  /* Implicitly reset this on each phase of the benchmark */
  num_hashes = 0;
  return rv;
}

uint64_t get_hashed_bytes() {
  uint64_t rv = num_hashed_bytes;
  /* Implicitly reset this on each phase of the benchmark */
  num_hashed_bytes = 0;
  return rv;
}

}
