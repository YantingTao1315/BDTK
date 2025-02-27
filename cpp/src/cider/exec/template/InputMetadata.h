/*
 * Copyright(c) 2022-2023 Intel Corporation.
 * Copyright (c) OmniSci, Inc. and its affiliates.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef QUERYENGINE_INPUTMETADATA_H
#define QUERYENGINE_INPUTMETADATA_H

#include "exec/template/RelAlgExecutionUnit.h"
#include "exec/template/common/descriptors/InputDescriptors.h"
#include "util/memory/DataProvider.h"
#include "util/memory/Fragmenter.h"

#include <unordered_map>

namespace Catalog_Namespace {
class Catalog;
}  // namespace Catalog_Namespace

class Executor;

class TemporaryTable {
 public:
  TemporaryTable() {}
  TemporaryTable(const ResultSetPtr& rs) { results_.emplace_back(rs); }
  TemporaryTable(ResultSetPtr&& rs) { results_.emplace_back(rs); }
  TemporaryTable(const std::vector<ResultSetPtr>& results) : results_(results) {}
  TemporaryTable(std::vector<ResultSetPtr>&& results) : results_(results) {}

  TemporaryTable(const TemporaryTable& other) = default;
  TemporaryTable(TemporaryTable&& other) = default;

  TemporaryTable& operator=(const TemporaryTable& other) = default;
  TemporaryTable& operator=(TemporaryTable&& other) = default;

  int getFragCount() const { return static_cast<int>(results_.size()); }

  const ResultSetPtr& getResultSet(const int frag_id) const {
    CHECK(frag_id < getFragCount());
    return results_[frag_id];
  }

  SQLTypeInfo getColType(const size_t col_idx) const;

  bool empty() const { return results_.empty(); }

  ResultSetPtr& operator[](size_t idx) { return results_[idx]; }
  const ResultSetPtr& operator[](size_t idx) const { return results_[idx]; }

 private:
  std::vector<ResultSetPtr> results_;
};

using TemporaryTables = std::unordered_map<int, TemporaryTable>;

struct InputTableInfo {
  int db_id;
  int table_id;
  Fragmenter_Namespace::TableInfo info;
};

class InputTableInfoCache {
 public:
  InputTableInfoCache(Executor* executor);

  Fragmenter_Namespace::TableInfo getTableInfo(const int table_id);

  void clear();

 private:
  std::unordered_map<int, Fragmenter_Namespace::TableInfo> cache_;
  Executor* executor_;
};

size_t get_frag_count_of_table(const int table_id, Executor* executor);

#endif  // QUERYENGINE_INPUTMETADATA_H
