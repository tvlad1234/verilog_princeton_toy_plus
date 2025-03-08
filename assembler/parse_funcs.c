#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "assembler_types.h"

int getReg(char name[], int comma)
{
    int reg = -1;

    if (comma)
    {
        if (name[strlen(name) - 1] != ',')
            return REG_EXPECT_COMMA;
        name[strlen(name) - 1] = 0;
    }

    for (int i = 0; i < 16; i++)
        if (!strcmp(name, reg_names[i]))
            reg = i;

    if (reg == -1)
        return REG_EXPECT_NAME;

    return reg;
}

void assert_reg(int reg, int linenum, char *filename)
{
    if (reg == REG_EXPECT_NAME)
    {
        printf("\n\rIn file %s, at line %d: expected register name \n\r", filename, linenum);
        exit(-1);
    }
    else if (reg == REG_EXPECT_COMMA)
    {
        printf("\n\rIn file %s, at line %d: expected comma after register name\n\r", filename, linenum);
        exit(-1);
    }
    else if (reg == REG_INVALID_REG)
    {
        printf("\n\rIn file %s, at line %d: invalid register R%d\n\r", filename, linenum, reg);
        exit(-1);
    }
}

int getValue(char token_ptr[])
{
    int addr;
    if (token_ptr[0] == '0' && token_ptr[1] == 'x')
    {
        sscanf(token_ptr + 2, "%x", &addr);
        return addr;
    }

    if (token_ptr[0] == '\'')
        return token_ptr[1];

    else
    {
        int l = strlen(token_ptr);
        for (int i = 0; i < l; i++)
            if (token_ptr[i] > '9' || token_ptr[i] < '0')
                return -1;
    }
    return atoi(token_ptr);
}

void getImm(char str1[], char str2[], int *imm_p, int *type_p)
{
    int addr, imm_type;

    if (!strcmp(str1, "OFFSET"))
        imm_type = IMM_OFFSET;
    else if (!strcmp(str1, "SEGMENT"))
        imm_type = IMM_SEGMENT;
    else
        imm_type = IMM_DIRECT;

    if (imm_type == IMM_DIRECT)
        addr = getValue(str1);
    else
        addr = getValue(str2);

    *imm_p = addr;
    *type_p = imm_type;
}

extern label_t labels[];
extern mem_loc_t memory[];
extern int num_labels, num_mem_loc, current_addr;

