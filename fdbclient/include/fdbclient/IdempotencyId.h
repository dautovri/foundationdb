/*
 * IdempotencyId.h
 *
 * This source file is part of the FoundationDB open source project
 *
 * Copyright 2013-2022 Apple Inc. and the FoundationDB project authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "flow/Arena.h"
#include "flow/IRandom.h"

struct IdempotencyId {
	explicit IdempotencyId(UID id) : IdempotencyId(StringRef(reinterpret_cast<const uint8_t*>(&id), sizeof(UID))) {}
	explicit IdempotencyId(StringRef id) {
		ASSERT(id.size() >= 16);
		ASSERT(id.size() < 256);
		if (id.size() == 16 &&
		    /* If it's 16 bytes but first < 256 we still need to use an indirection to avoid ambiguity. */
		    reinterpret_cast<const uint64_t*>(id.begin())[0] >= 256) {
			first = reinterpret_cast<const uint64_t*>(id.begin())[0];
			second.id = reinterpret_cast<const uint64_t*>(id.begin())[1];
		} else {
			first = id.size();
			second.ptr = new uint8_t[id.size()];
			memcpy(second.ptr, id.begin(), id.size());
		}
	}

	bool operator==(const IdempotencyId& other) const { return asStringRef() == other.asStringRef(); }

	IdempotencyId(IdempotencyId&& other) { *this = std::move(other); }

	IdempotencyId& operator=(IdempotencyId&& other) {
		first = other.first;
		if (other.indirect()) {
			second.ptr = other.second.ptr;
			other.first = 256; // Make sure other no longer thinks it has ownership. Anything >= 256 would do.
		} else {
			second.id = other.second.id;
		}
		return *this;
	}

	~IdempotencyId() {
		if (indirect()) {
			delete second.ptr;
		}
	}

private:
	bool indirect() const { return first < 256; }
	StringRef asStringRef() const {
		if (indirect()) {
			return StringRef(reinterpret_cast<const uint8_t*>(second.ptr), first);
		} else {
			return StringRef(reinterpret_cast<const uint8_t*>(this), sizeof(*this));
		}
	}
	uint64_t first;
	union {
		uint64_t id;
		uint8_t* ptr;
	} second; // If first < 256, then ptr is valid. Otherwise id is valid.
	friend std::hash<IdempotencyId>;
};

namespace std {
template <>
struct hash<IdempotencyId> {
	std::size_t operator()(const IdempotencyId& id) const { return std::hash<StringRef>{}(id.asStringRef()); }
};
} // namespace std

// The plan is to use this as a key in a potentially large hashtable, so it should be compact.
static_assert(sizeof(IdempotencyId) == 16);
