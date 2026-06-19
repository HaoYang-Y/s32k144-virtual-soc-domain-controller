# =====================================================================
# uart_logger Makefile
# =====================================================================
# 用法:
#   make           编译
#   make clean     清理二进制（保留日志）
#   make distclean 清理二进制 + 日志文件
# =====================================================================

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall

TARGET   := uart_logger
SRC      := uart_logger.cpp
LOGDIR   := log

.PHONY: all clean distclean

all: $(TARGET)

$(TARGET): $(SRC)
	@echo "[CXX] $(TARGET)"
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	@echo "[CLEAN] $(TARGET)"
	rm -f $(TARGET)

distclean: clean
	@echo "[CLEAN] $(LOGDIR)"
	rm -rf $(LOGDIR)
