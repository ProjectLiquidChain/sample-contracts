#include <string.h>
#include <vertex.h>

extern Event Mint(address to, uint64_t amount);
extern Event Transfer(address from, address to, uint64_t amount);

const int ADDR_SIZE=35;
const char OWNER[] = "OWNER";
const char IS_PAUSE[] = "IS_PAUSE";

// sdk_storage_set
void sdk_storage_set(byte_t* key, size_t key_size, byte_t* value, size_t value_size) {
    chain_storage_set(key, key_size, value, value_size);
}

// sdk_storage_get
byte_t* sdk_storage_get(byte_t* key, size_t key_size) {
  int size = chain_storage_size_get(key, key_size);
  if (size == 0) {
      return 0;
  }
  byte_t* ret = (byte_t*) malloc(size * sizeof(byte_t));
  chain_storage_get(key, key_size, ret);
  return ret;
}

int from_bytes(uint8_t* bytes){
  if (bytes==0) {
    return 0;
  }
  return *(uint32_t*)bytes;
}

int sdk_caller_is_creator() {
  address caller = (address) malloc(ADDR_SIZE * sizeof(byte_t));
  address creator = (address) malloc(ADDR_SIZE * sizeof(byte_t));
  chain_get_caller(caller);
  chain_get_creator(creator);
  int n = memcmp(creator, caller, ADDR_SIZE);
  free(caller);
  free(creator);
  if (n == 0) {
    return 1;
  }
  return 0;
}

int caller_is_owner() {
  address caller = (address) malloc(ADDR_SIZE * sizeof(byte_t));
  chain_get_caller(caller);
  address owner = (address) malloc(ADDR_SIZE * sizeof(byte_t));
  owner = sdk_storage_get((byte_t *)OWNER, sizeof(OWNER));
  int n = memcmp(owner, caller, ADDR_SIZE);
  free(caller);
  free(owner);
  if (n == 0) {
    return 1;
  }
  return 0;
}

int set_owner(address owner) {
  if (!caller_is_owner()) {
    return -1;
  }
  sdk_storage_set((byte_t *)OWNER, sizeof(OWNER), owner, ADDR_SIZE);
  return 0;
}

int pause() {
  if (!caller_is_owner()) {
    return -1;
  }
  uint8_t pause = 1;
  sdk_storage_set((byte_t *)IS_PAUSE, sizeof(IS_PAUSE), &pause, 1);
  return 0;
}

int unpause() {
  if (!caller_is_owner()) {
    return -1;
  }
  uint8_t unpause = 0;
  sdk_storage_set((byte_t *)IS_PAUSE, sizeof(IS_PAUSE), &unpause, 1);
  return 0;
}

int is_pausing() {
  return from_bytes(sdk_storage_get((byte_t *)IS_PAUSE, sizeof(IS_PAUSE)));
}

int change_balance(address to, uint64_t amount, int sign){
  uint64_t to_balance = from_bytes(sdk_storage_get(to, ADDR_SIZE));
  if (sign < 0) {
    if (to_balance < amount) {
      return -1;
    }
    to_balance -= amount;
  } else {
    to_balance += amount;
  }
  sdk_storage_set(to, ADDR_SIZE, (uint8_t*)&to_balance, 8);
  return 0;
}

int set_owner_to_creator() {
  address creator = (address) malloc(ADDR_SIZE * sizeof(byte_t));
  chain_get_creator(creator);
  sdk_storage_set((byte_t *)OWNER, sizeof(OWNER), creator, ADDR_SIZE);
  free(creator);
  return 0;
}

int mint(uint64_t amount) {
  // set up genesis owner
  if (!caller_is_owner()) {
    address owner = (address) malloc(ADDR_SIZE * sizeof(byte_t));
    owner = sdk_storage_get((byte_t *)OWNER, sizeof(OWNER));
    if (!owner) {
      set_owner_to_creator();
    }
    free(owner);
  }

  if (!caller_is_owner()) {
    return -1;
  }

  // minting
  address caller = (address) malloc(ADDR_SIZE * sizeof(byte_t));
  chain_get_caller(caller);
  int success = change_balance(caller, amount, 1);
  if (success != -1) {
    Mint(caller, amount);
  }
  free(caller);
  return success;
}

int get_balance(address address){
  return from_bytes(sdk_storage_get(address, ADDR_SIZE));
}

int transfer(address to, uint64_t amount){
  if (is_pausing()) {
    return -1;
  }
  byte_t from[ADDR_SIZE];
  chain_get_caller(from);
  int success = change_balance(from, amount, -1);
  if (success != -1) {
    success = change_balance(to, amount, 1);
  }
  if (success != -1) {
    Transfer(from, to, amount);
    return success;
  }
  return -1;
}
