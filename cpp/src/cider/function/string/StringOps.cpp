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

#include "StringOps.h"
#include "cider/CiderException.h"
namespace StringOps_Namespace {

std::regex StringOp::generateRegex(const std::string& op_name,
                                   const std::string& regex_pattern,
                                   const std::string& regex_params,
                                   const bool supports_sub_matches) {
  bool is_case_sensitive = false;
  bool is_case_insensitive = false;

  for (const auto& c : regex_params) {
    switch (c) {
      case 'c':
        is_case_sensitive = true;
        break;
      case 'i':
        is_case_insensitive = true;
        break;
      case 'e': {
        if (!supports_sub_matches) {
          CIDER_THROW(CiderCompileException,
                      op_name + " does not support 'e' (sub-matches) option.");
        }
        // We use e to set sub-expression group in a separate initializer
        // but need to have this entry to not error on the default path
        break;
      }
      default: {
        if (supports_sub_matches) {
          CIDER_THROW(CiderCompileException,
                      "Unrecognized regex parameter for " + op_name +
                          ", expected either 'c' 'i', or 'e'.");
        }
        CIDER_THROW(CiderCompileException,
                    "Unrecognized regex parameter for " + op_name +
                        ", expected either 'c' or 'i'.");
      }
    }
  }
  if (!is_case_sensitive && !is_case_insensitive) {
    CIDER_THROW(CiderCompileException,
                op_name +
                    " params must either specify case-sensitivity ('c') or "
                    "case-insensitivity ('i').");
  }
  if (is_case_sensitive && is_case_insensitive) {
    CIDER_THROW(CiderCompileException,
                op_name +
                    " params cannot specify both case-sensitivity ('c') and "
                    "case-insensitivity ('i').");
  }
  if (is_case_insensitive) {
    return std::regex(regex_pattern,
                      std::regex_constants::extended | std::regex_constants::optimize |
                          std::regex_constants::icase);
  } else {
    return std::regex(regex_pattern,
                      std::regex_constants::extended | std::regex_constants::optimize);
  }
}

NullableStrType TryStringCast::operator()(const std::string& str) const {
  CHECK("Invalid string output for TryStringCast");
  return NullableStrType();
}

Datum TryStringCast::numericEval(const std::string_view& str) const {
  if (str.empty()) {
    return NullDatum(return_ti_);
  }
  // Need to make copy for now b/c StringToDatum can mod SQLTypeInfo arg
  SQLTypeInfo return_ti(return_ti_);
  try {
    return StringToDatum(str, return_ti);
  } catch (std::runtime_error& e) {
    return NullDatum(return_ti);
  }
}

NullableStrType CharLength::operator()(const std::string& str) const {
  CIDER_THROW(CiderCompileException, "invalid call to operator() for CharLength");
}

Datum CharLength::numericEval(const std::string_view& str) const {
  Datum res_datum;
  res_datum.bigintval = static_cast<int64_t>(str.length());
  return res_datum;
}

NullableStrType Lower::operator()(const std::string& str) const {
  std::string output_str(str);
  std::transform(
      output_str.begin(), output_str.end(), output_str.begin(), [](unsigned char c) {
        return std::tolower(c);
      });
  return output_str;
}

NullableStrType Upper::operator()(const std::string& str) const {
  std::string output_str(str);
  std::transform(
      output_str.begin(), output_str.end(), output_str.begin(), [](unsigned char c) {
        return std::toupper(c);
      });
  return output_str;
}

NullableStrType InitCap::operator()(const std::string& str) const {
  std::string output_str(str);
  bool last_char_whitespace = true;  // Beginning of string counts as whitespace
  for (auto& c : output_str) {
    if (isspace(c) || delimiter_bitmap_[reinterpret_cast<const uint8_t&>(c)]) {
      last_char_whitespace = true;
      continue;
    }
    if (last_char_whitespace) {
      c = toupper(c);
      last_char_whitespace = false;
    } else {
      c = tolower(c);
    }
  }
  return output_str;
}

NullableStrType Reverse::operator()(const std::string& str) const {
  const std::string reversed_str = std::string(str.rbegin(), str.rend());
  return reversed_str;
}

NullableStrType Repeat::operator()(const std::string& str) const {
  std::string repeated_str;
  repeated_str.reserve(str.size() * n_);
  for (size_t r = 0; r < n_; ++r) {
    repeated_str += str;
  }
  return repeated_str;
}

NullableStrType Concat::operator()(const std::string& str) const {
  return reverse_order_ ? str_literal_ + str : str + str_literal_;
}

NullableStrType Pad::operator()(const std::string& str) const {
  return pad_mode_ == Pad::PadMode::LEFT ? lpad(str) : rpad(str);
}

std::string Pad::lpad(const std::string& str) const {
  const auto str_len = str.size();
  const size_t chars_to_fill = str_len < padded_length_ ? padded_length_ - str_len : 0UL;
  if (chars_to_fill == 0UL) {
    return str.substr(0, padded_length_);
  }
  // If here we need to add characters from the padding_string_
  // to fill the difference between str_len and padded_length_
  if (padding_string_length_ == 1UL) {
    return std::string(chars_to_fill, padding_char_) + str;
  }

  std::string fitted_padding_str;
  fitted_padding_str.reserve(chars_to_fill);
  for (size_t i = 0; i < chars_to_fill; ++i) {
    fitted_padding_str.push_back(padding_string_[i % padding_string_length_]);
  }
  return fitted_padding_str + str;
}

std::string Pad::rpad(const std::string& str) const {
  const auto str_len = str.size();
  const size_t chars_to_fill = str_len < padded_length_ ? padded_length_ - str_len : 0UL;
  if (chars_to_fill == 0UL) {
    return str.substr(str_len - padded_length_, std::string::npos);
  }
  // If here we need to add characters from the padding_string_
  // to fill the difference between str_len and padded_length_
  if (padding_string_length_ == 1UL) {
    return str + std::string(chars_to_fill, padding_char_);
  }

  std::string fitted_padding_str;
  fitted_padding_str.reserve(chars_to_fill);
  for (size_t i = 0; i < chars_to_fill; ++i) {
    fitted_padding_str.push_back(padding_string_[i % padding_string_length_]);
  }
  return str + fitted_padding_str;
}

Pad::PadMode Pad::op_kind_to_pad_mode(const SqlStringOpKind op_kind) {
  switch (op_kind) {
    case SqlStringOpKind::LPAD:
      return PadMode::LEFT;
    case SqlStringOpKind::RPAD:
      return PadMode::RIGHT;
    default:
      UNREACHABLE();
      // Not reachable, but make compiler happy
      return PadMode::LEFT;
  };
}

NullableStrType Trim::operator()(const std::string& str) const {
  const auto str_len = str.size();
  size_t trim_begin = 0;
  if (trim_mode_ == TrimMode::LEFT || trim_mode_ == TrimMode::BOTH) {
    while (trim_begin < str_len &&
           trim_char_bitmap_[reinterpret_cast<const uint8_t&>(str[trim_begin])]) {
      ++trim_begin;
    }
  }
  size_t trim_end = str_len - 1;
  if (trim_mode_ == TrimMode::RIGHT || trim_mode_ == TrimMode::BOTH) {
    while (trim_end > trim_begin &&
           trim_char_bitmap_[reinterpret_cast<const uint8_t&>(str[trim_end])]) {
      --trim_end;
    }
  }
  if (trim_begin == 0 && trim_end == str_len - 1) {
    // no trimming should be applied
    return str;
  }
  if (trim_begin >= trim_end) {
    // all chars should be trimmed
    return std::string("");
  }
  return str.substr(trim_begin, trim_end + 1 - trim_begin);
}

Trim::TrimMode Trim::op_kind_to_trim_mode(const SqlStringOpKind op_kind) {
  switch (op_kind) {
    case SqlStringOpKind::TRIM:
      return Trim::TrimMode::BOTH;
    case SqlStringOpKind::LTRIM:
      return Trim::TrimMode::LEFT;
    case SqlStringOpKind::RTRIM:
      return Trim::TrimMode::RIGHT;
    default:
      UNREACHABLE();
      // Not reachable, but make compiler happy
      return Trim::TrimMode::BOTH;
  };
}

NullableStrType Substring::operator()(const std::string& str) const {
  // If start_ is negative then we start abs(start_) characters from the end
  // of the string
  const int64_t str_len = str.size();
  const int64_t wrapped_start = start_ >= 0 ? start_ : str_len + start_;
  const size_t capped_start =
      wrapped_start > str_len ? str_len : (wrapped_start < 0 ? 0 : wrapped_start);
  return str.substr(capped_start, length_);
}

NullableStrType Overlay::operator()(const std::string& base_str) const {
  // If start_ is negative then we start abs(start_) characters from the end
  // of the string
  const int64_t str_len = base_str.size();
  const int64_t wrapped_start = start_ >= 0 ? start_ : str_len + start_;
  const size_t capped_start =
      wrapped_start > str_len ? str_len : (wrapped_start < 0 ? 0 : wrapped_start);
  std::string replaced_str = base_str.substr(0, capped_start);
  replaced_str += insert_str_;
  const size_t remainder_start =
      std::min(wrapped_start + replacement_length_, size_t(str_len));
  const size_t remainder_length = static_cast<size_t>(str_len) - remainder_start;
  replaced_str += base_str.substr(remainder_start, remainder_length);
  return replaced_str;
}

NullableStrType Replace::operator()(const std::string& str) const {
  std::string replaced_str(str);

  size_t search_start_index = 0;
  while (true) {
    search_start_index = replaced_str.find(pattern_str_, search_start_index);
    if (search_start_index == std::string::npos) {
      break;
    }
    replaced_str.replace(search_start_index, pattern_str_len_, replacement_str_);
    search_start_index += replacement_str_len_;
  }
  return replaced_str;
}

NullableStrType SplitPart::operator()(const std::string& str) const {
  // If split_part_ is negative then it is taken as the number
  // of split parts from the end of the string
  // if limit_ is zero then no limit is specified

  if (delimiter_ == "") {
    return str;
  }

  if (limit_ == 1) {
    // should return a list with only 1 string (which should not be splitted)
    if (split_part_ == 1) {
      return str;
    } else {
      return NullableStrType();  // out of range
    }
  }

  const size_t str_len = str.size();
  size_t delimiter_pos = reverse_ ? str_len : 0UL;
  size_t last_delimiter_pos;
  size_t delimiter_idx = 0UL;
  size_t limit_counter = 0UL;

  do {
    last_delimiter_pos = delimiter_pos;
    delimiter_pos =
        reverse_ ? str.rfind(delimiter_, delimiter_pos - 1UL)
                 : str.find(delimiter_,
                            // shouldn't skip delimiter length on first search attempt
                            delimiter_pos == 0 ? 0 : delimiter_pos + delimiter_length_);
    // do ++limit_counter in the loop to prevent bugs caused by shortcut execution
    ++limit_counter;
    // however, we still keep ++delimiter_idx in while condition check to ensure
    // the property that delimiter_idx == 0 iff delimiter does not exist in input string
  } while (delimiter_pos != std::string::npos && ++delimiter_idx < split_part_ &&
           (limit_ == 0 || limit_counter < limit_));

  if (limit_ && limit_counter == limit_) {
    // split has reached maximum split limit
    // treat whatever remains as a whole by extending delimiter_pos to end-of-string
    delimiter_pos = std::string::npos;
  }

  if (delimiter_idx == 0 && split_part_ == 1) {
    // delimiter does not exist, but the first split is requested, return the entire str
    return str;
  }

  if (delimiter_pos == std::string::npos &&
      (delimiter_idx < split_part_ - 1UL || delimiter_idx < 1UL)) {
    // split_part_ was out of range
    return NullableStrType();  // null string
  }
  if (reverse_) {
    const size_t substr_start =
        delimiter_pos == std::string::npos ? 0UL : delimiter_pos + delimiter_length_;
    return str.substr(substr_start, last_delimiter_pos - substr_start);
  } else {
    const size_t substr_start =
        split_part_ == 1UL ? 0UL : last_delimiter_pos + delimiter_length_;
    return str.substr(substr_start, delimiter_pos - substr_start);
  }
}

NullableStrType RegexpReplace::operator()(const std::string& str) const {
  const int64_t str_len = str.size();
  const size_t wrapped_start = static_cast<size_t>(std::min(
      start_pos_ >= 0 ? start_pos_ : std::max(str_len + start_pos_, 0L), str_len));
  if (occurrence_ == 0L) {
    // occurrence_ == 0: replace all occurrences
    std::string result;
    std::string::const_iterator replace_start(str.cbegin() + wrapped_start);
    std::regex_replace(std::back_inserter(result),
                       replace_start,
                       str.cend(),
                       regex_pattern_,
                       replacement_);
    return str.substr(0UL, wrapped_start) + result;
  } else {
    // only replace n-th occurrence
    const auto occurrence_match_pos = RegexpReplace::get_nth_regex_match(
        str,
        wrapped_start,
        regex_pattern_,
        occurrence_ > 0 ? occurrence_ - 1 : occurrence_);
    if (occurrence_match_pos.first == std::string::npos) {
      // No match found, return original string
      return str;
    }
    std::string result;
    std::string::const_iterator replace_start(str.cbegin() + occurrence_match_pos.first);
    std::string::const_iterator replace_end(str.cbegin() + occurrence_match_pos.second);
    std::string replaced_match;
    std::regex_replace(std::back_inserter(replaced_match),
                       replace_start,
                       replace_end,
                       regex_pattern_,
                       replacement_);
    return str.substr(0UL, occurrence_match_pos.first) + replaced_match +
           str.substr(occurrence_match_pos.second, std::string::npos);
  }
}

std::pair<size_t, size_t> RegexpReplace::get_nth_regex_match(
    const std::string& str,
    const size_t start_pos,
    const std::regex& regex_pattern,
    const int64_t occurrence) {
  std::vector<std::pair<size_t, size_t>> regex_match_positions;
  std::string::const_iterator search_start(str.cbegin() + start_pos);
  std::smatch match;
  int64_t match_idx = 0;
  size_t string_pos = start_pos;
  while (std::regex_search(search_start, str.cend(), match, regex_pattern)) {
    string_pos += match.position(0) + match.length(0);
    regex_match_positions.emplace_back(
        std::make_pair(string_pos - match.length(0), string_pos));
    if (match_idx++ == occurrence) {
      return regex_match_positions.back();
    }
    search_start =
        match.suffix().first;  // Move to position after last char of matched string
    // Position is relative to last match/initial iterator, so need to increment our
    // string_pos accordingly
  }
  // occurrence only could have a valid match if negative here,
  // but don't want to check in inner loop for performance reasons
  const int64_t wrapped_match = occurrence >= 0 ? occurrence : match_idx + occurrence;
  if (wrapped_match < 0 || wrapped_match >= match_idx) {
    // Represents a non-match
    return std::make_pair(std::string::npos, std::string::npos);
  }
  return regex_match_positions[wrapped_match];
}

NullableStrType RegexpSubstr::operator()(const std::string& str) const {
  const int64_t str_len = str.size();
  const size_t wrapped_start = static_cast<size_t>(std::min(
      start_pos_ >= 0 ? start_pos_ : std::max(str_len + start_pos_, 0L), str_len));
  int64_t match_idx = 0;
  // Apears std::regex_search does not support string_view?
  std::vector<std::string> regex_matches;
  std::string::const_iterator search_start(str.cbegin() + wrapped_start);
  std::smatch match;
  while (std::regex_search(search_start, str.cend(), match, regex_pattern_)) {
    if (match_idx++ == occurrence_) {
      if (sub_match_info_.first) {
        return RegexpSubstr::get_sub_match(match, sub_match_info_);
      }
      return NullableStrType(match[0]);
    }
    regex_matches.emplace_back(match[0]);
    search_start =
        match.suffix().first;  // Move to position after last char of matched string
  }
  const int64_t wrapped_match = occurrence_ >= 0 ? occurrence_ : match_idx + occurrence_;
  if (wrapped_match < 0 || wrapped_match >= match_idx) {
    return NullableStrType();
  }
  if (sub_match_info_.first) {
    return RegexpSubstr::get_sub_match(match, sub_match_info_);
  }
  return regex_matches[wrapped_match];
}

std::string RegexpSubstr::get_sub_match(const std::smatch& match,
                                        const std::pair<bool, int64_t> sub_match_info) {
  const int64_t num_sub_matches = match.size() - 1;
  const int64_t wrapped_sub_match = sub_match_info.second >= 0
                                        ? sub_match_info.second
                                        : num_sub_matches + sub_match_info.second;
  if (wrapped_sub_match < 0 || wrapped_sub_match >= num_sub_matches) {
    return "";
  }
  return match[wrapped_sub_match + 1];
}

std::pair<bool, int64_t> RegexpSubstr::set_sub_match_info(
    const std::string& regex_pattern,
    const int64_t sub_match_group_idx) {
  // in OmnisciDb, whether to return submatches is controlled by an optional 'e' parameter
  // <https://github.com/heavyai/heavydb/blob/master/Tests/StringFunctionsTest.cpp#L1353>
  // but in regexp_extract, it should automatically return capturing groups
  // whenever the parameter sub_match_group_idx > 0

  // if (regex_pattern.find("e", 0UL) == std::string::npos) {
  //   return std::make_pair(false, 0UL);
  // }
  if (sub_match_group_idx < 0) {
    CIDER_THROW(CiderCompileException, "sub_match_group_idx must be non-negative");
  }
  bool do_sub_match = sub_match_group_idx > 0L;
  return std::make_pair(do_sub_match,
                        do_sub_match ? sub_match_group_idx - 1 : sub_match_group_idx);
}

std::string StringOps::operator()(const std::string& str) const {
  // std::string modified_str(str);
  NullableStrType modified_str(str);
  for (const auto& string_op : string_ops_) {
    modified_str = string_op->operator()(modified_str.str);
    if (modified_str.is_null) {
      return "";  // How we currently represent dictionary-encoded nulls
    }
  }
  return modified_str.str;
}

std::string_view StringOps::operator()(const std::string_view sv,
                                       std::string& sv_storage) const {
  sv_storage = sv;
  NullableStrType nullable_str(sv);
  for (const auto& string_op : string_ops_) {
    nullable_str = string_op->operator()(nullable_str.str);
    if (nullable_str.is_null) {
      return "";
    }
  }
  sv_storage = nullable_str.str;
  return sv_storage;
}

Datum StringOps::numericEval(const std::string_view& str) const {
  NullableStrType modified_str(str);
  const auto num_string_producing_ops = string_ops_.size() - 1;
  for (size_t string_op_idx = 0; string_op_idx < num_string_producing_ops;
       ++string_op_idx) {
    const auto& string_op = string_ops_[string_op_idx];
    modified_str = string_op->operator()(modified_str.str);
    if (modified_str.is_null) {
      break;
    }
  }
  return string_ops_.back()->numericEval(modified_str.str);
}

std::vector<std::unique_ptr<const StringOp>> StringOps::genStringOpsFromOpInfos(
    const std::vector<StringOpInfo>& string_op_infos) const {
  // Should we handle pure literal expressions here as well
  // even though they are currently rewritten to string literals?
  std::vector<std::unique_ptr<const StringOp>> string_ops;
  string_ops.reserve(string_op_infos.size());
  for (const auto& string_op_info : string_op_infos) {
    string_ops.emplace_back(std::move(gen_string_op(string_op_info)));
  }
  return string_ops;
}

// Free functions follow

std::unique_ptr<const StringOp> gen_string_op(const StringOpInfo& string_op_info) {
  std::optional<std::string> var_string_optional_literal;
  const auto op_kind = string_op_info.getOpKind();
  const auto& return_ti = string_op_info.getReturnType();

  if (string_op_info.hasNullLiteralArg()) {
    return std::make_unique<const NullOp>(var_string_optional_literal, op_kind);
  }

  const auto num_non_variable_literals = string_op_info.numNonVariableLiterals();
  if (string_op_info.hasVarStringLiteral()) {
    CHECK_EQ(num_non_variable_literals + 1UL, string_op_info.numLiterals());
    var_string_optional_literal = string_op_info.getStringLiteral(0);
  }

  switch (op_kind) {
    case SqlStringOpKind::LOWER: {
      CHECK_EQ(num_non_variable_literals, 0UL);
      return std::make_unique<const Lower>(var_string_optional_literal);
    }
    case SqlStringOpKind::UPPER: {
      CHECK_EQ(num_non_variable_literals, 0UL);
      return std::make_unique<const Upper>(var_string_optional_literal);
    }
    case SqlStringOpKind::INITCAP: {
      CHECK_EQ(num_non_variable_literals, 0UL);
      return std::make_unique<const InitCap>(var_string_optional_literal);
    }
    case SqlStringOpKind::REVERSE: {
      CHECK_EQ(num_non_variable_literals, 0UL);
      return std::make_unique<const Reverse>(var_string_optional_literal);
    }
    case SqlStringOpKind::REPEAT: {
      CHECK_EQ(num_non_variable_literals, 1UL);
      const auto num_repeats_literal = string_op_info.getIntLiteral(1);
      return std::make_unique<const Repeat>(var_string_optional_literal,
                                            num_repeats_literal);
    }
    case SqlStringOpKind::CHAR_LENGTH: {
      CHECK_EQ(num_non_variable_literals, 0);
      return std::make_unique<const CharLength>(var_string_optional_literal);
    }
    case SqlStringOpKind::CONCAT:
    case SqlStringOpKind::RCONCAT: {
      CHECK_EQ(num_non_variable_literals, 1UL);
      const auto str_literal = string_op_info.getStringLiteral(1);
      // Handle lhs literals by having RCONCAT operator set a flag
      return std::make_unique<const Concat>(
          var_string_optional_literal, str_literal, op_kind == SqlStringOpKind::RCONCAT);
    }
    case SqlStringOpKind::LPAD:
    case SqlStringOpKind::RPAD: {
      CHECK_EQ(num_non_variable_literals, 2UL);
      const auto padded_length_literal = string_op_info.getIntLiteral(1);
      const auto padding_string_literal = string_op_info.getStringLiteral(2);
      return std::make_unique<Pad>(var_string_optional_literal,
                                   op_kind,
                                   padded_length_literal,
                                   padding_string_literal);
    }
    case SqlStringOpKind::TRIM:
    case SqlStringOpKind::LTRIM:
    case SqlStringOpKind::RTRIM: {
      CHECK_EQ(num_non_variable_literals, 1UL);
      const auto trim_chars_literal = string_op_info.getStringLiteral(1);
      return std::make_unique<Trim>(
          var_string_optional_literal, op_kind, trim_chars_literal);
    }
    case SqlStringOpKind::SUBSTRING: {
      CHECK_GE(num_non_variable_literals, 1UL);
      CHECK_LE(num_non_variable_literals, 2UL);
      const auto start_pos_literal = string_op_info.getIntLiteral(1);
      const bool has_length_literal = string_op_info.intLiteralArgAtIdxExists(2);
      if (has_length_literal) {
        const auto length_literal = string_op_info.getIntLiteral(2);
        return std::make_unique<const Substring>(
            var_string_optional_literal, start_pos_literal, length_literal);
      } else {
        return std::make_unique<const Substring>(var_string_optional_literal,
                                                 start_pos_literal);
      }
    }
    case SqlStringOpKind::OVERLAY: {
      CHECK_GE(num_non_variable_literals, 2UL);
      CHECK_LE(num_non_variable_literals, 3UL);
      const auto replace_string_literal = string_op_info.getStringLiteral(1);
      const auto start_pos_literal = string_op_info.getIntLiteral(2);
      const bool has_length_literal = string_op_info.intLiteralArgAtIdxExists(3);
      if (has_length_literal) {
        const auto length_literal = string_op_info.getIntLiteral(3);
        return std::make_unique<const Overlay>(var_string_optional_literal,
                                               replace_string_literal,
                                               start_pos_literal,
                                               length_literal);
      } else {
        return std::make_unique<const Overlay>(
            var_string_optional_literal, replace_string_literal, start_pos_literal);
      }
    }
    case SqlStringOpKind::REPLACE: {
      CHECK_GE(num_non_variable_literals, 2UL);
      CHECK_LE(num_non_variable_literals, 2UL);
      const auto pattern_string_literal = string_op_info.getStringLiteral(1);
      const auto replacement_string_literal = string_op_info.getStringLiteral(2);
      return std::make_unique<const Replace>(var_string_optional_literal,
                                             pattern_string_literal,
                                             replacement_string_literal);
    }
    case SqlStringOpKind::SPLIT_PART: {
      CHECK_GE(num_non_variable_literals, 2UL);
      CHECK_LE(num_non_variable_literals, 3UL);
      const auto delimiter_literal = string_op_info.getStringLiteral(1);
      const bool has_limit = string_op_info.intLiteralArgAtIdxExists(3);
      if (has_limit) {
        // split(input, delimiter, limit)[split_part]
        const auto limit_literal = string_op_info.getIntLiteral(2);
        const auto split_part_literal = string_op_info.getIntLiteral(3);
        return std::make_unique<const SplitPart>(var_string_optional_literal,
                                                 delimiter_literal,
                                                 limit_literal,
                                                 split_part_literal);
      } else {
        // split(input, delimiter)[split_part] / split_part(input, delimiter, index)
        const auto split_part_literal = string_op_info.getIntLiteral(2);
        return std::make_unique<const SplitPart>(
            var_string_optional_literal, delimiter_literal, split_part_literal);
      }
    }
    case SqlStringOpKind::REGEXP_REPLACE: {
      // 4 mandatory literals + up to 3 optional arguments
      CHECK_GE(num_non_variable_literals, 4UL);
      CHECK_LE(num_non_variable_literals, 7UL);
      const auto pattern_literal = string_op_info.getStringLiteral(1);
      const auto replacement_literal = string_op_info.getStringLiteral(2);
      const auto start_pos_literal = string_op_info.getIntLiteral(3);
      const auto occurrence_literal = string_op_info.getIntLiteral(4);

      // TODO: (YBRua) currently only supports case sensitive ('c')
      // this interface should be changed to comply with substrait specification
      // which uses enum types instead of char[] for specifying options
      // const auto regex_params_literal = string_op_info.getStringLiteral(5);
      const std::string regex_params_literal = "c";

      return std::make_unique<const RegexpReplace>(var_string_optional_literal,
                                                   pattern_literal,
                                                   replacement_literal,
                                                   start_pos_literal,
                                                   occurrence_literal,
                                                   regex_params_literal);
    }
    case SqlStringOpKind::REGEXP_SUBSTR: {
      // 3 required parameters and 3 options, although options are not supported for now
      CHECK_GE(num_non_variable_literals, 3UL);
      CHECK_LE(num_non_variable_literals, 6UL);
      const auto pattern_literal = string_op_info.getStringLiteral(1);
      const auto start_pos_literal = string_op_info.getIntLiteral(2);
      const auto occurrence_literal = string_op_info.getIntLiteral(3);
      // const auto case_sensitivitiy_literal = string_op_info.getStringLiteral(4);
      // const auto multiline_literal = string_op_info.getIntLiteral(5);
      // const auto dotall_literal = stringop_info.getIntLiteral(6);

      std::string regex_params_literal = "c";  // case sensitive
      int64_t sub_match_idx_literal = 0;       // return the entire match

      return std::make_unique<const RegexpSubstr>(var_string_optional_literal,
                                                  pattern_literal,
                                                  start_pos_literal,
                                                  occurrence_literal,
                                                  regex_params_literal,
                                                  sub_match_idx_literal);
    }
    case SqlStringOpKind::REGEXP_EXTRACT: {
      // PrestoDb extension function, reuses exising functionalities in RegexpSubstr
      CHECK_GE(num_non_variable_literals, 2UL);
      CHECK_LE(num_non_variable_literals, 2UL);
      const auto pattern_literal = string_op_info.getStringLiteral(1);
      // id of caputuring group to be returned, 0 means returning entire match
      const auto sub_match_idx_literal = string_op_info.getIntLiteral(2);

      std::string regex_params_literal = "c";
      int64_t start_pos_literal = 1;   // start from beginning
      int64_t occurrence_literal = 1;  // returns the first match

      return std::make_unique<const RegexpSubstr>(var_string_optional_literal,
                                                  pattern_literal,
                                                  start_pos_literal,
                                                  occurrence_literal,
                                                  regex_params_literal,
                                                  sub_match_idx_literal);
    }
    case SqlStringOpKind::TRY_STRING_CAST: {
      CHECK_EQ(num_non_variable_literals, 0UL);
      return std::make_unique<const TryStringCast>(return_ti,
                                                   var_string_optional_literal);
    }
    default: {
      UNREACHABLE();
      return std::make_unique<NullOp>(var_string_optional_literal, op_kind);
    }
  }
  // Make compiler happy
  return std::make_unique<NullOp>(var_string_optional_literal, op_kind);
}

std::pair<std::string, bool /* is null */> apply_string_op_to_literals(
    const StringOpInfo& string_op_info) {
  CHECK(string_op_info.hasVarStringLiteral());
  if (string_op_info.hasNullLiteralArg()) {
    const std::string null_str{""};
    return std::make_pair(null_str, true);
  }
  const auto string_op = gen_string_op(string_op_info);
  return string_op->operator()().toPair();
}

Datum apply_numeric_op_to_literals(const StringOpInfo& string_op_info) {
  CHECK(string_op_info.hasVarStringLiteral());
  const auto string_op = gen_string_op(string_op_info);
  return string_op->numericEval();
}

}  // namespace StringOps_Namespace