#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <list>

#include "share.h"
#include "factory.h"
#include "miner_record.h"

namespace poolsim {

class MiningPool;

struct PPLNSConfig {
    uint64_t n;
};

struct BlockMetaData{
    std::string reward_scheme;
    uint64_t shares_per_block = 0;
    std::string miner_address;
};

struct QBBlockMetaData : BlockMetaData {
    uint64_t credit_balance_receiver = 0;
    std::string receiver_address;
    uint64_t reset_balance_receiver = 0;
    double prop_credits_lost = 0;
};

class RewardScheme {
public:
    virtual ~RewardScheme();

    virtual void handle_share(const std::string& miner_address, const Share& share) = 0;

    // Set the mining pool for this reward scheme
    // RewardScheme and MiningPool should be a 1 to 1 relationship
    void set_mining_pool(std::shared_ptr<MiningPool> mining_pool);

    // returns the metadata needed when a block has been mined
    virtual nlohmann::json get_block_metadata() = 0;

    // returns the metadata for a miner
    virtual nlohmann::json get_miner_metadata(const std::string& miner_address) = 0;

    // Returns the mining_pool of this reward scheme as a shared_ptr
    // Use this rather than accessing the weak_ptr property
    std::shared_ptr<MiningPool> get_mining_pool();
protected:
    std::weak_ptr<MiningPool> mining_pool;

    uint64_t shares_per_block;
};

MAKE_FACTORY(RewardSchemeFactory, RewardScheme, const nlohmann::json&);

template <typename T, typename RecordClass=MinerRecord, typename BlockData=BlockMetaData>
class BaseRewardScheme :
    public RewardScheme,
    public Creatable1<RewardScheme, T, const nlohmann::json&> {
protected:
    std::vector<std::shared_ptr<RecordClass>> records;

    // increments mined block and credits stats for a given record
    virtual void update_record(std::shared_ptr<RecordClass> record, const Share& share) = 0;

    // returns the metadata needed when a block has been mined
    virtual nlohmann::json get_block_metadata() override;

    // returns the metadata for a miner
    virtual nlohmann::json get_miner_metadata(const std::string& miner_address) override;

    // returns record of a miner if it exists, otherwise a new record is created and returned
    std::shared_ptr<RecordClass> find_record(const std::string& miner_address);

    // stores the meta data associated to the last block mined
    BlockData block_meta_data;
};

template <typename T, typename RecordClass, typename BlockData>
std::shared_ptr<RecordClass> BaseRewardScheme<T, RecordClass, BlockData>::find_record(const std::string& miner_address) {
  for (auto iter = records.begin(); iter != records.end(); ++iter) {
    if ((*iter)->get_miner() == miner_address)
      return (*iter);
  }

  auto record = std::make_shared<RecordClass>(miner_address);
  records.push_back(record);
  return record;
}

template<typename T, typename RecordClass, typename BlockData>
nlohmann::json BaseRewardScheme<T, RecordClass, BlockData>::get_block_metadata() {
    nlohmann::json j;
    to_json(j, block_meta_data);
    return j;
}

template<typename T, typename RecordClass, typename BlockData>
nlohmann::json BaseRewardScheme<T, RecordClass, BlockData>::get_miner_metadata(const std::string& miner_address) {
    nlohmann::json j;
    to_json(j, *find_record(miner_address));
    return j;
}


// Pay-per-share reward scheme
class PPSRewardScheme: public BaseRewardScheme<PPSRewardScheme> {
public:
    explicit PPSRewardScheme(const nlohmann::json& args);

    void handle_share(const std::string& miner_address, const Share& share) override;

private:
    void update_record(std::shared_ptr<MinerRecord> record, const Share& share) override;
};


// Pay-per-last-n-shares reward scheme
class PPLNSRewardScheme: public BaseRewardScheme<PPLNSRewardScheme> {
public:
    explicit PPLNSRewardScheme(const nlohmann::json& args);

    void handle_share(const std::string& miner_address, const Share& share) override;

    void set_n(uint64_t _n);

private:
    void update_record(std::shared_ptr<MinerRecord> record, const Share& share) override;

    // the number of last shares over which a reward will be distributed
    uint64_t n = 0;
    // list of miner addresses that submitted last n shares
    std::list<std::string> last_n_shares;
    // inserts a miners address to the list of address of last n miners that submitted a share
    void insert_share(std::string miner_address);
};

// Queue-based reward scheme
class QBRewardScheme: public BaseRewardScheme<QBRewardScheme, QBRecord, QBBlockMetaData> {
public:
    explicit QBRewardScheme(const nlohmann::json& args);

    void handle_share(const std::string& miner_address, const Share& share) override;
    
    uint64_t get_credits(const std::string& miner_address);

protected:
    // updates stats of top miner in pool and resets the top miners credits
    void reward_top_miner();
    // updates the given record based on the type of share accordingly 
    void update_record(std::shared_ptr<QBRecord> record, const Share& share) override;
    // returns the total sum of credit balances by pool
    uint64_t get_credits_sum();
};

void from_json(const nlohmann::json& j, PPLNSConfig& r);
void to_json(nlohmann::json& j, const BlockMetaData& b);
void to_json(nlohmann::json& j, const QBBlockMetaData& b);

}