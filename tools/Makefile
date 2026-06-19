# =====================================================================
# tools/ 顶层 Makefile — 递归构建所有工具
# =====================================================================
# 用法:
#   make           编译所有工具
#   make clean     清理所有工具的编译产物
#   make distclean 清理所有工具的编译产物 + 日志
#
# 单独构建某个工具:
#   cd uart_logger && make
# =====================================================================

SUBDIRS := uart_logger

.PHONY: all clean distclean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	@echo "==> 进入 $@/"
	$(MAKE) -C $@

clean:
	@for dir in $(SUBDIRS); do \
		echo "==> 清理 $$dir/"; \
		$(MAKE) -C $$dir clean; \
	done

distclean:
	@for dir in $(SUBDIRS); do \
		echo "==> 深度清理 $$dir/"; \
		$(MAKE) -C $$dir distclean; \
	done
