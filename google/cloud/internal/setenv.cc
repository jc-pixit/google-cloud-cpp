// Copyright 2018 Google LLC
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

#include "google/cloud/internal/setenv.h"
#ifdef WIN32
// We need _putenv_s(), which is defined here:
#include <stdlib.h>
#else
// On Unix-like systems we need setenv()/unsetenv(), which are defined here:
#include <cstdlib>
#endif  // WIN32

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

void UnsetEnv(char const* variable) {
#ifdef WIN32
  // Use _putenv_s() instead of SetEnvironmentVariable() because std::getenv()
  // caches the environment during program startup.
  (void)_putenv_s(variable, "");
#else
  unsetenv(variable);
#endif  // WIN32
}

void SetEnv(char const* variable, char const* value) {
  // Use _putenv_s() instead of SetEnvironmentVariable() because std::getenv()
  // caches the environment during program startup.
  if (value != nullptr) {
#ifdef WIN32
    (void)_putenv_s(variable, value);
#else
    (void)setenv(variable, value, 1);
#endif  // WIN32
  } else {
    UnsetEnv(variable);
  }
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
