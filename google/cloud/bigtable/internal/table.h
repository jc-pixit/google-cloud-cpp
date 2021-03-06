// Copyright 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_H_

#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/filters.h"
#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/mutations.h"
#include "google/cloud/bigtable/read_modify_write_rule.h"
#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/row_set.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Return the full table name.
 *
 * The full table name is:
 *
 * `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
 *
 * Where the project id and instance id come from the @p client parameter.
 */
inline std::string TableName(std::shared_ptr<DataClient> client,
                             std::string const& table_id) {
  return InstanceName(std::move(client)) + "/tables/" + table_id;
}

namespace internal {
template <typename Request>
void SetCommonTableOperationRequest(Request& request,
                                    std::string const& app_profile_id,
                                    std::string const& table_name) {
  request.set_app_profile_id(app_profile_id);
  request.set_table_name(table_name);
}

}  // namespace internal

/// A simple wrapper to represent the response from `Table::SampleRowKeys()`.
struct RowKeySample {
  std::string row_key;
  std::int64_t offset_bytes;
};

/**
 * This namespace contains implementations of the API that do not raise
 * exceptions. It is subject to change without notice, and therefore, not
 * recommended for direct use by applications.
 *
 * Provide APIs to access and modify data in a Cloud Bigtable table.
 */
namespace noex {
class Table {
 public:
  Table(std::shared_ptr<DataClient> client, std::string const& table_id)
      : client_(std::move(client)),
        app_profile_id_(bigtable::AppProfileId("")),
        table_name_(bigtable::TableId(TableName(client_, table_id))),
        rpc_retry_policy_(bigtable::DefaultRPCRetryPolicy()),
        rpc_backoff_policy_(bigtable::DefaultRPCBackoffPolicy()),
        metadata_update_policy_(table_name(), MetadataParamTypes::TABLE_NAME),
        idempotent_mutation_policy_(
            bigtable::DefaultIdempotentMutationPolicy()) {}

  template <typename RPCRetryPolicy, typename RPCBackoffPolicy,
            typename IdempotentMutationPolicy>
  Table(std::shared_ptr<DataClient> client, std::string const& table_id,
        RPCRetryPolicy retry_policy, RPCBackoffPolicy backoff_policy,
        IdempotentMutationPolicy idempotent_mutation_policy)
      : client_(std::move(client)),
        app_profile_id_(bigtable::AppProfileId("")),
        table_name_(bigtable::TableId(TableName(client_, table_id))),
        rpc_retry_policy_(retry_policy.clone()),
        rpc_backoff_policy_(backoff_policy.clone()),
        metadata_update_policy_(table_name(), MetadataParamTypes::TABLE_NAME),
        idempotent_mutation_policy_(idempotent_mutation_policy.clone()) {}

  Table(std::shared_ptr<DataClient> client,
        bigtable::AppProfileId app_profile_id, std::string const& table_id)
      : client_(std::move(client)),
        app_profile_id_(std::move(app_profile_id)),
        table_name_(bigtable::TableId(TableName(client_, table_id))),
        rpc_retry_policy_(bigtable::DefaultRPCRetryPolicy()),
        rpc_backoff_policy_(bigtable::DefaultRPCBackoffPolicy()),
        metadata_update_policy_(table_name(), MetadataParamTypes::TABLE_NAME),
        idempotent_mutation_policy_(
            bigtable::DefaultIdempotentMutationPolicy()) {}

  template <typename RPCRetryPolicy, typename RPCBackoffPolicy,
            typename IdempotentMutationPolicy>
  Table(std::shared_ptr<DataClient> client,
        bigtable::AppProfileId app_profile_id, std::string const& table_id,
        RPCRetryPolicy retry_policy, RPCBackoffPolicy backoff_policy,
        IdempotentMutationPolicy idempotent_mutation_policy)
      : client_(std::move(client)),
        app_profile_id_(std::move(app_profile_id)),
        table_name_(bigtable::TableId(TableName(client_, table_id))),
        rpc_retry_policy_(retry_policy.clone()),
        rpc_backoff_policy_(backoff_policy.clone()),
        metadata_update_policy_(table_name(), MetadataParamTypes::TABLE_NAME),
        idempotent_mutation_policy_(idempotent_mutation_policy.clone()) {}

  std::string const& table_name() const { return table_name_.get(); }

  //@{
  /**
   * @name No exception versions of Table::*
   *
   * These functions provide the same functionality as their counterparts in the
   * `bigtable::Table` class, but do not raise exceptions on errors, instead
   * they return the error on the status parameter.
   */
  std::vector<FailedMutation> Apply(SingleRowMutation&& mut);

  std::vector<FailedMutation> BulkApply(BulkMutation&& mut,
                                        grpc::Status& status);

  RowReader ReadRows(RowSet row_set, Filter filter,
                     bool raise_on_error = false);

  RowReader ReadRows(RowSet row_set, std::int64_t rows_limit, Filter filter,
                     bool raise_on_error = false);

  std::pair<bool, Row> ReadRow(std::string row_key, Filter filter,
                               grpc::Status& status);

  bool CheckAndMutateRow(std::string row_key, Filter filter,
                         std::vector<Mutation> true_mutations,
                         std::vector<Mutation> false_mutations,
                         grpc::Status& status);

  template <typename... Args>
  Row ReadModifyWriteRow(std::string row_key, grpc::Status& status,
                         bigtable::ReadModifyWriteRule rule, Args&&... rules) {
    ::google::bigtable::v2::ReadModifyWriteRowRequest request;
    request.set_row_key(std::move(row_key));
    bigtable::internal::SetCommonTableOperationRequest<
        ::google::bigtable::v2::ReadModifyWriteRowRequest>(
        request, app_profile_id_.get(), table_name_.get());

    // Generate a better compile time error message than the default one
    // if the types do not match
    static_assert(
        bigtable::internal::conjunction<
            std::is_convertible<Args, bigtable::ReadModifyWriteRule>...>::value,
        "The arguments passed to ReadModifyWriteRow(row_key,...) must be "
        "convertible to bigtable::ReadModifyWriteRule");

    // TODO(#336) - optimize this code by not copying the parameter pack.
    // Add first default rule
    *request.add_rules() = rule.as_proto_move();
    // Add if any additional rule is present
    std::initializer_list<bigtable::ReadModifyWriteRule> rule_list{
        std::forward<Args>(rules)...};
    for (auto args_rule : rule_list) {
      *request.add_rules() = args_rule.as_proto_move();
    }

    return CallReadModifyWriteRowRequest(request, status);
  }

  template <template <typename...> class Collection = std::vector>
  Collection<bigtable::RowKeySample> SampleRows(grpc::Status& status) {
    Collection<bigtable::RowKeySample> result;
    SampleRowsImpl(
        [&result](bigtable::RowKeySample rs) {
          result.emplace_back(std::move(rs));
        },
        [&result]() { result.clear(); }, status);
    return result;
  }

  //@}

 private:
  /**
   * Send request ReadModifyWriteRowRequest to modify the row and get it back
   */
  Row CallReadModifyWriteRowRequest(
      ::google::bigtable::v2::ReadModifyWriteRowRequest const& request,
      grpc::Status& status);

  /**
   * Refactor implementation to `.cc` file.
   *
   * Provides a compilation barrier so that the application is not
   * exposed to all the implementation details.
   *
   * @param inserter Function to insert the object to result.
   * @param clearer Function to clear the result object if RPC fails.
   */
  void SampleRowsImpl(
      std::function<void(bigtable::RowKeySample)> const& inserter,
      std::function<void()> const& clearer, grpc::Status& status);

  std::shared_ptr<DataClient> client_;
  bigtable::AppProfileId app_profile_id_;
  bigtable::TableId table_name_;
  std::shared_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::shared_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<IdempotentMutationPolicy> idempotent_mutation_policy_;
};

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_H_
