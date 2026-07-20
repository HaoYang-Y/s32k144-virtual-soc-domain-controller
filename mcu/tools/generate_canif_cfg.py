#!/usr/bin/env python3
"""
generate_canif_cfg.py — 从 signals.yaml 生成 CanIf 层 AUTOSAR 配置

对标 AUTOSAR CP CanIf 模块：
  - SWS_CanIf_00050: CanIf_Transmit — 按 PduId 查 CanIf_PduConfig[] → Can_Write
  - SWS_CanIf_00030: CanIf_RxIndication — CAN 帧到达 → 匹配 can_id → 查 PduId
  - SWS_CanIf_00040: CanIf_TxConfirmation — 发送完成通知

输出文件：
  - CanIf_Cfg.c     — CanIf_PduConfig[] 数组定义
  - CanIf_PduId.h   — PDU ID 宏定义

用法：
  python3 generate_canif_cfg.py signals.yaml [-o <output_dir>]
"""

import argparse
import math
import os
import sys
from collections import OrderedDict

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML required. Install: pip3 install pyyaml", file=sys.stderr)
    sys.exit(1)


# ============================================================
# 数据模型
# ============================================================

def load_signals(yaml_path):
    """加载 YAML 信号矩阵，返回信号列表"""
    with open(yaml_path, 'r', encoding='utf-8') as f:
        data = yaml.safe_load(f)
    if not data or 'signals' not in data:
        raise ValueError(f"Invalid YAML: missing top-level 'signals' key in {yaml_path}")
    return data['signals']


def group_into_pdus(signals):
    """
    按 (can_id, direction) 分组 → PDU 列表。

    每个 PDU:
      - pdu_id:   自动分配（从 0 开始）
      - can_id:   CAN 报文 ID
      - direction: 'tx' | 'rx'
      - dlc:      根据信号最大 bit 范围计算（向上取整到标准 DLC）
      - signals:  该 PDU 包含的信号列表

    AUTOSAR 概念：
      一个 PDU = 一帧 CAN 报文。同 can_id 的 tx 和 rx 是两个不同 PDU。
    """
    groups = OrderedDict()

    for sig in signals:
        can_id = sig['can_id']
        direction = sig.get('direction', 'rx')
        key = (can_id, direction)

        if key not in groups:
            groups[key] = {
                'can_id': can_id,
                'direction': direction,
                'signals': [],
            }
        groups[key]['signals'].append(sig)

    # 分配 PDU ID + 计算 DLC
    pdus = []
    for pdu_id, (key, grp) in enumerate(groups.items()):
        can_id, direction = key
        # 计算最小 DLC: max(start_bit + length) → bytes
        max_bit = 0
        for s in grp['signals']:
            end_bit = s['start_bit'] + s['length']
            if end_bit > max_bit:
                max_bit = end_bit
        min_bytes = math.ceil(max_bit / 8)
        # 标准 CAN: DLC 取 8（与硬件 mailbox 8 字节 payload 一致）
        dlc = 8 if min_bytes <= 8 else min_bytes

        pdus.append({
            'pdu_id': pdu_id,
            'can_id': can_id,
            'direction': direction,
            'dlc': dlc,
            'signals': grp['signals'],
        })

    return pdus


def pdu_macro_name(pdu):
    """生成 PDU ID 宏名称，如 CANIF_PDU_ID_TX_0x123"""
    hex_str = f"0x{pdu['can_id']:X}" if isinstance(pdu['can_id'], int) else str(pdu['can_id'])
    hex_part = hex_str.replace('0x', '0x').replace('0X', '0x')
    return f"CANIF_PDU_ID_{pdu['direction'].upper()}_{hex_str}"


# ============================================================
# 代码生成
# ============================================================

HEADER_COMMENT = """/**
 * @file    {filename}
 * @brief   [AUTOSAR CP] CanIf 配置 — 由 generate_canif_cfg.py 自动生成
 *
 * @note    数据源: mcu/config/signals.yaml
 *          不要手改此文件 — 改 YAML 后重新运行生成脚本
 */
"""


