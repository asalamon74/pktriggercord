LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := pktriggercord-cli
LOCAL_SRC_FILES := ../../pslr_enum.c \
	../../pslr_lens.c \
	../../pslr_model.c \
	../../pslr_scsi.c \
	../../pslr.c \
	../../pktriggercord-servermode.c \
	../../pktriggercord-cli.c
DEFINES 	:= -DANDROID -DVERSION=\"$(VERSION)\" 
LOCAL_CFLAGS  	:= $(DEFINES) -frtti -I.. -Istlport -g -fPIE
LOCAL_LDLIBS	:= -llog -lstdc++ -fPIE -pie

include $(BUILD_EXECUTABLE)
