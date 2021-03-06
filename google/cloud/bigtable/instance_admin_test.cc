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

#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/grpc_error.h"
#include "google/cloud/bigtable/internal/make_unique.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>

namespace {
namespace btproto = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

using MockAdminClient = bigtable::testing::MockInstanceAdminClient;

std::string const kProjectId = "the-project";

/// A fixture for the bigtable::InstanceAdmin tests.
class InstanceAdminTest : public ::testing::Test {
 protected:
  void SetUp() override {
    using namespace ::testing;

    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  }

  std::shared_ptr<MockAdminClient> client_ =
      std::make_shared<MockAdminClient>();
};

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_instances_lambda = [](std::string expected_token,
                                       std::string returned_token,
                                       std::vector<std::string> instance_ids) {
  return [expected_token, returned_token, instance_ids](
             grpc::ClientContext* ctx,
             btproto::ListInstancesRequest const& request,
             btproto::ListInstancesResponse* response) {
    auto const project_name = "projects/" + kProjectId;
    EXPECT_EQ(project_name, request.parent());
    EXPECT_EQ(expected_token, request.page_token());

    EXPECT_NE(nullptr, response);
    for (auto const& instance_id : instance_ids) {
      auto& instance = *response->add_instances();
      instance.set_name(project_name + "/instances/" + instance_id);
    }
    // Return the right token.
    response->set_next_page_token(returned_token);
    return grpc::Status::OK;
  };
};

// A lambda to create lambdas. Basically we would be rewriting the same lambda
// twice without using this thing.
auto create_cluster = []() {
  return [](grpc::ClientContext* ctx, btproto::GetClusterRequest const& request,
            btproto::Cluster* response) {
    EXPECT_NE(nullptr, response);
    response->set_name(request.name());
    return grpc::Status::OK;
  };
};

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_clusters_lambda =
    [](std::string expected_token, std::string returned_token,
       std::string instance_id, std::vector<std::string> cluster_ids) {
      return [expected_token, returned_token, instance_id, cluster_ids](
                 grpc::ClientContext* ctx,
                 btproto::ListClustersRequest const& request,
                 btproto::ListClustersResponse* response) {
        auto const instance_name =
            "projects/" + kProjectId + "/instances/" + instance_id;
        EXPECT_EQ(instance_name, request.parent());
        EXPECT_EQ(expected_token, request.page_token());

        EXPECT_NE(nullptr, response);
        for (auto const& cluster_id : cluster_ids) {
          auto& cluster = *response->add_clusters();
          cluster.set_name(instance_name + "/clusters/" + cluster_id);
        }
        // Return the right token.
        response->set_next_page_token(returned_token);
        return grpc::Status::OK;
      };
    };

/**
 * Helper class to create the expectations for a simple RPC call.
 *
 * Given the type of the request and responses, this struct provides a function
 * to create a mock implementation with the right signature and checks.
 *
 * @tparam RequestType the protobuf type for the request.
 * @tparam ResponseType the protobuf type for the response.
 */
template <typename RequestType, typename ResponseType>
struct MockRpcFactory {
  using SignatureType = grpc::Status(grpc::ClientContext* ctx,
                                     RequestType const& request,
                                     ResponseType* response);

  /// Refactor the boilerplate common to most tests.
  static std::function<SignatureType> Create(std::string expected_request) {
    return std::function<SignatureType>(
        [expected_request](grpc::ClientContext* ctx, RequestType const& request,
                           ResponseType* response) {
          RequestType expected;
          // Cannot use ASSERT_TRUE() here, it has an embedded "return;"
          EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
              expected_request, &expected));
          std::string delta;
          google::protobuf::util::MessageDifferencer differencer;
          differencer.ReportDifferencesToString(&delta);
          EXPECT_TRUE(differencer.Compare(expected, request)) << delta;

          EXPECT_NE(nullptr, response);
          return grpc::Status::OK;
        });
  }
};

}  // anonymous namespace

/// @test Verify basic functionality in the `bigtable::InstanceAdmin` class.
TEST_F(InstanceAdminTest, Default) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_EQ("the-project", tested.project_id());
}

TEST_F(InstanceAdminTest, CopyConstructor) {
  bigtable::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::InstanceAdmin copy(source);
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, MoveConstructor) {
  bigtable::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::InstanceAdmin copy(std::move(source));
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, CopyAssignment) {
  std::shared_ptr<MockAdminClient> other_client =
      std::make_shared<MockAdminClient>();
  std::string other_project = "other-project";
  EXPECT_CALL(*other_client, project())
      .WillRepeatedly(testing::ReturnRef(other_project));

  bigtable::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::InstanceAdmin dest(other_client);
  EXPECT_NE(expected, dest.project_id());
  dest = source;
  EXPECT_EQ(expected, dest.project_id());
}

TEST_F(InstanceAdminTest, MoveAssignment) {
  std::shared_ptr<MockAdminClient> other_client =
      std::make_shared<MockAdminClient>();
  std::string other_project = "other-project";
  EXPECT_CALL(*other_client, project())
      .WillRepeatedly(testing::ReturnRef(other_project));

  bigtable::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::InstanceAdmin dest(other_client);
  EXPECT_NE(expected, dest.project_id());
  dest = std::move(source);
  EXPECT_EQ(expected, dest.project_id());
}

/// @test Verify that `bigtable::InstanceAdmin::ListInstances` works in the easy
/// case.
TEST_F(InstanceAdminTest, ListInstances) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  auto mock_list_instances = create_list_instances_lambda("", "", {"t0", "t1"});
  EXPECT_CALL(*client_, ListInstances(_, _, _))
      .WillOnce(Invoke(mock_list_instances));

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListInstances();
  std::string instance_name = tested.project_name();
  ASSERT_EQ(2UL, actual.size());
  EXPECT_EQ(instance_name + "/instances/t0", actual[0].name());
  EXPECT_EQ(instance_name + "/instances/t1", actual[1].name());
}

/// @test Verify that `bigtable::InstanceAdmin::ListInstances` handles failures.
TEST_F(InstanceAdminTest, ListInstancesRecoverableFailures) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, btproto::ListInstancesRequest const& request,
         btproto::ListInstancesResponse* response) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      };
  auto batch0 = create_list_instances_lambda("", "token-001", {"t0", "t1"});
  auto batch1 = create_list_instances_lambda("token-001", "", {"t2", "t3"});
  EXPECT_CALL(*client_, ListInstances(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch0))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch1));

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListInstances();
  std::string project_name = tested.project_name();
  ASSERT_EQ(4UL, actual.size());
  EXPECT_EQ(project_name + "/instances/t0", actual[0].name());
  EXPECT_EQ(project_name + "/instances/t1", actual[1].name());
  EXPECT_EQ(project_name + "/instances/t2", actual[2].name());
  EXPECT_EQ(project_name + "/instances/t3", actual[3].name());
}

/**
 * @test Verify that `bigtable::InstanceAdmin::ListInstances` handles
 * unrecoverable failures.
 */
TEST_F(InstanceAdminTest, ListInstancesUnrecoverableFailures) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, ListInstances(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

// After all the setup, make the actual call we want to test.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(tested.ListInstances(), std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.ListInstances(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that `bigtable::InstanceAdmin::CreateInstance` works.
TEST_F(InstanceAdminTest, CreateInstance) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, CreateInstance(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          btproto::CreateInstanceRequest const& request,
                          google::longrunning::Operation* response) {
        auto const project_name = "projects/" + kProjectId;
        EXPECT_EQ(project_name, request.parent());
        return grpc::Status::OK;
      }));

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance'
      display_name: 'foo bar'
      state: READY
      type: PRODUCTION
  )";
  btproto::Instance expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));
  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(Invoke(
          [&expected](grpc::ClientContext*,
                      google::longrunning::GetOperationRequest const& request,
                      google::longrunning::Operation* operation) {
            operation->set_done(true);
            auto any = bigtable::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(expected);
            operation->set_allocated_response(any.release());
            return grpc::Status::OK;
          }));

  auto future = tested.CreateInstance(bigtable::InstanceConfig(
      bigtable::InstanceId("test-instance"), bigtable::DisplayName("foo bar"),
      {{"c1", {"a-zone", 3, bigtable::ClusterConfig::SSD}}}));
  auto actual = future.get();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected, actual)) << delta;
}

/// @test Verify that `bigtable::InstanceAdmin::CreateInstance` works.
TEST_F(InstanceAdminTest, CreateInstanceImmediatelyReady) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance'
      display_name: 'foo bar'
      state: READY
      type: PRODUCTION
  )";
  btproto::Instance expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));
  EXPECT_CALL(*client_, CreateInstance(_, _, _))
      .WillOnce(
          Invoke([&expected](grpc::ClientContext*,
                             btproto::CreateInstanceRequest const& request,
                             google::longrunning::Operation* response) {
            auto const project_name = "projects/" + kProjectId;
            EXPECT_EQ(project_name, request.parent());
            response->set_done(true);
            response->set_name("operation-name");
            auto any = bigtable::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(expected);
            response->set_allocated_response(any.release());
            return grpc::Status::OK;
          }));

  EXPECT_CALL(*client_, GetOperation(_, _, _)).Times(0);

  auto future = tested.CreateInstance(bigtable::InstanceConfig(
      bigtable::InstanceId("test-instance"), bigtable::DisplayName("foo bar"),
      {{"c1", {"a-zone", 3, bigtable::ClusterConfig::SSD}}}));
  auto actual = future.get();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected, actual)) << delta;
}

