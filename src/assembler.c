#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "asm.h"

#define     USAGE() { printf("USAGE: ./assemble [program.asm]\n"); exit(0);}
#define     CHECK(argc) { if (argc < 2) { USAGE(); } }

/* assembler for the Hack ISA. */

/* todo:
 * convenient addressing with m[xxx] or m[variable]
 * 
 *
 */



FILE *output;

uint16_t g_addr;
struct line_data *head;
struct line_data *cur;
struct symbol_data *symhead;
struct symbol_data *symcur;
uint32_t lnum;
uint32_t sym_count;


FILE *open_file(char *file) {
    FILE *fp = fopen(file, "r+");
    assert(fp != -1);
    return fp;
}

uint8_t *read_file(char *file) {
    FILE *fp = fopen(file, "r+");
            
    fseek(fp, 0, SEEK_END);
    uint64_t size = ftell(fp);
    uint8_t *buf = malloc(size + 1);
    fseek(fp, 0, SEEK_SET);
    fread(buf, 1, size, fp);
    return buf;

}

void INIT_LIST_HEAD(void) {
    symhead = malloc(sizeof(struct symbol_data) + 1);
    symhead->symbol = "DEBUG";
    symhead->list.next = NULL;
    symhead->list.prev = NULL;
    symcur = symhead;

    head = malloc(sizeof(struct line_data) + 1);
    head->data = "DEBUG";
    head->list.next = NULL;
    head->list.prev = NULL;
    cur = head;
    return;
}

struct line_data *line_add(struct line_data *new) {
    new->num = lnum;
    lnum++;
    struct line_data *tmp = cur;
    new->list.prev = tmp;
    new->list.next = NULL;
    tmp->list.next = new;
    cur = new;
    return cur;
}

struct symbol_data *sym_add(struct symbol_data *new) {
    struct symbol_data *tmp = symcur;
    new->list.prev = tmp;
    new->list.next = NULL;
    tmp->list.next = new;
    symcur = new;
    return symcur;
}

struct line_data *readline(uint8_t *buf) {
    uint8_t i = 0;
    if(buf[i] == NULL) { return NULL; }
    struct line_data *line = malloc(sizeof(struct line_data) + 1);
    line->data = malloc(0x50);
    line->opcode = malloc(18);
    while(buf[i] != 0xA) {
        line->data[i] = buf[i];
        i++;
    }
    line->size = i;

    struct line_data *ret = line_add(line);

    return ret;

}

void populate_list(uint8_t *buf) {
    size_t offset = 0;
    for(int i = 0; cur = readline(buf + offset + i); i++) {
        offset += cur->size;
    }
    return;
}
 

void show_list(struct line_data *ptr) {
    while(ptr) {
        printf("line %d: %s\n", ptr->num, ptr->data);
        ptr = ptr->list.next;
    }
    return;
}

void show_syms(struct symbol_data *ptr) { 
    while(ptr->list.next) { 
        printf("Symbol:%s\nAddress:%d\n", ptr->symbol, ptr->addr);
        ptr = ptr->list.next;
    }
}

struct c_inst *tokenize_c(uint8_t *ptr) {
    struct c_inst *x = malloc(sizeof(struct c_inst) + 1);
    // dest and jump are mutually exclusive

    uint8_t **val1, **val2;
    uint8_t *TOKEN = { 0 };

    if(strstr(ptr, ";")) { // this breaks if for example a ';' is in comment on same line
        x->type = JMP;
        val1 = &x->comp;
        val2 = &x->jump;
        TOKEN = "\r;";
    }
    else { 
        x->type = EQ;
        val1 = &x->dest;
        val2 = &x->comp;
        x->jump = "000";
        TOKEN = "\r=";
    }

    uint8_t *tok = strtok(ptr, TOKEN);
    *val1 = tok;
    uint8_t *tok2 = strtok(NULL, TOKEN);
    strtok(tok2, "/");
    strtok(tok2, " ");
    *val2 = tok2;
//    printf("val1: %s\n", *val1);
//    printf("val2: %s\n", *val2);
    return x;
}

uint8_t check_opcode(uint8_t type, struct c_inst *instruction) {
    uint8_t k = 0;
    uint8_t **mnem;
    uint8_t *val;
    switch(type) { 
        case JMP:
            k = 7;
            mnem = jmp_mnemonic;
            val = instruction->jump;
            break;
        case DEST:
            k = 8;
            mnem = dest_mnemonic;
            val = instruction->dest;
            break;
    
        case COMP:
            k = 28;
            mnem = c_mnemonic;
            val = instruction->comp;
            break;
    }
    
    for(int i = 0; i < k; i++) { 
        if(!strcmp(val, mnem[i])) { 
            return i;
        }
    
    }

}

