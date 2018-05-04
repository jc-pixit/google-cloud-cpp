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

#include "bigtable/client/internal/instance_admin.h"
#include "bigtable/client/internal/unary_client_utils.h"
#include <type_traits>

namespace btproto = ::google::bigtable::admin::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {
static_assert(std::is_copy_assignable<bigtable::noex::InstanceAdmin>::value,
              "bigtable::noex::InstanceAdmin must be CopyAssignable");

using ClientUtils =
    bigtable::internal::noex::UnaryClientUtils<bigtable::InstanceAdminClient>;

std::vector<btproto::Instance> InstanceAdmin::ListInstances(
    grpc::Status& status) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  // Build the RPC request, try to minimize copying.
  std::vector<btproto::Instance> result;
  std::string page_token;
  do {
    btproto::ListInstancesRequest request;
    request.set_page_token(std::move(page_token));
    request.set_parent(project_name_);

    auto response = ClientUtils::MakeCall(
        *client_, *rpc_policy, *backoff_policy, metadata_update_policy_,
        &InstanceAdminClient::ListInstances, request,
        "InstanceAdmin::ListInstances", status, true);
    if (not status.ok()) {
      return result;
    }

    for (auto& x : *response.mutable_instances()) {
      result.emplace_back(std::move(x));
    }
    page_token = std::move(*response.mutable_next_page_token());
  } while (not page_token.empty());
  return result;
}

btproto::Instance InstanceAdmin::GetInstance(std::string const& instance_id,
                                             grpc::Status& status) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  btproto::GetInstanceRequest request;
  // Setting instance name.
  request.set_name(project_name_ + "/instances/" + instance_id);

  // Call RPC call to get response
  return ClientUtils::MakeCall(*client_, *rpc_policy, *backoff_policy,
                               metadata_update_policy_,
                               &InstanceAdminClient::GetInstance, request,
                               "InstanceAdmin::GetInstance", status, true);
}

void InstanceAdmin::DeleteInstance(std::string const& instance_id,
                                   grpc::Status& status) {
  btproto::DeleteInstanceRequest request;
  request.set_name(InstanceName(instance_id));

  // This API is not idempotent, lets call it without retry
  ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy_,
      &InstanceAdminClient::DeleteInstance, request,
      "InstanceAdmin::DeleteInstance", status);
}
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable