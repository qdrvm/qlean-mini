/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <boost/asio/ip/tcp.hpp>
#include <libp2p/crypto/key.hpp>
#include <libp2p/multi/multiaddress.hpp>
#include <utils/ctor_limiters.hpp>

#include "app/validator_keys_manifest.hpp"
#include "crypto/xmss/xmss_provider.hpp"

namespace lean::app {
  class Configuration : Singleton<Configuration> {
   public:
    using Endpoint = boost::asio::ip::tcp::endpoint;

    struct DatabaseConfig {
      std::filesystem::path directory = "db";
      size_t cache_size = 1 << 30;  // 1GiB
    };

    struct MetricsConfig {
      Endpoint endpoint;
      std::optional<bool> enabled;
    };

    Configuration();
    virtual ~Configuration() = default;

    // /// Generate yaml-file with actual config
    // virtual void generateConfigFile() const = 0;

    [[nodiscard]] virtual const std::string &nodeVersion() const;
    [[nodiscard]] virtual const std::string &nodeName() const;
    [[nodiscard]] virtual const std::string &nodeId() const;
    [[nodiscard]] virtual const std::filesystem::path &basePath() const;
    [[nodiscard]] virtual const std::filesystem::path &modulesDir() const;
    [[nodiscard]] virtual const std::filesystem::path &bootnodesFile() const;
    [[nodiscard]] virtual const std::filesystem::path &validatorRegistryPath()
        const;
    [[nodiscard]] virtual const std::filesystem::path &genesisConfigPath()
        const;
    [[nodiscard]] virtual const std::optional<libp2p::Multiaddress> &
    listenMultiaddr() const;
    [[nodiscard]] virtual const libp2p::crypto::KeyPair &nodeKey() const;
    [[nodiscard]] virtual const crypto::xmss::XmssKeypair &xmssKeypair() const;
    [[nodiscard]] virtual const std::optional<size_t> &maxBootnodes() const;
    [[nodiscard]] virtual const std::filesystem::path &
    validatorKeysManifestPath() const;
    [[nodiscard]] virtual bool cliIsAggregator() const;
    [[nodiscard]] virtual uint64_t cliSubnetCount() const;

    [[nodiscard]] virtual double fakeXmssAggregateSignaturesRate() const;
    [[nodiscard]] virtual double fakeXmssVerifyAggregatedSignaturesRate() const;

    [[nodiscard]] virtual const DatabaseConfig &database() const;

    [[nodiscard]] virtual const MetricsConfig &metrics() const;

   private:
    friend class Configurator;  // for external configure

    std::string version_;
    std::string name_;
    std::string node_id_;
    std::filesystem::path base_path_;
    std::filesystem::path modules_dir_;
    std::filesystem::path bootnodes_file_;
    std::filesystem::path validator_registry_path_;
    std::filesystem::path genesis_config_path_;
    std::optional<libp2p::Multiaddress> listen_multiaddr_;
    std::optional<libp2p::crypto::KeyPair> node_key_;
    std::optional<size_t> max_bootnodes_;

    std::filesystem::path xmss_public_key_path_;
    std::filesystem::path xmss_secret_key_path_;
    std::optional<crypto::xmss::XmssKeypair> xmss_keypair_;

    std::filesystem::path validator_keys_manifest_path_;
    bool cli_is_aggregator_ = false;
    uint64_t cli_subnet_count_ = 1;

    double fake_xmss_aggregate_signatures_rate_ = 22.704;
    double fake_xmss_verify_aggregated_signatures_rate_ = 3463.106;

    DatabaseConfig database_;
    MetricsConfig metrics_;
  };

}  // namespace lean::app
