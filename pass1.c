#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct symbol {
    char label[20];
    int address;
    int index;
} symtab[50];

int symcount = 0, lc = 0;

char *optab[][3] = {
    {"STOP", "IS", "00"}, {"ADD", "IS", "01"}, {"SUB", "IS", "02"},
    {"MOVER", "IS", "04"}, {"MOVEM", "IS", "05"},
    {"START", "AD", "01"}, {"END", "AD", "02"},
    {"DS", "DL", "01"}, {"DC", "DL", "02"}
};

int getOpcodeIndex(char *mnemonic) {
    for (int i = 0; i < 9; i++) {
        if (strcmp(optab[i][0], mnemonic) == 0)
            return i;
    }
    return -1;
}

int getRegisterCode(char *reg) {
    if (strcmp(reg, "AREG") == 0) return 1;
    if (strcmp(reg, "BREG") == 0) return 2;
    if (strcmp(reg, "CREG") == 0) return 3;
    if (strcmp(reg, "DREG") == 0) return 4;
    return 0;
}

int search_symtab(char *label) {
    for (int i = 0; i < symcount; i++) {
        if (strcmp(symtab[i].label, label) == 0)
            return i;
    }
    return -1;
}

void add_symbol(char *label, int address) {
    int idx = search_symtab(label);
    if (idx != -1) {
        if (symtab[idx].address == -1)
            symtab[idx].address = address;
        return;
    }
    strcpy(symtab[symcount].label, label);
    symtab[symcount].address = address;
    symtab[symcount].index = symcount + 1;
    symcount++;
}

int main() {
    FILE *fp = fopen("input.asm", "r");
    FILE *ic = fopen("intermediate.txt", "w");
    FILE *st = fopen("symtab.txt", "w");
    char line[100], label[20], opcode[20], op1[20], op2[20];

    while (fgets(line, sizeof(line), fp)) {
        strcpy(label, "-"); strcpy(opcode, "-"); strcpy(op1, "-"); strcpy(op2, "-");
        int count = sscanf(line, "%s %s %s %s", label, opcode, op1, op2);

        /* shift tokens if no label */
        if (count == 3 && getOpcodeIndex(label) != -1) {
            strcpy(op2, op1);
            strcpy(op1, opcode);
            strcpy(opcode, label);
            strcpy(label, "-");
        } else if (count == 2 && getOpcodeIndex(label) != -1) {
            strcpy(op1, opcode);
            strcpy(opcode, label);
            strcpy(label, "-");
        }

        int opIndex = getOpcodeIndex(opcode);
        if (opIndex == -1) continue;

        if (strcmp(optab[opIndex][0], "START") == 0) {
            lc = atoi(op1);
            fprintf(ic, "(AD,01) (C,%d)\n", lc);
            continue;
        }

        if (strcmp(optab[opIndex][0], "END") == 0) {
            fprintf(ic, "(AD,02)\n)");
            continue;
        }

        if (strcmp(label, "-") != 0)
            add_symbol(label, lc);

        if (strcmp(optab[opIndex][0], "DS") == 0) {
            fprintf(ic, "(DL,01) (C,%s)\n", op1);
            lc += atoi(op1);
            continue;
        }

        if (strcmp(optab[opIndex][0], "DC") == 0) {
            fprintf(ic, "(DL,02) (C,%s)\n", op1);
            lc++;
            continue;
        }

        if (strcmp(optab[opIndex][1], "IS") == 0) {
            int reg = getRegisterCode(op1);
            int symIdx = search_symtab(op2);
            if (symIdx == -1) {
                add_symbol(op2, -1);
                symIdx = symcount - 1;
            }
            fprintf(ic, "(%s,%s) (R,%d) (S,%d)\n",
                    optab[opIndex][1], optab[opIndex][2], reg, symtab[symIdx].index);
            lc++;
        }
    }

    for (int i = 0; i < symcount; i++) {
        fprintf(st, "%d\t%s\t%d\n", symtab[i].index, symtab[i].label, symtab[i].address);
    }

    fclose(fp); fclose(ic); fclose(st);
    printf("Pass-I complete.\n");
    return 0;
}
