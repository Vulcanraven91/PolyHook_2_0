//
// Created by steve on 3/22/17.
//

#ifndef POLYHOOK_2_0_CAPSTONEDISASSEMBLER_HPP
#define POLYHOOK_2_0_CAPSTONEDISASSEMBLER_HPP

#include "headers/ADisassembler.hpp"
#include "headers/Maybe.hpp"

#include <capstone/include/capstone/capstone.h>

#include <inttypes.h>
#include <string.h>
#include <memory>   //memset on linux
#include <iostream> //for debug printing

namespace PLH {

class CapstoneDisassembler : public ADisassembler
{
public:
    CapstoneDisassembler(PLH::Mode mode) : ADisassembler(mode) {
        cs_mode capmode = (mode == PLH::Mode::x64 ? CS_MODE_64 : CS_MODE_32);
        if (cs_open(CS_ARCH_X86, capmode, &m_capHandle) != CS_ERR_OK)
            m_errorCallback.invoke(PLH::Message("Capstone Init Failed"));

        cs_option(m_capHandle, CS_OPT_DETAIL, CS_OPT_ON);
    }

    virtual std::vector<std::shared_ptr<PLH::Instruction>>
    disassemble(uint64_t firstInstruction, uint64_t start, uint64_t end) override;

    virtual void writeEncoding(const PLH::Instruction& instruction) const override;

    virtual bool isConditionalJump(const PLH::Instruction& instruction) const override;

private:
    /**When new instructions are inserted we have to re-itterate the list and add
     * a child to an instruction if the new instruction points to it**/
    void modifyParentIndices(std::vector<std::shared_ptr<PLH::Instruction>>& haystack,
                             std::shared_ptr<PLH::Instruction>& newInstruction) const {

        for (uint8_t i = 0; i < haystack.size(); i++) {
            //Check for things that the new instruction may point too
            if (haystack.at(i)->getAddress() == newInstruction->getDestination())
                haystack.at(i)->addChild(newInstruction);

            //Check for things that may point to the new instruction
            if (haystack.at(i)->getDestination() == newInstruction->getAddress())
                newInstruction->addChild(haystack.at(i));
        }
    }

    x86_reg getIpReg() const {
        if (m_mode == PLH::Mode::x64)
            return X86_REG_RIP;
        else //if(m_Mode == PLH::ADisassembler::Mode::x86)
            return X86_REG_EIP;
    }

    bool hasGroup(const cs_insn* inst, const x86_insn_group grp) const {
        uint8_t GrpSize = inst->detail->groups_count;

        for (int i = 0; i < GrpSize; i++) {
            if (inst->detail->groups[i] == grp)
                return true;
        }
        return false;
    }

    void setDisplacementFields(Instruction* inst, const cs_insn* capInst) const;

    /* For immediate types capstone gives us only the final destination, but *we* care about the base + displacement values.
     * Immediates can be encoded either as some value relative to a register, or a straight up hardcoded address, we need
     * to figure out which so that we can do code relocation later. To deconstruct the info we need first we read the imm value byte
     * by byte out of the instruction, if that value is less than what capstone told us is the destination then we know that it is relative and we have to add the base.
     * Otherwise if our retreived displacement is equal to the given destination then it is a true absolute jmp/call (only possible in x64),
     * if it's greater then something broke.*/
    void copyAndSExtendDisp(PLH::Instruction* inst,
                            const uint8_t offset,
                            const uint8_t size,
                            const int64_t immDestination) const;

    csh m_capHandle;
};
}
#endif //POLYHOOK_2_0_CAPSTONEDISASSEMBLER_HPP
