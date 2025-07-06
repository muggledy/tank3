#gcc freeglut makefile
#`make` or `make SANITIZE=1`
#####################################################
target := tank.exe
project_path := ./src
#####################################################
# 默认不启用 AddressSanitizer
SANITIZE ?= 0

#project_build := $(wildcard $(project_path)/*.c)
# 使用 shell 命令递归查找 src 目录下的所有 .c 文件
project_build := $(shell find $(project_path) -name "*.c")

# 过滤掉符合排除模式的文件
# 使用 find 命令结合 shell 函数来实现递归查找和过滤
exclude_files := $(shell find $(project_path) -name "not_make_*.c")
project_build := $(filter-out $(exclude_files), $(project_build))

project_build_o := $(patsubst %.c,%.o,$(project_build))

SDL_FLAGS = $(shell pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_mixer libevent)

# 指定头文件搜索路径列表(针对的是project_path变量目录下的.c文件)
#header_paths := ./src/view ./src/event
header_paths := $(shell find $(project_path) -mindepth 1 -type d -printf '%p\n')
# 生成 -I 选项字符串
CFLAGS = $(foreach path,$(header_paths),-I"$(path)") -g
LDFLAGS = -lm -lbsd -pthread # 添加链接数学库等选项

ifeq ($(SANITIZE), 1)
    CFLAGS += -fsanitize=address
    LDFLAGS += -fsanitize=address
    $(info AddressSanitizer(ASan) Enabled!)
else
    $(info AddressSanitizer(ASan) Disabled)
endif

# 默认目标
all: $(target)

$(target): $(project_build_o)
	@echo "正在链接 $(target)..."
	gcc $^ \
	$(SDL_FLAGS) \
	-o $@ $(LDFLAGS) \
	&& echo "成功: $(target) 已生成" \
		|| (echo "失败: 无法生成 $(target)" && false)

$(project_path)/%.o:$(project_path)/%.c
	gcc -c $< -o $@ $(CFLAGS)

clean:
	@echo "Cleaning..."
	@find $(project_path) -name "*.o" -type f -delete
	rm $(target)

.PHONY: all clean

$(info all .c files: $(project_build))
# 打印过滤后的待编译文件列表
$(info Filtered project source file(.c): $(project_build))
$(info Filtered project build file(.o): $(project_build_o))
$(info CFLAGS: $(CFLAGS))
$(info LDFLAGS: $(LDFLAGS))
