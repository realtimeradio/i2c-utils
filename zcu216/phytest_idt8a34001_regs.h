#ifndef SRC_IDT8A34001_REGS_H_ 
#define SRC_IDT8A34001_REGS_H_ 

#include <stdint.h>

#define IDT8A34001_NUM_VALUES 	517
#define IDT8A34001_MAX_LENGTH 	57
extern uint8_t idt_values [IDT8A34001_NUM_VALUES][IDT8A34001_MAX_LENGTH]; 
extern uint8_t idt_lengths [IDT8A34001_NUM_VALUES]; 

#endif /* SRC_IDT8A34001_REGS_H_ */
