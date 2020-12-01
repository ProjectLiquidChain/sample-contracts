#include "chain.h"
#include <string.h>

// Storage keys, prefixes and types
#define OWNER_KEY "OWNER"
#define BALANCES_PREFIX "BALANCES"
#define BALANCES_KEY_SIZE (sizeof(BALANCES_PREFIX) + ADDRESS_SIZE)
#define ALLOWANCES_PREFIX "ALLOWANCES"
#define ALLOWANCES_KEY_SIZE (sizeof(ALLOWANCES_PREFIX) + ADDRESS_SIZE * 2)
#define PAUSE_KEY "PAUSE"
#define TOTAL_SUPPLY_KEY "TOTAL_SUPPLY"
#define SYMBOL "QASH"
#define DECIMALS 6

typedef uint8_t balance_key_t[BALANCES_KEY_SIZE];
typedef uint8_t allowance_key_t[ALLOWANCES_KEY_SIZE];

// Events
Event Owner(address_t owner);
Event ChangeOwner(address_t old_owner, address_t new_owner);
Event Mint(address_t address, uint64_t value);
Event Transfer(address_t from, address_t to, uint64_t value, uint64_t memo);
Event Approval(address_t owner, address_t spender, uint64_t value);
Event Pause();
Event Unpause();

/**
 * Simple assertion, exit on false
 */
void assert(int expression) {
  if (!expression) {
    exit(1);
  }
}

/**
 * Math safe uint64 add, exit on overflow
 */
uint64_t add(uint64_t a, uint64_t b) {
  uint64_t c = a + b;
  assert(c >= a);
  return c;
}

/**
 * Math safe uint64 add, exit on underflow
 */
uint64_t sub(uint64_t a, uint64_t b) {
  assert(b <= a);
  return a - b;
}

/**
 * Build key of balance for storage
 */
void _build_balance_key(balance_key_t key, address_t address) {
  memcpy(key, BALANCES_PREFIX, sizeof(BALANCES_PREFIX));
  memcpy(key + sizeof(BALANCES_PREFIX), address, ADDRESS_SIZE);
}

/**
 * Build key of allowance for storage
 */
void _build_allowance_key(allowance_key_t key, address_t owner, address_t spender) {
  memcpy(key, ALLOWANCES_PREFIX, sizeof(ALLOWANCES_PREFIX));
  memcpy(key + sizeof(ALLOWANCES_PREFIX), owner, ADDRESS_SIZE);
  memcpy(key + sizeof(ALLOWANCES_PREFIX) + ADDRESS_SIZE, spender, ADDRESS_SIZE);
}

// Functions

/**
 * Init function
 * - Mint a fixed supply of tokens
 * - Set owner to caller
 * - Lock init
 */
void init(uint64_t value) {
  // Not init yet
  assert(chain_storage_size_get(OWNER_KEY, sizeof(OWNER_KEY)) == 0);
  address_t owner;
  chain_get_caller(owner);
  // Init
  chain_storage_set(OWNER_KEY, sizeof(OWNER_KEY), owner, ADDRESS_SIZE);
  // Emit event set owner
  Owner(owner);
  // Mint supply
  balance_key_t key;
  _build_balance_key(key, owner);
  chain_storage_set(key, BALANCES_KEY_SIZE, &value, sizeof(value));
  chain_storage_set(TOTAL_SUPPLY_KEY, sizeof(TOTAL_SUPPLY_KEY), &value, sizeof(value));
  Mint(owner, value);
}

/**
 * Return owner address via Owner event
 */
void get_owner() {
  address_t owner;
  chain_storage_get(OWNER_KEY, sizeof(OWNER_KEY), owner);
  Owner(owner);
}

/**
 * Check if caller is owner
 */
uint8_t is_owner() {
  address_t caller;
  address_t owner;
  chain_get_caller(caller);
  chain_storage_get(OWNER_KEY, sizeof(OWNER_KEY), owner);
  return memcmp(owner, caller, ADDRESS_SIZE) == 0;
}

/**
 * Change the owner of contract
 * Require caller is owner
 */
void change_owner(address_t new_owner) {
  assert(is_owner());
  address_t owner;
  chain_storage_get(OWNER_KEY, sizeof(OWNER_KEY), owner);
  chain_storage_set(OWNER_KEY, sizeof(OWNER_KEY), new_owner, ADDRESS_SIZE);
  ChangeOwner(owner, new_owner);
}

