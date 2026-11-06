/*
 * cjson_owner.hpp - Owners for the nodes and the text of the backend.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * The backend allocates and releases with its own functions, so neither a node
 * nor a printed text can be held by a raw pointer that a conversion may throw
 * past. Both owners exist once so that the read side and the write side cannot
 * disagree about who releases what, and so that the release also happens when a
 * conversion fails.
 */
#ifndef JSONLENS_DETAIL_CJSON_OWNER_HPP
#define JSONLENS_DETAIL_CJSON_OWNER_HPP

#include "jsonlens/detail/cjson.hpp"

namespace jsonlens {
namespace detail {

/* Owns one node of the backend tree. */
class CJsonTree {
public:
  explicit CJsonTree(cJSON *root) : root_(root) {}
  ~CJsonTree() { cJSON_Delete(root_); }

  cJSON *get() const { return root_; }
  bool empty() const { return root_ == 0; }

  /* Gives the node away. The caller owns it from here on. */
  cJSON *release() {
    cJSON *const root = root_;
    root_ = 0;
    return root;
  }

private:
  CJsonTree(const CJsonTree &other) = delete;
  CJsonTree &operator=(const CJsonTree &other) = delete;

  cJSON *root_;
};

/* Owns the text the backend printed, which the backend frees with its own
   function and not with the operator delete of this program. */
class CJsonText {
public:
  explicit CJsonText(char *text) : text_(text) {}
  ~CJsonText() { cJSON_free(text_); }

  const char *get() const { return text_; }
  bool empty() const { return text_ == 0; }

private:
  CJsonText(const CJsonText &other) = delete;
  CJsonText &operator=(const CJsonText &other) = delete;

  char *text_;
};

} /* namespace detail */
} /* namespace jsonlens */

#endif /* JSONLENS_DETAIL_CJSON_OWNER_HPP */
