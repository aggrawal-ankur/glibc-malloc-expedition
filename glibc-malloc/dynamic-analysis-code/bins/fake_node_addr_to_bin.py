# Generate Arrays of base sizes for each category.

SMALLBIN_WIDTH = 16
SMALL_SIZES = [SMALLBIN_WIDTH*i for i in range(2, 63+1)]

CAT1_BASE = 1024
LARGEBIN_CAT1 = [(CAT1_BASE+(64*i))  for i in range(32)]

CAT2_BASE = CAT1_BASE + (64*32)
LARGEBIN_CAT2 = [(CAT2_BASE+(512*i))  for i in range(16)]

CAT3_BASE = CAT2_BASE + (512*16)
LARGEBIN_CAT3 = [(CAT3_BASE+(4096*i))  for i in range(8)]

CAT4_BASE = CAT3_BASE + (4096*8)
LARGEBIN_CAT4 = [(CAT4_BASE+(32768*i))  for i in range(4)]

CAT5_BASE = CAT4_BASE + (32768*4)
LARGEBIN_CAT5 = [(CAT5_BASE+(262144*i))  for i in range(2)]

# Main printing logic.
def main_logic():
  print("| Bin # | BASE SIZE | Bin Headers | <main_arena+OFFSET> |")
  print("| ----- | --------- | ----------- | ------------------- |")
  print(f"| Bin #1 | Unsort. B | (bins[0], bins[1]) | <main_arena+8> |")

  BIN_HDRS = 2
  BASE_OFFSET = 24

  for BIN_NUM in range(2, 63+1):
    print(f"| Bin #{BIN_NUM} | {SMALL_SIZES[BIN_NUM-2]} | (bins[{BIN_HDRS}], bins[{BIN_HDRS+1}]) | <main_arena+{BASE_OFFSET}> |")
    BIN_HDRS += 2
    BASE_OFFSET += 16

  for BIN_NUM in range(64, 64+32):
    print(f"| Bin #{BIN_NUM} | {LARGEBIN_CAT1[BIN_NUM-64]} | (bins[{BIN_HDRS}], bins[{BIN_HDRS+1}]) | <main_arena+{BASE_OFFSET}> |")
    BIN_HDRS += 2
    BASE_OFFSET += 16

  for BIN_NUM in range(96, 96+16):
    print(f"| Bin #{BIN_NUM} | {LARGEBIN_CAT2[BIN_NUM-96]} | (bins[{BIN_HDRS}], bins[{BIN_HDRS+1}]) | <main_arena+{BASE_OFFSET}> |")
    BIN_HDRS += 2
    BASE_OFFSET += 16

  for BIN_NUM in range(112, 112+8):
    print(f"| Bin #{BIN_NUM} | {LARGEBIN_CAT3[BIN_NUM-112]} | (bins[{BIN_HDRS}], bins[{BIN_HDRS+1}]) | <main_arena+{BASE_OFFSET}> |")
    BIN_HDRS += 2
    BASE_OFFSET += 16

  for BIN_NUM in range(120, 120+4):
    print(f"| Bin #{BIN_NUM} | {LARGEBIN_CAT4[BIN_NUM-120]} | (bins[{BIN_HDRS}], bins[{BIN_HDRS+1}]) | <main_arena+{BASE_OFFSET}> |")
    BIN_HDRS += 2
    BASE_OFFSET += 16

  for BIN_NUM in range(124, 124+2):
    print(f"| Bin #{BIN_NUM} | {LARGEBIN_CAT5[BIN_NUM-124]} | (bins[{BIN_HDRS}], bins[{BIN_HDRS+1}]) | <main_arena+{BASE_OFFSET}> |")
    BIN_HDRS += 2
    BASE_OFFSET += 16

# Formatted output saved directly to a file.
def main_logic_with_pretty_print():
  with open("./mapping.txt", "w") as f:
    f.write("| Bin #    | BASE SIZE | Bin Headers            | Fake_node Address |\n")
    f.write("| -----    | --------- | -----------            | ----------------- |\n")
    f.write("| Bin #1   | NA        | (bins[0],   bins[1])   | <main_arena+8>    |\n")

    BIN_HDRS = 2
    BASE_OFFSET = 24

    for BIN_NUM in range(2, 63+1):
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      ma = f"<main_arena+{BASE_OFFSET}>"
      f.write(f"| Bin #{BIN_NUM:<3} | {SMALL_SIZES[BIN_NUM-2]:<9} | {lb:<11} {ub:<10} | {ma:<17} |\n")
      BIN_HDRS += 2
      BASE_OFFSET += 16

    for BIN_NUM in range(64, 64+32):
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      f.write(f"| Bin #{BIN_NUM:<3} | {LARGEBIN_CAT1[BIN_NUM-64]:<9} | {lb:<11} {ub:<10} | {ma:<17} |\n")
      BIN_HDRS += 2
      BASE_OFFSET += 16

    for BIN_NUM in range(96, 96+16):
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      f.write(f"| Bin #{BIN_NUM:<3} | {LARGEBIN_CAT2[BIN_NUM-96]:<9} | {lb:<11} {ub:<10} | {ma:<17} |\n")
      BIN_HDRS += 2
      BASE_OFFSET += 16

    for BIN_NUM in range(112, 112+8):
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      f.write(f"| Bin #{BIN_NUM:<3} | {LARGEBIN_CAT3[BIN_NUM-112]:<9} | {lb:<11} {ub:<10} | {ma:<17} |\n")
      BIN_HDRS += 2
      BASE_OFFSET += 16

    for BIN_NUM in range(120, 120+4):
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      f.write(f"| Bin #{BIN_NUM:<3} | {LARGEBIN_CAT4[BIN_NUM-120]:<9} | {lb:<11} {ub:<10} | {ma:<17} |\n")
      BIN_HDRS += 2
      BASE_OFFSET += 16

    for BIN_NUM in range(124, 124+2):
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      f.write(f"| Bin #{BIN_NUM:<3} | {LARGEBIN_CAT5[BIN_NUM-124]:<9} | {lb:<11} {ub:<10} | {ma:<17} |\n")
      BIN_HDRS += 2
      BASE_OFFSET += 16


main_logic_with_pretty_print()