/**
 * Return the balance of an address. If address does not exist, return 0
 */
uint64_t get_balance(address_t address) {
  balance_key_t key;
  _build_balance_key(key, address);
  uint64_t balance = 0;
  if (chain_storage_size_get(key, BALANCES_KEY_SIZE)) {
    chain_storage_get(key, BALANCES_KEY_SIZE, &balance); 
  }
  return balance;
}

/**
 * Check if transfer is paused
 */
uint8_t is_paused() {
  uint8_t flag = 0;
  if (chain_storage_size_get(PAUSE_KEY, sizeof(PAUSE_KEY))) {
    chain_storage_get(PAUSE_KEY, sizeof(PAUSE_KEY), &flag);
  }
  return flag;
}

/**
 * Pause the token transfer
 * Require caller is owner
 */
void pause() {
  assert(is_owner() && !is_paused());
  uint8_t flag = 1;
  chain_storage_set(PAUSE_KEY, sizeof(PAUSE_KEY), &flag, sizeof(flag));
  Pause();
}

/**
 * Resume the token transfer
 * Require caller is owner
 */
void unpause() {
  assert(is_owner() && is_paused());
  uint8_t flag = 0;
  chain_storage_set(PAUSE_KEY, sizeof(PAUSE_KEY), &flag, sizeof(flag));
  Unpause();
}

/**
 * Internal transfer function
 */
void _transfer(address_t from, address_t to, uint64_t value, uint64_t memo) {
  assert(!is_paused());

  // Get current balance
  uint64_t from_balance = get_balance(from);
  uint64_t to_balance = get_balance(to);
  
  // From has enough balance
  assert(from_balance >= value);

  // Safe math will exit if not enough balance
  from_balance = sub(from_balance, value);
  to_balance = add(to_balance, value);

  // Update storage
  balance_key_t from_balance_key, to_balance_key;
  _build_balance_key(from_balance_key, from);
  _build_balance_key(to_balance_key, to);
  chain_storage_set(from_balance_key, BALANCES_KEY_SIZE, &from_balance, sizeof(from_balance));
  chain_storage_set(to_balance_key, BALANCES_KEY_SIZE, &to_balance, sizeof(to_balance));

  Transfer(from, to, value, memo);
}

/**
 * Transfer tokens from caller to another address with a memo
 */
void transfer(address_t to, uint64_t value, uint64_t memo) { 
  address_t from;
  chain_get_caller(from);

  _transfer(from, to, value, memo);
}

/**
 * Return current allowance that an token holder allows spender to transfer
 */
uint64_t get_allowance(address_t owner, address_t spender) {
  allowance_key_t key;
  _build_allowance_key(key, owner, spender);
  uint64_t value = 0;
  if (chain_storage_size_get(key, ALLOWANCES_KEY_SIZE)) {
    chain_storage_get(key, ALLOWANCES_KEY_SIZE, &value); 
  }
  return value;
}

/**
 * Approve a spender to transfer tokens on behalf of a token holder
 */
void approve(address_t spender, uint64_t value) {
  address_t owner;
  chain_get_caller(owner);
  allowance_key_t key;
  _build_allowance_key(key, owner, spender);
  chain_storage_set(key, ALLOWANCES_KEY_SIZE, &value, sizeof(value));
  Approval(owner, spender, value);
}

/**
 * Transfer token on behalf of token holder
 * The amount must not greater than the allowance was set by `approve`
 */
void transfer_from(address_t from, address_t to, uint64_t value, uint64_t memo) {
  address_t spender;
  chain_get_caller(spender);

  // Check allowance
  uint64_t allowance = get_allowance(from, spender);
  allowance = sub(allowance, value);

  // Update storage
  allowance_key_t key;
  _build_allowance_key(key, from, spender);
  chain_storage_set(key, ALLOWANCES_KEY_SIZE, &allowance, sizeof(allowance));
  
  _transfer(from, to, value, memo);
}

/**
 * Return the decimals of token
 */
uint8_t get_decimals() {
  return DECIMALS;
}

/**
 * Return symbol, encoding in uint64, max 8 character 
 */
uint64_t get_symbol() {
  return *((uint64_t *)SYMBOL);
}

/**
 * Return total supply of token
 */
uint64_t get_total_supply() {
  uint64_t total_supply;
  chain_storage_get(TOTAL_SUPPLY_KEY, sizeof(TOTAL_SUPPLY_KEY), &total_supply);
  return total_supply;
}