/// @test Failures while polling in `bigtable::InstanceAdmin::CreateInstance`.
TEST_F(InstanceAdminTest, CreateInstancePollRecoverableFailures) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, CreateInstance(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          btproto::CreateInstanceRequest const& request,
                          google::longrunning::Operation* response) {
        auto const project_name = "projects/" + kProjectId;
        EXPECT_EQ(project_name, request.parent());
        return grpc::Status::OK;
      }));

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance'
      display_name: 'foo bar'
      state: READY
      type: PRODUCTION
  )";
  btproto::Instance expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));
  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation*) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      }))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation*) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      }))
      .WillOnce(Invoke(
          [&expected](grpc::ClientContext*,
                      google::longrunning::GetOperationRequest const& request,
                      google::longrunning::Operation* operation) {
            operation->set_done(true);
            auto any = bigtable::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(expected);
            operation->set_allocated_response(any.release());
            return grpc::Status::OK;
          }));

  auto future = tested.CreateInstance(bigtable::InstanceConfig(
      bigtable::InstanceId("test-instance"), bigtable::DisplayName("foo bar"),
      {{"c1", {"a-zone", 3, bigtable::ClusterConfig::SSD}}}));
  auto actual = future.get();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected, actual)) << delta;
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
/// @test Failures in `bigtable::InstanceAdmin::CreateInstance`.
TEST_F(InstanceAdminTest, CreateInstanceRequestFailure) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, CreateInstance(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  auto future = tested.CreateInstance(bigtable::InstanceConfig(
      bigtable::InstanceId("test-instance"), bigtable::DisplayName("foo bar"),
      {{"c1", {"a-zone", 3, bigtable::ClusterConfig::SSD}}}));

  EXPECT_THROW(future.get(), bigtable::GRpcError);
}

/// @test Failures while polling in `bigtable::InstanceAdmin::CreateInstance`.
TEST_F(InstanceAdminTest, CreateInstancePollUnrecoverableFailure) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, CreateInstance(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          btproto::CreateInstanceRequest const& request,
                          google::longrunning::Operation* response) {
        auto const project_name = "projects/" + kProjectId;
        EXPECT_EQ(project_name, request.parent());
        return grpc::Status::OK;
      }));

  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  auto future = tested.CreateInstance(bigtable::InstanceConfig(
      bigtable::InstanceId("test-instance"), bigtable::DisplayName("foo bar"),
      {{"c1", {"a-zone", 3, bigtable::ClusterConfig::SSD}}}));
  EXPECT_THROW(future.get(), bigtable::GRpcError);
}

/// @test Polling in `bigtable::InstanceAdmin::CreateInstance` returns failure.
TEST_F(InstanceAdminTest, CreateInstancePollReturnsFailure) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, CreateInstance(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          btproto::CreateInstanceRequest const& request,
                          google::longrunning::Operation* response) {
        auto const project_name = "projects/" + kProjectId;
        EXPECT_EQ(project_name, request.parent());
        return grpc::Status::OK;
      }));

  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(
          Invoke([](grpc::ClientContext*,
                    google::longrunning::GetOperationRequest const& request,
                    google::longrunning::Operation* operation) {
            operation->set_done(true);
            auto error = bigtable::internal::make_unique<google::rpc::Status>();
            error->set_code(grpc::StatusCode::FAILED_PRECONDITION);
            error->set_message("something is broken");
            operation->set_allocated_error(error.release());
            return grpc::Status::OK;
          }));

  auto future = tested.CreateInstance(bigtable::InstanceConfig(
      bigtable::InstanceId("test-instance"), bigtable::DisplayName("foo bar"),
      {{"c1", {"a-zone", 3, bigtable::ClusterConfig::SSD}}}));
  EXPECT_THROW(future.get(), bigtable::GRpcError);
}

/// @test Failures in `bigtable::InstanceAdmin::UpdateInstance`.
TEST_F(InstanceAdminTest, UpdateInstanceRequestFailure) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, UpdateInstance(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  btproto::Instance instance;
  bigtable::InstanceUpdateConfig instance_update_config(std::move(instance));
  auto future = tested.UpdateInstance(std::move(instance_update_config));
  EXPECT_THROW(future.get(), bigtable::GRpcError);
}

