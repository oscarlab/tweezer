#include "openssl/evp.h"
#include "openssl/bio.h"
#include "rocksdb/status.h"
#include "rocksdb/slice.h"
#include "table/format.h"
#include "openssl/rand.h"
#include "sgx/enc_dec.h"
#include <string>
#include <map>
#include "sgx/hmac.h"
#include "table/block_based/block.h"
#include <cstdlib>
#include "file/filename.h"

namespace ROCKSDB_NAMESPACE{
//Manifest key and hash are suppose to be safe !!
unsigned char manifest_hash[48];
unsigned char manifest_key[32];

std::map<uint64_t, std::string> KeyList;

extern pthread_rwlock_t key_lock;

bool isKeyExist(std::string FileName){
	pthread_rwlock_rdlock(&key_lock);
	uint64_t table_number = TableFileNameToNumber(FileName);
	if(KeyList.find(table_number) == KeyList.end()){
		pthread_rwlock_unlock(&key_lock);
		return false;
	}
	else {
		pthread_rwlock_unlock(&key_lock);
		return true;
	}
}

void GenerateKey(std::string FileName, unsigned char* out){
	uint64_t table_number = TableFileNameToNumber(FileName);
	unsigned char new_key[32];
	int result = RAND_bytes(new_key,32);
	if (result == -1)
		assert(true);
	std::string key((char*) new_key, 32);
	pthread_rwlock_wrlock(&key_lock);
	KeyList.insert(make_pair(table_number,key));
	pthread_rwlock_unlock(&key_lock);
	if(out != NULL) {
		memcpy(out,new_key,32);
	}
}

void AddSSTKeyToList(uint64_t file_number, unsigned char* key) {
	pthread_rwlock_wrlock(&key_lock);
	KeyList.insert(make_pair(file_number,std::string((char*)key,32)));
	pthread_rwlock_unlock(&key_lock);
}

void DeleteSSTKeyFromList(uint64_t file_number) {
	pthread_rwlock_wrlock(&key_lock);
	if(KeyList.find(file_number) == KeyList.end()){
		pthread_rwlock_unlock(&key_lock);
		return ;
	} else{
		KeyList.erase(file_number);
	}
	pthread_rwlock_unlock(&key_lock);
}

unsigned char memtable_key[32];

unsigned char gcm_iv[] = {
    0x99, 0xaa, 0x3e, 0x68, 0xed, 0x81, 0x73, 0xa0, 0xee, 0xd0, 0x66, 0x84
};

unsigned char gcm_aad[] = {
    0x4d, 0x23, 0xc3, 0xce, 0xc3, 0x34, 0xb4, 0x9b, 0xdb, 0x37, 0x0c, 0x43,
    0x7f, 0xec, 0x78, 0xde
};

//Be aware that memtable key do not need to be flusehd to storage. This temporal key. Contents of memtable is stored in WAL.
void GenerateMemKey(void) {
	GenerateRandomBytes((char*)memtable_key,32);
}

std::string FilenameToKey(std::string file_name){
	std::string key;
	uint64_t table_number = TableFileNameToNumber(file_name);
	pthread_rwlock_rdlock(&key_lock);
	key = KeyList[table_number];
	pthread_rwlock_unlock(&key_lock);
	return key;
}

// data, key, iv, aad : Input
// tags : Output
void Encryption(Slice data, unsigned char* key, unsigned char* iv, unsigned char *aad, unsigned char *tags) {
#if 0
  EVP_CIPHER_CTX *ctx;
  unsigned char *outbuf = (unsigned char*) data.data();
  int outlen;
  ctx = EVP_CIPHER_CTX_new();
  EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
	//IV : 12 bytes (96bits)
  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL);
  EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv);
	//AAD : 16 bytes (128bits)
  EVP_EncryptUpdate(ctx, NULL, &outlen, aad, 16);
  EVP_EncryptUpdate(ctx, outbuf, &outlen,(const unsigned char*)data.data(), data.size());
  EVP_EncryptFinal_ex(ctx, outbuf, &outlen);
	if(tags != nullptr)
		//tags : 16 bytes (128bits)
	  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tags);
  EVP_CIPHER_CTX_free(ctx);
