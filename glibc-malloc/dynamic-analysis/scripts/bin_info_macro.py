# This script is entirely based on the macros defined in malloc.c
# Therefore, it represents runtime-correctness.

# The macros, expressed as functions.

def smallbin_index(sz, SMALLBIN_WIDTH, SMALLBIN_CORRECTION):
  if (SMALLBIN_WIDTH == 16):
    return ((sz >> 4) + SMALLBIN_CORRECTION)
  else:
    return ((sz >> 3) + SMALLBIN_CORRECTION)

# Config #1
def largebin_index_32(sz):
  if ((sz >> 6) <= 38):
    return (sz >> 6) + 56

  elif ((sz >> 9) <= 20):
    return (sz >> 9) + 91

  elif ((sz >> 12) <= 10):
    return (sz >> 12) + 110

  elif ((sz >> 15) <= 4):
    return (sz >> 15) + 119

  elif ((sz >> 18) <= 2):
    return (sz >> 18) + 124

  else:
    return 126


# Config #2
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


# Config #3
# def largebin_index_32_big(sz):
#   if ((sz >> 6) <= 45):
#     return ((sz >> 6) + 49)

#   elif ((sz >> 9) <= 20):
#     return ((sz >> 9) + 91)

#   elif ((sz >> 12) <= 10):
#     return ((sz >> 12) + 110)

#   elif ((sz >> 15) <= 4):
#     return ((sz >> 15) + 119)

#   elif ((sz >> 18) <= 2):
#     return ((sz >> 18) + 124)

#   else:
#     return 126


# Orchestrator
def largebin_index(sz, arch):
  match arch:
    case "32":
      return largebin_index_32(sz)
    case "64":
      return largebin_index_64(sz)
    # case "32_big":
    #   return largebin_index_32_big(sz)


# Generate smallbins
def generate_smallbins(SBIN_WIDTH):
  return [i*SBIN_WIDTH for i in range(2, 63+1)]


# Populates the bin_size_map dictionaries.
def generate_largebins(MIN_LARGE_SIZE:int, SMALLBIN_WIDTH:int, arch:str):
  bin_size_map = dict()
  sz = MIN_LARGE_SIZE
  SBW = SMALLBIN_WIDTH

  bin_ = 64
  while (bin_ != 126):
    # Note: We only touch the 126th bin.
    bin_num = largebin_index(sz, arch)

    if bin_num not in bin_size_map:
      bin_size_map[bin_num] = [sz]
    else:
      bin_size_map[bin_num].append(sz)

    sz += SBW
    bin_ = bin_num

  return bin_size_map 


# Output saved directly to a file.
def generate_mapping(smallbins:list, largebins:dict, arch:str):
  output_file = f"./bin-info-runtime-{arch}.txt"
  with open(output_file, "w") as f:
    f.write("The unsorted bin and the smallbins.\n\n")
    f.write("| Sr. | Bin #    | Size Class | Bin Headers            | Fake_node Address |\n")
    f.write("| --- | -----    | ---------- | -----------            | ----------------- |\n")
    f.write("| 1   | Bin #1   | NA         | (bins[0],   bins[1])   | <main_arena+8>    |\n")


    sr = 2
    BIN_HDRS = 2
    ARENA_OFFSET = 24

    for sbin in smallbins:
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      ma = f"<main_arena+{ARENA_OFFSET}>"
      f.write(f"| {sr:<3} | Bin #{sr:<3} | {sbin:<10} | {lb:<11} {ub:<10} | {ma:<17} |\n")

      BIN_HDRS += 2
      ARENA_OFFSET += 16
      sr += 1

    f.write("\n\n")


    f.write("Largebins.\n\n")
    f.write("| Sr. | Bin # | Bin Headers            | Fake Node         | Base Class | Last Class | Fixed Classes |\n")
    f.write("| --- | ----- | -----------            | ---------         | ---------- | ---------- | ------------- |\n")

    sr = 1
    for bin_num, bins in largebins.items():
      lb = f"(bins[{BIN_HDRS}],"
      ub = f"bins[{BIN_HDRS+1}])"
      ma = f"<main_arena+{ARENA_OFFSET}>"

      f.write(f"| {sr:<3} | {bin_num:<5} | {lb:<11} {ub:<10} | {ma:<17} | {bins[0]:<10} | {bins[-1]:<10} | ({len(bins)}) {bins} |\n")

      BIN_HDRS += 2
      ARENA_OFFSET += 16
      sr += 1


def main():
  SMALLBINS_32 = generate_smallbins(8)
  LARGEBINS_32 = generate_largebins(512, 8, "32")
  generate_mapping(SMALLBINS_32, LARGEBINS_32, "32")

  SMALLBINS_64 = generate_smallbins(16)
  LARGEBINS_64 = generate_largebins(1024, 16, "64")
  generate_mapping(SMALLBINS_64, LARGEBINS_64, "64")

  # SMALLBINS_32_BIG = generate_smallbins(8)
  # LARGEBINS_32_BIG = generate_largebins(512, 8, "32_big")
  # generate_mapping(SMALLBINS_32_BIG, LARGEBINS_32_BIG, "32_big")


main()