/// @test Failures while polling in `bigtable::InstanceAdmin::UpdateInstance`.
TEST_F(InstanceAdminTest, UpdateInstancePollUnrecoverableFailure) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, UpdateInstance(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          btproto::PartialUpdateInstanceRequest const& request,
                          google::longrunning::Operation* response) {
        return grpc::Status::OK;
      }));

  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  btproto::Instance instance;
  bigtable::InstanceUpdateConfig instance_update_config(std::move(instance));
  auto future = tested.UpdateInstance(std::move(instance_update_config));
  EXPECT_THROW(future.get(), bigtable::GRpcError);
}

/// @test Polling in `bigtable::InstanceAdmin::UpdateInstance` returns failure.
TEST_F(InstanceAdminTest, UpdateInstancePollReturnsFailure) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, UpdateInstance(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          btproto::PartialUpdateInstanceRequest const& request,
                          google::longrunning::Operation* response) {
        return grpc::Status::OK;
      }));

  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(
          Invoke([](grpc::ClientContext*,
                    google::longrunning::GetOperationRequest const& request,
                    google::longrunning::Operation* operation) {
            operation->set_done(true);
            auto error = bigtable::internal::make_unique<google::rpc::Status>();
            error->set_code(grpc::StatusCode::FAILED_PRECONDITION);
            error->set_message("something is broken");
            operation->set_allocated_error(error.release());
            return grpc::Status::OK;
          }));

  btproto::Instance instance;
  bigtable::InstanceUpdateConfig instance_update_config(std::move(instance));
  auto future = tested.UpdateInstance(std::move(instance_update_config));
  EXPECT_THROW(future.get(), bigtable::GRpcError);
}

/// @test Failures in `bigtable::InstanceAdmin::UpdateCluster`.
TEST_F(InstanceAdminTest, UpdateClusterRequestFailure) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, UpdateCluster(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  btproto::Cluster cluster;
  bigtable::ClusterConfig cluster_config(std::move(cluster));
  auto future = tested.UpdateCluster(std::move(cluster_config));
  EXPECT_THROW(future.get(), bigtable::GRpcError);
}

/// @test Failures while polling in `bigtable::InstanceAdmin::UpdateCluster`.
TEST_F(InstanceAdminTest, UpdateClusterPollUnrecoverableFailure) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, UpdateCluster(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*, btproto::Cluster const& request,
                          google::longrunning::Operation* response) {
        return grpc::Status::OK;
      }));

  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  btproto::Cluster cluster;
  bigtable::ClusterConfig cluster_config(std::move(cluster));
  auto future = tested.UpdateCluster(std::move(cluster_config));
  EXPECT_THROW(future.get(), bigtable::GRpcError);
}

/// @test Polling in `bigtable::InstanceAdmin::UpdateCluster` returns failure.
TEST_F(InstanceAdminTest, UpdateClusterPollReturnsFailure) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, UpdateCluster(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*, btproto::Cluster const& request,
                          google::longrunning::Operation* response) {
        return grpc::Status::OK;
      }));

  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(
          Invoke([](grpc::ClientContext*,
                    google::longrunning::GetOperationRequest const& request,
                    google::longrunning::Operation* operation) {
            operation->set_done(true);
            auto error = bigtable::internal::make_unique<google::rpc::Status>();
            error->set_code(grpc::StatusCode::FAILED_PRECONDITION);
            error->set_message("something is broken");
            operation->set_allocated_error(error.release());
            return grpc::Status::OK;
          }));

  btproto::Cluster cluster;
  bigtable::ClusterConfig cluster_config(std::move(cluster));
  auto future = tested.UpdateCluster(std::move(cluster_config));
  EXPECT_THROW(future.get(), bigtable::GRpcError);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

/// @test Verify that `bigtable::InstanceAdmin::UpdateInstance` works.
TEST_F(InstanceAdminTest, UpdateInstance) {
  using ::testing::_;
  using ::testing::Invoke;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, UpdateInstance(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          btproto::PartialUpdateInstanceRequest const& request,
                          google::longrunning::Operation* response) {
        auto const instance_name =
            "projects/my-project/instances/test-instance";
        EXPECT_EQ(instance_name, request.instance().name());
        return grpc::Status::OK;
      }));

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance'
      display_name: 'foo bar'
      state: READY
      type: PRODUCTION
      labels: {
        key: 'foo1'
        value: 'bar1'
      }
      labels: {
        key: 'foo2'
        value: 'bar2'
      }
  )";

  btproto::Instance expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));

  btproto::Instance expected_copy;
  expected_copy.CopyFrom(expected);

  bigtable::InstanceUpdateConfig instance_update_config(std::move(expected));

  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(
          Invoke([&expected_copy](
                     grpc::ClientContext*,
                     google::longrunning::GetOperationRequest const& request,
                     google::longrunning::Operation* operation) {
            operation->set_done(true);
            auto any = bigtable::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(expected_copy);
            operation->set_allocated_response(any.release());
            return grpc::Status::OK;
          }));

  auto future = tested.UpdateInstance(std::move(instance_update_config));
  auto actual = future.get();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected_copy, actual)) << delta;
}

