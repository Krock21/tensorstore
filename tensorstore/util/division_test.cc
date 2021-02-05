// Copyright 2020 The TensorStore Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tensorstore/util/division.h"

namespace {

static_assert(3 == tensorstore::FloorOfRatio(10, 3));
static_assert(-4 == tensorstore::FloorOfRatio(-10, 3));

static_assert(4 == tensorstore::CeilOfRatio(10, 3));
static_assert(-3 == tensorstore::CeilOfRatio(-10, 3));

static_assert(10 == tensorstore::RoundUpTo(7, 5));
static_assert(10 == tensorstore::RoundUpTo(10, 5));

static_assert(3 == tensorstore::NonnegativeMod(10, 7));
static_assert(4 == tensorstore::NonnegativeMod(-10, 7));

}  // namespace
