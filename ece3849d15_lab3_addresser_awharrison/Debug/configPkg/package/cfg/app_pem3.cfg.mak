# invoke SourceDir generated makefile for app.pem3
app.pem3: .libraries,app.pem3
.libraries,app.pem3: package/cfg/app_pem3.xdl
	$(MAKE) -f C:\Users\ABBYHA~1\Documents\ece3849\ece3849d15_lab3_addresser_awharrison/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\ABBYHA~1\Documents\ece3849\ece3849d15_lab3_addresser_awharrison/src/makefile.libs clean

