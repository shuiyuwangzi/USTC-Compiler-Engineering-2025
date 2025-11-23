#include "passes/DeadCode.hpp"
#include "Instruction.hpp"
#include "logging.hpp"
#include <memory>
#include <vector>


// 处理流程：两趟处理，mark 标记有用变量，sweep 删除无用指令
void DeadCode::run() {
    bool changed{};
    func_info->run();
    do {
        changed = false;
        for (auto &F : m_->get_functions()) {
            auto func = &F;
            changed |= clear_basic_blocks(func);
            mark(func);
            changed |= sweep(func);
        }
    } while (changed);
    LOG_INFO << "dead code pass erased " << ins_count << " instructions";
}

bool DeadCode::clear_basic_blocks(Function *func) {
    bool changed = 0;
    std::vector<BasicBlock *> to_erase;
    for (auto &bb1 : func->get_basic_blocks()) {
        auto bb = &bb1;
        if(bb->get_pre_basic_blocks().empty() && bb != func->get_entry_block()) {
            to_erase.push_back(bb);
            changed = 1;
        }
    }
    for (auto &bb : to_erase) {
        bb->erase_from_parent();
        delete bb;
    }
    return changed;
}

void DeadCode::mark(Function *func) {
    // TODO
    marked.clear();
    work_list.clear();

    for (auto &bb : func->get_basic_blocks()) {
        for (auto &instr : bb.get_instructions()) {
            if (is_critical(&instr)) {
                // 标记为活指令（true 表示活的）
                marked[&instr] = true;
                work_list.push_back(&instr);
            }
        }
    }
    while (!work_list.empty()) {
        auto curr = work_list.front();
        work_list.pop_front();
        const auto& operands = curr->get_operands();
        for (auto op_iter = operands.cbegin(); op_iter != operands.cend(); ++op_iter) {
            const Instruction* const_op = dynamic_cast<const Instruction*>(*op_iter);
            if (!const_op) {
                continue; 
            }
            Instruction* target_instr = const_cast<Instruction*>(const_op);
            if (marked.find(target_instr) == marked.end() || !marked[target_instr]) {
                marked[target_instr] = true;
                work_list.push_back(target_instr);
            }
        }
    }
}

void DeadCode::mark(Instruction *ins) {
    // TODO
        if (marked.find(ins) != marked.end() && marked[ins] == true) {
        return;
    }
    marked[ins] = true;
    for (auto op : ins->get_operands()) {
        if (auto op_i = dynamic_cast<Instruction *>(op)) {
            mark(op_i);
        }
    }
}

bool DeadCode::sweep(Function *func) {
    // TODO: 删除无用指令
    // 提示：
    std::unordered_set<Instruction *> wait_del{};
    // 1. 遍历函数的基本块，删除所有标记为true的指令

    // 2. 删除指令后，可能会导致其他指令的操作数变为无用，因此需要再次遍历函数的基本块
    // 3. 如果删除了指令，返回true，否则返回false
    // 4. 注意：删除指令时，需要先删除操作数的引用，然后再删除指令本身
    // 5. 删除指令时，需要注意指令的顺序，不能删除正在遍历的指令

    // 1. 收集所有未被标记的指令
    for (auto&& basic_block : func->get_basic_blocks()) {
        for (auto&& inst : basic_block.get_instructions()) {
            auto inst_addr = std::addressof(inst); // 显式获取地址，替代直接&
            // 原逻辑等价表述：未标记 或 标记为无效
            const auto mark_iter = marked.find(inst_addr);
            if (mark_iter == marked.end() || !mark_iter->second) {
                wait_del.insert(inst_addr);
            }
        }
    }
    if (wait_del.empty()) {
        return false;
    }
    // 2. 执行删除
        for (auto i : wait_del) {
        i->get_parent()->erase_instr(i);
        ins_count++; 
    }
    
    return not wait_del.empty(); // changed
}

bool DeadCode::is_critical(Instruction *ins) {
    // TODO: 判断指令是否是无用指令
    // 提示：
    // 1. 如果是函数调用，且函数是纯函数，则无用
    // 2. 如果是无用的分支指令，则无用
    // 3. 如果是无用的返回指令，则无用
    // 4. 如果是无用的存储指令，则无用
    auto curr_inst = ins;
    if (curr_inst->is_call()) {
        // 安全转换为调用指令（增加空指针防御，兼容异常场景）
        auto call_instr = dynamic_cast<CallInst*>(curr_inst);
        if (!call_instr) {
            return true;
        }
        auto callee_operand = call_instr->get_operand(0);
        auto callee_func = dynamic_cast<Function*>(callee_operand);

        return (callee_func == nullptr) || !func_info->is_pure_function(callee_func);
    }
    if (curr_inst->is_br() || curr_inst->is_ret() || curr_inst->is_store()) {
        return true;
    }
    return false;
}


void DeadCode::sweep_globally() {
    std::vector<Function *> unused_funcs;
    std::vector<GlobalVariable *> unused_globals;
    for (auto &f_r : m_->get_functions()) {
        if (f_r.get_use_list().size() == 0 and f_r.get_name() != "main")
            unused_funcs.push_back(&f_r);
    }
    for (auto &glob_var_r : m_->get_global_variable()) {
        if (glob_var_r.get_use_list().size() == 0)
            unused_globals.push_back(&glob_var_r);
    }
    // changed |= unused_funcs.size() or unused_globals.size();
    for (auto func : unused_funcs)
        m_->get_functions().erase(func);
    for (auto glob : unused_globals)
        m_->get_global_variable().erase(glob);
}