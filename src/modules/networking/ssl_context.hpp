/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <boost/asio/ssl/context.hpp>
#include <openssl/x509.h>

#include "utils/ctor_limiters.hpp"

namespace lean {

  /**
   * Shared TLS client context.
   *
   * This object holds only global TLS configuration: protocol version,
   * CA trust store lookup and OpenSSL options. Hostname verification must be
   * configured per-connection on the corresponding ssl_stream, because each
   * connection targets a specific host.
   */
  class AsioSslContext : public boost::asio::ssl::context,
                         Singleton<AsioSslContext> {
   public:
    AsioSslContext() : context{tlsv13_client} {
      // Try to ensure that the system certificate store is discoverable on
      // platforms where OpenSSL might not have a working default.
      [[maybe_unused]] static bool find_system_certificates = [] {
        // SSL_CERT_FILE
        if (getenv(X509_get_default_cert_file_env()) != nullptr) {
          return true;
        }
        // SSL_CERT_DIR
        if (getenv(X509_get_default_cert_dir_env()) != nullptr) {
          return true;
        }

        constexpr auto extra_file = "/etc/ssl/cert.pem";
        if (std::string_view{X509_get_default_cert_file()} != extra_file
            and std::filesystem::exists(extra_file)) {
          setenv(X509_get_default_cert_file_env(), extra_file, true);
          return true;
        }

        constexpr auto extra_dir = "/etc/ssl/certs";
        std::error_code ec;
        if (std::string_view{X509_get_default_cert_dir()} != extra_dir
            and std::filesystem::directory_iterator{extra_dir, ec}
                    != std::filesystem::directory_iterator{}) {
          setenv(X509_get_default_cert_dir_env(), extra_dir, true);
          return true;
        }

        return true;
      }();

      set_options(default_workarounds | no_sslv2 | no_sslv3 | no_tlsv1
                  | no_tlsv1_1 | no_tlsv1_2 | single_dh_use);

      // Load system CAs (SSL_CERT_FILE / SSL_CERT_DIR are respected by
      // OpenSSL).
      set_default_verify_paths();

      // NOTE: verify_mode and verify_callback are set per-connection on the
      // ssl_stream to bind verification to the target hostname.
    }
  };

}  // namespace lean