/// @test Verify that `bigtable::InstanceAdmin::UpdateInstance` works.
TEST_F(InstanceAdminTest, UpdateInstanceImmediatelyReady) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance'
      display_name: 'foo bar'
      state: READY
      type: PRODUCTION
  )";
  btproto::Instance expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));

  btproto::Instance expected_copy;
  expected_copy.CopyFrom(expected);

  bigtable::InstanceUpdateConfig instance_update_config(std::move(expected));

  EXPECT_CALL(*client_, UpdateInstance(_, _, _))
      .WillOnce(Invoke(
          [&expected_copy](grpc::ClientContext*,
                           btproto::PartialUpdateInstanceRequest const& request,
                           google::longrunning::Operation* response) {
            auto const instance_name =
                "projects/my-project/instances/test-instance";
            EXPECT_EQ(instance_name, request.instance().name());
            response->set_done(true);
            response->set_name("operation-name");
            auto any = bigtable::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(expected_copy);
            response->set_allocated_response(any.release());
            return grpc::Status::OK;
          }));

  EXPECT_CALL(*client_, GetOperation(_, _, _)).Times(0);

  auto future = tested.UpdateInstance(std::move(instance_update_config));
  auto actual = future.get();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected_copy, actual)) << delta;
}

/// @test Failures while polling in `bigtable::InstanceAdmin::UpdateInstance`.
TEST_F(InstanceAdminTest, UpdateInstancePollRecoverableFailures) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, UpdateInstance(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          btproto::PartialUpdateInstanceRequest const& request,
                          google::longrunning::Operation* response) {
        auto const instance_name =
            "projects/my-project/instances/test-instance";
        EXPECT_EQ(instance_name, request.instance().name());
        return grpc::Status::OK;
      }));

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance'
      display_name: 'foo bar'
      state: READY
      type: PRODUCTION
  )";
  btproto::Instance expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));

  btproto::Instance expected_copy;
  expected_copy.CopyFrom(expected);

  bigtable::InstanceUpdateConfig instance_update_config(std::move(expected));

  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation*) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      }))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation*) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      }))
      .WillOnce(
          Invoke([&expected_copy](
                     grpc::ClientContext*,
                     google::longrunning::GetOperationRequest const& request,
                     google::longrunning::Operation* operation) {
            operation->set_done(true);
            auto any = bigtable::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(expected_copy);
            operation->set_allocated_response(any.release());
            return grpc::Status::OK;
          }));

  auto future = tested.UpdateInstance(std::move(instance_update_config));
  auto actual = future.get();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected_copy, actual)) << delta;
}

/// @test Verify that DeleteInstance works in the positive case.
TEST_F(InstanceAdminTest, DeleteInstance) {
  using namespace ::testing;
  using google::protobuf::Empty;
  bigtable::InstanceAdmin tested(client_);
  std::string expected_text = R"""(
  name: 'projects/the-project/instances/the-instance'
      )""";
  auto mock = MockRpcFactory<btproto::DeleteInstanceRequest, Empty>::Create(
      expected_text);
  EXPECT_CALL(*client_, DeleteInstance(_, _, _)).WillOnce(Invoke(mock));
  // After all the setup, make the actual call we want to test.
  tested.DeleteInstance("the-instance");
}

