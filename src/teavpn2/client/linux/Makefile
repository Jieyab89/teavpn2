#
# SPDX-License-Identifier: GPL-2.0-only
#
# @author Ammar Faizi <ammarfaizi2@gmail.com> https://www.facebook.com/ammarfaizi2
# @license GPL-2.0-only
#
# Copyright (C) 2021  Ammar Faizi
#

DEP_DIRS += $(BASE_DEP_DIR)/src/teavpn2/client/linux

OBJ_TMP_CC := \
	$(BASE_DIR)/src/teavpn2/client/linux/udp.o \
	$(BASE_DIR)/src/teavpn2/client/linux/udp_epoll.o

OBJ_PRE_CC += $(OBJ_TMP_CC)

$(OBJ_TMP_CC):
	$(CC_PRINT)
	$(Q)$(CC) $(PIC_FLAGS) $(DEPFLAGS) $(CFLAGS) -c $(O_TO_C) -o $(@)
