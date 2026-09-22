# Exception and IRQ entry stubs.  See kernel/idt.h for the frame that
# isr_common_stub() builds.
#
# Vectors 8, 10, 11, 12, 13, 14, 17 and 21 push an error code themselves, so
# their stubs must NOT push a dummy one.  Otherwise the frame is shifted by 8
# bytes - the handler reads the wrong rip - and iretq pops the CPU's error code
# as the return address, which triple faults.

# Define the macro for exceptions that DO NOT push an error code
.macro	ISR_NOERRCODE num
.global	_exception_handler_\num
.type	_exception_handler_\num, @function
_exception_handler_\num:
	pushq	$0	# Push a dummy error code to keep stack uniform
	pushq	$\num	# Push vector number
	jmp isr_common_stub
.endm

# Define macro for exceptions that DO push error code
.macro ISR_ERRCODE num
.global	_exception_handler_\num
.type	_exception_handler_\num, @function
_exception_handler_\num:
	pushq	$\num	# Push the vector number (error code is already pushed)
	jmp isr_common_stub
.endm

# 32 Intel architecture exceptions
ISR_NOERRCODE	0	# Divide-by-zero
ISR_NOERRCODE	1	# Debug
ISR_NOERRCODE	2	# Non-maskable Interrupt
ISR_NOERRCODE	3	# Breakpoint
ISR_NOERRCODE	4	# Overflow
ISR_NOERRCODE	5	# Bound Range Exceeded
ISR_NOERRCODE	6	# Invalid Opcode
ISR_NOERRCODE	7	# Device Not Available
ISR_ERRCODE	8	# Double Fault (pushes an error code)
ISR_NOERRCODE	9	# Coprocessor Segment Overrun
ISR_ERRCODE	10	# Invalid TSS (pushes an error code)
ISR_ERRCODE	11	# Segment Not Present (pushes an error code)
ISR_ERRCODE	12	# Stack-Segment Fault (pushes an error code)
ISR_ERRCODE	13	# General Protection Fault (pushes an error code)
ISR_ERRCODE	14	# Page Fault (pushes an error code)
ISR_NOERRCODE	15	# Reserved
ISR_NOERRCODE	16	# x87 Floating-Point Exception
ISR_ERRCODE	17	# Alignment Check (pushes an error code)
ISR_NOERRCODE	18	# Machine Check
ISR_NOERRCODE	19	# SIMD Floating-Point Exception
ISR_NOERRCODE	20	# Virtualization Exception
ISR_ERRCODE	21	# Control Protection Exception (pushes an error code)
ISR_NOERRCODE	22	# Reserved
ISR_NOERRCODE	23	# Reserved
ISR_NOERRCODE	24	# Reserved
ISR_NOERRCODE	25	# Reserved
ISR_NOERRCODE	26	# Reserved
ISR_NOERRCODE	27	# Reserved
ISR_NOERRCODE	28	# Reserved
ISR_NOERRCODE	29	# Reserved
ISR_NOERRCODE	30	# Reserved
ISR_NOERRCODE	31	# Reserved

# IRQs (PIC Handlers) - Vectors 32 to 47
ISR_NOERRCODE	32	# IRQ0: Timer
ISR_NOERRCODE	33	# IRQ1: Keyboard
# IRQs (PIC Handlers) - Vectors 32 to 47
ISR_NOERRCODE	34	# IRQ2: Cascade
ISR_NOERRCODE	35	# IRQ3: COM2
ISR_NOERRCODE	36	# IRQ4: COM1
ISR_NOERRCODE	37	# IRQ5: LPT2
ISR_NOERRCODE	38	# IRQ6: Floppy
ISR_NOERRCODE	39	# IRQ7: LPT1
ISR_NOERRCODE	40	# IRQ8: RTC
ISR_NOERRCODE	41	# IRQ9: Free
ISR_NOERRCODE	42	# IRQ10: Free
ISR_NOERRCODE	43	# IRQ11: Free
ISR_NOERRCODE	44	# IRQ12: PS/2 Mouse
ISR_NOERRCODE	45	# IRQ13: FPU
ISR_NOERRCODE	46	# IRQ14: Primary ATA
ISR_NOERRCODE	47	# IRQ15: Secondary ATA

