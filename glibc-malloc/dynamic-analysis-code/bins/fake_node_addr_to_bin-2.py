# The largebin section of this script is based on largebin_index_64.
# Therefore, it represents runtime-correctness.


# The macro, expressed as a function.
def largebin_index_64(sz):
  if ((sz >> 6) <= 48):
    return ((sz >> 6) + 48)

  elif ((sz >> 9) <= 20):
    return ((sz >> 9) + 91)

  elif ((sz >> 12) <= 10):
    return ((sz >> 12) + 110)

  elif ((sz >> 15) <= 4):
    return ((sz >> 15) + 119)

  elif ((sz >> 18) <= 2):
    return ((sz >> 18) + 124)

  else:
    return 126


# Populates the bin_size_map dictionary.
def bin2size_mapping(bin_size_map):
  sz = 1024
  SMALLBIN_WIDTH = 16

  # Since the last bin captures all the remaining sizes, we are just triggering the 126th bin.
  # I am not sure, but it doesn't seem like there is a theoretical way to find where the last bin tops, apart from what 1 bin of 2**21 width starting from BASE can contain.
  while (sz != (524288+SMALLBIN_WIDTH)):
    bin_num = largebin_index_64(sz)

    if bin_num not in bin_size_map:
      bin_size_map[bin_num] = [sz]
    else:
      bin_size_map[bin_num].append(sz)

    sz += SMALLBIN_WIDTH


# Formatted output saved directly to a file.
def generate_mapping():
  bin_size_map = dict()
  bin2size_mapping(bin_size_map)

  with open("./mapping-2.txt", "w") as f:
    f.write("The unsorted bin and the smallbins.\n\n")
    f.write("| Sr. | Bin #    | Size Class | Bin Width | Bin Headers            | Fake_node Address |\n")
    f.write("| --- | -----    | ---------- | --------- | -----------            | ----------------- |\n")
    f.write("| 1   | Bin #1   | NA         | NA        | (bins[0],   bins[1])   | <main_arena+8>    |\n")

    BIN_HDRS = 2
    ARENA_OFFSET = 24

    i = 2
    SIZE = 32
    SMALLBIN_WIDTH = 16
    for BIN_NUM in range(2, 63+1):
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      ma = f"<main_arena+{ARENA_OFFSET}>"
      f.write(f"| {i:<3} | Bin #{BIN_NUM:<3} | {SIZE:<10} | pow(2, 4) | {lb:<11} {ub:<10} | {ma:<17} |\n")

      i += 1
      SIZE += SMALLBIN_WIDTH
      BIN_HDRS += 2
      ARENA_OFFSET += 16


    f.write("\n\n")
    f.write("Largebins.\n\n")
    f.write("| Sr. | Bin # | Bin Headers            | Fake Node         | Base Class | Last Class | Fixed Classes                |\n")
    f.write("| --- | ----- | -----------            | ---------         | ---------- | ---------- | -------------                |\n")

    i = 1
    for key, value in bin_size_map.items():
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      ma = f"<main_arena+{ARENA_OFFSET}>"

      f.write(f"| {i:<3} | {key:<5} | {lb:<11} {ub:<10} | {ma:<17} | {value[0]:<10} | {value[-1]:<10} | ({len(value)}) {value} |\n")
      i += 1

      BIN_HDRS += 2
      ARENA_OFFSET += 16


generate_mapping()