def generate_pdu_id_header(pdus, output_dir):
    """生成 CanIf_PduId.h — PDU ID 宏定义"""
    filepath = os.path.join(output_dir, 'CanIf_PduId.h')
    content = HEADER_COMMENT.format(filename='CanIf_PduId.h')
    content += """
#ifndef CANIF_PDUID_H
#define CANIF_PDUID_H

/* ===================================================================
 *  PDU ID 宏定义（对标 AUTOSAR CanIf_PduIdType）
 * =================================================================== */

"""
    for pdu in pdus:
        macro = pdu_macro_name(pdu)
        desc = f"0x{pdu['can_id']:X}" if isinstance(pdu['can_id'], int) else str(pdu['can_id'])
        content += f"/** @brief {pdu['direction'].upper()} PDU — CAN ID {desc} */\n"
        content += f"#define {macro:<40} {pdu['pdu_id']}U\n"
        content += "\n"

    content += f"/** @brief 已配置的 PDU 总数 */\n"
    content += f"#define CANIF_PDU_COUNT  {len(pdus)}U\n"
    content += "\n#endif /* CANIF_PDUID_H */\n"

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"  Generated: {filepath}  ({len(pdus)} PDUs)")
    return filepath


def generate_canif_cfg_c(pdus, output_dir):
    """生成 CanIf_Cfg.c — CanIf_PduConfig[] 数组定义"""
    filepath = os.path.join(output_dir, 'CanIf_Cfg.c')
    content = HEADER_COMMENT.format(filename='CanIf_Cfg.c')
    content += """
#include "CanIf_Cfg.h"
#include "CanIf_PduId.h"

/* ===================================================================
 *  CanIf PDU 配置表（对标 AUTOSAR CanIf_PduConfigType）
 *
 *  每个 PDU 对应一条 CAN 报文（group by can_id + direction）
 *  CanIf_Transmit() / CanIf_RxIndication() 据此查表
 * =================================================================== */

const CanIf_PduConfigType CanIf_PduConfig[CANIF_PDU_COUNT] = {
"""
    for pdu in pdus:
        macro = pdu_macro_name(pdu)
        can_id_str = f"0x{pdu['can_id']:08X}UL" if isinstance(pdu['can_id'], int) else f"{pdu['can_id']}UL"
        content += f"    {{{macro}, 0U, {can_id_str}, {pdu['dlc']}U}},"
        content += f"  /* {pdu['direction'].upper()}: CAN ID 0x{pdu['can_id']:X} */\n" if isinstance(pdu['can_id'], int) else f"  /* {pdu['direction'].upper()}: CAN ID {pdu['can_id']} */\n"

    content += """};

const uint8_t CanIf_PduConfig_Count = CANIF_PDU_COUNT;
"""

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"  Generated: {filepath}  ({len(pdus)} entries)")
    return filepath


# ============================================================
# 主入口
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description='从 signals.yaml 生成 CanIf 层 AUTOSAR 配置'
    )
    parser.add_argument('yaml_path', help='signals.yaml 路径')
    parser.add_argument('-o', '--output-dir', default=None,
                        help='输出目录（默认: YAML 同目录下的 CanIf config）')
    args = parser.parse_args()

    yaml_path = os.path.abspath(args.yaml_path)
    if not os.path.isfile(yaml_path):
        print(f"ERROR: YAML file not found: {yaml_path}", file=sys.stderr)
        sys.exit(1)

    if args.output_dir:
        output_dir = os.path.abspath(args.output_dir)
    else:
        # 默认输出到 mcu/EcuAbstraction/CanIf/config/
        script_dir = os.path.dirname(os.path.abspath(__file__))
        mcu_dir = os.path.dirname(script_dir)
        output_dir = os.path.join(mcu_dir, 'EcuAbstraction', 'CanIf', 'config')

    os.makedirs(output_dir, exist_ok=True)

    print(f"Reading:  {yaml_path}")
    signals = load_signals(yaml_path)
    print(f"  Loaded {len(signals)} signal(s)")

    pdus = group_into_pdus(signals)
    print(f"  Grouped into {len(pdus)} PDU(s):")
    for p in pdus:
        sig_names = [s['name'] for s in p['signals']]
        print(f"    PDU {p['pdu_id']}: {p['direction'].upper()} CAN 0x{p['can_id']:X} "
              f"DLC={p['dlc']} signals={sig_names}")

    print(f"\nGenerating to: {output_dir}")
    generate_pdu_id_header(pdus, output_dir)
    generate_canif_cfg_c(pdus, output_dir)

    print("\nDone. Next step: rebuild MCU firmware.")


if __name__ == '__main__':
    main()
