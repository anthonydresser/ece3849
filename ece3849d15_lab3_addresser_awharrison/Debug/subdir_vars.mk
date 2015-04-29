################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CMD_SRCS += \
../lm3s2110.cmd 

CFG_SRCS += \
../app.cfg 

C_SRCS += \
../main.c \
../network.c \
C:/StellarisWare/utils/ustdlib.c 

OBJS += \
./main.obj \
./network.obj \
./ustdlib.obj 

GEN_SRCS += \
./configPkg/compiler.opt \
./configPkg/linker.cmd 

C_DEPS += \
./main.pp \
./network.pp \
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
"main.pp" \
"network.pp" \
"ustdlib.pp" 

OBJS__QUOTED += \
"main.obj" \
"network.obj" \
"ustdlib.obj" 

GEN_OPTS__FLAG += \
--cmd_file="./configPkg/compiler.opt" 

GEN_CMDS__FLAG += \
-l"./configPkg/linker.cmd" 

C_SRCS__QUOTED += \
"../main.c" \
"../network.c" \
"C:/StellarisWare/utils/ustdlib.c" 


