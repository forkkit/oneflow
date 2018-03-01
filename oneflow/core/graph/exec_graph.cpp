#include "oneflow/core/graph/exec_graph.h"

namespace oneflow {

void ExecNode::BindBnInOpAndRegst(const std::string& bn_in_op,
                                  std::weak_ptr<RegstDesc> regst) {
  CHECK(bn_in_op2regst_.emplace(bn_in_op, regst).second);
}

void ExecNode::ToProto(bool is_forward, DeviceType device_type,
                       const ParallelContext* parallel_ctx,
                       ExecNodeProto* ret) const {
  op_->GenKernelConf(GetBlobDesc4BnInOpFunc(), is_forward, device_type,
                     parallel_ctx, op_ctx_.get(), ret->mutable_kernel_conf());
  for (const auto& bn_regst : bn_in_op2regst_) {
    const std::string& bn_in_op = bn_regst.first;
    auto regst = bn_regst.second.lock();
    if (!regst) { continue; }
    PbMapPair<std::string, int64_t> pair{bn_in_op, regst->regst_desc_id()};
    CHECK(ret->mutable_bn_in_op2regst_desc_id()->insert(pair).second);
  }
}

void ExecNode::InferBlobDescs(const ParallelContext* parallel_ctx,
                              DeviceType device_type) {
  op_->InferBlobDescs(GetBlobDesc4BnInOpFunc(), parallel_ctx, device_type,
                      &op_ctx_);
}

std::function<BlobDesc*(const std::string&)> ExecNode::GetBlobDesc4BnInOpFunc()
    const {
  return [this](const std::string& bn_in_op) -> BlobDesc* {
    auto it = bn_in_op2regst_.find(bn_in_op);
    if (it == bn_in_op2regst_.end()) { return nullptr; }
    std::shared_ptr<RegstDesc> regst = it->second.lock();
    if (!regst) { return nullptr; }
    const std::string& lbn = this->op()->Lbn4BnInOp(bn_in_op);
    return regst->MutBlobDesc(lbn);
  };
}

void ExecGraph::ToExecSequence(bool is_forward, DeviceType device_type,
                               const ParallelContext* parallel_ctx,
                               ExecSequence* ret) const {
  TopoForEachNode([&](ExecNode* node) {
    node->ToProto(is_forward, device_type, parallel_ctx, ret->add_exec_node());
  });
}

}  // namespace oneflow
