# This script is based on the bin pyramid annotation.
# Therefore, it represents theoretical correctness only.
# It doesn't include the "what's left" bin.


def generate_smallbins(SMALLBIN_WIDTH):
  return [(SMALLBIN_WIDTH*i)  for i in range(2, 63+1)]


def generate_largebins(SMALLBIN_WIDTH:int, MIN_LARGE_SIZE, LARGEBIN_CAT_COUNTS:list, NCLASSES:list):
  bin_num = 64
  BASE_SIZE = MIN_LARGE_SIZE

  LARGEBINS = dict()
  for i in range(5):
    for j in range(LARGEBIN_CAT_COUNTS[i]):
      LARGEBINS[bin_num] = [(BASE_SIZE + (SMALLBIN_WIDTH*i))  for i in range(NCLASSES[i])]
      BASE_SIZE = BASE_SIZE + SMALLBIN_WIDTH*NCLASSES[i]
      bin_num += 1

  return LARGEBINS


def generate_mapping(SMALLBINS, LARGEBINS, arch:str):
  outfile = f"./bin-info-theoretical-{arch}.txt"
  with open(outfile, "w") as f:
    f.write("The unsorted bin and the smallbins.\n\n")
    f.write("| Sr. | Bin #   | Size Class | Bin Headers            | Fake Chunk Addr   |\n")
    f.write("| --- | -----   | ---------- | -----------            | ---------------   |\n")
    f.write("| 1   | Bin #1  | NA         | (bins[0],   bins[1])   | <main_arena+8>    |\n")

    BIN_HDRS = 2
    ARENA_OFFSET = 24

    sr = 2
    for sbin in SMALLBINS:
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      ma = f"<main_arena+{ARENA_OFFSET}>"

      f.write(f"| {sr:<3} | Bin #{sr:<2} | {sbin:<10} | {lb:<11} {ub:<10} | {ma:<17} |\n")

      BIN_HDRS += 2
      ARENA_OFFSET += 16
      sr += 1

    f.write("\n\n")


    f.write("The largebins.\n\n")
    f.write("| Sr. | Bin #    | Bin Headers            | Fake Chunk Addr   | Base Class | Last Class | Fixed Classes |\n")
    f.write("| --- | -----    | -----------            | ---------------   | ---------- | ---------- | ------------- |\n")

    sr = 1
    for bin_num, bins in LARGEBINS.items():
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      ma = f"<main_arena+{ARENA_OFFSET}>"

      f.write(f"| {sr:<3} | Bin #{bin_num:<3} | {lb:<11} {ub:<10} | {ma:<17} | {bins[0]:<10} | {bins[-1]:<10} | ({len(bins)}) {bins} |\n")
      sr += 1


def main():
  SMALLBINS_32 = generate_smallbins(8)
  SMALLBINS_64 = generate_smallbins(16)

  LARGEBIN_CAT_COUNTS = [32, 16, 8, 4, 2]
  LARGEBIN_WIDTHS = [2**6, 2**9, 2**12, 2**15, 2**18]

  NCLASSES_32 = [int(LW/8)  for LW in LARGEBIN_WIDTHS]
  NCLASSES_64 = [int(LW/16)  for LW in LARGEBIN_WIDTHS]

  LARGEBINS_32 = generate_largebins(8, 512, LARGEBIN_CAT_COUNTS, NCLASSES_32)
  LARGEBINS_64 = generate_largebins(16, 1024, LARGEBIN_CAT_COUNTS, NCLASSES_64)

  generate_mapping(SMALLBINS_32, LARGEBINS_32, "32")
  generate_mapping(SMALLBINS_64, LARGEBINS_64, "64")


main()
