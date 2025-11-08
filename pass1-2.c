#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_MNT 20
#define MAX_MDT 200
#define MAX_ARGS 10

/* MNT, MDT structures */
struct MNT {
    char name[20];
    int mdt_index;
} mnt[MAX_MNT];

struct MDT {
    char line[200];
} mdt[MAX_MDT];

int mntc = 0, mdtc = 0;

/* Helper: trim newline and trailing spaces */
void trim_newline(char *s) {
    int i = strlen(s) - 1;
    while (i >= 0 && (s[i] == '\n' || s[i] == '\r')) { s[i] = '\0'; i--; }
}

/* PASS-1: build MNT and MDT and write intermediate */
void pass1() {
    FILE *fin = fopen("MACIN.txt", "r");
    FILE *fint = fopen("intermediate.txt", "w");
    if (!fin) { printf("Cannot open MACIN.txt\n"); exit(1); }
    if (!fint) { printf("Cannot open intermediate.txt for writing\n"); exit(1); }

    char line[200];
    int in_macro = 0;
    char macro_name[50] = "";

    while (fgets(line, sizeof(line), fin)) {
        trim_newline(line);
        if (strlen(line) == 0) continue;

        /* If line is "MACRO" start macro capture */
        if (strcmp(line, "MACRO") == 0) {
            in_macro = 1;
            macro_name[0] = '\0';
            continue;
        }

        if (in_macro) {
            /* If MEND, store and stop macro capture */
            if (strcmp(line, "MEND") == 0) {
                strcpy(mdt[mdtc++].line, "MEND");
                in_macro = 0;
                continue;
            }
            /* First line after MACRO is prototype: macro_name formal args */
            if (macro_name[0] == '\0') {
                /* Extract macro name (token before space) */
                char proto[200];
                strcpy(proto, line);
                char *p = strtok(proto, " \t");
                if (p) {
                    strcpy(macro_name, p);
                    strcpy(mnt[mntc].name, macro_name);
                    mnt[mntc].mdt_index = mdtc;
                    mntc++;
                }
            }
            /* Store this line in MDT */
            strcpy(mdt[mdtc++].line, line);
        } else {
            /* Not in macro -> write to intermediate */
            fprintf(fint, "%s\n", line);
        }
    }

    fclose(fin);
    fclose(fint);

    /* Also write mnt.txt and mdt.txt for inspection */
    FILE *fmnt = fopen("mnt.txt", "w");
    FILE *fmdt = fopen("mdt.txt", "w");
    if (fmnt) {
        for (int i = 0; i < mntc; i++)
            fprintf(fmnt, "%s %d\n", mnt[i].name, mnt[i].mdt_index);
        fclose(fmnt);
    }
    if (fmdt) {
        for (int i = 0; i < mdtc; i++)
            fprintf(fmdt, "%s\n", mdt[i].line);
        fclose(fmdt);
    }
}

/* Find MNT entry by name, return index or -1 */
int find_mnt(char *name) {
    for (int i = 0; i < mntc; i++) {
        if (strcmp(mnt[i].name, name) == 0) return i;
    }
    return -1;
}

/* Trim leading/trailing spaces */
void trim_spaces(char *s) {
    // left trim
    while (*s == ' ' || *s == '\t') memmove(s, s+1, strlen(s));
    // right trim
    int n = strlen(s);
    while (n>0 && (s[n-1]==' '||s[n-1]=='\t' || s[n-1]=='\r' || s[n-1]=='\n')) s[--n]='\0';
}

/* PASS-2: expand macros using MNT/MDT in memory */
void pass2() {
    FILE *fint = fopen("intermediate.txt", "r");
    FILE *fout = fopen("output.txt", "w");
    if (!fint) { printf("Cannot open intermediate.txt\n"); exit(1); }
    if (!fout) { printf("Cannot open output.txt for writing\n"); exit(1); }

    char line[200];
    while (fgets(line, sizeof(line), fint)) {
        trim_newline(line);
        if (strlen(line) == 0) continue;

        /* Extract first token to see if it's a macro name */
        char copy[200];
        strcpy(copy, line);
        char *first = strtok(copy, " \t");
        if (!first) continue;

        int mindex = find_mnt(first);
        if (mindex == -1) {
            /* Not a macro call: write as-is */
            fprintf(fout, "%s\n", line);
        } else {
            /* Macro call: parse actual arguments (if any) */
            char *rest = strchr(line, ' ');
            char actuals[MAX_ARGS][100];
            int actual_count = 0;
            if (rest) {
                char argsstr[200];
                strcpy(argsstr, rest + 1);
                trim_spaces(argsstr);
                /* split by comma */
                char *t = strtok(argsstr, ",");
                while (t && actual_count < MAX_ARGS) {
                    trim_spaces(t);
                    strcpy(actuals[actual_count++], t);
                    t = strtok(NULL, ",");
                }
            }

            /* Read prototype from MDT to get formal parameters */
            int start = mnt[mindex].mdt_index;
            char prototype[200];
            strcpy(prototype, mdt[start].line);
            /* prototype looks like: INCR &ARG1,&ARG2  (maybe with space after name) */
            char proto_copy[200];
            strcpy(proto_copy, prototype);
            char *pname = strtok(proto_copy, " \t");
            char *formals_part = strtok(NULL, "\n");
            char formals[MAX_ARGS][100];
            int formal_count = 0;
            if (formals_part) {
                /* split by comma */
                char *tk = strtok(formals_part, ",");
                while (tk && formal_count < MAX_ARGS) {
                    trim_spaces(tk);
                    strcpy(formals[formal_count++], tk);
                    tk = strtok(NULL, ",");
                }
            }

            /* For each line in MDT following prototype until MEND, replace formal with actuals */
            for (int j = start + 1; j < mdtc && strcmp(mdt[j].line, "MEND") != 0; j++) {
                char outline[300];
                strcpy(outline, mdt[j].line);

                /* Replace each formal param (like &ARG1) by corresponding actual */
                for (int k = 0; k < formal_count; k++) {
                    char *pos = strstr(outline, formals[k]);
                    while (pos) {
                        char before[300] = "", after[300] = "";
                        int prefix_len = pos - outline;
                        strncpy(before, outline, prefix_len);
                        before[prefix_len] = '\0';
                        strcpy(after, pos + strlen(formals[k]));
                        /* Build new outline */
                        char temp[300];
                        if (k < actual_count)
                            snprintf(temp, sizeof(temp), "%s%s%s", before, actuals[k], after);
                        else
                            snprintf(temp, sizeof(temp), "%s%s%s", before, "", after);
                        strcpy(outline, temp);
                        pos = strstr(outline, formals[k]); /* look for next occurrence */
                    }
                }
                /* Write expanded line to output */
                trim_spaces(outline);
                fprintf(fout, "%s\n", outline);
            }
        }
    }

    fclose(fint);
    fclose(fout);
}

int main() {
    /* Run pass1 then pass2 */
    //pass1();
    pass2();
    printf("Macro expansion complete. See output.txt\n");
    return 0;
}