void parseLine(char *lineptr, int linenum, char *filename)
{
    char line[40][40];

    if (lineptr[0] == '#')
        return;

    // Break it into tokens
    char *p = strtok(lineptr, SEP_SPACE);
    int token_num = 0;
    while (p)
    {

        if (p[strlen(p) - 1] == '\n')
            p[strlen(p) - 1] = 0;

        strcpy(line[token_num], p);
        token_num++;
        p = strtok(NULL, SEP_SPACE);
    }

    // Process tokens
    int current_token = 0;
    char *token_ptr = line[current_token];

    if (token_ptr[0] == 0)
        return;

    // check if first token is label:
    if (current_token == 0 && token_ptr[strlen(token_ptr) - 1] == ':')
    {
        token_ptr[strlen(token_ptr) - 1] = 0;

        strcpy(labels[num_labels].name, token_ptr);
        labels[num_labels].addr = current_addr;

        printf("Label \"%s\" at addr %d\n\r", labels[num_labels].name, labels[num_labels].addr);

        num_labels++;
        token_ptr = line[++current_token];
    }

    if (current_token < token_num)
    {
        int line_type = 0;
        // search for opcode:
        uint8_t opcode;
        for (int i = 0; i < OP_NUMBER && !line_type; i++)
            if (!strcmp(token_ptr, opcode_strings[i]))
            {
                line_type = LINE_OPCODE;
                opcode = i;
            }

        // search for macro:
        uint8_t macro;
        for (int i = 0; i < MACRO_NUMBER && !line_type; i++)
            if (!strcmp(token_ptr, macro_strings[i]))
            {
                line_type = LINE_MACRO;
                macro = i;
            }

        // search for data declarations
        if (!strcmp(token_ptr, "DW"))
        {
            line_type = LINE_DATA;
            while (current_token < token_num - 1)
            {
                token_ptr = line[++current_token];
                memory[current_addr].addr = current_addr;
                memory[current_addr].type = MEM_TYPE_DATA;
                memory[current_addr].instr.filename = filename;
                memory[current_addr].instr.linenum = linenum;
                int data = getValue(token_ptr);
                memory[current_addr].data = data;
                current_addr++;
            }
        }

        if (!line_type)
        {
            printf("\n\rIn file %s, at line %d: unknown instruction \"%s\"\n\r", filename, linenum, token_ptr);
            exit(-1);
        }

        else if (line_type == LINE_OPCODE)
        {
            memory[current_addr].instr.opcode = opcode;
            memory[current_addr].instr.filename = filename;
            memory[current_addr].instr.linenum = linenum;
            memory[current_addr].type = MEM_TYPE_INST;

            token_ptr = line[++current_token];

            // instr type:
            uint8_t instr_format;

            if ((opcode > 0 && opcode < 7) || opcode == 0xa || opcode == 0xb)
                instr_format = INST_FORMAT_1;
            else if (opcode == 0)
                instr_format = INST_HALT;
            else if (opcode == 0xe)
                instr_format = INST_FORMAT_J;
            else
                instr_format = INST_FORMAT_2;

            memory[current_addr].instr.format = instr_format;

            // parameter parsing
            if (instr_format == INST_FORMAT_1)
            {
                int reg;

                // d
                reg = getReg(token_ptr, 1);
                assert_reg(reg, linenum, filename);
                memory[current_addr].instr.rd = reg;
                token_ptr = line[++current_token];

                // s
                if (opcode != 0xa && opcode != 0xb)
                {
                    reg = getReg(token_ptr, 1);
                    assert_reg(reg, linenum, filename);
                    memory[current_addr].instr.rs = reg;
                    token_ptr = line[++current_token];
                }
                else
                    memory[current_addr].instr.rs = 0;

                // t
                reg = getReg(token_ptr, 0);
                assert_reg(reg, linenum, filename);
                memory[current_addr].instr.rt = reg;
                token_ptr = line[++current_token];
            }

            else if (instr_format == INST_FORMAT_2)
            {
                // d
                int reg = getReg(token_ptr, 1);
                assert_reg(reg, linenum, filename);
                memory[current_addr].instr.rd = reg;
                token_ptr = line[++current_token];

                int imm_type, addr;

                getImm(token_ptr, line[++current_token], &addr, &imm_type);

                memory[current_addr].instr.imm_type = imm_type;
                if (addr == -1)
                {
                    memory[current_addr].instr.imm_labeled = 1;

                    if (imm_type != IMM_DIRECT)
                        token_ptr = line[current_token];

                    strcpy(memory[current_addr].instr.label_name, token_ptr);
                }
                else
                {
                    memory[current_addr].instr.imm_labeled = 0;
                    memory[current_addr].instr.addr = addr;
                }
            }

            else if (instr_format == INST_FORMAT_J)
            {
                // d
                int reg = getReg(token_ptr, 0);
                assert_reg(reg, linenum, filename);
                memory[current_addr].instr.rd = reg;
                token_ptr = line[++current_token];
            }

            // Next address
            memory[current_addr].addr = current_addr;
            current_addr++;
        }

        else if (line_type == LINE_MACRO)
        {
            token_ptr = line[++current_token];

            if (macro == MACRO_INC || macro == MACRO_DEC)
            {
                int rd = getReg(token_ptr, 0);
                assert_reg(rd, linenum, filename);

                memory[current_addr].type = MEM_TYPE_INST;
                memory[current_addr].addr = current_addr;
                macroIncDec(macro, rd, &memory[current_addr].instr);
                current_addr++;
            }

            else if (macro == MACRO_PUSH)
            {
                int rd = getReg(token_ptr, 0);
                assert_reg(rd, linenum, filename);

                // decrement stack pointer
                memory[current_addr].type = MEM_TYPE_INST;
                memory[current_addr].addr = current_addr;
                macroIncDec(1, 13, &memory[current_addr].instr);
                current_addr++;

                // store rd to stack
                memory[current_addr].type = MEM_TYPE_INST;
                memory[current_addr].addr = current_addr;
                format1inst(&memory[current_addr].instr, 0x0B, rd, 0, 13);
                current_addr++;
            }

            else if (macro == MACRO_POP)
            {
                int rd = getReg(token_ptr, 0);
                assert_reg(rd, linenum, filename);

                // load rd from stack
                memory[current_addr].type = MEM_TYPE_INST;
                memory[current_addr].addr = current_addr;
                format1inst(&memory[current_addr].instr, 0x0A, rd, 0, 13);
                current_addr++;

                // increment stack pointer
                memory[current_addr].type = MEM_TYPE_INST;
                memory[current_addr].addr = current_addr;
                macroIncDec(0, 13, &memory[current_addr].instr);
                current_addr++;
            }
        }
    }
}
