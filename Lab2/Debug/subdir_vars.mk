################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CMD_SRCS += \
../lm3s8962.cmd 

CFG_SRCS += \
../app.cfg 

C_SRCS += \
../buttons.c \
../frame_graphics.c \
../main.c \
C:/StellarisWare/boards/ek-lm3s8962/drivers/rit128x96x4.c \
C:/StellarisWare/utils/ustdlib.c 

OBJS += \
./buttons.obj \
./frame_graphics.obj \
./main.obj \
./rit128x96x4.obj \
./ustdlib.obj 

GEN_SRCS += \
./configPkg/compiler.opt \
./configPkg/linker.cmd 

C_DEPS += \
./buttons.pp \
./frame_graphics.pp \
./main.pp \
./rit128x96x4.pp \
./ustdlib.pp 

GEN_MISC_DIRS += \
./configPkg/ 

GEN_CMDS += \
./configPkg/linker.cmd 

GEN_OPTS += \
./configPkg/compiler.opt 

GEN_SRCS__QUOTED += \
"configPkg\compiler.opt" \
"configPkg\linker.cmd" 

GEN_MISC_DIRS__QUOTED += \
"configPkg\" 

C_DEPS__QUOTED += \
"buttons.pp" \
"frame_graphics.pp" \
"main.pp" \
"rit128x96x4.pp" \
"ustdlib.pp" 

OBJS__QUOTED += \
"buttons.obj" \
"frame_graphics.obj" \
"main.obj" \
"rit128x96x4.obj" \
"ustdlib.obj" 

GEN_OPTS__FLAG += \
--cmd_file="./configPkg/compiler.opt" 

GEN_CMDS__FLAG += \
-l"./configPkg/linker.cmd" 

C_SRCS__QUOTED += \
"../buttons.c" \
"../frame_graphics.c" \
"../main.c" \
"C:/StellarisWare/boards/ek-lm3s8962/drivers/rit128x96x4.c" \
"C:/StellarisWare/utils/ustdlib.c" 


