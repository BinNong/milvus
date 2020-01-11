// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "SegmentReader.h"

#include <memory>

#include "Vector.h"
#include "codecs/default/DefaultCodec.h"
#include "store/Directory.h"
#include "utils/Exception.h"
#include "utils/Log.h"

namespace milvus {
namespace segment {

SegmentReader::SegmentReader(const std::string& directory) {
    directory_ptr_ = std::make_shared<store::Directory>(directory);
}

Status
SegmentReader::LoadCache(bool& in_cache) {
    in_cache = false;
    return Status::OK();
}

Status
SegmentReader::Load() {
    // TODO(zhiru)
    codec::DefaultCodec default_codec;
    try {
        directory_ptr_->Create();
        default_codec.GetVectorsFormat()->read(directory_ptr_, segment_ptr_->vectors_ptr_);
        default_codec.GetDeletedDocsFormat()->read(directory_ptr_, segment_ptr_->deleted_docs_ptr_);
    } catch (Exception& e) {
        std::string err_msg = "Failed to load segment. " + std::string(e.what());
        ENGINE_LOG_ERROR << err_msg;
        return Status(e.code(), err_msg);
    }
    return Status::OK();
}

Status
SegmentReader::LoadUids(std::vector<doc_id_t>& uids) {
    codec::DefaultCodec default_codec;
    try {
        directory_ptr_->Create();
        default_codec.GetVectorsFormat()->readUids(directory_ptr_, uids);
    } catch (Exception& e) {
        std::string err_msg = "Failed to load uids. " + std::string(e.what());
        ENGINE_LOG_ERROR << err_msg;
        return Status(e.code(), err_msg);
    }
    return Status::OK();
}

Status
SegmentReader::GetSegment(SegmentPtr& segment_ptr) {
    segment_ptr = segment_ptr_;
}

Status
SegmentReader::LoadBloomFilter(segment::IdBloomFilterPtr& id_bloom_filter_ptr) {
    codec::DefaultCodec default_codec;
    try {
        directory_ptr_->Create();
        default_codec.GetIdBloomFilterFormat()->read(directory_ptr_, id_bloom_filter_ptr);
    } catch (Exception& e) {
        std::string err_msg = "Failed to load bloom filter. " + std::string(e.what());
        ENGINE_LOG_ERROR << err_msg;
        return Status(e.code(), err_msg);
    }
    return Status::OK();
}

}  // namespace segment
}  // namespace milvus