# Vectors 48..255 have no hardware here yet, but they still need an entry: an
# interrupt on a not-present vector is delivered as #GP with the RIP of whatever
# was interrupted, which reads like a fault in the interrupted instruction.
# With an entry, c_interrupt_handler reports the actual vector instead.
ISR_NOERRCODE	48
ISR_NOERRCODE	49
ISR_NOERRCODE	50
ISR_NOERRCODE	51
ISR_NOERRCODE	52
ISR_NOERRCODE	53
ISR_NOERRCODE	54
ISR_NOERRCODE	55
ISR_NOERRCODE	56
ISR_NOERRCODE	57
ISR_NOERRCODE	58
ISR_NOERRCODE	59
ISR_NOERRCODE	60
ISR_NOERRCODE	61
ISR_NOERRCODE	62
ISR_NOERRCODE	63
ISR_NOERRCODE	64
ISR_NOERRCODE	65
ISR_NOERRCODE	66
ISR_NOERRCODE	67
ISR_NOERRCODE	68
ISR_NOERRCODE	69
ISR_NOERRCODE	70
ISR_NOERRCODE	71
ISR_NOERRCODE	72
ISR_NOERRCODE	73
ISR_NOERRCODE	74
ISR_NOERRCODE	75
ISR_NOERRCODE	76
ISR_NOERRCODE	77
ISR_NOERRCODE	78
ISR_NOERRCODE	79
ISR_NOERRCODE	80
ISR_NOERRCODE	81
ISR_NOERRCODE	82
ISR_NOERRCODE	83
ISR_NOERRCODE	84
ISR_NOERRCODE	85
ISR_NOERRCODE	86
ISR_NOERRCODE	87
ISR_NOERRCODE	88
ISR_NOERRCODE	89
ISR_NOERRCODE	90
ISR_NOERRCODE	91
ISR_NOERRCODE	92
ISR_NOERRCODE	93
ISR_NOERRCODE	94
ISR_NOERRCODE	95
ISR_NOERRCODE	96
ISR_NOERRCODE	97
ISR_NOERRCODE	98
ISR_NOERRCODE	99
ISR_NOERRCODE	100
ISR_NOERRCODE	101
ISR_NOERRCODE	102
ISR_NOERRCODE	103
ISR_NOERRCODE	104
ISR_NOERRCODE	105
ISR_NOERRCODE	106
ISR_NOERRCODE	107
ISR_NOERRCODE	108
ISR_NOERRCODE	109
ISR_NOERRCODE	110
ISR_NOERRCODE	111
ISR_NOERRCODE	112
ISR_NOERRCODE	113
ISR_NOERRCODE	114
ISR_NOERRCODE	115
ISR_NOERRCODE	116
ISR_NOERRCODE	117
ISR_NOERRCODE	118
ISR_NOERRCODE	119
ISR_NOERRCODE	120
ISR_NOERRCODE	121
ISR_NOERRCODE	122
ISR_NOERRCODE	123
ISR_NOERRCODE	124
ISR_NOERRCODE	125
ISR_NOERRCODE	126
ISR_NOERRCODE	127
ISR_NOERRCODE	128
ISR_NOERRCODE	129
ISR_NOERRCODE	130
ISR_NOERRCODE	131
ISR_NOERRCODE	132
ISR_NOERRCODE	133
ISR_NOERRCODE	134
ISR_NOERRCODE	135
ISR_NOERRCODE	136
ISR_NOERRCODE	137
ISR_NOERRCODE	138
ISR_NOERRCODE	139
ISR_NOERRCODE	140
ISR_NOERRCODE	141
ISR_NOERRCODE	142
ISR_NOERRCODE	143
ISR_NOERRCODE	144
ISR_NOERRCODE	145
ISR_NOERRCODE	146
ISR_NOERRCODE	147
ISR_NOERRCODE	148
ISR_NOERRCODE	149
ISR_NOERRCODE	150
ISR_NOERRCODE	151
ISR_NOERRCODE	152
ISR_NOERRCODE	153
ISR_NOERRCODE	154
ISR_NOERRCODE	155
ISR_NOERRCODE	156
ISR_NOERRCODE	157
ISR_NOERRCODE	158
ISR_NOERRCODE	159
ISR_NOERRCODE	160
ISR_NOERRCODE	161
ISR_NOERRCODE	162
ISR_NOERRCODE	163
ISR_NOERRCODE	164
ISR_NOERRCODE	165
ISR_NOERRCODE	166
ISR_NOERRCODE	167
ISR_NOERRCODE	168
ISR_NOERRCODE	169
ISR_NOERRCODE	170
ISR_NOERRCODE	171
ISR_NOERRCODE	172
ISR_NOERRCODE	173
ISR_NOERRCODE	174
ISR_NOERRCODE	175
ISR_NOERRCODE	176
ISR_NOERRCODE	177
ISR_NOERRCODE	178
ISR_NOERRCODE	179
ISR_NOERRCODE	180
ISR_NOERRCODE	181
ISR_NOERRCODE	182
ISR_NOERRCODE	183
ISR_NOERRCODE	184
ISR_NOERRCODE	185
ISR_NOERRCODE	186
ISR_NOERRCODE	187
ISR_NOERRCODE	188
ISR_NOERRCODE	189
ISR_NOERRCODE	190
ISR_NOERRCODE	191
ISR_NOERRCODE	192
ISR_NOERRCODE	193
ISR_NOERRCODE	194
ISR_NOERRCODE	195
ISR_NOERRCODE	196
ISR_NOERRCODE	197
ISR_NOERRCODE	198
ISR_NOERRCODE	199
ISR_NOERRCODE	200
ISR_NOERRCODE	201
ISR_NOERRCODE	202
ISR_NOERRCODE	203
ISR_NOERRCODE	204
ISR_NOERRCODE	205
ISR_NOERRCODE	206
ISR_NOERRCODE	207
ISR_NOERRCODE	208
ISR_NOERRCODE	209
ISR_NOERRCODE	210
ISR_NOERRCODE	211
ISR_NOERRCODE	212
ISR_NOERRCODE	213
ISR_NOERRCODE	214
ISR_NOERRCODE	215
ISR_NOERRCODE	216
ISR_NOERRCODE	217
ISR_NOERRCODE	218
ISR_NOERRCODE	219
ISR_NOERRCODE	220
ISR_NOERRCODE	221
ISR_NOERRCODE	222
ISR_NOERRCODE	223
ISR_NOERRCODE	224
ISR_NOERRCODE	225
ISR_NOERRCODE	226
ISR_NOERRCODE	227
ISR_NOERRCODE	228
ISR_NOERRCODE	229
ISR_NOERRCODE	230
ISR_NOERRCODE	231
ISR_NOERRCODE	232
ISR_NOERRCODE	233
ISR_NOERRCODE	234
ISR_NOERRCODE	235
ISR_NOERRCODE	236
ISR_NOERRCODE	237
ISR_NOERRCODE	238
ISR_NOERRCODE	239
ISR_NOERRCODE	240
ISR_NOERRCODE	241
ISR_NOERRCODE	242
ISR_NOERRCODE	243
ISR_NOERRCODE	244
ISR_NOERRCODE	245
ISR_NOERRCODE	246
ISR_NOERRCODE	247
ISR_NOERRCODE	248
ISR_NOERRCODE	249
ISR_NOERRCODE	250
ISR_NOERRCODE	251
ISR_NOERRCODE	252
ISR_NOERRCODE	253
ISR_NOERRCODE	254
ISR_NOERRCODE	255