void handle_c(struct line_data *ptr) { 
    struct c_inst *instruction = tokenize_c(ptr->data);
    uint8_t cidx = check_opcode(COMP, instruction);
    sprintf(ptr->opcode, "111");
    if(instruction->type == JMP) {
        strcat(ptr->opcode, c_opcode[cidx]);
        strcat(ptr->opcode, "000");
        uint8_t jidx = check_opcode(JMP, instruction);
        strcat(ptr->opcode, jmp_opcode[jidx]);
        return;
    }
    else { 
        strcat(ptr->opcode, c_opcode[cidx]);
        uint8_t didx = check_opcode(DEST, instruction);
        strcat(ptr->opcode, dest_opcode[didx]);
        strcat(ptr->opcode, "000\n");
        return;
    
    }
}

void handle_all_inst(struct line_data *start) {
    struct line_data *tmp = start;
    while(tmp) {
        if (tmp->state == DONE || tmp->data[0] == NULL) { goto next; }
        handle_c(tmp);
next:
        tmp = tmp->list.next;
    }

}

void flush_file(struct line_data *ptr, char *arg) {
    uint8_t *tok = strtok(arg, ".");
    uint8_t *fout = malloc(0x24);
    sprintf(fout, "%s.hack", tok);
    printf("%s\n", fout);
    output = fopen(fout, "w+");
    while(ptr) {
        fprintf(output, ptr->opcode);
        ptr = ptr->list.next;
    }
}

struct symbol_data *does_sym_exist(uint8_t *token) {
    struct symbol_data *s = symhead;
    while(s->list.next) {
        if(!strcmp(token, s->symbol)) {
            return (struct symbol_data *) s;
        }
        s = s->list.next;
    }
    return NULL;
}

void first_pass(struct line_data *ptr) {
    uint8_t *tok;
    uint8_t *tok2;
    struct symbol_data *s;
    struct symbol_data *x;
    while(ptr) {
	/* Problem: Label address actually have to be calculated(depending on position), and can be referenced before actually being declared. To deal with this, allow the routine that handles symbols to do as it pleases, while the other routine will detect if such an error has occured and fix it. */
        /* also, this is very hacky and bad. */
	tok, tok2 = 0;
        if(strstr(ptr->data, ")")) { 
            tok = strtok(ptr->data, ")");
            tok2 = strtok(ptr->data, "\r");
            ++tok2;
            /* Symbol has already been found. Let's see if it "should" be. */
            if(s = does_sym_exist(tok2)) { 
                if(s->type == VAL) { 
                    /* Discrepancy. Label takes precedence. */
                    s->addr = ptr->num - (1+sym_count);
                    /* Update type so this doesn't erroneously happen again. */
                    s->type = LABEL;
                    /* Fix address errors(g_addr was mistakenly increased).*/
                    s = s->list.next;
                    while(s->list.next) {
                        if (s->type == VAL)
                            s->addr = s->addr - 1;
                        s = s->list.next;
                    }
                }
            }
            /* Symbol has not been found. Let's add it to the symbol table. */
            else { 
                symcur->symbol = tok2;
                symcur->addr = ptr->num - (1+sym_count);
                symcur->type = LABEL;
                struct symbol_data *new = malloc(sizeof(struct symbol_data) + 1);
                sym_add(new);
            }
            ++sym_count;
            ptr->state = DONE;
        }
        else if(strstr(ptr->data, "@") && ptr->data[1] > 0x3A) { 
            tok = strtok(ptr->data, "@");
            tok2 = strtok(tok, "\r");
            if(x = does_sym_exist(tok2) == NULL) { 
                symcur->symbol = tok2;
                symcur->addr = g_addr;
                symcur->type = VAL;
                struct symbol_data *new = malloc(sizeof(struct symbol_data) + 1);
                sym_add(new);
                ++g_addr;
            }
        }
    ptr = ptr->list.next;

    }    
}

void second_pass(struct line_data *ptr) { 
	while(ptr) {
		if(strstr(ptr->data, "@")) {
			uint8_t *tok = strtok(ptr->data, "@");
			uint8_t *tok2 = strtok(tok, "\r");
			if(tok2[0] <= 0x39) { 
				uint16_t n = strtol(tok2, NULL, 10);
				dec2bin((uint16_t)n, ptr->opcode);
				ptr->state = DONE;
				goto cont;
			}
			struct symbol_data *s = does_sym_exist(tok2);
			dec2bin((uint16_t)(s->addr), ptr->opcode);
			ptr->state = DONE;
		}
cont:
	ptr = ptr->list.next;
	}

}

void main(int argc, char *argv[]) {
    sym_count = 0;
    CHECK(argc);
    if(!strstr(argv[1], ".asm"))
        USAGE();
    lnum = 1;
    g_addr = 16;
    uint8_t *buf = read_file(argv[1]);
    INIT_LIST_HEAD();
    INIT_SYMS();

    populate_list(buf);
    struct symbol_data *s = symhead;
    struct line_data *ptr = head;
    struct line_data *strt = ptr->list.next;
    struct line_data *nxt = strt->list.next;
    first_pass(strt);
    second_pass(strt);
    handle_all_inst(strt);
    flush_file(strt, argv[1]);
}