/// @test Verify unrecoverable error for DeleteInstance
TEST_F(InstanceAdminTest, DeleteInstanceUnrecoverableError) {
  using namespace ::testing;
  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteInstance(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
// After all the setup, make the actual call we want to test.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(tested.DeleteInstance("other-instance"), std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.DeleteInstance("other-instance"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that recoverable error for DeleteInstance
TEST_F(InstanceAdminTest, DeleteInstanceRecoverableError) {
  using namespace ::testing;
  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteInstance(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

// After all the setup, make the actual call we want to test.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(tested.DeleteInstance("other-instance"), std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.DeleteInstance("other-instance"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that `bigtable::InstanceAdmin::ListClusters` works in the easy
/// case.
TEST_F(InstanceAdminTest, ListClusters) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  std::string const& instance_id = "the-instance";
  auto mock_list_clusters =
      create_list_clusters_lambda("", "", instance_id, {"t0", "t1"});
  EXPECT_CALL(*client_, ListClusters(_, _, _))
      .WillOnce(Invoke(mock_list_clusters));

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListClusters(instance_id);
  std::string instance_name = tested.InstanceName(instance_id);
  ASSERT_EQ(2UL, actual.size());
  EXPECT_EQ(instance_name + "/clusters/t0", actual[0].name());
  EXPECT_EQ(instance_name + "/clusters/t1", actual[1].name());
}

/// @test Verify that `bigtable::InstanceAdmin::ListClusters` handles failures.
TEST_F(InstanceAdminTest, ListClustersRecoverableFailures) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, btproto::ListClustersRequest const& request,
         btproto::ListClustersResponse* response) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      };
  std::string const& instance_id = "the-instance";
  auto batch0 =
      create_list_clusters_lambda("", "token-001", instance_id, {"t0", "t1"});
  auto batch1 =
      create_list_clusters_lambda("token-001", "", instance_id, {"t2", "t3"});
  EXPECT_CALL(*client_, ListClusters(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch0))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch1));

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListClusters(instance_id);
  std::string instance_name = tested.InstanceName(instance_id);
  ASSERT_EQ(4UL, actual.size());
  EXPECT_EQ(instance_name + "/clusters/t0", actual[0].name());
  EXPECT_EQ(instance_name + "/clusters/t1", actual[1].name());
  EXPECT_EQ(instance_name + "/clusters/t2", actual[2].name());
  EXPECT_EQ(instance_name + "/clusters/t3", actual[3].name());
}

/**
 * @test Verify that `bigtable::InstanceAdmin::ListClusters` handles
 * unrecoverable failures.
 */
TEST_F(InstanceAdminTest, ListClustersUnrecoverableFailures) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, ListClusters(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  std::string const& instance_id = "the-instance";
// After all the setup, make the actual call we want to test.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(tested.ListClusters(instance_id), std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.ListClusters(instance_id),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify positive scenario for GetCluster
TEST_F(InstanceAdminTest, GetCluster) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  auto mock = create_cluster();
  EXPECT_CALL(*client_, GetCluster(_, _, _)).WillOnce(Invoke(mock));
  bigtable::InstanceId instance_id("the-instance");
  bigtable::ClusterId cluster_id("the-cluster");
  // After all the setup, make the actual call we want to test.
  auto cluster = tested.GetCluster(instance_id, cluster_id);
  EXPECT_EQ("projects/the-project/instances/the-instance/clusters/the-cluster",
            cluster.name());
}

/// @test Verify unrecoverable error for GetCluster
TEST_F(InstanceAdminTest, GetClusterUnrecoverableError) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, GetCluster(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  bigtable::InstanceId instance_id("other-instance");
  bigtable::ClusterId cluster_id("other-cluster");

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(tested.GetCluster(instance_id, cluster_id), std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.GetCluster(instance_id, cluster_id),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify recoverable errors for GetCluster
TEST_F(InstanceAdminTest, GetClusterRecoverableError) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  auto mock_recoverable_failure = [](grpc::ClientContext* ctx,
                                     btproto::GetClusterRequest const& request,
                                     btproto::Cluster* response) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };

  auto mock_cluster = create_cluster();
  EXPECT_CALL(*client_, GetCluster(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_cluster));

  // After all the setup, make the actual call we want to test.
  bigtable::InstanceId instance_id("the-instance");
  bigtable::ClusterId cluster_id("the-cluster");
  // After all the setup, make the actual call we want to test.
  auto cluster = tested.GetCluster(instance_id, cluster_id);
  EXPECT_EQ("projects/the-project/instances/the-instance/clusters/the-cluster",
            cluster.name());
}

/// @test Verify that DeleteCluster works in the positive case.
TEST_F(InstanceAdminTest, DeleteCluster) {
  using namespace ::testing;
  using google::protobuf::Empty;
  bigtable::InstanceAdmin tested(client_);
  std::string expected_text = R"""(
  name: 'projects/the-project/instances/the-instance/clusters/the-cluster'
      )""";
  auto mock = MockRpcFactory<btproto::DeleteClusterRequest, Empty>::Create(
      expected_text);
  EXPECT_CALL(*client_, DeleteCluster(_, _, _)).WillOnce(Invoke(mock));
  bigtable::InstanceId instance_id("the-instance");
  bigtable::ClusterId cluster_id("the-cluster");
  // After all the setup, make the actual call we want to test.
  tested.DeleteCluster(instance_id, cluster_id);
}