#endif
}

// data, key, iv, aad, tags : Input
// data : Output
void Decryption(Slice data, unsigned char* key, unsigned char* iv, unsigned char *aad, unsigned char *tags) {
#if 0
	unsigned char *outbuf = (unsigned char *)data.data();
	int outlen;
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
	//IV : 12 bytes (96bits)
  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL);
  EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv);
	//AAD : 16 bytes (128bits)
  EVP_DecryptUpdate(ctx, NULL, &outlen, aad, 16);
  EVP_DecryptUpdate(ctx, outbuf, &outlen, (const unsigned char*)data.data(), data.size());
	if(tags != nullptr) {
		//tags : 16 bytes (128bits)
  	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16,tags);
		int rv = EVP_DecryptFinal_ex(ctx, outbuf, &outlen);
		if (rv <= 0) {
			fprintf(stdout,"tags verification fail \n");
		}
	} else {
		EVP_DecryptFinal_ex(ctx, outbuf, &outlen);
	}
  EVP_CIPHER_CTX_free(ctx);
#endif
}

void log_encrypt(const Slice &record, char* key, char* tags, char* aad){
#if 0
	unsigned char* target_aad = aad ? (unsigned char*) aad : (unsigned char*)gcm_aad;
	size_t target_aad_size = aad ? 16 : sizeof(gcm_aad);
	const unsigned char* log_key = reinterpret_cast<const unsigned char*>(key);
	unsigned char *output = (unsigned char *)record.data();
	EVP_CIPHER_CTX *ctx;
	int outlen;
	ctx = EVP_CIPHER_CTX_new();
	/* Set cipher type and mode */
	EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
	/* Set IV length if default 96 bits is not appropriate */
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, sizeof(gcm_iv), NULL);
	/* Initialise key and IV */
	EVP_EncryptInit_ex(ctx, NULL, NULL, log_key, gcm_iv);
	/* Encrypt plaintext */
	EVP_EncryptUpdate(ctx, NULL, &outlen, target_aad, target_aad_size);
	EVP_EncryptUpdate(ctx, output, &outlen, (const unsigned char*)record.data(), record.size());
	// No output
	EVP_EncryptFinal_ex(ctx, output, &outlen);
	if (tags != nullptr) {
		EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tags);
	}
	EVP_CIPHER_CTX_free(ctx);
#endif
}

void log_decrypt(const Slice &record, char* key,char *tags, char* aad){
#if 0
	unsigned char* target_aad = aad ? (unsigned char*) aad : (unsigned char*)gcm_aad;
  size_t target_aad_size = aad ? 16 : sizeof(gcm_aad);
	const unsigned char* log_key = reinterpret_cast<const unsigned char*>(key);
	unsigned char *outbuf = (unsigned char *)record.data();
	//BIO_dump_fp(stdout, outbuf, block.size());
	int outlen;
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, sizeof(gcm_iv), NULL);
	/* Specify key and IV */
	EVP_DecryptInit_ex(ctx, NULL, NULL, log_key, gcm_iv);
	EVP_DecryptUpdate(ctx, NULL, &outlen, target_aad, target_aad_size);
	EVP_DecryptUpdate(ctx, outbuf, &outlen, (const unsigned char*)record.data(), record.size());
//	fprintf(stdout,"outlen %d \n",outlen);
  if(tags != nullptr)
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16,tags);
	int rv = EVP_DecryptFinal_ex(ctx, outbuf, &outlen);
	if (rv <= 0 && tags != nullptr) {
		fprintf(stdout,"log digest fail \n");
		exit(-1);
	}
	EVP_CIPHER_CTX_free(ctx);
#endif
}

void GenerateRandomBytes(char *output, int size) {
  RAND_status();
  int result = RAND_bytes(reinterpret_cast<unsigned char*>(output),size);
  if (result == -1) {
		fprintf(stdout, "Random number generation fail \n");
		exit(-1);
	}
}


}