# Table of all 256 entry points, so idt_init() can install them all.
.section .rodata
.align 8
.global	_exception_stub_table
_exception_stub_table:
	.quad	_exception_handler_0
	.quad	_exception_handler_1
	.quad	_exception_handler_2
	.quad	_exception_handler_3
	.quad	_exception_handler_4
	.quad	_exception_handler_5
	.quad	_exception_handler_6
	.quad	_exception_handler_7
	.quad	_exception_handler_8
	.quad	_exception_handler_9
	.quad	_exception_handler_10
	.quad	_exception_handler_11
	.quad	_exception_handler_12
	.quad	_exception_handler_13
	.quad	_exception_handler_14
	.quad	_exception_handler_15
	.quad	_exception_handler_16
	.quad	_exception_handler_17
	.quad	_exception_handler_18
	.quad	_exception_handler_19
	.quad	_exception_handler_20
	.quad	_exception_handler_21
	.quad	_exception_handler_22
	.quad	_exception_handler_23
	.quad	_exception_handler_24
	.quad	_exception_handler_25
	.quad	_exception_handler_26
	.quad	_exception_handler_27
	.quad	_exception_handler_28
	.quad	_exception_handler_29
	.quad	_exception_handler_30
	.quad	_exception_handler_31
	.quad	_exception_handler_32
	.quad	_exception_handler_33
	.quad	_exception_handler_34
	.quad	_exception_handler_35
	.quad	_exception_handler_36
	.quad	_exception_handler_37
	.quad	_exception_handler_38
	.quad	_exception_handler_39
	.quad	_exception_handler_40
	.quad	_exception_handler_41
	.quad	_exception_handler_42
	.quad	_exception_handler_43
	.quad	_exception_handler_44
	.quad	_exception_handler_45
	.quad	_exception_handler_46
	.quad	_exception_handler_47
	.quad	_exception_handler_48
	.quad	_exception_handler_49
	.quad	_exception_handler_50
	.quad	_exception_handler_51
	.quad	_exception_handler_52
	.quad	_exception_handler_53
	.quad	_exception_handler_54
	.quad	_exception_handler_55
	.quad	_exception_handler_56
	.quad	_exception_handler_57
	.quad	_exception_handler_58
	.quad	_exception_handler_59
	.quad	_exception_handler_60
	.quad	_exception_handler_61
	.quad	_exception_handler_62
	.quad	_exception_handler_63
	.quad	_exception_handler_64
	.quad	_exception_handler_65
	.quad	_exception_handler_66
	.quad	_exception_handler_67
	.quad	_exception_handler_68
	.quad	_exception_handler_69
	.quad	_exception_handler_70
	.quad	_exception_handler_71
	.quad	_exception_handler_72
	.quad	_exception_handler_73
	.quad	_exception_handler_74
	.quad	_exception_handler_75
	.quad	_exception_handler_76
	.quad	_exception_handler_77
	.quad	_exception_handler_78
	.quad	_exception_handler_79
	.quad	_exception_handler_80
	.quad	_exception_handler_81
	.quad	_exception_handler_82
	.quad	_exception_handler_83
	.quad	_exception_handler_84
	.quad	_exception_handler_85
	.quad	_exception_handler_86
	.quad	_exception_handler_87
	.quad	_exception_handler_88
	.quad	_exception_handler_89
	.quad	_exception_handler_90
	.quad	_exception_handler_91
	.quad	_exception_handler_92
	.quad	_exception_handler_93
	.quad	_exception_handler_94
	.quad	_exception_handler_95
	.quad	_exception_handler_96
	.quad	_exception_handler_97
	.quad	_exception_handler_98
	.quad	_exception_handler_99
	.quad	_exception_handler_100
	.quad	_exception_handler_101
	.quad	_exception_handler_102
	.quad	_exception_handler_103
	.quad	_exception_handler_104
	.quad	_exception_handler_105
	.quad	_exception_handler_106
	.quad	_exception_handler_107
	.quad	_exception_handler_108
	.quad	_exception_handler_109
	.quad	_exception_handler_110
	.quad	_exception_handler_111
	.quad	_exception_handler_112
	.quad	_exception_handler_113
	.quad	_exception_handler_114
	.quad	_exception_handler_115
	.quad	_exception_handler_116
	.quad	_exception_handler_117
	.quad	_exception_handler_118
	.quad	_exception_handler_119
	.quad	_exception_handler_120
	.quad	_exception_handler_121
	.quad	_exception_handler_122
	.quad	_exception_handler_123
	.quad	_exception_handler_124
	.quad	_exception_handler_125
	.quad	_exception_handler_126
	.quad	_exception_handler_127
	.quad	_exception_handler_128
	.quad	_exception_handler_129
	.quad	_exception_handler_130
	.quad	_exception_handler_131
	.quad	_exception_handler_132
	.quad	_exception_handler_133
	.quad	_exception_handler_134
	.quad	_exception_handler_135
	.quad	_exception_handler_136
	.quad	_exception_handler_137
	.quad	_exception_handler_138
	.quad	_exception_handler_139
	.quad	_exception_handler_140
	.quad	_exception_handler_141
	.quad	_exception_handler_142
	.quad	_exception_handler_143
	.quad	_exception_handler_144
	.quad	_exception_handler_145
	.quad	_exception_handler_146
	.quad	_exception_handler_147
	.quad	_exception_handler_148
	.quad	_exception_handler_149
	.quad	_exception_handler_150
	.quad	_exception_handler_151
	.quad	_exception_handler_152
	.quad	_exception_handler_153
	.quad	_exception_handler_154
	.quad	_exception_handler_155
	.quad	_exception_handler_156
	.quad	_exception_handler_157
	.quad	_exception_handler_158
	.quad	_exception_handler_159
	.quad	_exception_handler_160
	.quad	_exception_handler_161
	.quad	_exception_handler_162
	.quad	_exception_handler_163
	.quad	_exception_handler_164
	.quad	_exception_handler_165
	.quad	_exception_handler_166
	.quad	_exception_handler_167
	.quad	_exception_handler_168
	.quad	_exception_handler_169
	.quad	_exception_handler_170
	.quad	_exception_handler_171
	.quad	_exception_handler_172
	.quad	_exception_handler_173
	.quad	_exception_handler_174
	.quad	_exception_handler_175
	.quad	_exception_handler_176
	.quad	_exception_handler_177
	.quad	_exception_handler_178
	.quad	_exception_handler_179
	.quad	_exception_handler_180
	.quad	_exception_handler_181
	.quad	_exception_handler_182
	.quad	_exception_handler_183
	.quad	_exception_handler_184
	.quad	_exception_handler_185
	.quad	_exception_handler_186
	.quad	_exception_handler_187
	.quad	_exception_handler_188
	.quad	_exception_handler_189
	.quad	_exception_handler_190
	.quad	_exception_handler_191
	.quad	_exception_handler_192
	.quad	_exception_handler_193
	.quad	_exception_handler_194
	.quad	_exception_handler_195
	.quad	_exception_handler_196
	.quad	_exception_handler_197
	.quad	_exception_handler_198
	.quad	_exception_handler_199
	.quad	_exception_handler_200
	.quad	_exception_handler_201
	.quad	_exception_handler_202
	.quad	_exception_handler_203
	.quad	_exception_handler_204
	.quad	_exception_handler_205
	.quad	_exception_handler_206
	.quad	_exception_handler_207
	.quad	_exception_handler_208
	.quad	_exception_handler_209
	.quad	_exception_handler_210
	.quad	_exception_handler_211
	.quad	_exception_handler_212
	.quad	_exception_handler_213
	.quad	_exception_handler_214
	.quad	_exception_handler_215
	.quad	_exception_handler_216
	.quad	_exception_handler_217
	.quad	_exception_handler_218
	.quad	_exception_handler_219
	.quad	_exception_handler_220
	.quad	_exception_handler_221
	.quad	_exception_handler_222
	.quad	_exception_handler_223
	.quad	_exception_handler_224
	.quad	_exception_handler_225
	.quad	_exception_handler_226
	.quad	_exception_handler_227
	.quad	_exception_handler_228
	.quad	_exception_handler_229
	.quad	_exception_handler_230
	.quad	_exception_handler_231
	.quad	_exception_handler_232
	.quad	_exception_handler_233
	.quad	_exception_handler_234
	.quad	_exception_handler_235
	.quad	_exception_handler_236
	.quad	_exception_handler_237
	.quad	_exception_handler_238
	.quad	_exception_handler_239
	.quad	_exception_handler_240
	.quad	_exception_handler_241
	.quad	_exception_handler_242
	.quad	_exception_handler_243
	.quad	_exception_handler_244
	.quad	_exception_handler_245
	.quad	_exception_handler_246
	.quad	_exception_handler_247
	.quad	_exception_handler_248
	.quad	_exception_handler_249
	.quad	_exception_handler_250
	.quad	_exception_handler_251
	.quad	_exception_handler_252
	.quad	_exception_handler_253
	.quad	_exception_handler_254
	.quad	_exception_handler_255

.text
.extern c_interrupt_handler

isr_common_stub:
	# Save CPU state (Registers)
	pushq	%r15;	pushq	%r14;	pushq	%r13;	pushq	%r12
	pushq	%r11;	pushq	%r10;	pushq	%r9;	pushq	%r8
	pushq	%rbp;	pushq	%rdi;	pushq	%rsi;	pushq	%rdx
	pushq	%rcx;	pushq	%rbx;	pushq	%rax

	# Pass the stack pointer to our C handler (System V ABI uses %rdi)
	movq	%rsp, %rdi
	call c_interrupt_handler

	# Restore CPU state
	popq	%rax;	popq	%rbx;	popq	%rcx;	popq	%rdx
	popq	%rsi;	popq	%rdi;	popq	%rbp;	popq	%r8
	popq	%r9;	popq	%r10;	popq	%r11;	popq	%r12
	popq	%r13;	popq	%r14;	popq	%r15

	addq	$16, %rsp	# Clean up the error code and vector number
	iretq