/// @test Verify unrecoverable error for DeleteCluster
TEST_F(InstanceAdminTest, DeleteClusterUnrecoverableError) {
  using namespace ::testing;
  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteCluster(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  bigtable::InstanceId instance_id("other-instance");
  bigtable::ClusterId cluster_id("other-cluster");
// After all the setup, make the actual call we want to test.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(tested.DeleteCluster(instance_id, cluster_id), std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.DeleteCluster(instance_id, cluster_id),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that recoverable error for DeleteCluster
TEST_F(InstanceAdminTest, DeleteClusterRecoverableError) {
  using namespace ::testing;
  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteCluster(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  bigtable::InstanceId instance_id("other-instance");
  bigtable::ClusterId cluster_id("other-cluster");
// After all the setup, make the actual call we want to test.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(tested.DeleteCluster(instance_id, cluster_id), std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.DeleteCluster(instance_id, cluster_id),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that `bigtable::InstanceAdmin::CreateCluster` works.
TEST_F(InstanceAdminTest, CreateCluster) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, CreateCluster(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          btproto::CreateClusterRequest const& request,
                          google::longrunning::Operation* response) {
        auto const project_name =
            "projects/" + kProjectId + "/instances/test-instance";
        EXPECT_EQ(project_name, request.parent());
        return grpc::Status::OK;
      }));

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance'
      location: 'projects/my-project/locations/fake-zone'
      default_storage_type: SSD
  )";

  auto mock_successs = [](grpc::ClientContext* ctx,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
    operation->set_done(false);
    return grpc::Status::OK;
  };
  btproto::Cluster expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));
  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillOnce(Invoke(mock_successs))
      .WillOnce(Invoke(mock_successs))
      .WillOnce(Invoke(
          [&expected](grpc::ClientContext*,
                      google::longrunning::GetOperationRequest const& request,
                      google::longrunning::Operation* operation) {
            operation->set_done(true);
            auto any = bigtable::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(expected);
            operation->set_allocated_response(any.release());
            return grpc::Status::OK;
          }));

  auto future = tested.CreateCluster(
      bigtable::ClusterConfig("fake-zone", 10, bigtable::ClusterConfig::SSD),
      bigtable::InstanceId("test-instance"),
      bigtable::ClusterId("other-cluster"));

  auto actual = future.get();
  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected, actual)) << delta;
}

/// @test Verify that `bigtable::InstanceAdmin::CreateCluster` works.
TEST_F(InstanceAdminTest, CreateClusterImmediatelyReady) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance'
      location: 'projects/my-project/locations/fake-zone'
      default_storage_type: SSD
  )";
  btproto::Cluster expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));
  EXPECT_CALL(*client_, CreateCluster(_, _, _))
      .WillOnce(Invoke([&expected](grpc::ClientContext*,
                                   btproto::CreateClusterRequest const& request,
                                   google::longrunning::Operation* response) {
        auto const project_name =
            "projects/" + kProjectId + "/instances/test-instance";
        EXPECT_EQ(project_name, request.parent());
        response->set_done(true);
        response->set_name("operation-name");
        auto any = bigtable::internal::make_unique<google::protobuf::Any>();
        any->PackFrom(expected);
        response->set_allocated_response(any.release());
        return grpc::Status::OK;
      }));

  EXPECT_CALL(*client_, GetOperation(_, _, _)).Times(0);

  auto future = tested.CreateCluster(
      bigtable::ClusterConfig("fake-zone", 10, bigtable::ClusterConfig::SSD),
      bigtable::InstanceId("test-instance"),
      bigtable::ClusterId("other-cluster"));
  auto actual = future.get();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected, actual)) << delta;
}

/// @test Failures while polling in `bigtable::InstanceAdmin::CreateCluster`.
TEST_F(InstanceAdminTest, CreateClusterPollRecoverableFailures) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, CreateCluster(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          btproto::CreateClusterRequest const& request,
                          google::longrunning::Operation* response) {
        auto const project_name =
            "projects/" + kProjectId + "/instances/test-instance";
        EXPECT_EQ(project_name, request.parent());
        return grpc::Status::OK;
      }));

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance'
      location: 'projects/my-project/locations/fake-zone'
      default_storage_type: SSD
  )";

  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx,
         google::longrunning::GetOperationRequest const&,
         google::longrunning::Operation*) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      };
  btproto::Cluster expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));
  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation*) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      }))
      .WillOnce(Invoke(
          [&expected](grpc::ClientContext*,
                      google::longrunning::GetOperationRequest const& request,
                      google::longrunning::Operation* operation) {
            operation->set_done(true);
            auto any = bigtable::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(expected);
            operation->set_allocated_response(any.release());
            return grpc::Status::OK;
          }));

  auto future = tested.CreateCluster(
      bigtable::ClusterConfig("fake-zone", 10, bigtable::ClusterConfig::SSD),
      bigtable::InstanceId("test-instance"),
      bigtable::ClusterId("other-cluster"));
  auto actual = future.get();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected, actual)) << delta;
}

