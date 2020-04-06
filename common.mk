
#.PHONY:all clean

ifeq ($(DEBUG),true)
# -g是生成调试信息。GUN调试器可以利用该信息
CC = g++ -std=c++11 -g
VERSION = debug
else
CC = g++ -std=c++11
VERSION = release
endif

#CC = gcc

#$(wildcard *.cxx)表示扫描当前目录下所有.cxx文件
#SRCS = nginx.cxx ngx_c_conf.h
SRCS = $(wildcard *.cxx)

#OBJS = nginx.o ngx_c_conf.o //这种写法太累，下行换一种写法：把字符串中的.cxx换成.o
OBJS = $(SRCS:.cxx=.o)

#字符串中的.c换成.d
DEPS = $(SRCS:.cxx=.d)

#可以指定BIN文件的位置，addprefix是增加前缀函数
BIN := $(addprefix $(BUILD_ROOT)/,$(BIN))

#定义存放ojb文件的目录，目录统一到一个位置才方便后续链接
LINK_OBJ_DIR = $(BUILD_ROOT)/app/link_obj
DEP_DIR = $(BUILD_ROOT)/app/dep

#-p是递归目录，没有就创建，有就不要创建了
$(shell mkdir -p $(LINK_OBJ_DIR))
$(shell mkdir -p $(DEP_DIR))

OBJS := $(addprefix $(LINK_OBJ_DIR)/,$(OBJS))
DEPS := $(addprefix $(DEP_DIR)/,$(DEPS))

#找到目录中的所有.o文件
LINK_OBJ = $(wildcard $(LINK_OBJ_DIR)/*.o)
#因为构建依赖关系时app目录下这个.o文件还没有构建出来，所以LINK_OBJ是缺少这个.o的，要加进来
LINK_OBJ += $(OBJS)

all:$(DEPS) $(OBJS) $(BIN)

ifneq ("$(wildcard $(DEPS))","")
include $(DEPS)
endif

$(BIN):$(LINK_OBJ)
	@echo "---------------------build $(VERSION) mode------------------------------!!!"
	$(CC) -o $@ $^ -lpthread

$(LINK_OBJ_DIR)/%.o:%.cxx
	$(CC) -I$(INCLUDE_PATH) -o $@ -c $(filter %.cxx,$^)

$(DEP_DIR)/%.d:%.cxx
	echo -n $(LINK_OBJ_DIR)/ > $@
	gcc -I$(INCLUDE_PATH) -MM $^ >> $@

