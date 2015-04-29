## THIS IS A GENERATED FILE -- DO NOT EDIT
.configuro: .libraries,em3 linker.cmd package/cfg/app_pem3.oem3

linker.cmd: package/cfg/app_pem3.xdl
	$(SED) 's"^\"\(package/cfg/app_pem3cfg.cmd\)\"$""\"C:/Users/Abby Harrison/Documents/ece3849/ece3849d15_lab2_addresser_awharrison/.config/xconfig_app/\1\""' package/cfg/app_pem3.xdl > $@
