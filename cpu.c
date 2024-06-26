#include "cpu.h"
#include "font.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void initialize_state(struct StateChip8 *state) {
    memset(state, 0, sizeof(struct StateChip8));
    state->PC = PROGRAM_OFFSET;
    load_font(state);
}

void load_rom(struct StateChip8 *state, uint8_t *rom, size_t rom_size) {

}

void load_font(struct StateChip8 *state) {
    uint8_t *font = font_gen(); // generate the font
    memcpy(state->memory + FONT_OFFSET, font, FONT_SIZE); // copy the font to the memory(memcpy-> dist, src, size)
}

void update_emu(struct StateChip8 *state) {
    if(state->delay_timer > 0) {
        state->delay_timer--;
    }
    if(state->sound_timer > 0) {
        state->sound_timer--;
    }
}

void draw(struct StateChip8 *state, uint16_t opcode, uint8_t vx, uint8_t vy) {
    uint8_t rx = state->V[vx]; // sprite x
    uint8_t ry = state->V[vy]; // sprite y
    uint8_t sprite_height = opcode & 0x000F;
    uint8_t sprite_row;
    state->V[0xF] = 0; // reset collision bit

    for(int y = 0; y < sprite_height; y++) {
        sprite_row = state->memory[state->I + y];
        for(int x = 0; x < 8; x++) {
            if((sprite_row & (0x80 >> x)) != 0) {
                int display_pixel_address = ((ry + y) * DISPLAY_WIDTH + rx + x) % DISPLAY_SIZE;
                if(state->display[display_pixel_address] == 1) {
                    state->V[0xF] = 1;
                }
                state->display[display_pixel_address] ^= 1;
            }
        }
    }
    state->draw_flag = 1;
}

void emulate_op(struct StateChip8 *state) {
    // fetch
    uint16_t opcode = state->memory[state->PC];
    opcode <<= 8;
    opcode |= state->memory[state->PC + 1];
    uint16_t pc_old = state->PC;
    state->PC += 2; // add 2byte(16bit) for next instruction

    // parse vx and vy
    uint8_t vx, vy;
    // get the first nibble of the opcode
    vx = (opcode & 0x0F00) >> 8;
    vy = (opcode & 0x00F0) >> 4;

    // decode the first 4 bits
    switch(opcode & 0xF000) {
        case 0x0000:
            switch(opcode & 0x00FF) {
                case 0x00E0:
                    // clear display
                    memset(state->display, 0, DISPLAY_SIZE * sizeof(uint8_t));
                    break;
                case 0x00EE:
                    state->PC = state->stack[state->SP];
                    state->SP--;
                    break;
            }
            break;
        case 0x1000:
            // jump to NNN
            state->PC = opcode & 0x0FFF;
            break;
        case 0x2000:
            // call subroutine at NNN
            state->SP++;
            state->stack[state->SP] = state->PC;
            state->PC = opcode & 0x0FFF;
            break;
        case 0x3000:
            // skip if vx == NN
            if(state->V[vx] == (opcode & 0x00FF)) {
                state->PC += 2;
            }
            break;
        case 0x4000:
            // skip if vx != NN
            if(state->V[vx] != (opcode & 0x00FF)) {
                state->PC += 2;
            }
            break;
        case 0x5000:
            // skip if vx == vy
            if(state->V[vx] == state->V[vy]) {
                state->PC += 2;
            }
            break;
        case 0x6000:
            state->V[vx] = opcode & 0x00FF; // set the register vx to NN
            break;
        case 0x7000:
            state->V[vx] += opcode & 0x00FF; // add the NN to register vx
            break;
        case 0x8000: // Logical and arithmetic instructions
            switch(opcode & 0x000F) {
                case 0x0000:
                    // vx = vy
                    state->V[vx] = state->V[vy];
                    break;
                case 0x0001:
                    // vx = vx | vy
                    state->V[vx] |= state->V[vy];
                    break;
                case 0x0002:
                    // vx = vx & vy
                    state->V[vx] &= state->V[vy];
                    break;
                case 0x0003:
                    // vx = vx XOR vy
                    state->V[vx] ^= state->V[vy];
                    break;
                case 0x0004: {
                    // vx = vx + vy, vf=carry
                    uint16_t result = state->V[vx] + state->V[vy];
                    uint8_t overflow = result > 0xFF ? 1 : 0; // if result > 255 ? 1 : 0
                    state->V[vx] = result & 0xFF;
                    state->V[0xF] = overflow;
                    break;
                }
                case 0x0005: {
                    // vx = vx - vy
                    uint8_t overflow = state->V[vx] >= state->V[vy] ? 1 : 0;
                    state->V[vx] = state->V[vx] - state->V[vy];
                    state->V[0xF] = overflow;
                    break;
                }

                case 0x0006: {
                    // store the lsb of vx in vf and shift vx to the right by 1
                    uint8_t overflow = state->V[vy] & 0x1; // get the vx's lsb
                    state->V[vx] >>= 1; // shift
                    state->V[0xF] = overflow;
                    break;
                }

                case 0x0007: {
                    // vx = vx - vy
                    uint8_t overflow = state->V[vy] >= state->V[vx] ? 1 : 0;
                    state->V[vx] = state->V[vy] - state->V[vx];
                    state->V[0xF] = overflow;
                    break;
                }
                case 0x000E: {
                    // store the msb of vx in vf and shift vx to the left by 1
                    uint8_t overflow = state->V[vy] >> 7; // get the vx's msb
                    state->V[vx] <<= 1; // shift
                    state->V[0xF] = overflow;
                    break;
                }
                default:
                    printf("unknown opcode [0x0000]: 0x%X\n", opcode);
            }
            break;
        case 0x9000:
            // skip if vx != vy
            if(state->V[vx] != state->V[vy]) {
                state->PC += 2;
            }
            break;
        case 0xA000:
            // set the index register to NNN
            state->I = opcode & 0x0FFF;
            break;
        case 0xB000:
            // jump to NNN with offset(R[0])
            state->PC = (opcode & 0x0FFF) + state->V[0];
        case 0xC000:
            // generate random num and AND it with NN
            state->V[vx] = rand() & (opcode & 0x00FF);
            break;
        case 0xD000:
            draw(state,opcode, vx, vy);
            break;
        case 0xE000:
            switch(opcode & 0x00FF) {
                case 0x009E:
                    // skip next instruction if key at vx is pressed
                    if(state->keys[state->V[vx]]) {
                        state->PC += 2;
                    }
                    break;
                case 0x00A1:
                    // skip next instruction if key at vy is pressed
                    if(!(state->keys[state->V[vx]])) {
                        state->PC += 2;
                    }
                    break;
            }
            break;
        case 0xF000:
            switch(opcode & 0x00FF) {
                case 0x0007:
                    // set vx = timer delay
                    state->V[vx] = state->delay_timer;
                    break;
                case 0x0015:
                    // set delay timer to vx
                    state->delay_timer = state->V[vx];
                    break;
                case 0x0018:
                    // set sound timer to vx
                    state->sound_timer = state->V[vx];
                    break;
                case 0x000A: {
                    // wait for a key press, store it in vx
                    int keypress = 0;
                    for(int i = 0; i < KEY_COUNT; i++) {
                        if(state->keys[i] != 0) {
                            state->V[vx] = i;
                            keypress = 1;
                            printf("keypressed:%i", i);
                            break;
                        }
                    }
                    if(!keypress) {
                        state->PC -= 2;
                    }
                    break;
                }

                case 0x001E:
                    // add vx to I
                    state->I += state->V[vx];
                    break;
                case 0x0029:
                    // set i to the location of sprite for the charactre in vx
                    state->I = state->V[vx] * 5 + FONT_OFFSET;
                    break;
                case 0x0033:
                    state->memory[state->I] = state->V[vx] / 100;
                    state->memory[state->I + 1] = (state->V[vx] / 10) % 10;
                    state->memory[state->I + 2] = (state->V[vx] % 100) % 10;
                    break;
                case 0x0055:
                    for(int i = 0; i <= vx; i++) {
                        state->memory[state->I + i] = state->V[i];
                    }
                    break;
                case 0x0065:
                    for(int i = 0; i <= vx; i++) {
                        state->V[i] = state->memory[state->I + i];
                    }
                    break;
            }
    }
}