/// @test Verify that `bigtable::InstanceAdmin::UpdateCluster` works.
TEST_F(InstanceAdminTest, UpdateCluster) {
  using ::testing::_;
  using ::testing::Invoke;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, UpdateCluster(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*, btproto::Cluster const& request,
                          google::longrunning::Operation* response) {
        auto const cluster_name =
            "projects/my-project/instances/test-instance/clusters/test-cluster";
        EXPECT_EQ(cluster_name, request.name());
        return grpc::Status::OK;
      }));

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance/clusters/test-cluster'
      location: 'Location1'
      state: READY
      serve_nodes: 0
      default_storage_type: SSD
  )";

  btproto::Cluster expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));

  btproto::Cluster expected_copy;
  expected_copy.CopyFrom(expected);

  bigtable::ClusterConfig cluster_config(std::move(expected));

  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation* operation) {
        operation->set_done(false);
        return grpc::Status::OK;
      }))
      .WillOnce(
          Invoke([&expected_copy](
                     grpc::ClientContext*,
                     google::longrunning::GetOperationRequest const& request,
                     google::longrunning::Operation* operation) {
            operation->set_done(true);
            auto any = bigtable::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(expected_copy);
            operation->set_allocated_response(any.release());
            return grpc::Status::OK;
          }));

  auto future = tested.UpdateCluster(std::move(cluster_config));
  auto actual = future.get();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected_copy, actual)) << delta;
}

/// @test Verify that `bigtable::InstanceAdmin::UpdateCluster` works.
TEST_F(InstanceAdminTest, UpdateClusterImmediatelyReady) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance/clusters/test-cluster'
      location: 'Location1'
      state: READY
      serve_nodes: 0
      default_storage_type: SSD
  )";
  btproto::Cluster expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));

  btproto::Cluster expected_copy;
  expected_copy.CopyFrom(expected);
  bigtable::ClusterConfig cluster_config(std::move(expected));

  EXPECT_CALL(*client_, UpdateCluster(_, _, _))
      .WillOnce(Invoke([&expected_copy](
                           grpc::ClientContext*,
                           btproto::Cluster const& request,
                           google::longrunning::Operation* response) {
        auto const cluster_name =
            "projects/my-project/instances/test-instance/clusters/test-cluster";
        EXPECT_EQ(cluster_name, request.name());
        response->set_done(true);
        response->set_name("operation-name");
        auto any = bigtable::internal::make_unique<google::protobuf::Any>();
        any->PackFrom(expected_copy);
        response->set_allocated_response(any.release());
        return grpc::Status::OK;
      }));

  EXPECT_CALL(*client_, GetOperation(_, _, _)).Times(0);

  auto future = tested.UpdateCluster(std::move(cluster_config));
  auto actual = future.get();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected_copy, actual)) << delta;
}

/// @test Failures while polling in `bigtable::InstanceAdmin::UpdateCluster`.
TEST_F(InstanceAdminTest, UpdateClusterPollRecoverableFailures) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, UpdateCluster(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*, btproto::Cluster const& request,
                          google::longrunning::Operation* response) {
        auto const cluster_name =
            "projects/my-project/instances/test-instance/clusters/test-cluster";
        EXPECT_EQ(cluster_name, request.name());
        return grpc::Status::OK;
      }));

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance/clusters/test-cluster'
      location: 'Location1'
      state: READY
      serve_nodes: 0
      default_storage_type: SSD
  )";
  btproto::Cluster expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));

  btproto::Cluster expected_copy;
  expected_copy.CopyFrom(expected);

  bigtable::ClusterConfig cluster_config(std::move(expected));

  EXPECT_CALL(*client_, GetOperation(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation*) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      }))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          google::longrunning::GetOperationRequest const&,
                          google::longrunning::Operation*) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      }))
      .WillOnce(
          Invoke([&expected_copy](
                     grpc::ClientContext*,
                     google::longrunning::GetOperationRequest const& request,
                     google::longrunning::Operation* operation) {
            operation->set_done(true);
            auto any = bigtable::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(expected_copy);
            operation->set_allocated_response(any.release());
            return grpc::Status::OK;
          }));

  auto future = tested.UpdateCluster(std::move(cluster_config));
  auto actual = future.get();

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected_copy, actual)) << delta;
}
