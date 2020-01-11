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

#include "SegmentWriter.h"

#include <memory>

#include "SegmentReader.h"
#include "Vector.h"
#include "codecs/default/DefaultCodec.h"
#include "store/Directory.h"
#include "utils/Exception.h"
#include "utils/Log.h"

namespace milvus {
namespace segment {

SegmentWriter::SegmentWriter(const std::string& directory) {
    directory_ptr_ = std::make_shared<store::Directory>(directory);
    segment_ptr_ = std::make_shared<Segment>();
}

Status
SegmentWriter::AddVectors(const std::string& field_name, const std::vector<uint8_t>& data,
                          const std::vector<doc_id_t>& uids) {
    auto found = segment_ptr_->vectors_ptr_->vectors_map.find(field_name);
    if (found == segment_ptr_->vectors_ptr_->vectors_map.end()) {
        segment_ptr_->vectors_ptr_->vectors_map[field_name] = std::make_shared<Vector>();
    }
    segment_ptr_->vectors_ptr_->vectors_map[field_name]->AddData(data);
    segment_ptr_->vectors_ptr_->vectors_map[field_name]->AddUids(uids);

    return Status::OK();
}

Status
SegmentWriter::Serialize() {
    // TODO(zhiru)
    auto status = WriteVectors();
    if (!status.ok()) {
        return status;
    }
    status = WriteBloomFilter();
    return status;
}

Status
SegmentWriter::WriteVectors() {
    codec::DefaultCodec default_codec;
    try {
        directory_ptr_->Create();
        default_codec.GetVectorsFormat()->write(directory_ptr_, segment_ptr_->vectors_ptr_);
    } catch (Exception& e) {
        std::string err_msg = "Failed to write vectors. " + std::string(e.what());
        ENGINE_LOG_ERROR << err_msg;
        return Status(e.code(), err_msg);
    }
    return Status::OK();
}

Status
SegmentWriter::WriteBloomFilter() {
    codec::DefaultCodec default_codec;
    try {
        directory_ptr_->Create();
        default_codec.GetIdBloomFilterFormat()->create(directory_ptr_, segment_ptr_->id_bloom_filter_ptr_);
        // TODO(zhiru): ?
        for (auto& kv : segment_ptr_->vectors_ptr_->vectors_map) {
            auto& uids = kv.second->GetUids();
            for (auto& uid : uids) {
                segment_ptr_->id_bloom_filter_ptr_->Add(uid);
            }
        }
        default_codec.GetIdBloomFilterFormat()->write(directory_ptr_, segment_ptr_->id_bloom_filter_ptr_);

    } catch (Exception& e) {
        std::string err_msg = "Failed to write vectors. " + std::string(e.what());
        ENGINE_LOG_ERROR << err_msg;
        return Status(e.code(), err_msg);
    }
    return Status::OK();
}

Status
SegmentWriter::WriteDeletedDocs(const DeletedDocsPtr& deleted_docs) {
    codec::DefaultCodec default_codec;
    try {
        directory_ptr_->Create();
        default_codec.GetDeletedDocsFormat()->write(directory_ptr_, deleted_docs);
    } catch (Exception& e) {
        std::string err_msg = "Failed to write deleted docs. " + std::string(e.what());
        ENGINE_LOG_ERROR << err_msg;
        return Status(e.code(), err_msg);
    }
    return Status::OK();
}

Status
SegmentWriter::WriteBloomFilter(const IdBloomFilterPtr& id_bloom_filter_ptr) {
    codec::DefaultCodec default_codec;
    try {
        directory_ptr_->Create();
        default_codec.GetIdBloomFilterFormat()->write(directory_ptr_, id_bloom_filter_ptr);
    } catch (Exception& e) {
        std::string err_msg = "Failed to write bloom filter. " + std::string(e.what());
        ENGINE_LOG_ERROR << err_msg;
        return Status(e.code(), err_msg);
    }
    return Status::OK();
}

Status
SegmentWriter::Cache() {
    // TODO(zhiru)
    return Status::OK();
}

Status
SegmentWriter::GetSegment(SegmentPtr& segment_ptr) {
    segment_ptr = segment_ptr_;
}

Status
SegmentWriter::Merge(const std::string& dir_to_merge, int vector_type_size) {
    SegmentReader segment_reader_to_merge(dir_to_merge);
    bool in_cache;
    auto status = segment_reader_to_merge.LoadCache(in_cache);
    if (!in_cache) {
        status = segment_reader_to_merge.Load();
        if (!status.ok()) {
            std::string msg = "Failed to load segment from " + dir_to_merge;
            ENGINE_LOG_ERROR << msg;
            return Status(DB_ERROR, msg);
        }
    }
    SegmentPtr segment_to_merge;
    segment_reader_to_merge.GetSegment(segment_to_merge);
    segment_ptr_->vectors_ptr_->Clear();
    auto offsets_to_delete = segment_ptr_->deleted_docs_ptr_->GetDeletedDocs();
    IdBloomFilterPtr id_bloom_filter_ptr;
    for (auto& kv : segment_to_merge->vectors_ptr_->vectors_map) {
        auto& uids = kv.second->GetUids();
        for (size_t i = 0; i < uids.size(); ++i) {
            auto found = std::find(offsets_to_delete.begin(), offsets_to_delete.end(), uids[i]);
            if (found != offsets_to_delete.end()) {
                kv.second->Erase(i, vector_type_size);
            }
        }
        AddVectors(kv.first, kv.second->GetData(), uids);
    }

    return Status::OK();
}

size_t
SegmentWriter::Size() {
    return segment_ptr_->vectors_ptr_->Size() + segment_ptr_->id_bloom_filter_ptr_->Size();
}

size_t
SegmentWriter::VectorCount() {
    size_t count = 0;
    for (auto& kv : segment_ptr_->vectors_ptr_->vectors_map) {
        count += kv.second->GetCount();
    }
    return count;
}

}  // namespace segment
}  // namespace milvus
