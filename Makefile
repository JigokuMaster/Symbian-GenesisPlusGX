prebuild:
	cd group && bldmake bldfiles gcce urel

build:
	cd group && abld build -v gcce urel

clean: 
	cd group && abld reallyclean gcce urel

sis:
	cd sis && makesis -v -d$(EPOCROOT) *.pkg



APP_NAME=GenesisPlusGX
SIS="$(APP_NAME)_gcce.sis"
EXE=$(APP_NAME).exe
EXE_FP=$(EPOCROOT)/epoc32/release/gcce/urel/$(EXE)

mksis:
	cd sis && PLATFORM=gcce TARGET=urel makesis -v -d$(EPOCROOT) $(APP_NAME)_gcce.pkg

mksisx:
	cd sis && signsis $(APP_NAME)_gcce.sis $(APP_NAME)_gcce.sisx mycert.cer mykey.key

depoly:
	renv send "sis/$(SIS)" "D:\\$(SIS)"
	renv send "sis/$(SIS)x" "D:\\$(SIS)x"
run:
	renv send "$(EXE_FP)" "C:\\sys\\bin\\$(EXE)"
	renv start -w $(EXE)




