#include <stdint.h>
#include <stddef.h>
#include <boot/kernel.h>
#include <dogeio.h>
#include <string.h>
#include <system.h>
#include <bool.h>
#include <basicutil.h>
#include <start.h>

typedef struct __attribute__((packed)) {
    uint16_t offset_low;      
    uint16_t selector;        
    uint8_t  ist;             
    uint8_t  type_attributes; 
    uint16_t offset_middle;   
    uint32_t offset_high;     
    uint32_t reserved;        
} idt_entry_t;

extern uint64_t sys_exit(uint64_t status);

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idtr_t;

typedef struct __attribute__((packed)) {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

#define IDT_GATE_INT32 0x8E 

static idt_entry_t idt[256];
static idtr_t      idtr;

extern uint64_t isr_stub_table[32];
extern void idt_load(idtr_t *idtr_ptr);
bool is_kernel_dead = false;

static void uint64_to_dec_forward(uint64_t value, char* buf) {
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[32];
    int i = 0;
    while (value > 0) {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

static void uint64_to_hex_forward(uint64_t value, char* buf) {
    char hex_digits[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    if (value == 0) {
        buf[2] = '0';
        buf[3] = '\0';
        return;
    }
    char tmp[32];
    int i = 0;
    while (value > 0) {
        tmp[i++] = hex_digits[value & 0xF];
        value >>= 4;
    }
    int j = 2;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t ist, uint8_t flags) {
    idt[vector].offset_low      = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector        = 0x08;              
    idt[vector].ist             = ist & 0x07;          
    idt[vector].type_attributes = flags;
    idt[vector].offset_middle   = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high     = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].reserved        = 0;
}

static void dump_stack_frames(uint64_t start_rip, uint64_t start_rbp, bool to_screen) {
    char buf[64];
    uint64_t rip = start_rip;
    uint64_t rbp = start_rbp;

    if (to_screen) {
        dogeio_println("--- Call Stack Trace ---");
    } else {
        log("--- Call Stack Trace ---");
    }

    for (int i = 0; i < 6; i++) {
        if (rip == 0 || rbp == 0) break;

        uint64_to_hex_forward(rip, buf);
        char line[128];
        str_strcpy(line, "  #");
        char num_buf[4];
        num_buf[0] = '0' + (char)i;
        num_buf[1] = '\0';
        str_strcat(line, num_buf);
        str_strcat(line, " RIP: ");
        str_strcat(line, buf);

        if (to_screen) {
            dogeio_println(line);
        } else {
            log(line);
        }

        if (rbp < 0xFFFF800000000000ULL || (rbp & 7) != 0) {
            break;
        }

        uint64_t *frame_ptr = (uint64_t *)rbp;
        uint64_t next_rbp = frame_ptr[0];
        uint64_t next_rip = frame_ptr[1];

        // Stacks grow down, so caller's rbp must be strictly greater than current rbp
        if (next_rbp <= rbp) {
            break;
        }

        rbp = next_rbp;
        rip = next_rip;
    }
}

void isr_common_handler(interrupt_frame_t *frame) {
    char str_buf[64];
    char log_buf[128];
    int selected_option = 0;

    if ((frame->cs & 0x03u) == 3) {
        uint64_to_dec_forward(frame->vector, str_buf);
        str_strcpy(log_buf, "[User Fault] VECTOR: ");
        str_strcat(log_buf, str_buf);
        log(log_buf);

        uint64_to_hex_forward(frame->rip, str_buf);
        str_strcat(log_buf, " | RIP: ");
        str_strcat(log_buf, str_buf);
        log(log_buf);

        if (frame->vector == 14) {
            uint64_t cr2;
            __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));
            uint64_to_hex_forward(cr2, str_buf);
            str_strcat(log_buf, " | CR2: ");
            str_strcat(log_buf, str_buf);
            log(log_buf);
        }

        dogeio_println("");
        dogeio_color_change(COLOR_BRIGHT_RED);
        
        switch (frame->vector) {
            case 0:  
                dogeio_println("[Error] Divide-by-Zero error in program.");
                dogeio_println("You may revert version of the application, contact the developer");
                dogeio_println("or if you wrote the code, check ya loops. Much Error.");
                break;
            case 6:  
                dogeio_println("[Error] Invalid opcode instruction executed."); 
                dogeio_println("CPU has lower IQ than what the app expects.");
                dogeio_println("If you wrote the code, maybe use less complex instructions.");
                break;
            case 13: 
                dogeio_println("[Error] General Protection Violation."); 
                dogeio_println("App tried to touch the kernel, but kernel is pure uranium rods.");
                dogeio_println("Not Wow.");
                break;
            case 14: 
                dogeio_println("[Error] Segmentation / Page Fault violation."); 
                dogeio_println("App tried the touch memory that is NULL or not allocated.");
                dogeio_println("If you wrote the code, check your pointers and also make");
                dogeio_println("Conditions for NULL or memory access. Very Angy.");
                break;
            default: 
                dogeio_println("[Error] Application exception intercepted.");
                dogeio_println("I dont fucking know what happend, but is it broken.");
                break;
        }

        is_kernel_dead = false;
        system_dogeshell();

        while (1) {
            __asm__ __volatile__("hlt");
        }
    }

    is_kernel_dead = true;
    uint64_to_dec_forward(frame->vector, str_buf);
    str_strcpy(log_buf, "VECTOR: ");
    str_strcat(log_buf, str_buf);
    log(log_buf);

    uint64_to_hex_forward(frame->rip, str_buf);
    str_strcpy(log_buf, "RIP: ");
    str_strcat(log_buf, str_buf);
    log(log_buf);

    uint64_to_hex_forward(frame->error_code, str_buf);
    str_strcpy(log_buf, "ERR: ");
    str_strcat(log_buf, str_buf);
    log(log_buf);
    
    if (frame->vector == 14) {
        uint64_t cr2;
        __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));
        uint64_to_hex_forward(cr2, str_buf);
        str_strcpy(log_buf, "CR2: ");
        str_strcat(log_buf, str_buf);
        log(log_buf);
    }

    dump_stack_frames(frame->rip, frame->rbp, false);

    while (true) {
        dogeio_clear();
        dogeio_color_change(COLOR_WHITE);
        dogeio_background_change(COLOR_BLUE);
        dogeio_clear();

        const char* error_panic[9] = {
            "    ##",
            "##  #",
            "   #",
            "##  #",
            "    ##",
            "",
            "Your Computer has ran into a problem and needs to such restart :(",
            "Much crash, please select an option and press Enter.",
            ""
        };

        for (int i = 0; i < 9; i++) {
            dogeio_println(error_panic[i]);
        }
        
        dogeio_println(selected_option == 0 ? "-> [ Reboot Computer ]"   : "   [ Reboot Computer ]");
        dogeio_println(selected_option == 1 ? "-> [ Shutdown Computer ]" : "   [ Shutdown Computer ]");
        dogeio_println("");

        uint64_to_dec_forward(frame->vector, str_buf);
        str_strcpy(log_buf, "VECTOR: ");
        str_strcat(log_buf, str_buf);
        dogeio_println(log_buf);

        uint64_to_hex_forward(frame->rip, str_buf);
        str_strcpy(log_buf, "RIP: ");
        str_strcat(log_buf, str_buf);
        dogeio_println(log_buf);

        uint64_to_hex_forward(frame->error_code, str_buf);
        str_strcpy(log_buf, "ERR: ");
        str_strcat(log_buf, str_buf);
        dogeio_println(log_buf);

        if (frame->vector == 14) {
            uint64_t cr2;
            __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));
            uint64_to_hex_forward(cr2, str_buf);
            str_strcpy(log_buf, "CR2: ");
            str_strcat(log_buf, str_buf);
            dogeio_println(log_buf);
        }

        dogeio_println("");
        dump_stack_frames(frame->rip, frame->rbp, true);

        while (true) {
            uint16_t key = dogeio_get_key();

            if (key == KEY_UP || key == KEY_DOWN) {
                selected_option = (selected_option == 0) ? 1 : 0;
                break;
            } 
            else if (key == KEY_ENTER) {
                if (selected_option == 0) {
                    core_reboot();
                } else if (selected_option == 1) {
                    core_shutdown();
                }
            }
        }
    }
}

void init_idt(void) {
    for (int i = 0; i < 256; i++) {
        idt_set_gate((uint8_t)i, 0, 0, 0);
    }
    
    for (uint8_t vector = 0; vector < 32; vector++) {
        uint8_t ist_index = 0;
        if (vector == 8) {
            ist_index = 1;
        }
        idt_set_gate(vector, isr_stub_table[vector], ist_index, IDT_GATE_INT32);
    }
    
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;
    
    idt_load(&idtr);
}