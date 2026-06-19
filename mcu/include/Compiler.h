/**
 * @file    Compiler.h
 * @brief   AUTOSAR 编译器抽象宏定义
 *
 * @note    对应 AUTOSAR_SWS_Compiler 规范
 *          定义 AUTOSAR 内存段映射宏和编译器相关关键字
 */

#ifndef COMPILER_H
#define COMPILER_H

/* 标准 AUTOSAR 内存段宏（空定义，不启用段映射） */
#define AUTOMATIC
#define TYPEDEF
#define NULL_PTR          ((void *)0)
#define INLINE            inline
#define LOCAL_INLINE      static inline

/* 函数修饰 */
#define FUNC(RetType, MemClass)             RetType
#define P2VAR(PtrType, MemClass, Id)        PtrType *
#define P2CONST(PtrType, MemClass, Id)      const PtrType *
#define CONSTP2VAR(PtrType, MemClass, Id)   PtrType * const
#define CONSTP2CONST(PtrType, MemClass, Id) const PtrType * const
#define VAR(VarType, MemClass, Id)          VarType
#define CONST(ConstType, MemClass)          const ConstType

#endif /* COMPILER_H */
