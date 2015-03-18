################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
third_party/lwip-1.3.2/apps/httpserver_raw/httpd.obj: C:/StellarisWare/third_party/lwip-1.3.2/apps/httpserver_raw/httpd.c $(GEN_OPTS) $(GEN_SRCS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccsv5/tools/compiler/arm_5.0.4/bin/armcl" -mv7M3 --code_state=16 --abi=eabi -me -O2 -g --include_path="C:/ti/ccsv5/tools/compiler/arm_5.0.4/include" --include_path="C:/StellarisWare/boards/ek-lm3s8962" --include_path="C:/StellarisWare" --include_path="C:/Users/Abby Harrison/Documents/ECE 3849/qs_ek-lm3s8962" --include_path="C:/StellarisWare/third_party/lwip-1.3.2/src/include" --include_path="C:/StellarisWare/third_party/lwip-1.3.2/src/include/ipv4" --include_path="C:/StellarisWare/third_party/lwip-1.3.2/apps" --include_path="C:/StellarisWare/third_party/lwip-1.3.2/ports/stellaris/include" --gcc --define=ccs="ccs" --define=PART_LM3S8962 --diag_warning=225 --display_error_number --gen_func_subsections=on --ual --preproc_with_compile --preproc_dependency="third_party/lwip-1.3.2/apps/httpserver_raw/httpd.pp" --obj_directory="third_party/lwip-1.3.2/apps/httpserver_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '


