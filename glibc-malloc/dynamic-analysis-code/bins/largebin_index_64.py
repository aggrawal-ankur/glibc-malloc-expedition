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


def generate_bin2size_mapping(bin_size_map):
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


def generate_table(bin_size_map):
  with open("./b2s_mapping.txt", "w") as f:
    f.write("| Sr. | Bin # | Base Class | Last Class | Fixed Classes |\n")
    f.write("| --- | ----- | ---------- | ---------- | ------------- |\n")

    i = 1
    for key, value in bin_size_map.items():
      f.write(f"| {i:<3} | {key:<5} | {value[0]:<10} | {value[-1]:<10} | ({len(value)}) {value} |\n")
      i += 1

bin_size_map = dict()

generate_bin2size_mapping(bin_size_map)
generate_table(bin_size_map)
